// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/Geometry.h"

#include "SectionIntervals.h"
#include "openstrata/rn/Validation.h"

#include <algorithm>
#include <cmath>
#include <variant>

namespace openstrata::rn {

namespace {

// sin(z) / z, with sinc(0) = 1; the Taylor series below |z| = 1e-4, where
// its first omitted term is below 1e-18 (ADR 0004 section 2).
double Sinc(double z) noexcept {
  if (std::abs(z) < 1e-4) {
    const double z2 = z * z;
    return 1.0 - z2 / 6.0 + z2 * z2 / 120.0;
  }
  return std::sin(z) / z;
}

// The last element whose start is at or before `s`, or the first element.
template <typename Container, typename Start>
std::size_t LastAtOrBefore(const Container& items, double s, Start start) noexcept {
  const auto after = std::upper_bound(items.begin(), items.end(), s,
      [&](double value, const auto& item) { return value < start(item); });
  return after == items.begin() ? 0 : static_cast<std::size_t>(after - items.begin()) - 1;
}

const Lane* FindLane(const LaneSection& section, std::int32_t number) noexcept {
  for (const Lane& lane : section.lanes) {
    if (lane.number == number) {
      return &lane;
    }
  }
  return nullptr;
}

} // namespace

double Curvature(const PlanViewSegment& segment) noexcept {
  const Arc* arc = std::get_if<Arc>(&segment.shape);
  return arc ? arc->curvature : 0.0;
}

Pose2 EvaluateSegment(const PlanViewSegment& segment, double s) noexcept {
  const double kappa = Curvature(segment);
  const double u = s - segment.s;
  const double theta = segment.hdg + kappa * u / 2.0;
  const double chord = u * Sinc(kappa * u / 2.0);
  return Pose2{segment.x + chord * std::cos(theta), segment.y + chord * std::sin(theta),
      segment.hdg + kappa * u};
}

std::size_t SegmentIndexAt(const ReferenceLine& line, double s) noexcept {
  return LastAtOrBefore(line.segments, s, [](const PlanViewSegment& g) { return g.s; });
}

Pose2 EvaluateReferenceLine(const ReferenceLine& line, double s) noexcept {
  return EvaluateSegment(line.segments[SegmentIndexAt(line, s)], s);
}

std::size_t RecordIndexAt(const PiecewiseCubic& cubic, double s) noexcept {
  return LastAtOrBefore(cubic.records, s, [](const CubicRecord& r) { return r.s0; });
}

double EvaluateCubic(const PiecewiseCubic& cubic, double s) noexcept {
  if (cubic.records.empty()) {
    return 0.0;
  }
  const CubicRecord& r = cubic.records[RecordIndexAt(cubic, s)];
  const double d = s - r.s0;
  return r.a + d * (r.b + d * (r.c + d * r.d));
}

std::size_t SectionIndexAt(const Road& road, double s) noexcept {
  return LastAtOrBefore(road.sections, s, [](const LaneSection& section) { return section.s; });
}

double BorderOffset(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  const LaneSection& section = road.sections[sectionIndex];
  const double ds = s - section.s;
  const std::int32_t step = laneNumber > 0 ? 1 : -1;
  double t = EvaluateCubic(road.laneOffset, s);
  for (std::int32_t number = step; laneNumber != 0 && number != laneNumber + step; number += step) {
    if (const Lane* lane = FindLane(section, number)) {
      t += step * EvaluateCubic(lane->width, ds);
    }
  }
  return t;
}

Point3 BorderPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  const Pose2 pose = EvaluateReferenceLine(road.referenceLine, s);
  const double t = BorderOffset(road, sectionIndex, laneNumber, s);
  return Point3{pose.x - t * std::sin(pose.hdg), pose.y + t * std::cos(pose.hdg),
      EvaluateCubic(road.elevation, s)};
}

Point3 LaneCenterPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  const Point3 outer = BorderPoint(road, sectionIndex, laneNumber, s);
  const Point3 inner = BorderPoint(road, sectionIndex, laneNumber > 0 ? laneNumber - 1 : laneNumber + 1, s);
  return Point3{(outer.x + inner.x) / 2.0, (outer.y + inner.y) / 2.0, (outer.z + inner.z) / 2.0};
}

// ------------------------------------------------------------- intervals

namespace detail {

double Poly3::MaxAbsSecond(double length) const noexcept {
  return std::max(std::abs(Second(0.0)), std::abs(Second(length)));
}

Poly3& Poly3::operator+=(const Poly3& other) noexcept {
  c0 += other.c0;
  c1 += other.c1;
  c2 += other.c2;
  c3 += other.c3;
  return *this;
}

Poly3& Poly3::operator-=(const Poly3& other) noexcept {
  c0 -= other.c0;
  c1 -= other.c1;
  c2 -= other.c2;
  c3 -= other.c3;
  return *this;
}

Extremum Maximum(const Poly3& p, double length) noexcept {
  Extremum best{0.0, p.Value(0.0)};
  const auto consider = [&](double x) {
    if (x > 0.0 && x <= length) {
      const double value = p.Value(x);
      if (value > best.value) {
        best = Extremum{x, value};
      }
    }
  };
  consider(length);
  // f'(x) = c1 + 2 c2 x + 3 c3 x^2.
  const double qa = 3.0 * p.c3;
  const double qb = 2.0 * p.c2;
  const double qc = p.c1;
  if (qa == 0.0) {
    if (qb != 0.0) {
      consider(-qc / qb);
    }
  } else {
    const double disc = qb * qb - 4.0 * qa * qc;
    if (disc >= 0.0) {
      // The stable form of the two roots.
      const double q = -0.5 * (qb + std::copysign(std::sqrt(disc), qb));
      consider(q / qa);
      if (q != 0.0) {
        consider(qc / q);
      }
    }
  }
  return best;
}

Poly3 Shift(const CubicRecord& r, double delta) noexcept {
  const double d2 = delta * delta;
  return Poly3{r.a + r.b * delta + r.c * d2 + r.d * d2 * delta, r.b + 2.0 * r.c * delta + 3.0 * r.d * d2,
      r.c + 3.0 * r.d * delta, r.d};
}

double SectionEnd(const Road& road, std::size_t sectionIndex) noexcept {
  return sectionIndex + 1 < road.sections.size() ? road.sections[sectionIndex + 1].s : road.length;
}

std::vector<double> SectionStations(const Road& road, std::size_t sectionIndex) {
  constexpr double tolerance = ValidationTolerances::kStationMetres;
  const LaneSection& section = road.sections[sectionIndex];
  const double start = section.s;
  const double end = SectionEnd(road, sectionIndex);

  std::vector<double> inner;
  const auto add = [&](double s) {
    if (s > start && s < end) {
      inner.push_back(s);
    }
  };
  for (const PlanViewSegment& segment : road.referenceLine.segments) {
    add(segment.s);
  }
  for (const CubicRecord& r : road.elevation.records) {
    add(r.s0);
  }
  for (const CubicRecord& r : road.laneOffset.records) {
    add(r.s0);
  }
  for (const Lane& lane : section.lanes) {
    for (const CubicRecord& r : lane.width.records) {
      add(start + r.s0);
    }
  }
  std::sort(inner.begin(), inner.end());

  std::vector<double> stations{start};
  for (double s : inner) {
    if (s - stations.back() > tolerance && end - s > tolerance) {
      stations.push_back(s);
    }
  }
  stations.push_back(end);
  return stations;
}

namespace {

Poly3 InForce(const PiecewiseCubic& cubic, double frameOrigin, double s0, double mid) {
  if (cubic.records.empty()) {
    return Poly3{};
  }
  const CubicRecord& r = cubic.records[RecordIndexAt(cubic, mid - frameOrigin)];
  return Shift(r, s0 - frameOrigin - r.s0);
}

} // namespace

Interval MakeInterval(const Road& road, std::size_t sectionIndex, double s0, double s1) {
  const LaneSection& section = road.sections[sectionIndex];
  const double mid = 0.5 * (s0 + s1);

  Interval interval;
  interval.s0 = s0;
  interval.length = s1 - s0;
  interval.segment = SegmentIndexAt(road.referenceLine, mid);
  interval.offset = InForce(road.laneOffset, 0.0, s0, mid);
  interval.elevation = InForce(road.elevation, 0.0, s0, mid);

  const std::size_t count = section.lanes.size();
  interval.widths.resize(count);
  interval.borders.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (section.lanes[i].number != 0) {
      interval.widths[i] = InForce(section.lanes[i].width, section.s, s0, mid);
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    const std::int32_t number = section.lanes[i].number;
    Poly3 border = interval.offset;
    for (std::size_t j = 0; j < count; ++j) {
      const std::int32_t other = section.lanes[j].number;
      if (number > 0 && other > 0 && other <= number) {
        border += interval.widths[j];
      } else if (number < 0 && other < 0 && other >= number) {
        border -= interval.widths[j];
      }
    }
    interval.borders[i] = border;
  }
  return interval;
}

} // namespace detail

} // namespace openstrata::rn
