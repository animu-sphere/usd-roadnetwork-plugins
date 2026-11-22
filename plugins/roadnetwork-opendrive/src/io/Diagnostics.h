// SPDX-License-Identifier: Apache-2.0
#pragma once

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

// 9xx: internal.
inline constexpr std::string_view kInternal = "[ODR-E901]";

} // namespace openstrata::rn::odr::codes

namespace openstrata::rn::odr {

/// A failure found by the reader. `code` is one of the bracketed constants
/// above; `detail` says what was found, without the asset name, which the
/// caller adds.
struct Diagnostic {
  std::string_view code;
  std::string detail;
};

} // namespace openstrata::rn::odr
