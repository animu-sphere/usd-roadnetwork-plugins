// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// Prim names derived from source IDs (ADR 0005 sections 1 and 2,
/// docs/adr/0005-prim-path-and-id-stability.md). Every format bundle names
/// prims through these functions, so that the same ID gives the same path in
/// every format. None of them depends on OpenUSD.
namespace openstrata::rn::usd {

/// `E(id)`: keeps every ASCII letter and digit of the UTF-8 bytes of `id`, in
/// order, and writes every other byte, the underscore included, as `_`
/// followed by two lowercase hexadecimal digits. `E` is injective, and its
/// result is ASCII and a valid identifier tail. The empty ID encodes to the
/// empty string; PrimName() rejects it.
std::string EncodeId(std::string_view id);

/// `E⁻¹`: the ID whose encoding is `encoded`, or nothing when `encoded` is not
/// an encoding. That is the case for a byte other than an ASCII letter, digit,
/// or `_`; a `_` not followed by two lowercase hexadecimal digits; and an
/// escape of a letter or digit, which `E` never writes. For every ID,
/// `DecodeId(EncodeId(id)) == id`.
std::optional<std::string> DecodeId(std::string_view encoded);

/// `<prefix>_E(id)`, for example `road_1` or `road__2d1`. Nothing for an
/// empty ID, which validation rejects before naming (RN-E101).
std::optional<std::string> PrimName(std::string_view prefix, std::string_view id);

/// The prefixes of ADR 0005 section 1.
inline constexpr std::string_view kRoadPrefix = "road";
inline constexpr std::string_view kJunctionPrefix = "junction";
inline constexpr std::string_view kConnectionPrefix = "connection";

/// `section_<n>`, with `n` the 0-based source-order index in decimal.
std::string SectionName(std::uint32_t sectionIndex);

/// `left_<n>` for lane number `n > 0`, `center` for 0, and `right_<n>` for
/// lane number `-n`.
std::string LaneName(std::int32_t laneNumber);

} // namespace openstrata::rn::usd
