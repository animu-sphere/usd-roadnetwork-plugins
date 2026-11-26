// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/RoadNetworkData.h"

#include <cstddef>
#include <cstdint>

/// Evaluation of the reference-line representation in `double`
/// (ADR 0004 section 2, docs/adr/0004-opendrive-geometry-retention.md).
///
/// Every `s` here is measured along the road, except the `s` passed to
/// EvaluateCubic(), which is in the frame of the records it is given.
namespace openstrata::rn {

/// A position and heading in the plan view.
struct Pose2 {
  double x = 0.0;
  double y = 0.0;
  double hdg = 0.0; ///< Counter-clockwise from +x, not wrapped.
};

struct Point3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

/// The signed curvature of `segment` at `s`, positive to the left: 0 for a
/// line, constant for an arc, linear in `s` for a spiral, and
/// `(u'v'' - v'u'') / (u'^2 + v'^2)^1.5` for a ParamPoly3, which is 0 where
/// its tangent vanishes.
double CurvatureAt(const PlanViewSegment& segment, double s) noexcept;

/// The upper end of the ParamPoly3 parameter range: 1 for Normalized or an
/// absent range, and `length` for ArcLength (ADR 0010 section 2).
double ParameterRange(const ParamPoly3& curve, double length) noexcept;

/// The parameter `p` at distance `u` from the start of a ParamPoly3 segment
/// of length `length`: `u * p_max / length` (ADR 0010 section 2).
double ParameterAt(const ParamPoly3& curve, double length, double u) noexcept;

/// The length of the curve a segment describes. For a ParamPoly3 it is the
/// arc length of `(u(p), v(p))` over its parameter range, by 8 pieces of
/// 32-point Gauss-Legendre quadrature, which may differ from
/// `segment.length` (RN-W302); for the other shapes it is `segment.length`.
double CurveLength(const PlanViewSegment& segment) noexcept;

/// The pose at `s` on `segment`. Lines and arcs use the chord form of
/// ADR 0004 section 2: exact, and stable as the curvature tends to 0. A
/// spiral integrates its heading by piecewise 8-point Gauss-Legendre
/// quadrature, within `max(1e-9 m, 1e-13 |s - segment.s|)` of the exact
/// clothoid relative to its start (ADR 0010 section 1); its cost grows with
/// the heading change of the segment. A ParamPoly3 is closed form (ADR 0010
/// section 2). `s` outside the segment extrapolates the same shape.
Pose2 EvaluateSegment(const PlanViewSegment& segment, double s) noexcept;

/// The index of the segment that holds `s`: the last segment whose start is
/// at or before `s`, or the first segment when `s` precedes them all. The
/// line must have at least one segment.
std::size_t SegmentIndexAt(const ReferenceLine& line, double s) noexcept;

/// The reference-line pose at `s`. The line must have at least one segment.
Pose2 EvaluateReferenceLine(const ReferenceLine& line, double s) noexcept;

/// The index of the record in force at `s`: the last record whose `s0` is at
/// or before `s`, or the first record when `s` precedes them all. The
/// profile must have at least one record.
std::size_t RecordIndexAt(const PiecewiseCubic& cubic, double s) noexcept;

/// The value at `s` of the record in force (RecordIndexAt()). An empty
/// profile is 0 everywhere: no elevation, no lane offset, or no width.
double EvaluateCubic(const PiecewiseCubic& cubic, double s) noexcept;

/// The index of the lane section that holds `s`: the last section whose start
/// is at or before `s`, or the first section. The road must have at least
/// one section.
std::size_t SectionIndexAt(const Road& road, double s) noexcept;

/// The lateral position at `s` of the outer border of lane `laneNumber` in
/// section `sectionIndex`, positive to the left of the reference line. Lane
/// 0 names the center border, which is the lane offset. The border of a left
/// lane adds the widths of lanes 1..n to the offset, and that of a right lane
/// subtracts the widths of lanes -1..-m. A lane width is evaluated at `s`
/// relative to the section start. The section and every lane between the
/// center and `laneNumber` must exist.
double BorderOffset(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept;

/// The point at `s` on the outer border of lane `laneNumber` (lane 0: the
/// center border): the reference point moved BorderOffset() along the left
/// normal `(-sin hdg, cos hdg)`, with `z` the elevation at `s`.
Point3 BorderPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept;

/// The point at `s` on the centerline of lane `laneNumber` (not 0): the
/// midpoint of its inner and outer borders.
Point3 LaneCenterPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept;

} // namespace openstrata::rn
