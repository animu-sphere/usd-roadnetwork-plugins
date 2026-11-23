# Reports

Reports contain historical evidence from real runs and investigations. They
record what was observed on a specific tool version, runtime, host, and date;
they are not current-state contracts.

| Document | Contents |
| --- | --- |
| [format-spike-opendrive-2026-11-22.md](format-spike-opendrive-2026-11-22.md) | FMT-01: OpenDRIVE versions 1.4–1.9 and their recognition, specification licensing, coordinates and georeference, the elements Phase 1 needs, XML parsers, existing libraries, and fixtures. |
| [adr-probes-2026-11-22.md](adr-probes-2026-11-22.md) | ADR-01..05: prim-name validity, single-precision error, and composition on OpenUSD 26.08; plan-view continuity, link reciprocity, and tessellation size in CARLA `Town01` and `Town02`. |
| [adr-0001-lanelet2-mapping-2026-11-23.md](adr-0001-lanelet2-mapping-2026-11-23.md) | ADR 0001 acceptance evidence: the Lanelet2 `mapping_example.osm` mapped on paper onto the common layer — identity, derived lane-end edges, boundaries, coordinates, regulatory elements, and the USD layout. |
| [rel-01-inspection-2026-11-23.md](rel-01-inspection-2026-11-23.md) | REL-01: the `usdview` launch (L6) on the `lookdev` runtime; Storm drawings of the fixtures and CARLA, and single-precision steps far from the origin; and the Windows DLL timestamps that broke the reproducible package digest, fixed with `/Brepro`. |
| [format-spike-opendrive-2026-11-23.md](format-spike-opendrive-2026-11-23.md) | FMT-02: the Phase 2 elements in 222 OpenDRIVE networks by producer and version; the error of VIRES `odrSpiral` and of starting a spiral at non-zero curvature; `paramPoly3` length mismatches; redistributable fixtures; and the proposed Phase 2 scope. |

Name format spikes `format-spike-<format>-<yyyy-mm-dd>.md`. A spike covers the
items in [design policy §4.1](../design/DESIGN_POLICY.md#41-format-acceptance-prerequisites).

## What belongs where

- Current structure belongs in [architecture/](../architecture/).
- Current behavior belongs in [reference/](../reference/).
- Design rationale belongs in [design/](../design/) and [adr/](../adr/).
- Incomplete work belongs in [roadmap/](../roadmap/).
- Tagged behavior belongs in [releases/](../releases/).
- The root [CHANGELOG.md](../../CHANGELOG.md) summarizes user-visible changes.

When a report disagrees with a current-state document, the current-state
document wins and the report remains unchanged as history. A later report may
add a correction, but the original observation is not rewritten.
