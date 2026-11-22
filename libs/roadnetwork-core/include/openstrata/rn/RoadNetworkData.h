// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace openstrata::rn {

/// The format-independent road network that every decoder produces and the
/// shared USD writer consumes.
///
/// It has no members yet: identity, topology, and geometry are decided by
/// ADR 0001 (docs/adr/0001-roadnetwork-data-model.md). Until then a decoder
/// produces an empty value and the writer authors an empty layer.
struct RoadNetworkData {};

} // namespace openstrata::rn
