# Capability matrix

This table describes the current tree. Planned capabilities belong in the
[roadmap](../roadmap/), not here. A row moves to **supported** only in the PR
that lands the implementation and its deterministic tests.

As of 2026-11-22 the repository contains the documentation, the scaffolded
`roadnetwork-opendrive` bundle, and skeleton `roadnetwork-core` and
`roadnetwork-usd` libraries. The bundle registers the `.xodr` extension and,
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
| Stable diagnostics | unsupported | numbering proposed in [DIAGNOSTICS.md](DIAGNOSTICS.md) |

## USD authoring

| Capability | Status | Evidence / behavior |
| --- | --- | --- |
| Semantic layer (`/Asset/Network`) | unsupported | contract proposed in [WORKSPACE.md §5](../architecture/WORKSPACE.md#5-authored-stage-contract) |
| Visualization layer (`/Asset/Visualization`) | unsupported | — |
| Reference, payload, reload | unsupported | — |
| Export (`WriteToFile`) | unsupported | out of scope ([design policy §12](../design/DESIGN_POLICY.md#12-write-support)) |
