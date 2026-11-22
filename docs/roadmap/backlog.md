# Backlog

Open work without a phase assignment. The [current work](current.md) page owns
work assigned to the active phase. Each item names the repository responsible
for the next decision and the evidence needed to revisit it.

## Other format candidates

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Lanelet2 `.osm` | `usd-roadnetwork-plugins` | Phase 4 is scheduled and ADR 0008 has a signature that separates Lanelet2 from general OSM. | [ADR 0008](../adr/0008-lanelet2-osm-identification.md) |
| SUMO `.net.xml` | `usd-roadnetwork-plugins` | Phase 4 is scheduled, or `usd-mobility-runtime` needs traffic topology from SUMO. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| OpenStreetMap `.osm` / `.pbf` | `usd-roadnetwork-plugins` | A concrete use case defines which road graph and tags to import, and ADR 0008 has settled `.osm` routing. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| CityGML Transportation, GDF | `usd-roadnetwork-plugins` | A user need, an accessible specification, and a license review justify a dedicated ADR. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |
| OpenCRG | `usd-roadnetwork-plugins` | A consumer needs road-surface data alongside a network; evaluated as auxiliary data, not a network format. | [Format strategy](../design/DESIGN_POLICY.md#4-format-strategy) |

## OpenDRIVE beyond the MVP

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Signals and signal references | `usd-roadnetwork-plugins` | Phase 2 is scheduled; ADR 0002 has a property vocabulary for traffic controls. | [Design policy §7.2](../design/DESIGN_POLICY.md#72-after-the-mvp) |
| Road marks | `usd-roadnetwork-plugins` | Phase 2 is scheduled. | [OpenDRIVE mapping §5](../reference/OPENDRIVE_MAPPING.md#5-lanes) |
| Objects (guardrails, obstacles) | `usd-roadnetwork-plugins` | Phase 2 is scheduled. | [OpenDRIVE mapping §7](../reference/OPENDRIVE_MAPPING.md#7-signals-and-objects) |
| Surface meshes | `usd-roadnetwork-plugins` | ADR 0007 decides where generated surfaces live. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md) |

## Performance and loading

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| LOD, tiling, streaming, partial loading | `usd-roadnetwork-plugins` | Measurements on a large licensed network show a need, and ADR 0007 is accepted. | [Design policy §13](../design/DESIGN_POLICY.md#13-performance-and-large-data) |
| Runtime geometry generation (`UsdProcGenerativeProcedural`, Hydra) | `usd-roadnetwork-plugins` | ADR 0007 identifies it as the home for derived geometry. | [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md) |
| Remote assets through `usd-http-resolver` | `usd-roadnetwork-plugins` with `usd-http-resolver` | Relative-reference, security, cache, and failure behavior are defined. | [Design policy §13](../design/DESIGN_POLICY.md#13-performance-and-large-data) |

## USD contract and tooling

| Work | Owner | Revisit or close when | Reference |
| --- | --- | --- | --- |
| Codeless or typed USD schema | `usd-roadnetwork-plugins` | The shared vocabulary is stable across two formats. | [ADR 0002](../adr/0002-usd-schema-strategy.md) |
| Export to OpenDRIVE or another format | `usd-roadnetwork-plugins` | A concrete workflow needs export and a separate ADR defines losslessness and version targeting. | [Design policy §12](../design/DESIGN_POLICY.md#12-write-support) |
| Contributor guide for adding a format | `usd-roadnetwork-plugins` | The OpenDRIVE bundle is implemented, so the guide can generalize a working path. | [Workspace contract](../architecture/WORKSPACE.md) |
| Build and install guides | `usd-roadnetwork-plugins` | SCF-02 produces a working local build and package. | [Supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md) |
