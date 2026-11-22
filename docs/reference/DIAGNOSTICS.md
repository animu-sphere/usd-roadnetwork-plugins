# Import diagnostics

- **Status:** Proposed numbering scheme (2026-11-22). The scheme is ratified
  by [ADR 0006](../adr/0006-fileformat-contract.md) **before** the first code
  is assigned; after that, codes are never renumbered or reused.

Each import error and warning carries a bracketed stable code. Automation
matches the code, so message wording can evolve. Each bundle ships a
machine-readable catalog (severity, summary, action) in its plugin resources,
and a test cross-checks the catalog against the codes used in source.

## Code format

```text
<PREFIX>-<SEVERITY><NNN>
```

| Part | Values |
| --- | --- |
| `PREFIX` | Owner of the code (below). |
| `SEVERITY` | `E` error (the load fails), `W` warning (the load continues with an explicit, recorded loss). |
| `NNN` | Three digits; the hundreds digit is the category. |

## Prefixes

| Prefix | Owner | Catalog |
| --- | --- | --- |
| `ODR-` | `roadnetwork-opendrive` bundle: reader and decoder failures | planned: `plugins/roadnetwork-opendrive/plugin/resources/roadnetwork-opendrive/diagnostics.json` |
| `RN-` | Shared `roadnetwork-core` validation and `roadnetwork-usd` authoring, independent of the source format | planned: shipped with every bundle |

Future bundles take their own prefix (for example `LL2-` for Lanelet2 and
`SUMO-` for SUMO); ADR 0008 confirms them.

*Open (ADR 0006):* `usd-3dgs-plugins` lets each bundle own every code,
including writer failures. This proposal instead gives shared,
format-independent conditions such as lossy mapping one `RN-` code across
formats, which suits the cross-format contract tests. The ADR decides between
the two.

## Categories

| Range | Category | Examples (provisional) |
| --- | --- | --- |
| `0xx` | Input and container: unreadable file, malformed XML, wrong root element, unsupported version | `ODR-E001` XML parse error |
| `1xx` | Identity and references: missing, duplicated, or unresolved IDs; inconsistent links | `ODR-E101` unknown reference |
| `2xx` | Semantic mapping: unsupported element, lossy or ignored data | `RN-W201` lossy mapping |
| `3xx` | Geometry and numeric: non-finite values, invalid curve parameters, discontinuities beyond tolerance | — |
| `4xx` | Coordinates and georeference: unknown or unparsable CRS, conflicting offsets | — |
| `5xx` | Import limits: sizes and counts beyond declared bounds | — |
| `9xx` | Internal: invariant violations that indicate a bug | — |

## Rules

- A warning is allowed only when the loss is explicit and bounded; a broken
  road, lane, or junction relationship is always an error.
- Warnings that affect many elements are aggregated into one message with a
  count.
- File-open failures identify the source asset; reference failures identify
  the source element and ID.
