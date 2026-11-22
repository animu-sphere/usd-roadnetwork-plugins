# Capability matrix

This page owns what the current tree supports. Planned capabilities belong
in the [release plan](../roadmap/release-plan.md), which defines the feature
IDs in the *Feature* column. A row moves to **supported** only in the PR that
lands the implementation and its deterministic tests, and a supported row
must cite that evidence (`tools/check_docs.py` enforces it).

Today the `roadnetwork-opendrive` bundle registers `.xodr` and recognizes
the format and its version. A full read decodes roads, lanes, junctions,
profiles, and the lane graph into `RoadNetworkData` and validates the result.
Input that the reader, the decoder, or validation rejects fails with one
stable `ODR-` or `RN-` error, and data that is read but not represented is
reported in counted warnings. CARLA `Town01` and `Town02` decode and
validate. A file that passes still opens as an empty layer, because the
shared writer authors nothing until the stage contract (USD-01). The content
rows below therefore stay unsupported: the evidence column says what is
decoded and tested so far.

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
| Plan-view geometry: line, arc | F-04 | unsupported | decoded and validated, not authored until USD-01: `<line>` and `<arc>` become analytic segments ([mapping §3](OPENDRIVE_MAPPING.md#3-roads-and-reference-line)); `roadnetworkOpendrive_decoder` checks them on `decoder/network.xodr`, and the `roadnetwork-core` evaluator passes the closed-form tests of `roadnetworkCore_geometry` ([ADR 0004 evidence](../adr/0004-opendrive-geometry-retention.md#evidence)) |
| Plan-view geometry: spiral, poly3, paramPoly3 | F-12 | unsupported | rejected with `ODR-E201` (`decoder/spiral.xodr`) |
| Elevation and superelevation profiles | F-05, F-13 | unsupported | `<elevation>` decoded, not authored until USD-01; a non-zero `<superelevation>`, `<crossfall>`, or `<shape>` is rejected with `ODR-E202` (`decoder/superelevation.xodr`), and all-zero records are accepted ([mapping §4](OPENDRIVE_MAPPING.md#4-profiles)) |
| Lane sections, width, type | F-05 | unsupported | decoded and validated, not authored until USD-01: lane sections, `<laneOffset>`, `<width>`, lane `type` (the OpenDRIVE 1.4–1.9 vocabulary), and `level` ([mapping §5](OPENDRIVE_MAPPING.md#5-lanes)); `<border>` is `ODR-E203` and a temporary lane layer `ODR-E205` |
| Road and lane links | F-06 | unsupported | decoded and validated, not authored until USD-01: road links, and a canonical lane graph built from one-sided or two-sided lane links ([mapping §5.1](OPENDRIVE_MAPPING.md#51-lane-graph)); `roadnetworkOpendrive_decoder` checks the six edges of `decoder/network.xodr` from lane links and from junction lane links alone, and CARLA `Town01` and `Town02` give 270 and 356 edges |
| Junctions and connections | F-06 | unsupported | `default` junctions, connections, and lane links decoded and validated, not authored until USD-01 ([mapping §6](OPENDRIVE_MAPPING.md#6-junctions)); `direct`, `virtual`, and `crossing` junctions are `ODR-E204` |
| Signals, road marks, objects | F-13, F-14, F-15 | unsupported | not decoded; counted in `ODR-W201` to `ODR-W203` |
| Georeference and offset | F-09 | unsupported | decoded verbatim in full and metadata-only reads ([mapping §2](OPENDRIVE_MAPPING.md#2-coordinate-frames)), not authored until USD-01 |
| Metadata-only read | F-10 | unsupported | a metadata-only read checks the container, root, header, and version too (tested), and decodes the header only: provenance, georeference, offset, the reader's element counts, and the header's own warnings (`roadnetworkOpendrive_reader`, `roadnetworkOpendrive_decoder`); nothing is authored on `/Asset` until the stage contract (USD-01) |
| Stable diagnostics | F-11 | unsupported | numbering ratified ([ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)); both catalogs ship in the bundle resources and are cross-checked against source by `roadnetworkOpendrive_diagnostics_catalog`; the `RN-` validation codes are assigned and unit-tested; every full read validates the model and relays an `RN-` error or warning unchanged, and the decoder posts each warning code once per read with its count and first five locations (`roadnetworkOpendrive_file_format_contract`, `roadnetworkOpendrive_decoder`); the loss summary on `/Asset` waits for the stage contract (USD-01) |

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
| Semantic layer (`/Asset/Network`) | F-07 | unsupported | contract proposed in [WORKSPACE.md §5](../architecture/WORKSPACE.md#5-authored-stage-contract) |
| Visualization layer (`/Asset/Visualization`) | F-08 | unsupported | not authored yet; the `roadnetwork-core` tessellator keeps borders and centerlines within 1 cm of the analytic curves on a hand-authored road (`roadnetworkCore_tessellation`) |
| Reference, payload, reload | F-10 | unsupported | on the empty layer, sublayer and reference composition and reload (forced, unforced after a file change, and under an outer `SdfChangeBlock`) pass `roadnetworkOpendrive_file_format_contract`, and a failed reload keeps the previous layer content; nothing is claimed until the layer has content |
| Export (`WriteToFile`) | — | unsupported | out of scope ([design policy §12](../design/DESIGN_POLICY.md#12-write-support)); `plugInfo.json` declares `supportsWriting: false`, so saving or exporting through the format fails with an Sdf error (tested) |
