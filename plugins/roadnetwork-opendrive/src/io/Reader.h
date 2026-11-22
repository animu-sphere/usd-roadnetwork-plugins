// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "io/Diagnostics.h"

#include "openstrata/rn/RoadNetworkData.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

/// The OpenDRIVE reader: container checks, the `CanRead()` sniff, version
/// recognition, and the header scan (ADR 0006, FMT-01 section 3). It has no
/// OpenUSD dependency, and the XML parser (pugixml) stays out of this header.
namespace openstrata::rn::odr {

/// The most bytes `CanRead()` looks at (ADR 0006 section 1).
inline constexpr std::size_t kSniffBytes = 64 * 1024;

/// The `CanRead()` decision on the first bytes of an asset (at most
/// kSniffBytes). True when the bytes are UTF-8, any XML declaration names
/// UTF-8, and the first element is `OpenDRIVE` with no prefix and no default
/// namespace. Before it, only a UTF-8 byte-order mark, the XML declaration,
/// comments, processing instructions, a document type declaration, and
/// whitespace may appear. A root start tag that does not end within `prefix`
/// is a no.
bool LooksLikeOpenDrive(std::string_view prefix) noexcept;

/// Element counts from the header scan, without decoding geometry, profiles,
/// or lanes (ADR 0006 section 2).
struct ElementCounts {
  std::uint32_t roads = 0;     ///< `<road>` children of the root.
  std::uint32_t junctions = 0; ///< `<junction>` children of the root.
  /// `<lane>` elements in the `<left>`, `<center>`, and `<right>` groups of
  /// every lane section, center lanes included: one per lane prim (ADR 0005).
  std::uint32_t lanes = 0;
};

/// The Phase 1 version range: `revMajor` 1, `revMinor` 4 to 9 (FMT-01 3.3).
inline constexpr int kVersionMajor = 1;
inline constexpr int kMinVersionMinor = 4;
inline constexpr int kMaxVersionMinor = 9;

/// A parsed OpenDRIVE document whose container, root element, header, and
/// version have been checked. Its content is not decoded.
class Document {
public:
  /// Parses `bytes` and checks, in order: the encoding (ODR-E003), XML
  /// well-formedness (ODR-E002), the root element (ODR-E004), the header
  /// (ODR-E005), and the version (ODR-E006, ODR-E007). Returns null and fills
  /// `*error` on the first failure. `std::bad_alloc` may escape.
  static std::unique_ptr<Document> Parse(std::string_view bytes, Diagnostic* error);

  ~Document();
  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;

  /// The header provenance: format, version, and the `name`, `date`,
  /// `vendor`, and `version` attributes verbatim (empty when absent). The
  /// georeference and offset are read by the decoder (ODR-02).
  const SourceInfo& Source() const noexcept;

  ElementCounts CountElements() const;

private:
  struct Impl;
  explicit Document(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

} // namespace openstrata::rn::odr
