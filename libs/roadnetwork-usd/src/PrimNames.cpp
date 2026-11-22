// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/usd/PrimNames.h"

namespace openstrata::rn::usd {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

bool IsAsciiAlnum(unsigned char c) noexcept {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/// The value of a lowercase hexadecimal digit, or -1.
int LowerHexValue(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

} // namespace

std::string EncodeId(std::string_view id) {
  std::string encoded;
  encoded.reserve(id.size());
  for (const char ch : id) {
    const auto byte = static_cast<unsigned char>(ch);
    if (IsAsciiAlnum(byte)) {
      encoded.push_back(ch);
    } else {
      encoded.push_back('_');
      encoded.push_back(kHexDigits[byte >> 4]);
      encoded.push_back(kHexDigits[byte & 0x0f]);
    }
  }
  return encoded;
}

std::optional<std::string> DecodeId(std::string_view encoded) {
  std::string id;
  id.reserve(encoded.size());
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    const auto byte = static_cast<unsigned char>(encoded[i]);
    if (IsAsciiAlnum(byte)) {
      id.push_back(encoded[i]);
      continue;
    }
    if (byte != '_' || encoded.size() - i < 3) {
      return std::nullopt;
    }
    const int high = LowerHexValue(encoded[i + 1]);
    const int low = LowerHexValue(encoded[i + 2]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    const auto value = static_cast<unsigned char>(high * 16 + low);
    // E never escapes a letter or digit, so such an escape has no preimage.
    if (IsAsciiAlnum(value)) {
      return std::nullopt;
    }
    id.push_back(static_cast<char>(value));
    i += 2;
  }
  return id;
}

std::optional<std::string> PrimName(std::string_view prefix, std::string_view id) {
  if (id.empty()) {
    return std::nullopt;
  }
  std::string name(prefix);
  name.push_back('_');
  name += EncodeId(id);
  return name;
}

std::string SectionName(std::uint32_t sectionIndex) {
  return "section_" + std::to_string(sectionIndex);
}

std::string LaneName(std::int32_t laneNumber) {
  if (laneNumber == 0) {
    return "center";
  }
  if (laneNumber > 0) {
    return "left_" + std::to_string(laneNumber);
  }
  // Widened so that the most negative int32 has a positive magnitude.
  return "right_" + std::to_string(-static_cast<std::int64_t>(laneNumber));
}

} // namespace openstrata::rn::usd
