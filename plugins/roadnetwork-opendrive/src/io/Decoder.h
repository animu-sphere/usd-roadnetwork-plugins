// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "io/Diagnostics.h"
#include "io/Reader.h"

#include "openstrata/rn/RoadNetworkData.h"

#include <string_view>
#include <vector>

/// The OpenDRIVE semantic decoder (ADR 0001, ADR 0004,
/// docs/reference/OPENDRIVE_MAPPING.md). It turns a parsed Document into
/// RoadNetworkData and checks only what the source syntax shows: required
/// attributes, value syntax, element placement, the representations Phase 1
/// does not support, and lane links whose target the source leaves
/// ambiguous. Identity, references, lane numbering, and numeric invariants
/// are left to openstrata::rn::Validate(), which ImportNetwork() runs.
///
/// Every element and attribute that is read but not represented is counted
/// into one warning per kind of loss (ADR 0002 section 4, ADR 0006 section 4).
namespace openstrata::rn::odr {

/// The lane types of OpenDRIVE 1.4 to 1.9, deprecated values included
/// (ADR 0002 section 3, FMT-01 section 3.4). Any other value is ODR-E009.
bool IsKnownLaneType(std::string_view type) noexcept;

/// Reads the header's `<geoReference>` and `<offset>` into `*source`, which
/// already holds the header attributes (Document::Source()), and counts the
/// header elements it does not represent. Returns false and fills `*error`
/// on the first failure. `std::bad_alloc` may escape.
bool DecodeHeader(const Document& document, SourceInfo* source, std::vector<Warning>* warnings,
    Diagnostic* error);

/// Decodes the header, roads, junctions, and the lane graph into `*data`,
/// with `data->laneLinks` in canonical order. `*warnings` receives one entry
/// per warning code, in ascending code order. Returns false and fills
/// `*error` on the first failure. The result is not validated.
/// `std::bad_alloc` may escape.
bool Decode(const Document& document, RoadNetworkData* data, std::vector<Warning>* warnings,
    Diagnostic* error);

/// What one read produces.
struct ImportResult {
  RoadNetworkData data;          ///< `data.warnings` mirrors `warnings`.
  ElementCounts counts;          ///< From the header scan.
  std::vector<Warning> warnings; ///< Ascending code order.
};

/// One read of an asset's bytes: Document::Parse(), then either the header
/// only (`metadataOnly`, ADR 0006 section 2) or the full Decode() followed by
/// openstrata::rn::Validate(). A validation failure is relayed with its
/// shared code. Returns false and fills `*error` on the first failure.
/// `std::bad_alloc` may escape.
bool ImportNetwork(std::string_view bytes, bool metadataOnly, ImportResult* result,
    Diagnostic* error);

} // namespace openstrata::rn::odr
