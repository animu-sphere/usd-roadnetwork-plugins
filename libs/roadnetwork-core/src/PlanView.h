// SPDX-License-Identifier: Apache-2.0
#pragma once

// Private to roadnetwork-core: the spiral quadrature of ADR 0010 section 1
// and an evaluator that computes each spiral's full pieces once per
// reference line, so that sampling a long spiral costs one piece per point.
// EvaluateSegment() sums the same pieces in the same order, so both give the
// same pose to the last bit.

#include "openstrata/rn/Geometry.h"
#include "openstrata/rn/RoadNetworkData.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openstrata::rn::detail {

struct Vec2 {
  double x = 0.0;
  double y = 0.0;
};

/// A spiral cut into `pieces` pieces of equal length `piece`, so that the
/// phase of each piece changes by at most about 1 rad (ADR 0010 section 1).
struct SpiralFrame {
  double kappa0 = 0.0; ///< curvatureStart.
  double rate = 0.0;   ///< (curvatureEnd - curvatureStart) / length.
  double piece = 0.0;
  std::size_t pieces = 1;
};

SpiralFrame FrameOf(const PlanViewSegment& segment, const Spiral& spiral) noexcept;

/// `integral (cos phi, sin phi)` over [a, b], `phi(v) = kappa0 v + rate v^2 / 2`,
/// by 8-point Gauss-Legendre about the midpoint.
Vec2 SpiralPiece(const SpiralFrame& frame, double a, double b) noexcept;

/// The offset of the spiral at distance `u` from its start, in the frame of
/// its start heading. `prefix`, when not null, holds `frame.pieces` sums: the
/// offset at the start of each piece.
Vec2 SpiralOffset(const SpiralFrame& frame, const Vec2* prefix, double u) noexcept;

/// The offsets at the start of every piece, each the previous plus one piece.
std::vector<Vec2> SpiralPrefix(const SpiralFrame& frame);

/// EvaluateReferenceLine() with the spiral pieces computed once.
class ReferenceLineEvaluator {
public:
  explicit ReferenceLineEvaluator(const ReferenceLine& line);
  Pose2 operator()(double s) const noexcept;

private:
  const ReferenceLine& line_;
  std::vector<std::vector<Vec2>> prefix_; ///< Per segment; empty unless a spiral.
};

/// BorderPoint() from the reference pose at `s`, which the caller has
/// evaluated.
Point3 BorderPointAt(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber, double s,
    const Pose2& reference) noexcept;

} // namespace openstrata::rn::detail
