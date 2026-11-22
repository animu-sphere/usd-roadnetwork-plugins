# Capability matrix

This page owns what the current tree supports. Planned capabilities belong
in the [release plan](../roadmap/release-plan.md), which defines the feature
IDs in the *Feature* column. A row moves to **supported** only in the PR that
lands the implementation and its deterministic tests, and a supported row
must cite that evidence (`tools/check_docs.py` enforces it).

Today the `roadnetwork-opendrive` bundle registers `.xodr`, recognizes the
format and its version, and decodes and validates roads, lanes, junctions,
profiles, and the lane graph into `RoadNetworkData`. The shared
`roadnetwork-usd` writer authors the result into the
[stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract):
semantics under `/Asset/Network`, derived centerlines and borders under
`/Asset/Visualization`, and provenance and the loss summary on `/Asset`.
Input that the reader, the decoder, or validation rejects fails with one
stable `ODR-` or `RN-` error, and data that is read but not represented is
reported in counted warnings. ADRs 0001–0005, which define the model and the
stage contract, are accepted, and the rows below follow them. The format row stays
unsupported until the [Phase 1 exit](../roadmap/release-plan.md#phase-1-exit)
and the release gate pass.

## Status vocabulary

| Status | Meaning |
| --- | --- |
| **supported** | Implemented and covered by deterministic tests. |
| **policy default** | Authored by importer convention because the source does not define it. |
| **unsupported** | Intentionally rejected or not implemented. |
| **unverified** | Code or a dependency may allow it, but the project does not claim it without a fixture and gate. |

## Formats

| Format | Bundle | Status |
| --- | --- | --- |
| ASAM OpenDRIVE `.xodr` | `roadnetwork-opendrive` | unsupported (Phase 1 target) |
| Lanelet2 `.osm` | `roadnetwork-lanelet2` | unsupported (Phase 4 candidate) |
| SUMO `.net.xml` | `roadnetwork-sumo` | unsupported (Phase 4 candidate) |
| OpenStreetMap `.osm` / `.pbf` | — | unsupported (P2 candidate) |
| OpenCRG | — | unsupported (auxiliary data; evaluated separately) |

## OpenDRIVE input and semantic mapping

| Capability | Feature | Status | Evidence / behavior |
| --- | --- | --- | --- |
| Format detection (`CanRead`) | F-01 | supported | the [ADR 0006 §1](../adr/0006-fileformat-contract.md#1-canread) sniff of the first 64 KiB through `ArResolver`: a UTF-8 BOM, the XML declaration, comments, processing instructions, and a DOCTYPE may precede an unprefixed, un-namespaced `<OpenDRIVE>`; `roadnetworkOpendrive_reader` (24 prologue cases) and `roadnetworkOpendrive_file_format_contract` (the `reader/prologue-*` fixtures, CARLA `Town01` and `Town02`, a root beyond 64 KiB, UTF-16, a directory, and a missing asset) |
| Malformed-input rejection | F-01 | supported | `ODR-E001` to `ODR-E005` ([DIAGNOSTICS.md](DIAGNOSTICS.md#odr--reader)): exactly one error naming the asset, and no layer or stage, in full and metadata-only reads. Content errors from the decoder (`ODR-E008` to `ODR-E206`, [DIAGNOSTICS.md](DIAGNOSTICS.md#odr--decoder)) and from validation (`RN-`, relayed unchanged) fail a full read in the same way, and a metadata-only read of the same file opens. Each code has a negative fixture in the manifest's `negative` list, checked by `roadnetworkOpendrive_file_format_contract` and `roadnetworkOpendrive_decoder`. |
| Specification-version recognition | F-02 | supported | `<header revMajor revMinor>`: 1.4 to 1.9 open, other versions fail with `ODR-E007`, and malformed ones with `ODR-E006` ([FMT-01 §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy)); every `revMinor` from 0 to 11, and 2.0, in `roadnetworkOpendrive_file_format_contract` |
| Plan-view geometry: line, arc | F-04 | supported | `<line>` and `<arc>` become analytic segments ([mapping §3](OPENDRIVE_MAPPING.md#3-roads-and-reference-line)), authored in `double` as the `roadnetwork:referenceLine:*` arrays ([ADR 0002 §3](../adr/0002-usd-schema-strategy.md#3-phase-1-property-list)); `roadnetworkCore_geometry` (closed-form position and heading, [ADR 0004 evidence](../adr/0004-opendrive-geometry-retention.md#evidence)), `roadnetworkOpendrive_decoder` (`decoder/network.xodr`), and `roadnetworkUsd_layer_writer` (the reference-line group) |
| Plan-view geometry: spiral, poly3, paramPoly3 | F-12 | unsupported | rejected with `ODR-E201` (`decoder/spiral.xodr`) |
| Elevation profile | F-05 | supported | `<elevation>` is authored as the `roadnetwork:elevation:*` arrays and applied to the `z` of every visualization sample ([mapping §4](OPENDRIVE_MAPPING.md#4-profiles)); `roadnetworkUsd_layer_writer` and `roadnetworkCore_tessellation` |
| Superelevation, crossfall, lateral shape | F-13 | unsupported | a non-zero `<superelevation>`, `<crossfall>`, or `<shape>` is rejected with `ODR-E202` (`decoder/superelevation.xodr`); all-zero records are accepted and change nothing ([mapping §4](OPENDRIVE_MAPPING.md#4-profiles)) |
| Lane sections, width, type | F-05 | supported | lane sections, `<laneOffset>`, `<width>`, lane `type` (the OpenDRIVE 1.4–1.9 vocabulary), and `level` ([mapping §5](OPENDRIVE_MAPPING.md#5-lanes)) become `section_<n>` and lane prims ([ADR 0005 §2–§3](../adr/0005-prim-path-and-id-stability.md#2-derived-names)) with their properties; `roadnetworkOpendrive_decoder` and `roadnetworkUsd_layer_writer` (the section and lane groups, lane order); `<border>` is `ODR-E203` and a temporary lane layer `ODR-E205` |
| Road and lane links | F-06 | supported | road links become `roadnetwork:predecessor` and `:successor` relationships to a road or junction, with a contact token for a road; each canonical lane-graph edge ([mapping §5.1](OPENDRIVE_MAPPING.md#51-lane-graph)) is authored on both of its lanes as `roadnetwork:startLinks` or `:endLinks`, sorted by path ([ADR 0005 §4](../adr/0005-prim-path-and-id-stability.md#4-relationships)); `roadnetworkOpendrive_decoder` (the six edges of `decoder/network.xodr` from lane links and from junction lane links alone; 270 and 356 edges in CARLA `Town01` and `Town02`), `roadnetworkUsd_layer_writer`, and `roadnetworkOpendrive_file_format_contract` (targets remapped under two references) |
| Junctions and connections | F-06 | supported | `default` junctions and their connections ([mapping §6](OPENDRIVE_MAPPING.md#6-junctions)) become junction and connection prims with `roadnetwork:incomingRoad` and `:connectingRoad` relationships, the contact point, and the source lane numbers of each `<laneLink>`; a road in a junction has a `roadnetwork:junction` relationship; `roadnetworkUsd_layer_writer` (the junction and connection groups); `direct`, `virtual`, and `crossing` junctions are `ODR-E204` |
| Signals, road marks, objects | F-13, F-14, F-15 | unsupported | not decoded; counted in `ODR-W201` to `ODR-W203` |
| Georeference and offset | F-09 | supported | authored verbatim on `/Asset` as `roadnetwork:source:geoReference`, `:offset` (`double3`), and `:offsetHeading`, never parsed or applied ([ADR 0003 §3–§4](../adr/0003-crs-axes-units.md#3-georeference), [mapping §2](OPENDRIVE_MAPPING.md#2-coordinate-frames)); `roadnetworkOpendrive_file_format_contract` (`writer/projected.xodr` through a reference, and both CARLA towns) and `roadnetworkUsd_layer_writer` (absent values are not authored) |
| Axes and units | F-09 | supported | every layer authors `upAxis = "Z"` and `metersPerUnit = 1`, source facts for OpenDRIVE ([ADR 0003 §1](../adr/0003-crs-axes-units.md#1-axes-and-units), [mapping §2](OPENDRIVE_MAPPING.md#2-coordinate-frames)); speeds are authored in metres per second; `roadnetworkUsd_layer_writer` and `roadnetworkOpendrive_file_format_contract` (full and metadata-only reads agree) |
| Large coordinates | F-09 | supported | every value under `/Asset/Network` is a `double`; visualization points are `float` relative to an origin, a multiple of 1,024 m, authored as a `double` `xformOp:translate` on `/Asset/Visualization` when the points leave ±8,192 m ([ADR 0003 §5](../adr/0003-crs-axes-units.md#5-precision)); through a reference, every point of `writer/projected.xodr` (about 5,400 km from the origin) is within 0.5 mm of its closed-form curve, and CARLA authors no shift (`roadnetworkOpendrive_file_format_contract`); `roadnetworkUsd_layer_writer` checks the points against their samples. Storm still draws such a network in steps of the world-space `float` spacing (about 0.5 m at 5,400 km); reference it under a translate that brings it near the origin to inspect it ([REL-01 report §4](../reports/rel-01-inspection-2026-11-23.md#4-far-from-the-origin)) |
| Metadata-only read | F-10 | supported | checks the container, root, header, and version like a full read, decodes the header only, and authors the layer metadata, `/Asset` with the provenance and element counts of a full read and the warnings of the header scan, and an empty `/Asset/Network` ([ADR 0006 §2](../adr/0006-fileformat-contract.md#2-metadataonly)); `roadnetworkOpendrive_file_format_contract` on `basic.xodr`, `decoder/network.xodr`, and CARLA `Town01`, and `roadnetworkUsd_layer_writer` (a header-only write) |
| Stable diagnostics | F-11 | supported | numbering ratified ([ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)); both catalogs ship in the bundle resources and are cross-checked against source by `roadnetworkOpendrive_diagnostics_catalog`; every full read validates the model and relays an `RN-` error or warning unchanged, and each warning code is posted once per read with its count and first five locations (`roadnetworkOpendrive_file_format_contract`, `roadnetworkOpendrive_decoder`); the loss summary is authored on `/Asset` as `roadnetwork:import:warningCodes` and `:warningCounts`, which the contract test compares with the posted warnings; a writer failure is `RN-E901`, which leaves the layer unchanged (`roadnetworkUsd_layer_writer`) |

## File-format arguments

Phase 1 exposes none ([ADR 0006 §7](../adr/0006-fileformat-contract.md#7-file-format-arguments)).
Planned arguments are defined in [ADR 0009](../adr/0009-file-format-arguments-up-axis.md).

| Capability | Feature | Status | Evidence / behavior |
| --- | --- | --- | --- |
| `upAxis=Y` in the asset path or `FindOrOpen` arguments | F-27 | unsupported | — |
| `upAxis` from payload prim metadata (dynamic payload) | F-28 | unsupported | — |

## USD authoring

| Capability | Feature | Status | Evidence / behavior |
| --- | --- | --- | --- |
| Semantic layer (`/Asset/Network`) | F-07 | supported | `/Asset` (`Xform`, kind `component`, `defaultPrim`) and `/Asset/Network` with road, lane-section, lane, junction, and connection `Scope` prims in source order, the [ADR 0002 §3](../adr/0002-usd-schema-strategy.md#3-phase-1-property-list) properties (custom, `uniform`, no single-precision value), and relationships whose targets stay inside `/Asset`; names follow the injective encoding of [ADR 0005 §1](../adr/0005-prim-path-and-id-stability.md#1-encoding-source-ids); `roadnetworkUsd_layer_writer` (each property group, and the encoding and its inverse), `usdchecker` on the layers of `basic.xodr`, `decoder/network.xodr`, `writer/projected.xodr`, and CARLA `Town01` and `Town02` (`roadnetworkOpendrive_file_format_contract`), and the L5 golden of `basic.xodr` |
| Visualization layer (`/Asset/Visualization`) | F-08 | supported | `Centerlines` and `Boundaries` hold one linear `BasisCurves` per road, from `TessellateRoad()` within 1 cm of the analytic curves ([ADR 0004 §3](../adr/0004-opendrive-geometry-retention.md#3-tessellation)): one curve per lane of each section, with `roadnetwork:represents` naming each curve's lane ([ADR 0005 §5](../adr/0005-prim-path-and-id-stability.md#5-visualization)); `roadnetworkUsd_layer_writer` (curve order, counts, points, extent) and `roadnetworkCore_tessellation`; `usdview` opens `basic.xodr` at L6 on the `lookdev` runtime, and Storm draws the curves of the fixtures and of CARLA `Town01` and `Town02` ([REL-01 report §2–§3](../reports/rel-01-inspection-2026-11-23.md#2-usdview-launch-l6)) |
| Reference, payload, reload | F-10 | supported | sublayer, reference, and payload (unloaded, loaded, and unloaded again) composition; two references of one network keep their own relationship targets, also after flattening; reload (forced, unforced after a file change, and under an outer `SdfChangeBlock`) keeps every path and relationship target of an unchanged file, and a failed reload keeps the previous content (`roadnetworkOpendrive_file_format_contract`) |
| Export (`WriteToFile`) | — | unsupported | out of scope ([design policy §12](../design/DESIGN_POLICY.md#12-write-support)); `plugInfo.json` declares `supportsWriting: false`, so saving or exporting through the format fails with an Sdf error (tested) |
