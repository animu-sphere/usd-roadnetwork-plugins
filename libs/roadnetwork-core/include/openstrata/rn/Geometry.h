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

/// The curvature of a segment: 0 for a line.
double Curvature(const PlanViewSegment& segment) noexcept;

/// The pose at `s` on `segment`, in the chord form of ADR 0004 section 2:
/// exact for lines and arcs, and stable as the curvature tends to 0. `s`
/// outside the segment extrapolates the same shape.
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
