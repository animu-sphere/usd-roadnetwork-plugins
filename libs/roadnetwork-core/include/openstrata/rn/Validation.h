// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/RoadNetworkData.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openstrata::rn {

/// A diagnostic raised by shared library code.
struct Diagnostic {
  std::string code;    ///< e.g. "RN-E101".
  std::string message; ///< Starts with the bracketed code: "[RN-E101] ...".
};

/// A warning raised by shared validation: one per code, with the number of
/// affected elements and, in the message, the first five of them (ADR 0006
/// section 4).
struct ValidationWarning {
  std::string code;    ///< e.g. "RN-W301".
  std::uint32_t count = 0;
  std::string message; ///< Starts with the bracketed code.
};

/// Tolerances of the plan-view checks (ADR 0004 section 6, ADR 0010
/// section 2).
struct ValidationTolerances {
  static constexpr double kStationMetres = 1e-6;
  static constexpr double kPositionMetres = 0.01;
  static constexpr double kHeadingRadians = 0.001;
  /// A ParamPoly3 whose curve moves less than this per metre of `s`
  /// somewhere in its range has a vanishing tangent (RN-E310).
  static constexpr double kTangentSpeed = 1e-6;
  /// A ParamPoly3 whose curve length differs from its segment length by
  /// more than this fraction is counted in RN-W302.
  static constexpr double kCurveLengthFraction = 0.01;
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
///
/// When the network is valid and `warnings` is non-null, `*warnings`
/// receives the warnings validation raises, in ascending code order: a lane
/// border that folds back past a curve's center of curvature without making
/// any lane invalid (RN-W301, ADR 0004 section 2), and a ParamPoly3 whose
/// curve length is far from its segment length (RN-W302, ADR 0010
/// section 2).
std::optional<Diagnostic> Validate(const RoadNetworkData& data);
std::optional<Diagnostic> Validate(const RoadNetworkData& data, std::vector<ValidationWarning>* warnings);

} // namespace openstrata::rn
