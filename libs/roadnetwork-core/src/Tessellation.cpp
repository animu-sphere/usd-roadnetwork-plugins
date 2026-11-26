// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/Tessellation.h"

#include "PlanView.h"
#include "SectionIntervals.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace openstrata::rn {

namespace {

constexpr double kHalfPi = 1.5707963267948966192313216916398;

// The ADR 0004 section 3 error bound of a line or arc interval, as a
// function of the step `h`.
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

// The largest offset of a border on the outside of a turn whose curvature
// has the sign of `kappa`: to the right (t < 0) of a left turn, and the
// reverse.
double OutsideOffset(const detail::Interval& interval, double kappa) {
  const double away = kappa > 0.0 ? -1.0 : 1.0;
  double outside = -std::numeric_limits<double>::infinity();
  for (const detail::Poly3& border : interval.borders) {
    const detail::Poly3 p{away * border.c0, away * border.c1, away * border.c2, away * border.c3};
    outside = std::max(outside, detail::Maximum(p, interval.length).value);
  }
  return outside;
}

double SumMaxSecond(const detail::Interval& interval) {
  const double length = interval.length;
  double sum = interval.offset.MaxAbsSecond(length) + interval.elevation.MaxAbsSecond(length);
  for (const detail::Poly3& width : interval.widths) {
    sum += width.MaxAbsSecond(length);
  }
  return sum;
}

ErrorBound BoundFor(const Road& road, const detail::Interval& interval) {
  ErrorBound bound;
  const double kappa = CurvatureAt(road.referenceLine.segments[interval.segment], interval.s0);
  if (kappa != 0.0) {
    bound.absCurvature = std::abs(kappa);
    bound.outerRadius = std::max(0.0, 1.0 / bound.absCurvature + OutsideOffset(interval, kappa));
  }
  bound.sumMaxSecond = SumMaxSecond(interval);
  return bound;
}

// ADR 0010 section 4: the bound of a spiral or ParamPoly3 interval, checked
// part by part because it is not a closed function of the number of parts.
class CurveBound {
public:
  CurveBound(const Road& road, const detail::Interval& interval)
      : segment_(road.referenceLine.segments[interval.segment]), interval_(interval),
        sumMaxSecond_(SumMaxSecond(interval)), spiral_(std::holds_alternative<Spiral>(segment_.shape)) {
    // Between two stations the curvature keeps one sign (ADR 0010 section 4).
    const double kappa = CurvatureAt(segment_, interval.s0 + interval.length / 2.0);
    sign_ = kappa > 0.0 ? 1.0 : kappa < 0.0 ? -1.0 : 0.0;
    if (spiral_) {
      outside_ = OutsideOffset(interval, sign_ != 0.0 ? sign_ : 1.0);
    } else {
      for (const detail::Poly3& border : interval.borders) {
        const detail::Poly3 negated{-border.c0, -border.c1, -border.c2, -border.c3};
        tMax_ = std::max({tMax_, detail::Maximum(border, interval.length).value,
            detail::Maximum(negated, interval.length).value});
      }
    }
  }

  bool Fits(std::size_t n, double tolerance) const {
    const double h = interval_.length / static_cast<double>(n);
    const double cubic = h * h / 8.0 * sumMaxSecond_;
    double a = interval_.s0;
    Pose2 start = spiral_ ? Pose2{} : EvaluateSegment(segment_, a);
    for (std::size_t j = 1; j <= n; ++j) {
      const double b =
          interval_.s0 + interval_.length * static_cast<double>(j) / static_cast<double>(n);
      double plan = 0.0;
      if (spiral_) {
        plan = SpiralPart(a, b);
      } else {
        const Pose2 end = EvaluateSegment(segment_, b);
        plan = TrianglePart(start, end);
        start = end;
      }
      if (!(plan + cubic <= tolerance)) {
        return false;
      }
      a = b;
    }
    return true;
  }

private:
  // ADR 0004's arc term with the larger end curvature of the part.
  double SpiralPart(double a, double b) const {
    const double k = std::max(std::abs(CurvatureAt(segment_, a)), std::abs(CurvatureAt(segment_, b)));
    if (k == 0.0) {
      return 0.0;
    }
    const double half = k * (b - a) / 2.0;
    if (half > kHalfPi) {
      return std::numeric_limits<double>::infinity();
    }
    const double sine = std::sin(half / 2.0);
    const double oneMinusCos = 2.0 * sine * sine;
    // R_out (1 - cos) with R_out = 1/k + t_out, written so that a curvature
    // that tends to 0 does not overflow 1/k.
    return std::max(0.0, oneMinusCos / k + outside_ * oneMinusCos);
  }

  // The triangle bound of a part that turns one way by less than pi/2.
  double TrianglePart(const Pose2& start, const Pose2& end) const {
    constexpr double twoPi = 6.283185307179586476925286766559;
    double turn = std::remainder(end.hdg - start.hdg, twoPi);
    // The tangent of a cubic turns by less than 2 pi between two
    // inflections, so the sign of the curvature resolves the wrap.
    if (sign_ > 0.0 && turn < 0.0) {
      turn += twoPi;
    } else if (sign_ < 0.0 && turn > 0.0) {
      turn -= twoPi;
    }
    const double theta = std::abs(turn);
    if (!(theta < kHalfPi)) {
      return std::numeric_limits<double>::infinity();
    }
    const double chord = std::hypot(end.x - start.x, end.y - start.y);
    return (chord + 2.0 * tMax_ * std::sin(theta / 2.0)) / 2.0 * std::tan(theta / 2.0);
  }

  const PlanViewSegment& segment_;
  const detail::Interval& interval_;
  double sumMaxSecond_ = 0.0;
  bool spiral_ = false;
  double sign_ = 0.0;
  double outside_ = 0.0; ///< Spiral: the largest offset outside the turn.
  double tMax_ = 0.0;    ///< ParamPoly3: the largest |t| of a border.
};

// The fewest n for which `fits(n)`.
template <typename Fits>
std::size_t Fewest(const Fits& fits) {
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

std::size_t detail::PartsFor(const Road& road, const Interval& interval, double tolerance) {
  const PlanViewSegment& segment = road.referenceLine.segments[interval.segment];
  if (std::holds_alternative<Spiral>(segment.shape) || std::holds_alternative<ParamPoly3>(segment.shape)) {
    const CurveBound bound(road, interval);
    return Fewest([&](std::size_t n) { return bound.Fits(n, tolerance); });
  }
  const ErrorBound bound = BoundFor(road, interval);
  const double length = interval.length;
  return Fewest([&](std::size_t n) { return bound(length / static_cast<double>(n)) <= tolerance; });
}

SampledRoad TessellateRoad(const Road& road, const TessellationOptions& options) {
  SampledRoad result;
  result.roadId = road.id;
  if (road.referenceLine.segments.empty()) {
    return result;
  }
  const double tolerance = std::isfinite(options.toleranceMetres) && options.toleranceMetres > 0.0
                               ? options.toleranceMetres
                               : TessellationOptions{}.toleranceMetres;
  const detail::ReferenceLineEvaluator referenceLine(road.referenceLine);

  for (std::size_t sectionIndex = 0; sectionIndex < road.sections.size(); ++sectionIndex) {
    const LaneSection& section = road.sections[sectionIndex];
    SampledSection sampled;
    sampled.sectionIndex = static_cast<std::uint32_t>(sectionIndex);

    const std::vector<double> breaks = detail::SectionStations(road, sectionIndex);
    for (std::size_t i = 0; i + 1 < breaks.size(); ++i) {
      const detail::Interval interval = detail::MakeInterval(road, sectionIndex, breaks[i], breaks[i + 1]);
      const std::size_t parts = detail::PartsFor(road, interval, tolerance);
      for (std::size_t k = 0; k < parts; ++k) {
        sampled.stations.push_back(breaks[i] + interval.length * static_cast<double>(k) /
                                                   static_cast<double>(parts));
      }
    }
    sampled.stations.push_back(breaks.back());

    // One reference pose per station, shared by every border.
    std::vector<Pose2> poses;
    poses.reserve(sampled.stations.size());
    for (double s : sampled.stations) {
      poses.push_back(referenceLine(s));
    }

    // The center border first, then each non-center lane in source order.
    std::map<std::int32_t, std::size_t> borderOf;
    const auto addBorder = [&](std::int32_t number) {
      SampledCurve curve;
      curve.laneNumber = number;
      curve.points.reserve(sampled.stations.size());
      for (std::size_t k = 0; k < sampled.stations.size(); ++k) {
        curve.points.push_back(
            detail::BorderPointAt(road, sectionIndex, number, sampled.stations[k], poses[k]));
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
