// SPDX-License-Identifier: Apache-2.0
#pragma once

// Private to roadnetwork-core: the end pose of a plan-view segment, which
// validation needs for the continuity checks of ADR 0004 section 6. GEO-01 replaces
// it with the public evaluator.

#include "openstrata/rn/RoadNetworkData.h"

#include <cmath>
#include <variant>

namespace openstrata::rn::detail {

struct Pose2 {
  double x = 0.0;
  double y = 0.0;
  double hdg = 0.0;
};

/// sin(z) / z, with sinc(0) = 1; the Taylor series below |z| = 1e-4.
inline double Sinc(double z) {
  if (std::abs(z) < 1e-4) {
    const double z2 = z * z;
    return 1.0 - z2 / 6.0 + z2 * z2 / 120.0;
  }
  return std::sin(z) / z;
}

/// The pose at the end of `segment`, in the chord form of ADR 0004 section 2.
inline Pose2 SegmentEnd(const PlanViewSegment& segment) {
  const double kappa =
      std::holds_alternative<Arc>(segment.shape) ? std::get<Arc>(segment.shape).curvature : 0.0;
  const double u = segment.length;
  const double theta = segment.hdg + kappa * u / 2.0;
  const double chord = u * Sinc(kappa * u / 2.0);
  return Pose2{segment.x + chord * std::cos(theta), segment.y + chord * std::sin(theta),
      segment.hdg + kappa * u};
}

} // namespace openstrata::rn::detail
