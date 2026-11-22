# Capability matrix

This page owns what the current tree supports. Planned capabilities belong
in the [release plan](../roadmap/release-plan.md), which defines the feature
IDs in the *Feature* column. A row moves to **supported** only in the PR that
lands the implementation and its deterministic tests, and a supported row
must cite that evidence (`tools/check_docs.py` enforces it).

Today the `roadnetwork-opendrive` bundle registers `.xodr` and, for any file
it can open, authors an empty layer through the shared writer without
parsing the file. That is a placeholder, not support, so every OpenDRIVE row
below remains unsupported; in particular the malformed `invalid.xodr`
fixture still opens as an empty stage. The `roadnetwork-core` model and
validation are unit-tested but produced by no decoder yet.

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
| Format detection (`CanRead`) | F-01 | unsupported | — |
| Specification-version recognition | F-02 | unsupported | — |
| Plan-view geometry: line, arc | F-04 | unsupported | — |
| Plan-view geometry: spiral, poly3, paramPoly3 | F-12 | unsupported | — |
| Elevation and superelevation profiles | F-05, F-13 | unsupported | — |
| Lane sections, width, type | F-05 | unsupported | — |
| Road and lane links | F-06 | unsupported | — |
| Junctions and connections | F-06 | unsupported | — |
| Signals, road marks, objects | F-13, F-14, F-15 | unsupported | — |
| Georeference and offset | F-09 | unsupported | — |
| Metadata-only read | F-10 | unsupported | — |
| Stable diagnostics | F-11 | unsupported | numbering ratified ([ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)); both catalogs ship in the bundle resources and are cross-checked against source by `roadnetworkOpendrive_diagnostics_catalog`; the `RN-` validation codes are assigned and unit-tested, but no read raises a code yet and the `ODR-` catalog is empty |

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
| Visualization layer (`/Asset/Visualization`) | F-08 | unsupported | — |
| Reference, payload, reload | F-10 | unsupported | on the empty layer, sublayer and reference composition and reload (forced, unforced after a file change, and under an outer `SdfChangeBlock`) pass `roadnetworkOpendrive_file_format_contract`; nothing is claimed until the layer has content |
| Export (`WriteToFile`) | — | unsupported | out of scope ([design policy §12](../design/DESIGN_POLICY.md#12-write-support)); `plugInfo.json` declares `supportsWriting: false`, so saving or exporting through the format fails with an Sdf error (tested) |
