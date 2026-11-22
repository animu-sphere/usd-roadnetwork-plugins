// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/Geometry.h"
#include "openstrata/rn/RoadNetworkData.h"

#include <cstdint>
#include <string>
#include <vector>

/// Sampling of the reference-line representation into polylines
/// (ADR 0004 section 3, docs/adr/0004-opendrive-geometry-retention.md).
///
/// The samples are derived visualization data. The USD writer authors them
/// as they are and contains no geometry math of its own.
namespace openstrata::rn {

struct TessellationOptions {
  /// The error bound `epsilon` in metres: the largest allowed distance
  /// between a sampled polyline and the analytic curve. Phase 1 fixes it at
  /// the default (ADR 0004 section 3).
  double toleranceMetres = 0.01;
};

/// One polyline with one point per station of its section.
struct SampledCurve {
  std::int32_t laneNumber = 0; ///< The lane it belongs to; 0 for the center border.
  std::vector<Point3> points;
};

struct SampledSection {
  std::uint32_t sectionIndex = 0;
  /// Ascending `s` along the road, from the section start to its end. Every
  /// curve of the section has one point per station, so the borders of
  /// adjacent lanes share vertices.
  std::vector<double> stations;
  /// The center border, then the outer border of each non-center lane in
  /// source order (ADR 0005 section 5).
  std::vector<SampledCurve> borders;
  /// The centerline of each non-center lane, in source order.
  std::vector<SampledCurve> centerlines;
};

struct SampledRoad {
  std::string roadId;
  std::vector<SampledSection> sections; ///< Source order.
};

/// Samples every lane section of `road`. The road must have passed
/// Validate(); the result for a road that fails it is unspecified.
///
/// A section's stations are its start and end, every plan-view segment
/// boundary, and the start of every elevation, lane-offset, and width record
/// inside it. Each interval between two stations is split into the fewest
/// equal parts whose ADR 0004 error bound is within the tolerance. A line
/// with constant widths and elevation needs one part.
SampledRoad TessellateRoad(const Road& road, const TessellationOptions& options = {});

} // namespace openstrata::rn
