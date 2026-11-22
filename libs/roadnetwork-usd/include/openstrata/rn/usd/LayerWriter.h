// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/RoadNetworkData.h"
#include "openstrata/rn/Validation.h"

#include "pxr/pxr.h"

#include <cstdint>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class SdfLayer;
PXR_NAMESPACE_CLOSE_SCOPE

namespace openstrata::rn::usd {

/// The importer's record of one read, authored on `/Asset` as
/// `roadnetwork:import:*` (ADR 0002 section 3). The bundle supplies it,
/// because the counts come from its own scan of the source, which a
/// metadata-only read takes too (ADR 0006 section 2).
struct ImportRecord {
  std::string importer;        ///< The bundle identity, e.g. "roadnetwork-opendrive".
  std::string importerVersion; ///< The bundle version.
  std::uint32_t roadCount = 0;
  std::uint32_t junctionCount = 0;
  std::uint32_t laneCount = 0; ///< One per lane prim, center lanes included.
};

/// Replaces the contents of `layer` with the authored stage contract for
/// `data` (WORKSPACE.md section 5; ADRs 0002, 0003, and 0005):
///
/// - layer metadata `defaultPrim = "Asset"`, `upAxis = "Z"`, and
///   `metersPerUnit = 1`;
/// - `/Asset` (`Xform`, kind `component`) with the source and import
///   provenance and the warning summary of `data.warnings`;
/// - `/Asset/Network`, always, with a road, lane-section, and lane prim for
///   every lane, and a junction and connection prim for every connection;
/// - `/Asset/Visualization`, when there is at least one road: the lane
///   centerlines and borders that TessellateRoad() samples, as linear
///   `BasisCurves`, shifted by the ADR 0003 section 5 origin.
///
/// `data` must have passed Validate(). A header-only `data`, as a
/// metadata-only read decodes it, therefore authors `/Asset` and an empty
/// `/Asset/Network` only (ADR 0006 section 2).
///
/// The writer authors Sdf specs only, into a scratch layer, and transfers
/// the result into `layer` on success (ADR 0006 sections 3 and 5). On failure
/// `layer` is unchanged, `*error` (when non-null) receives an `RN-E901`
/// diagnostic, and the function returns false. The output is a pure function
/// of `data` and `record`.
bool WriteLayer(const RoadNetworkData& data, const ImportRecord& record, PXR_NS::SdfLayer& layer,
    Diagnostic* error);

} // namespace openstrata::rn::usd
