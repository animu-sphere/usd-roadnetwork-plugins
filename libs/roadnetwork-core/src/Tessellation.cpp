// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/Tessellation.h"

#include "SectionIntervals.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace openstrata::rn {

namespace {

constexpr double kHalfPi = 1.5707963267948966192313216916398;

// The ADR 0004 section 3 error bound of an interval, as a function of the
// step `h`.
struct ErrorBound {
  double absCurvature = 0.0;
  double outerRadius = 0.0;  ///< R_out = 1/|kappa| + t_max, for an arc.
  double sumMaxSecond = 0.0; ///< Sum of max |f''| over offset, widths, elevation.

  double operator()(double h) const noexcept {
    double arc = 0.0;
    if (absCurvature > 0.0) {
      const double half = absCurvature * h / 2.0;
      // A step that turns by more than pi is refused. 1 - cos falls again
      // past a full turn, so without a cap a longer step could look better.
      if (half > kHalfPi) {
        return std::numeric_limits<double>::infinity();
      }
      const double sine = std::sin(half / 2.0);
      arc = outerRadius * 2.0 * sine * sine; // R (1 - cos(half)), without cancellation
    }
    return arc + h * h / 8.0 * sumMaxSecond;
  }
};

ErrorBound BoundFor(const Road& road, const detail::Interval& interval) {
  ErrorBound bound;
  const double kappa = Curvature(road.referenceLine.segments[interval.segment]);
  const double length = interval.length;
  if (kappa != 0.0) {
    bound.absCurvature = std::abs(kappa);
    // Outside of a left turn is to the right (t < 0), and the reverse.
    double outside = -std::numeric_limits<double>::infinity();
    for (const detail::Poly3& border : interval.borders) {
      const double sign = kappa > 0.0 ? -1.0 : 1.0;
      const detail::Poly3 away{sign * border.c0, sign * border.c1, sign * border.c2, sign * border.c3};
      outside = std::max(outside, detail::Maximum(away, length).value);
    }
    bound.outerRadius = std::max(0.0, 1.0 / bound.absCurvature + outside);
  }
  bound.sumMaxSecond = interval.offset.MaxAbsSecond(length) + interval.elevation.MaxAbsSecond(length);
  for (const detail::Poly3& width : interval.widths) {
    bound.sumMaxSecond += width.MaxAbsSecond(length);
  }
  return bound;
}

// The fewest equal parts of `length` whose bound is within `tolerance`.
std::size_t PartsFor(const ErrorBound& bound, double length, double tolerance) {
  const auto fits = [&](std::size_t n) { return bound(length / static_cast<double>(n)) <= tolerance; };
  if (fits(1)) {
    return 1;
  }
  // The bound decreases with n, so double until it fits, then bisect.
  std::size_t high = 2;
  while (!fits(high)) {
    if (high > (std::size_t{1} << 40)) {
      return high; // Unreachable for validated, finite input.
    }
    high *= 2;
  }
  std::size_t low = high / 2; // Does not fit.
  while (high - low > 1) {
    const std::size_t mid = low + (high - low) / 2;
    (fits(mid) ? high : low) = mid;
  }
  return high;
}

} // namespace

SampledRoad TessellateRoad(const Road& road, const TessellationOptions& options) {
  SampledRoad result;
  result.roadId = road.id;
  if (road.referenceLine.segments.empty()) {
    return result;
  }
  const double tolerance = std::isfinite(options.toleranceMetres) && options.toleranceMetres > 0.0
                               ? options.toleranceMetres
                               : TessellationOptions{}.toleranceMetres;

  for (std::size_t sectionIndex = 0; sectionIndex < road.sections.size(); ++sectionIndex) {
    const LaneSection& section = road.sections[sectionIndex];
    SampledSection sampled;
    sampled.sectionIndex = static_cast<std::uint32_t>(sectionIndex);

    const std::vector<double> breaks = detail::SectionStations(road, sectionIndex);
    for (std::size_t i = 0; i + 1 < breaks.size(); ++i) {
      const detail::Interval interval = detail::MakeInterval(road, sectionIndex, breaks[i], breaks[i + 1]);
      const std::size_t parts = PartsFor(BoundFor(road, interval), interval.length, tolerance);
      for (std::size_t k = 0; k < parts; ++k) {
        sampled.stations.push_back(breaks[i] + interval.length * static_cast<double>(k) /
                                                   static_cast<double>(parts));
      }
    }
    sampled.stations.push_back(breaks.back());

    // The center border first, then each non-center lane in source order.
    std::map<std::int32_t, std::size_t> borderOf;
    const auto addBorder = [&](std::int32_t number) {
      SampledCurve curve;
      curve.laneNumber = number;
      curve.points.reserve(sampled.stations.size());
      for (double s : sampled.stations) {
        curve.points.push_back(BorderPoint(road, sectionIndex, number, s));
      }
      borderOf[number] = sampled.borders.size();
      sampled.borders.push_back(std::move(curve));
    };
    addBorder(0);
    for (const Lane& lane : section.lanes) {
      if (lane.number != 0) {
        addBorder(lane.number);
      }
    }

    for (const Lane& lane : section.lanes) {
      if (lane.number == 0) {
        continue;
      }
      const std::int32_t innerNumber = lane.number > 0 ? lane.number - 1 : lane.number + 1;
      const SampledCurve& outer = sampled.borders[borderOf.at(lane.number)];
      const SampledCurve& inner = sampled.borders[borderOf.at(innerNumber)];
      SampledCurve center;
      center.laneNumber = lane.number;
      center.points.reserve(outer.points.size());
      for (std::size_t k = 0; k < outer.points.size(); ++k) {
        const Point3& a = outer.points[k];
        const Point3& b = inner.points[k];
        center.points.push_back(Point3{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0, (a.z + b.z) / 2.0});
      }
      sampled.centerlines.push_back(std::move(center));
    }
    result.sections.push_back(std::move(sampled));
  }
  return result;
}

} // namespace openstrata::rn
