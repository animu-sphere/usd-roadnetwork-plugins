# Reports

Reports contain historical evidence from real runs and investigations. They
record what was observed on a specific tool version, runtime, host, and date;
they are not current-state contracts.

| Document | Contents |
| --- | --- |
| [format-spike-opendrive-2026-11-22.md](format-spike-opendrive-2026-11-22.md) | FMT-01: OpenDRIVE versions 1.4–1.9 and their recognition, specification licensing, coordinates and georeference, the elements Phase 1 needs, XML parsers, existing libraries, and fixtures. |

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
