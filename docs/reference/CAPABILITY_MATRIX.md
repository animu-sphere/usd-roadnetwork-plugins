# Capability matrix

This table describes the current tree. Planned capabilities belong in the
[roadmap](../roadmap/), not here. A row moves to **supported** only in the PR
that lands the implementation and its deterministic tests.

As of 2026-11-22 the repository contains the documentation, the scaffolded
`roadnetwork-opendrive` bundle, the `roadnetwork-core` model and validation
(unit-tested, but not yet produced by any decoder), and a skeleton
`roadnetwork-usd` library. The bundle registers the `.xodr` extension and,
for any file it can open, authors an empty layer through the shared writer
without parsing the file. That is a placeholder, not support, so every
OpenDRIVE row below remains unsupported; in particular the malformed
`invalid.xodr` fixture still opens as an empty stage.

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

| Capability | Status | Evidence / behavior |
| --- | --- | --- |
| Format detection (`CanRead`) | unsupported | — |
| Specification-version recognition | unsupported | — |
| Plan-view geometry: line, arc | unsupported | — |
| Plan-view geometry: spiral, poly3, paramPoly3 | unsupported | — |
| Elevation and superelevation profiles | unsupported | — |
| Lane sections, width, type | unsupported | — |
| Road and lane links | unsupported | — |
| Junctions and connections | unsupported | — |
| Signals, road marks, objects | unsupported | — |
| Georeference and offset | unsupported | — |
| Metadata-only read | unsupported | — |
| Stable diagnostics | unsupported | numbering ratified ([ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)); the `ODR-` and `RN-` catalogs ship empty in the bundle resources and are cross-checked against source by `roadnetworkOpendrive_diagnostics_catalog` |

## USD authoring

| Capability | Status | Evidence / behavior |
| --- | --- | --- |
| Semantic layer (`/Asset/Network`) | unsupported | contract proposed in [WORKSPACE.md §5](../architecture/WORKSPACE.md#5-authored-stage-contract) |
| Visualization layer (`/Asset/Visualization`) | unsupported | — |
| Reference, payload, reload | unsupported | on the empty layer, sublayer and reference composition and reload (forced, unforced after a file change, and under an outer `SdfChangeBlock`) pass `roadnetworkOpendrive_file_format_contract`; nothing is claimed until the layer has content |
| Export (`WriteToFile`) | unsupported | out of scope ([design policy §12](../design/DESIGN_POLICY.md#12-write-support)); `plugInfo.json` declares `supportsWriting: false`, so saving or exporting through the format fails with an Sdf error (tested) |
