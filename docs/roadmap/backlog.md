# Backlog

Open work that is not in the active phase. The [current work](current.md)
page owns work assigned to the active phase. Each item names the repository
responsible for the next decision and the evidence needed to revisit it.
When an item is a feature of a later phase, its `F-` ID points to the
[release plan](release-plan.md), which owns the feature's definition; this
page owns only the trigger to schedule it.

## Other format candidates

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Lanelet2 `.osm` (F-22, F-24) | `usd-roadnetwork-plugins` | Phase 4 is scheduled and ADR 0008 has a signature that separates Lanelet2 from general OSM. | [ADR 0008](../adr/0008-lanelet2-osm-identification.md) |
| SUMO `.net.xml` (F-23) | `usd-roadnetwork-plugins` | Phase 4 is scheduled, or `usd-mobility-runtime` needs traffic topology from SUMO. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| OpenStreetMap `.osm` / `.pbf` | `usd-roadnetwork-plugins` | A concrete use case defines which road graph and tags to import, and ADR 0008 has settled `.osm` routing. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| CityGML Transportation, GDF | `usd-roadnetwork-plugins` | A user need, an accessible specification, and a license review justify a dedicated ADR. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| OpenCRG | `usd-roadnetwork-plugins` | A consumer needs road-surface data alongside a network; evaluated as auxiliary data, not a network format. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |

## OpenDRIVE beyond the MVP

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Signals and signal references (F-14) | `usd-roadnetwork-plugins` | Phase 2 is scheduled; ADR 0002 has a property vocabulary for traffic controls. | [Design policy §7.2](../design/DESIGN_POLICY.md#72-after-the-mvp) |
| Road marks (F-13) | `usd-roadnetwork-plugins` | Phase 2 is scheduled. | [OpenDRIVE mapping §5](../reference/OPENDRIVE_MAPPING.md#5-lanes) |
| Objects (guardrails, obstacles) (F-15) | `usd-roadnetwork-plugins` | Phase 2 is scheduled. | [OpenDRIVE mapping §7](../reference/OPENDRIVE_MAPPING.md#7-signals-and-objects) |
| Surface meshes (F-17) | `usd-roadnetwork-plugins` | ADR 0007 decides where generated surfaces live. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md) |
| `direct` and `virtual` junctions | `usd-roadnetwork-plugins` | Phase 2 is scheduled and ADR 0001 can represent their connectivity. Phase 1 rejects them. | [FMT-01 spike §3.4](../reports/format-spike-opendrive-2026-11-22.md#34-differences-that-touch-the-phase-1-elements) |
| Lane `<border>` (F-13) | `usd-roadnetwork-plugins` | Phase 2 is scheduled. Phase 1 rejects it. | [FMT-01 spike §3.4](../reports/format-spike-opendrive-2026-11-22.md#34-differences-that-touch-the-phase-1-elements) |
| 1.9.0 lane layers and roadworks | `usd-roadnetwork-plugins` | A fixture that uses them is available and Phase 2 is scheduled. Phase 1 rejects them. | [FMT-01 spike §3.4](../reports/format-spike-opendrive-2026-11-22.md#34-differences-that-touch-the-phase-1-elements) |
| Compressed `.xodrz` | `usd-roadnetwork-plugins` | A user needs it and its container is documented; FMT-01 did not examine it. | [FMT-01 spike §10](../reports/format-spike-opendrive-2026-11-22.md#10-not-examined) |

## Performance and loading

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| LOD, tiling, streaming, partial loading (F-18, F-19, F-20) | `usd-roadnetwork-plugins` | Measurements on a large licensed network show a need, and ADR 0007 is accepted. | [Design policy §13](../design/DESIGN_POLICY.md#13-performance-and-large-data) |
| Runtime geometry generation (`UsdProcGenerativeProcedural`, Hydra) | `usd-roadnetwork-plugins` | ADR 0007 identifies it as the home for derived geometry. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md) |
| Remote assets through `usd-http-resolver` | `usd-roadnetwork-plugins` with `usd-http-resolver` | Relative-reference, security, cache, and failure behavior are defined. | [Design policy §13](../design/DESIGN_POLICY.md#13-performance-and-large-data) |

## USD contract and tooling

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| ADR 0009 probe: format arguments through sublayers, references, and payloads; dynamic payload metadata and precedence | `usd-roadnetwork-plugins` | Before F-27 is scheduled, or when a `Y`-up consumer needs it. | [ADR 0009](../adr/0009-file-format-arguments-up-axis.md#evidence-needed-before-acceptance) |
| Codeless or typed USD schema | `usd-roadnetwork-plugins` | The shared vocabulary is stable across two formats. | [ADR 0002](../adr/0002-usd-schema-strategy.md) |
| Export to OpenDRIVE or another format | `usd-roadnetwork-plugins` | A concrete workflow needs export and a separate ADR defines losslessness and version targeting. | [Design policy §12](../design/DESIGN_POLICY.md#12-write-support) |
| Contributor guide for adding a format | `usd-roadnetwork-plugins` | The OpenDRIVE bundle is implemented, so the guide can generalize a working path. | [Workspace contract](../architecture/WORKSPACE.md) |
| Build and install guides | `usd-roadnetwork-plugins` | SCF-02 produces a working local build and package. | [Supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md) |

## Decisions to record later

Questions that need an ADR, or an amendment to a Proposed one, before the
features that depend on them. None of them blocks Phase 1, and none changes
an existing ADR by being listed here.

| Question | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Query API boundary: queries over `RoadNetworkData` versus over the stage, how directed connectivity is exposed, and where runtime algorithms begin | `usd-roadnetwork-plugins` with `usd-mobility-runtime` | F-25 is scheduled. Until then no travel-direction API (such as `GetSuccessors()`) is published over the undirected lane graph. | [API boundary](../architecture/API_BOUNDARY.md#future-query-surface) |
| Projection of geodetic-only sources: the projection, the origin and how it is recorded, and the reported loss for formats such as Lanelet2 that have no metric frame | `usd-roadnetwork-plugins` | The Phase 4 format spike for Lanelet2 or OSM; decided in that format's ADR before it is accepted. | [ADR 0003 §2](../adr/0003-crs-axes-units.md#2-frame) |
| Lane graph beyond Phase 1: lateral adjacency, boundaries shared between lanes, and the physical elements that regulatory elements and signals refer to | `usd-roadnetwork-plugins` | The second format is mapped (F-22), or F-14 and F-15 are scheduled. | [Lanelet2 mapping §9](../reports/adr-0001-lanelet2-mapping-2026-11-23.md#9-effect-on-the-phase-1-adrs) |
| Travel-direction semantics: driving side, reversible and bidirectional lanes, time-dependent rules, turn restrictions | `usd-roadnetwork-plugins` | The second format is mapped (F-22 or F-23). | [ADR 0001 §3](../adr/0001-roadnetwork-data-model.md#3-topology) |
| Spatial-index lifecycle: whether bounding boxes and indexes live in the IR, in a derived cache, or in the runtime, and how they follow updates and unloading | `usd-roadnetwork-plugins` with `usd-mobility-runtime` | F-19 or F-25 is scheduled. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md) |
| Tile boundary and identity: identity across tiles, references to elements outside a tile, LOD consistency | `usd-roadnetwork-plugins` | F-19 is scheduled; a candidate amendment to ADR 0007. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md#constraints-for-earlier-phases) |
| Schema migration trigger: the condition for moving to applied API schemas and the compatibility test for stages authored before | `usd-roadnetwork-plugins` | The shared vocabulary is stable across two formats; a candidate amendment to ADR 0002. | [ADR 0002 §5](../adr/0002-usd-schema-strategy.md#5-migration-to-applied-api-schemas) |
| Runtime integration contract: CRS, origin, topology, supported version ranges, and remote-asset failure behavior | `usd-roadnetwork-plugins` with the integrating runtime | F-26 is scheduled. | [API boundary](../architecture/API_BOUNDARY.md#repository-ownership) |
