// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/// The ODR- codes this bundle raises (ADR 0006 section 6,
/// docs/reference/DIAGNOSTICS.md). Each code is defined here once, bracketed
/// as it starts a message, and is listed with its summary and action in
/// plugin/resources/roadnetwork-opendrive/diagnostics.json. Codes are never
/// renumbered or reused.
namespace openstrata::rn::odr::codes {

// 0xx: input and container.
inline constexpr std::string_view kAssetUnreadable = "[ODR-E001]";
inline constexpr std::string_view kMalformedXml = "[ODR-E002]";
inline constexpr std::string_view kNotUtf8 = "[ODR-E003]";
inline constexpr std::string_view kWrongRoot = "[ODR-E004]";
inline constexpr std::string_view kMissingHeader = "[ODR-E005]";
inline constexpr std::string_view kMalformedVersion = "[ODR-E006]";
inline constexpr std::string_view kUnsupportedVersion = "[ODR-E007]";
inline constexpr std::string_view kMissingRequired = "[ODR-E008]";
inline constexpr std::string_view kInvalidValue = "[ODR-E009]";
inline constexpr std::string_view kMisplacedElement = "[ODR-E010]";

// 1xx: identity and references.
inline constexpr std::string_view kLaneGroupMismatch = "[ODR-E101]";
inline constexpr std::string_view kLaneLinkWithoutRoadLink = "[ODR-E102]";
inline constexpr std::string_view kLaneLinkIntoJunction = "[ODR-E103]";
inline constexpr std::string_view kConnectionUnresolved = "[ODR-E104]";

// 2xx: semantic mapping. Errors reject a representation Phase 1 does not
// support; warnings report data that is read but not represented.
inline constexpr std::string_view kUnsupportedGeometry = "[ODR-E201]";
inline constexpr std::string_view kUnsupportedLateralProfile = "[ODR-E202]";
inline constexpr std::string_view kUnsupportedLaneBorder = "[ODR-E203]";
inline constexpr std::string_view kUnsupportedJunction = "[ODR-E204]";
inline constexpr std::string_view kUnsupportedLaneLayer = "[ODR-E205]";
inline constexpr std::string_view kUnknownStructuralElement = "[ODR-E206]";
inline constexpr std::string_view kRoadMarksIgnored = "[ODR-W201]";
inline constexpr std::string_view kSignalsIgnored = "[ODR-W202]";
inline constexpr std::string_view kObjectsIgnored = "[ODR-W203]";
inline constexpr std::string_view kUserDataIgnored = "[ODR-W204]";
inline constexpr std::string_view kSpeedStringIgnored = "[ODR-W205]";
inline constexpr std::string_view kElementsIgnored = "[ODR-W206]";
inline constexpr std::string_view kAttributesIgnored = "[ODR-W207]";
inline constexpr std::string_view kUnknownElementsIgnored = "[ODR-W208]";

// 9xx: internal.
inline constexpr std::string_view kInternal = "[ODR-E901]";

} // namespace openstrata::rn::odr::codes

namespace openstrata::rn::odr {

/// A failure found by the reader or the decoder. `code` is bracketed: one of
/// the constants above, or a shared validation code relayed unchanged.
/// `detail` says what was found, without the asset name, which the caller
/// adds.
struct Diagnostic {
  std::string code;
  std::string detail;
};

/// A warning posted once per read for one kind of loss (ADR 0006 section 4).
/// `count` is the number of affected elements or attributes; `detail` names
/// them by kind and gives the locations of at most the first five.
struct Warning {
  std::string code; ///< Bracketed, e.g. "[ODR-W201]".
  std::uint32_t count = 0;
  std::string detail;
};

} // namespace openstrata::rn::odr
