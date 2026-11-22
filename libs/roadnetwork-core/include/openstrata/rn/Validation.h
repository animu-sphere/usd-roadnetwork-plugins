// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/RoadNetworkData.h"

#include <optional>
#include <string>

namespace openstrata::rn {

/// A diagnostic raised by shared library code.
struct Diagnostic {
  std::string code;    ///< e.g. "RN-E101".
  std::string message; ///< Starts with the bracketed code: "[RN-E101] ...".
};

/// Tolerances of the plan-view checks (ADR 0004 section 6).
struct ValidationTolerances {
  static constexpr double kStationMetres = 1e-6;
  static constexpr double kPositionMetres = 0.01;
  static constexpr double kHeadingRadians = 0.001;
};

/// Checks the invariants of ADR 0001 section 6 and returns the first failure, or
/// nothing when the network is valid.
///
/// The categories are checked in order: identity, references, lane
/// structure, numeric. Within a category, elements are visited in source
/// order. Every bundle calls this after decoding and before writing, and
/// posts the returned message as its one error (ADR 0006 section 3). The message
/// names the element and its ID but not the source asset, which the bundle
/// adds.
std::optional<Diagnostic> Validate(const RoadNetworkData& data);

} // namespace openstrata::rn
