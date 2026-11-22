# API boundary

- **Status:** Proposed (2026-11-22).

This document owns the audience of each surface the repository exposes,
which repository owns which responsibility, and where compatibility
commitments live.

## Repository ownership

| Repository | Owns | Contract notes |
| --- | --- | --- |
| `usd-roadnetwork-plugins` | Format readers and decoders, `RoadNetworkData` and its validation, USD layer authoring, diagnostics, and the packaging and verification of its plugin artifacts. | Owns the data contract that runtimes read; does not run traffic, vehicles, or routing. |
| `usd-mobility-runtime` (candidate) | Integrated runtime composition: traffic, vehicles, pedestrians, collision, routing, and spatial queries built on the authored stages. | Uses the loaded semantics; never reimplements the parsing of a supported format, and no runtime logic enters the importer. Not a dependency of this repository. |
| [`open-strata`](https://github.com/animu-sphere/open-strata) | Build, inspection, test, and packaging tooling and the runtime matrix. | Pinned in [supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md). |
| [`usd-stage-runner`](https://github.com/animu-sphere/usd-stage-runner) | Interactive runtime hosting (integration candidate). | Verified by integration tests; this repository does not depend on it. |
| [`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) | Remote-asset resolution (composition candidate). | Relative references, caching, authorization, and failure behavior are designed before remote `.xodr` is supported. |
| Geospatial assets (DEM, terrain, other map data) | Placement alongside road networks. | The CRS, origin, and unit conventions of [ADR 0003](../adr/0003-crs-axes-units.md) are agreed first. |

## Tiers

| Tier | Audience | Installed? | Change policy |
| --- | --- | --- | --- |
| **Public behavior** | Users and applications opening or composing a USD stage. | Plugin binaries and `plugInfo.json`; no C++ header. | Changes to the stage hierarchy, property names, relationship direction, file-format arguments, or diagnostic codes need compatibility review. |
| **Contributor** | Developers adding a format bundle in this workspace or a fork. | `roadnetwork-core` and `roadnetwork-usd` headers, as a build necessity. | Changes update every in-repo bundle together. No external API or ABI guarantee. |
| **Plugin-private** | One bundle's translation units. | No. | Free to change. |

## Observable plugin behavior

Once accepted, the public contract covers:

- the **authored stage contract** in [WORKSPACE.md §5](WORKSPACE.md#5-authored-stage-contract);
- the **file-format arguments**, documented in the
  [capability matrix](../reference/CAPABILITY_MATRIX.md);
- the **stable diagnostic codes**, each with a machine-readable catalog
  shipped in the bundle's resources; codes are never renumbered or reused.

Integrated compatibility claims belong to the runtime that integrates the
plugins and must identify the component versions they cover.

## Future query surface

A read-only query interface (feature F-25 in the
[release plan](../roadmap/release-plan.md#phase-5--runtime-integration)) is
a candidate, not a commitment. Until its ADR exists:

- the lane graph is undirected, so only connectivity (for example "the lane
  ends joined to this one") may be offered; no API presents a lane-end link
  as a direction of travel;
- route planning, vehicle behavior, and spatial search stay with the runtime;
- the open questions are tracked in the
  [backlog](../roadmap/backlog.md#decisions-to-record-later).
