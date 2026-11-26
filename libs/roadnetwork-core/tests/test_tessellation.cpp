// SPDX-License-Identifier: Apache-2.0
// ADR 0004 section 3: on a hand-authored road with a non-zero lane offset,
// elevation, and width b, c, d terms, every sampled border and centerline
// stays within the tolerance of the analytic curve at the midpoint of every
// tessellation interval.
#include "openstrata/rn/Geometry.h"
#include "openstrata/rn/RoadNetworkData.h"
#include "openstrata/rn/Tessellation.h"
#include "openstrata/rn/Validation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace openstrata::rn;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

Lane MakeLane(std::int32_t number, std::vector<CubicRecord> width = {}) {
  Lane lane;
  lane.number = number;
  lane.type = number == 0 ? "none" : "driving";
  lane.width.records = std::move(width);
  return lane;
}

PlanViewSegment Next(const PlanViewSegment& previous, double length, double curvature) {
  const Pose2 end = EvaluateSegment(previous, previous.s + previous.length);
  PlanViewSegment next{previous.s + previous.length, end.x, end.y, end.hdg, length, Line{}};
  if (curvature != 0.0) {
    next.shape = Arc{curvature};
  }
  return next;
}

// A record from `s0` that continues `previous` in value and slope, with its
// own c and d terms.
CubicRecord Continue(const CubicRecord& previous, double s0, double c, double d) {
  const double u = s0 - previous.s0;
  const double value = previous.a + u * (previous.b + u * (previous.c + u * previous.d));
  const double slope = previous.b + u * (2.0 * previous.c + 3.0 * u * previous.d);
  return CubicRecord{s0, value, slope, c, d};
}

// A 120 m road far from the origin: a line, a left arc that turns by 1.6 rad,
// and a tighter right arc, in three lane sections. Every profile has
// non-zero b, c, and d terms, and records start inside sections. Records
// continue each other, as sampling a jump has no meaningful chord error.
Road MakeFixture() {
  Road road;
  road.id = "fixture";
  road.length = 120.0;
  PlanViewSegment line{0.0, 512000.25, 4100000.75, 0.3, 30.0, Line{}};
  const PlanViewSegment left = Next(line, 40.0, 0.04);
  const PlanViewSegment right = Next(left, 50.0, -0.05);
  road.referenceLine.segments = {line, left, right};

  const CubicRecord elevation{0.0, 12.0, 0.02, 1e-4, -2e-6};
  road.elevation.records = {elevation, Continue(elevation, 55.0, 2e-4, -1e-6)};
  const CubicRecord offset{0.0, 0.0, 0.01, 1e-3, -2e-5};
  road.laneOffset.records = {offset, Continue(offset, 45.0, 5e-4, 1e-6)};

  LaneSection first;
  first.lanes = {
      MakeLane(1, {CubicRecord{0.0, 3.0, 0.02, 1e-3, -1e-5}}),
      MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 3.5, -0.01, 5e-4, -4e-6}}),
      MakeLane(-2, {CubicRecord{0.0, 2.0, 0.0, 0.0, 0.0}, CubicRecord{12.0, 2.0, 0.05, -2e-3, 3e-5}}),
  };
  LaneSection second;
  second.s = 40.0;
  second.lanes = {
      MakeLane(2, {CubicRecord{0.0, 1.0, 0.03, 0.0, -2e-5}}),
      MakeLane(1, {CubicRecord{0.0, 3.2, 0.0, 1e-3, -3e-5}}),
      MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 3.4, 0.0, 0.0, 0.0}, CubicRecord{20.0, 3.4, 0.01, 0.0, 1e-5}}),
  };
  LaneSection third;
  third.s = 90.0;
  third.lanes = {
      MakeLane(1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}}),
      MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 3.5, 0.02, -5e-4, 0.0}}),
  };
  road.sections = {first, second, third};
  return road;
}

double Distance(const Point3& a, const Point3& b) {
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

Point3 Midpoint(const Point3& a, const Point3& b) {
  return Point3{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0, (a.z + b.z) / 2.0};
}

// The distance from `p` to the segment from `a` to `b`.
double DistanceToChord(const Point3& p, const Point3& a, const Point3& b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double dz = b.z - a.z;
  const double length2 = dx * dx + dy * dy + dz * dz;
  double f = length2 > 0.0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy + (p.z - a.z) * dz) / length2 : 0.0;
  f = std::min(1.0, std::max(0.0, f));
  return Distance(p, Point3{a.x + f * dx, a.y + f * dy, a.z + f * dz});
}

struct Measure {
  double maxError = 0.0;        ///< Chord midpoint to the analytic midpoint.
  double maxChordDistance = 0.0; ///< Analytic curve to the chord.
  std::size_t vertices = 0;
};

// How the error of an interval is judged against the tolerance.
enum class Metric {
  /// The chord's midpoint against the analytic point at the middle `s`, the
  /// ADR 0004 evidence. Along a curve whose `s` is not the distance, such as
  /// a ParamPoly3, the two midpoints also differ along the chord.
  Midpoint,
  /// The distance from the analytic curve, at seven points inside the
  /// interval, to the chord: the tolerance's definition.
  ChordDistance,
};

// Checks the structure of `sampled` and the error of every curve.
Measure CheckSampled(const Road& road, const SampledRoad& sampled, double tolerance,
    const std::string& what, Metric metric = Metric::Midpoint) {
  Measure measure;
  Check(sampled.roadId == road.id, what + ": road id");
  Check(sampled.sections.size() == road.sections.size(), what + ": one sampled section per section");
  for (std::size_t si = 0; si < sampled.sections.size() && si < road.sections.size(); ++si) {
    const SampledSection& section = sampled.sections[si];
    const LaneSection& source = road.sections[si];
    const std::string where = what + " section " + std::to_string(si);
    const double end = si + 1 < road.sections.size() ? road.sections[si + 1].s : road.length;
    Check(section.sectionIndex == si, where + ": section index");
    Check(section.stations.size() >= 2, where + ": at least two stations");
    Check(section.stations.front() == source.s && section.stations.back() == end,
        where + ": stations span the section");
    Check(std::adjacent_find(section.stations.begin(), section.stations.end(),
              [](double a, double b) { return !(a < b); }) == section.stations.end(),
        where + ": stations ascend strictly");

    // The center border, then each non-center lane in source order.
    std::vector<std::int32_t> expected{0};
    for (const Lane& lane : source.lanes) {
      if (lane.number != 0) {
        expected.push_back(lane.number);
      }
    }
    Check(section.borders.size() == expected.size(), where + ": one border per lane plus the center");
    Check(section.centerlines.size() + 1 == expected.size(), where + ": one centerline per lane");
    for (std::size_t i = 0; i < section.borders.size() && i < expected.size(); ++i) {
      Check(section.borders[i].laneNumber == expected[i], where + ": border order");
    }
    for (std::size_t i = 0; i < section.centerlines.size() && i + 1 < expected.size(); ++i) {
      Check(section.centerlines[i].laneNumber == expected[i + 1], where + ": centerline order");
    }

    const auto checkCurve = [&](const SampledCurve& curve, bool isCenterline) {
      const std::string name =
          where + (isCenterline ? " centerline " : " border ") + std::to_string(curve.laneNumber);
      Check(curve.points.size() == section.stations.size(), name + ": one point per station");
      if (curve.points.size() != section.stations.size()) {
        return;
      }
      const auto analytic = [&](double s) {
        return isCenterline ? LaneCenterPoint(road, si, curve.laneNumber, s)
                            : BorderPoint(road, si, curve.laneNumber, s);
      };
      for (std::size_t k = 0; k < curve.points.size(); ++k) {
        if (Distance(curve.points[k], analytic(section.stations[k])) > 1e-9) {
          Check(false, name + ": vertex " + std::to_string(k) + " is on the analytic curve");
        }
      }
      for (std::size_t k = 0; k + 1 < curve.points.size(); ++k) {
        const double s0 = section.stations[k];
        const double s1 = section.stations[k + 1];
        const double mid = 0.5 * (s0 + s1);
        const double midpoint = Distance(Midpoint(curve.points[k], curve.points[k + 1]), analytic(mid));
        double chord = 0.0;
        for (int j = 1; j < 8; ++j) {
          const double s = s0 + (s1 - s0) * j / 8.0;
          chord = std::max(chord, DistanceToChord(analytic(s), curve.points[k], curve.points[k + 1]));
        }
        measure.maxError = std::max(measure.maxError, midpoint);
        measure.maxChordDistance = std::max(measure.maxChordDistance, chord);
        const double error = metric == Metric::Midpoint ? midpoint : chord;
        if (error > tolerance) {
          std::fprintf(stderr, "FAIL: %s: interval %zu at s = %.6f is %.6g m off (tolerance %g)\n",
              name.c_str(), k, mid, error, tolerance);
          ++failures;
        }
      }
      measure.vertices += curve.points.size();
    };
    for (const SampledCurve& curve : section.borders) {
      checkCurve(curve, false);
    }
    for (const SampledCurve& curve : section.centerlines) {
      checkCurve(curve, true);
    }
  }
  return measure;
}

bool Contains(const std::vector<double>& values, double value) {
  return std::any_of(values.begin(), values.end(),
      [&](double v) { return std::abs(v - value) < 1e-9; });
}

void TestFixtureWithinTolerance() {
  const Road road = MakeFixture();
  RoadNetworkData data;
  data.roads = {road};
  const auto failure = Validate(data);
  Check(!failure, "the fixture is valid" + (failure ? ": " + failure->message : std::string()));

  for (double tolerance : {0.01, 0.001}) {
    TessellationOptions options;
    options.toleranceMetres = tolerance;
    const SampledRoad sampled = TessellateRoad(road, options);
    const std::string what = "tolerance " + std::to_string(tolerance);
    const Measure measure = CheckSampled(road, sampled, tolerance, what);
    std::size_t stations = 0;
    for (const SampledSection& section : sampled.sections) {
      stations += section.stations.size();
    }
    std::printf("%s: %zu stations, %zu vertices, max midpoint error %.3g m\n", what.c_str(),
        stations, measure.vertices, measure.maxError);
    Check(measure.maxError > tolerance / 100.0, what + ": the bound is not wildly conservative");
  }

  // Every ADR 0004 station is a sample: segment boundaries (30, 70), the
  // lane-offset and elevation record starts (45, 55), and width record
  // starts (12 in section 0, 60 in section 1).
  const SampledRoad sampled = TessellateRoad(road);
  if (sampled.sections.size() == 3) {
    Check(Contains(sampled.sections[0].stations, 12.0), "width record start is a station");
    Check(Contains(sampled.sections[0].stations, 30.0), "segment boundary is a station");
    Check(Contains(sampled.sections[1].stations, 45.0), "lane-offset record start is a station");
    Check(Contains(sampled.sections[1].stations, 55.0), "elevation record start is a station");
    Check(Contains(sampled.sections[1].stations, 60.0), "width record start in section 1 is a station");
    Check(Contains(sampled.sections[1].stations, 70.0), "segment boundary in section 1 is a station");
  }

  // A tighter tolerance never needs fewer samples.
  TessellationOptions fine;
  fine.toleranceMetres = 0.001;
  const SampledRoad finer = TessellateRoad(road, fine);
  for (std::size_t i = 0; i < sampled.sections.size() && i < finer.sections.size(); ++i) {
    Check(finer.sections[i].stations.size() >= sampled.sections[i].stations.size(),
        "1 mm needs at least as many stations as 1 cm in section " + std::to_string(i));
  }
}

void TestStraightConstantRoad() {
  // A line with constant widths and elevation needs one part per interval.
  Road road;
  road.id = "straight";
  road.length = 500.0;
  road.referenceLine.segments = {PlanViewSegment{0.0, 0.0, 0.0, 1.0, 300.0, Line{}},
      PlanViewSegment{300.0, 300.0 * std::cos(1.0), 300.0 * std::sin(1.0), 1.0, 200.0, Line{}}};
  road.elevation.records = {CubicRecord{0.0, 4.0, 0.0, 0.0, 0.0}};
  LaneSection a;
  a.lanes = {MakeLane(1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}}), MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}})};
  LaneSection b = a;
  b.s = 100.0;
  road.sections = {a, b};

  const SampledRoad sampled = TessellateRoad(road);
  Check(sampled.sections.size() == 2, "straight road: two sections");
  if (sampled.sections.size() == 2) {
    Check(sampled.sections[0].stations == std::vector<double>{0.0, 100.0},
        "straight road: section 0 is one interval");
    Check(sampled.sections[1].stations == std::vector<double>{100.0, 300.0, 500.0},
        "straight road: section 1 is cut only at the segment boundary");
  }
  CheckSampled(road, sampled, 1e-9, "straight road");

  // A linear width also needs no subdivision: its f'' is zero.
  road.sections[1].lanes[0].width.records[0].b = 0.01;
  const SampledRoad tapered = TessellateRoad(road);
  if (tapered.sections.size() == 2) {
    Check(tapered.sections[1].stations.size() == 3, "a linear width taper needs one part");
  }
}

void TestArcAloneAndLanesShareVertices() {
  // A constant-width arc: the bound reduces to R_out (1 - cos(kappa h / 2)).
  Road road;
  road.id = "arc";
  road.length = 100.0;
  road.referenceLine.segments = {PlanViewSegment{0.0, 0.0, 0.0, 0.0, 100.0, Arc{0.02}}};
  LaneSection only;
  only.lanes = {MakeLane(1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}}), MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}}),
      MakeLane(-2, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}})};
  road.sections = {only};
  const SampledRoad sampled = TessellateRoad(road);
  CheckSampled(road, sampled, 0.01, "constant-width arc");
  if (sampled.sections.size() == 1) {
    // R_out = 50 + 6 and kappa h / 2 = 1 / n: the smallest n with
    // 56 (1 - cos(1 / n)) <= 0.01 is 53.
    Check(sampled.sections[0].stations.size() == 54, "constant-width arc: 53 parts");
    const SampledSection& section = sampled.sections[0];
    // Lane -2's centerline lies between borders -1 and -2, vertex by vertex.
    if (section.borders.size() == 4 && section.centerlines.size() == 3) {
      const SampledCurve& inner = section.borders[2];
      const SampledCurve& outer = section.borders[3];
      const SampledCurve& center = section.centerlines[2];
      bool shared = true;
      for (std::size_t k = 0; k < center.points.size(); ++k) {
        shared = shared && Distance(center.points[k], Midpoint(inner.points[k], outer.points[k])) == 0.0;
      }
      Check(shared, "a centerline is built from the shared vertices of its borders");
    }
  }
}

// The segment after `previous`, at its end pose, with the given shape.
PlanViewSegment After(const PlanViewSegment& previous, double length,
    std::variant<Line, Arc, Spiral, ParamPoly3> shape) {
  const Pose2 end = EvaluateSegment(previous, previous.s + previous.length);
  return PlanViewSegment{previous.s + previous.length, end.x, end.y, end.hdg, length, shape};
}

// ADR 0010 evidence: a 210 m road far from the origin with a line, a spiral
// into an arc, a spiral whose curvature changes sign, a normalized
// paramPoly3 with an inflection, and an arcLength paramPoly3 that turns by
// 243 degrees, in two lane sections with varying offset, elevation, and
// widths.
Road MakeCurveFixture(double* inflection, double* spiralZero) {
  Road road;
  road.id = "curves";
  const PlanViewSegment line{0.0, 512000.25, 4100000.75, 0.3, 20.0, Line{}};
  const PlanViewSegment entry = After(line, 30.0, Spiral{0.0, 0.04});
  const PlanViewSegment arc = After(entry, 20.0, Arc{0.04});
  const PlanViewSegment reverse = After(arc, 35.0, Spiral{0.04, -0.03});
  *spiralZero = reverse.s + 20.0; // 0.04 / 0.07 of 35 m.

  // v'' changes sign at p = 1/2.
  ParamPoly3 s;
  s.bU = 60.0;
  s.cV = 12.0;
  s.dV = -8.0;
  s.pRange = ParamRange::Normalized;
  PlanViewSegment wave = After(reverse, 60.0, s);
  wave.length = CurveLength(wave); // Normalized: the curve does not depend on it.
  *inflection = wave.s + wave.length / 2.0;

  // u' = 40 (1 - 2p^2), v' = 80 p: u'v'' - v'u'' = 1600 (2 + 4p^2) > 0
  // throughout, the speed grows from 40 to 40 sqrt(5), and the tangent
  // turns by 116.6 degrees, more than one part may take.
  ParamPoly3 loop;
  loop.bU = 40.0;
  loop.dU = -80.0 / 3.0;
  loop.cV = 40.0;
  PlanViewSegment turn = After(wave, 60.0, loop); // No pRange: normalized.
  turn.length = CurveLength(turn);
  road.referenceLine.segments = {line, entry, arc, reverse, wave, turn};
  road.length = turn.s + turn.length;

  const CubicRecord elevation{0.0, 3.0, 0.01, 2e-4, -1e-6};
  road.elevation.records = {elevation, Continue(elevation, 110.0, -1e-4, 5e-7)};
  const CubicRecord offset{0.0, 0.0, 0.005, 2e-4, -2e-6};
  road.laneOffset.records = {offset, Continue(offset, 95.0, 1e-4, -1e-6)};

  LaneSection first;
  first.lanes = {
      MakeLane(2, {CubicRecord{0.0, 1.5, 0.0, 0.0, 0.0}}),
      MakeLane(1, {CubicRecord{0.0, 2.0, 0.01, 1e-4, -1e-6}}),
      MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 2.0, -0.005, 2e-4, -2e-6}}),
  };
  LaneSection second;
  second.s = 130.0;
  second.lanes = {
      MakeLane(1, {CubicRecord{0.0, 2.0, 0.0, 1e-4, -1e-6}}),
      MakeLane(0),
      MakeLane(-1, {CubicRecord{0.0, 2.0, 0.0, 0.0, 0.0}, CubicRecord{60.0, 2.0, 0.02, -1e-3, 0.0}}),
  };
  road.sections = {first, second};
  return road;
}

void TestCurvesWithinTolerance() {
  double inflection = 0.0;
  double spiralZero = 0.0;
  const Road road = MakeCurveFixture(&inflection, &spiralZero);
  RoadNetworkData data;
  data.roads = {road};
  std::vector<ValidationWarning> warnings;
  const auto failure = Validate(data, &warnings);
  Check(!failure, "the curve fixture is valid" + (failure ? ": " + failure->message : std::string()));
  for (const ValidationWarning& warning : warnings) {
    Check(false, "the curve fixture raises no warning: " + warning.message);
  }

  for (double tolerance : {0.01, 0.001}) {
    TessellationOptions options;
    options.toleranceMetres = tolerance;
    const SampledRoad sampled = TessellateRoad(road, options);
    const std::string what = "curves, tolerance " + std::to_string(tolerance);
    const Measure measure = CheckSampled(road, sampled, tolerance, what, Metric::ChordDistance);
    std::size_t stations = 0;
    for (const SampledSection& section : sampled.sections) {
      stations += section.stations.size();
    }
    std::printf("%s: %zu stations, %zu vertices, max distance to the chord %.3g m (midpoint error %.3g m)\n",
        what.c_str(), stations, measure.vertices, measure.maxChordDistance, measure.maxError);
    Check(measure.maxChordDistance > tolerance / 100.0, what + ": the bound is not wildly conservative");
  }

  // ADR 0010 section 4: the zero of the spiral's curvature and the
  // paramPoly3 inflection are stations, as are the segment boundaries.
  const SampledRoad sampled = TessellateRoad(road);
  if (sampled.sections.size() == 2) {
    Check(Contains(sampled.sections[0].stations, spiralZero), "the spiral's curvature zero is a station");
    Check(Contains(sampled.sections[1].stations, inflection), "the paramPoly3 inflection is a station");
    for (const PlanViewSegment& segment : road.referenceLine.segments) {
      const std::size_t section = segment.s < 130.0 ? 0 : 1;
      if (segment.s > 0.0 && std::abs(segment.s - 130.0) > 1e-9) {
        Check(Contains(sampled.sections[section].stations, segment.s),
            "segment boundary at s = " + std::to_string(segment.s) + " is a station");
      }
    }
  }
}

// A spiral and a paramPoly3 with constant widths and no curvature need one
// part each, as a line does.
void TestStraightCurves() {
  Road road;
  road.id = "straight curves";
  ParamPoly3 straight;
  straight.bU = 1.0;
  straight.pRange = ParamRange::ArcLength;
  road.referenceLine.segments = {PlanViewSegment{0.0, 0.0, 0.0, 0.7, 40.0, Spiral{0.0, 0.0}},
      PlanViewSegment{40.0, 40.0 * std::cos(0.7), 40.0 * std::sin(0.7), 0.7, 60.0, straight}};
  road.length = 100.0;
  LaneSection only;
  only.lanes = {MakeLane(1, {CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0}}), MakeLane(0)};
  road.sections = {only};
  const SampledRoad sampled = TessellateRoad(road);
  Check(sampled.sections.size() == 1, "straight curves: one section");
  if (sampled.sections.size() == 1) {
    Check(sampled.sections[0].stations == std::vector<double>{0.0, 40.0, 100.0},
        "straight curves: one part per segment");
  }
  CheckSampled(road, sampled, 1e-9, "straight curves");
}

} // namespace

int main() {
  TestFixtureWithinTolerance();
  TestStraightConstantRoad();
  TestArcAloneAndLanesShareVertices();
  TestCurvesWithinTolerance();
  TestStraightCurves();

  if (failures == 0) {
    std::printf("roadnetworkCore tessellation: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
