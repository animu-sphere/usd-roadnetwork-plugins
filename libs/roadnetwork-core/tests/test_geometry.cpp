// SPDX-License-Identifier: Apache-2.0
// ADR 0004 section 2: line and arc poses at known s against closed-form values,
// piecewise cubics, and lane borders.
#include "openstrata/rn/Geometry.h"
#include "openstrata/rn/RoadNetworkData.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace openstrata::rn;

constexpr double kPi = 3.141592653589793238462643383279;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

void Near(double actual, double expected, double tolerance, const std::string& what) {
  if (!(std::abs(actual - expected) <= tolerance)) {
    std::fprintf(stderr, "FAIL: %s: %.17g, expected %.17g (tolerance %g)\n", what.c_str(), actual,
        expected, tolerance);
    ++failures;
  }
}

void NearPose(const Pose2& actual, double x, double y, double hdg, double tolerance,
    const std::string& what) {
  Near(actual.x, x, tolerance, what + " x");
  Near(actual.y, y, tolerance, what + " y");
  Near(actual.hdg, hdg, 1e-12, what + " heading");
}

// Positions are tens of metres here, so 1e-9 m is about 1e5 ulp of slack
// for the closed forms, which cancel badly as the curvature shrinks.
constexpr double kPositionTolerance = 1e-9;

void TestLine() {
  const PlanViewSegment line{5.0, 1.0, 2.0, 0.7, 10.0, Line{}};
  Check(Curvature(line) == 0.0, "a line has zero curvature");
  for (double u : {0.0, 2.5, 10.0, 12.0}) {
    NearPose(EvaluateSegment(line, 5.0 + u), 1.0 + u * std::cos(0.7), 2.0 + u * std::sin(0.7), 0.7,
        kPositionTolerance, "line at u = " + std::to_string(u));
  }
}

// The circle through the start, from its center: exact, and well conditioned
// for the curvatures used with it.
Pose2 CircleAt(const PlanViewSegment& g, double kappa, double u) {
  const double cx = g.x - std::sin(g.hdg) / kappa;
  const double cy = g.y + std::cos(g.hdg) / kappa;
  const double h = g.hdg + kappa * u;
  return Pose2{cx + std::sin(h) / kappa, cy - std::cos(h) / kappa, h};
}

void TestArc() {
  // A left arc that turns by 5 rad (more than pi) over 100 m.
  const PlanViewSegment left{0.0, 3.0, -4.0, 0.3, 100.0, Arc{0.05}};
  Check(Curvature(left) == 0.05, "an arc reports its curvature");
  for (double u : {0.0, 1.0, 31.41592653589793, 62.83185307179586, 77.0, 100.0}) {
    const Pose2 expected = CircleAt(left, 0.05, u);
    NearPose(EvaluateSegment(left, u), expected.x, expected.y, expected.hdg, kPositionTolerance,
        "left arc at u = " + std::to_string(u));
  }
  // At a half turn the arc is a diameter away from its start, heading back.
  const Pose2 half = EvaluateSegment(left, kPi / 0.05);
  Near(std::hypot(half.x - 3.0, half.y + 4.0), 2.0 / 0.05, kPositionTolerance, "half turn: diameter");
  Near(half.hdg, 0.3 + kPi, 1e-12, "half turn: heading");

  // A right arc that starts mid-road and turns by more than a full circle.
  const PlanViewSegment right{40.0, -100.0, 250.0, -2.0, 80.0, Arc{-0.1}};
  for (double u : {0.0, 15.0, 2.0 * kPi / 0.1, 80.0}) {
    const Pose2 expected = CircleAt(right, -0.1, u);
    NearPose(EvaluateSegment(right, 40.0 + u), expected.x, expected.y, expected.hdg,
        kPositionTolerance, "right arc at u = " + std::to_string(u));
  }
  const Pose2 full = EvaluateSegment(right, 40.0 + 2.0 * kPi / 0.1);
  Near(full.x, -100.0, kPositionTolerance, "full circle returns to its start x");
  Near(full.y, 250.0, kPositionTolerance, "full circle returns to its start y");
}

// The Taylor expansion of the arc in kappa, to the kappa^2 term. Its error is
// about kappa^3 u^4 / 24, negligible for the curvatures below.
Pose2 ShallowArcAt(const PlanViewSegment& g, double kappa, double u) {
  const double c = std::cos(g.hdg);
  const double s = std::sin(g.hdg);
  const double ku = kappa * u;
  return Pose2{g.x + u * (c - ku / 2.0 * s - ku * ku / 6.0 * c),
      g.y + u * (s + ku / 2.0 * c - ku * ku / 6.0 * s), g.hdg + ku};
}

void TestShallowArc() {
  // |kappa| < 1e-8: the sinc Taylor branch. The arc still bows by
  // kappa u^2 / 2 = 4.5e-6 m at u = 30, which a line would miss.
  for (double kappa : {1e-8 * 0.99, -5e-9, 1e-12, 0.0}) {
    const PlanViewSegment g{0.0, 10.0, 20.0, 1.1, 30.0, Arc{kappa}};
    for (double u : {0.0, 7.0, 30.0}) {
      const Pose2 expected = ShallowArcAt(g, kappa, u);
      NearPose(EvaluateSegment(g, u), expected.x, expected.y, expected.hdg, 1e-12,
          "kappa " + std::to_string(kappa) + " at u = " + std::to_string(u));
    }
  }
  // Either side of the |kappa u / 2| = 1e-4 branch point agrees with the circle.
  for (double kappa : {0.99e-4 / 50.0, 1.01e-4 / 50.0}) {
    const PlanViewSegment g{0.0, 0.0, 0.0, 0.4, 100.0, Arc{kappa}};
    const Pose2 expected = CircleAt(g, kappa, 100.0);
    NearPose(EvaluateSegment(g, 100.0), expected.x, expected.y, expected.hdg, 1e-9,
        "branch point side kappa " + std::to_string(kappa));
  }
}

void TestReferenceLine() {
  ReferenceLine line;
  line.segments.push_back(PlanViewSegment{0.0, 0.0, 0.0, 0.0, 10.0, Line{}});
  line.segments.push_back(PlanViewSegment{10.0, 10.0, 0.0, 0.0, 10.0, Arc{0.1}});
  Check(SegmentIndexAt(line, -1.0) == 0, "s before the first segment uses the first");
  Check(SegmentIndexAt(line, 9.999) == 0, "s inside the first segment");
  Check(SegmentIndexAt(line, 10.0) == 1, "a segment boundary uses the later segment");
  Check(SegmentIndexAt(line, 25.0) == 1, "s past the end uses the last segment");
  const Pose2 end = EvaluateReferenceLine(line, 20.0);
  Near(end.x, 10.0 + std::sin(1.0) / 0.1, kPositionTolerance, "arc end x");
  Near(end.y, (1.0 - std::cos(1.0)) / 0.1, kPositionTolerance, "arc end y");
  Near(end.hdg, 1.0, 1e-12, "arc end heading");
}

void TestCubic() {
  PiecewiseCubic empty;
  Check(EvaluateCubic(empty, 3.0) == 0.0, "an empty profile is 0");

  PiecewiseCubic cubic;
  cubic.records.push_back(CubicRecord{2.0, 1.0, 0.5, -0.25, 0.125});
  cubic.records.push_back(CubicRecord{6.0, 4.0, 0.0, 0.0, 0.0});
  const auto first = [](double s) {
    const double d = s - 2.0;
    return 1.0 + 0.5 * d - 0.25 * d * d + 0.125 * d * d * d;
  };
  Near(EvaluateCubic(cubic, 3.5), first(3.5), 1e-15, "inside the first record");
  Near(EvaluateCubic(cubic, 0.0), first(0.0), 1e-15, "before the first record, the first extends");
  Near(EvaluateCubic(cubic, 6.0), 4.0, 0.0, "a record boundary uses the later record");
  Near(EvaluateCubic(cubic, 100.0), 4.0, 0.0, "the last record extends");
  Check(RecordIndexAt(cubic, 5.999) == 0 && RecordIndexAt(cubic, 6.0) == 1, "record index");
}

Lane MakeLane(std::int32_t number, CubicRecord width) {
  Lane lane;
  lane.number = number;
  if (number != 0) {
    lane.width.records.push_back(width);
  }
  return lane;
}

void TestBorders() {
  Road road;
  road.id = "r";
  road.length = 40.0;
  road.referenceLine.segments.push_back(PlanViewSegment{0.0, 5.0, 7.0, kPi / 2.0, 40.0, Line{}});
  road.laneOffset.records.push_back(CubicRecord{0.0, 0.5, 0.01, 0.0, 0.0});
  road.elevation.records.push_back(CubicRecord{0.0, 2.0, 0.1, 0.0, 0.0});
  LaneSection first;
  LaneSection second;
  second.s = 20.0;
  second.lanes = {
      MakeLane(2, CubicRecord{0.0, 1.0, 0.0, 0.0, 0.0}),
      MakeLane(1, CubicRecord{0.0, 3.0, 0.1, 0.0, 0.0}),
      MakeLane(0, {}),
      MakeLane(-1, CubicRecord{0.0, 3.5, 0.0, 0.01, 0.0}),
      MakeLane(-2, CubicRecord{0.0, 2.0, 0.0, 0.0, 0.0}),
  };
  first.lanes = {MakeLane(0, {}), MakeLane(-1, CubicRecord{0.0, 3.0, 0.0, 0.0, 0.0})};
  road.sections = {first, second};

  Check(SectionIndexAt(road, 19.9) == 0 && SectionIndexAt(road, 20.0) == 1, "section index");

  // At s = 25: ds = 5 in the second section.
  const double s = 25.0;
  const double offset = 0.5 + 0.01 * s;
  const double w1 = 3.0 + 0.1 * 5.0;
  const double w2 = 1.0;
  const double wm1 = 3.5 + 0.01 * 25.0;
  const double wm2 = 2.0;
  Near(BorderOffset(road, 1, 0, s), offset, 1e-14, "center border is the lane offset");
  Near(BorderOffset(road, 1, 1, s), offset + w1, 1e-14, "left lane 1 adds its width");
  Near(BorderOffset(road, 1, 2, s), offset + w1 + w2, 1e-14, "left lane 2 adds both widths");
  Near(BorderOffset(road, 1, -1, s), offset - wm1, 1e-14, "right lane -1 subtracts its width");
  Near(BorderOffset(road, 1, -2, s), offset - wm1 - wm2, 1e-14, "right lane -2 subtracts both");
  // Lane widths are relative to the section start; the first section's
  // lane -1 is evaluated at s = 25 as if it continued.
  Near(BorderOffset(road, 0, -1, s), offset - 3.0, 1e-14, "width relative to its own section");

  // Heading pi/2: the left normal is -x.
  const Point3 p = BorderPoint(road, 1, 2, s);
  Near(p.x, 5.0 - (offset + w1 + w2), 1e-12, "border point x along the left normal");
  Near(p.y, 7.0 + s, 1e-12, "border point y along the reference line");
  Near(p.z, 2.0 + 0.1 * s, 1e-14, "border point z is the elevation");

  const Point3 c = LaneCenterPoint(road, 1, -1, s);
  Near(c.x, 5.0 - (offset - wm1 / 2.0), 1e-12, "lane centerline is the middle of its borders");
  Near(c.y, 7.0 + s, 1e-12, "lane centerline y");
}

} // namespace

int main() {
  TestLine();
  TestArc();
  TestShallowArc();
  TestReferenceLine();
  TestCubic();
  TestBorders();

  if (failures == 0) {
    std::printf("roadnetworkCore geometry: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
