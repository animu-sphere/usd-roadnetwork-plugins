// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openstrata/rn/RoadNetworkData.h"

#include "pxr/pxr.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class SdfLayer;
PXR_NAMESPACE_CLOSE_SCOPE

namespace openstrata::rn::usd {

/// Replace the contents of `layer` with the authored form of `data`.
///
/// This is the only place a bundle's stage is authored (WORKSPACE.md §5).
/// The stage contract is not accepted yet (ADRs 0002, 0003, and 0005), so the
/// writer currently authors an empty layer: no prims and no layer metadata.
///
/// On failure, `layer` is left unchanged, `*error` (when non-null) receives
/// a message, and the function returns false.
bool WriteLayer(const RoadNetworkData& data, PXR_NS::SdfLayer& layer, std::string* error);

} // namespace openstrata::rn::usd
