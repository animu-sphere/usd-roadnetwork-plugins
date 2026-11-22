# Import diagnostics

- **Status:** Normative. The numbering scheme was ratified by
  [ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)
  on 2026-11-22, before the first code was assigned. Codes are never
  renumbered or reused. No code is assigned yet; both catalogs are empty.

Each import error and warning carries a bracketed stable code at the start
of its message (`[ODR-E001] ...`). Automation matches the code, so message
wording can evolve. Each owner keeps a machine-readable catalog (severity,
summary, action) in the
[`roadnetwork.diagnostics/v1` format](../adr/0006-fileformat-contract.md#6-diagnostic-numbering),
every bundle ships its own catalog and a copy of the `RN-` catalog in its
plugin resources, and the `roadnetworkOpendrive_diagnostics_catalog` test
cross-checks the catalogs against the codes used in source.

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
| `ODR-` | `roadnetwork-opendrive` bundle: reader and decoder failures | [`plugins/roadnetwork-opendrive/plugin/resources/roadnetwork-opendrive/diagnostics.json`](../../plugins/roadnetwork-opendrive/plugin/resources/roadnetwork-opendrive/diagnostics.json) |
| `RN-` | Shared `roadnetwork-core` validation and `roadnetwork-usd` authoring, independent of the source format; raised only by shared library code | [`libs/roadnetwork-core/resources/diagnostics.json`](../../libs/roadnetwork-core/resources/diagnostics.json), shipped in every bundle as `rn-diagnostics.json` |

Future bundles take their own prefix (for example `LL2-` for Lanelet2 and
`SUMO-` for SUMO); ADR 0008 confirms them.

Unlike `usd-3dgs-plugins`, where each bundle owns every code including
writer failures, a shared, format-independent condition such as a lossy
mapping has one `RN-` code across formats, which suits the cross-format
contract tests.

## Categories

The examples are illustrative; a code is assigned only in the PR that first
raises it.

| Range | Category | Examples (illustrative) |
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
- Each warning code is posted at most once per read, with the number of
  affected elements and the IDs of at most the first five.
- A failed read posts exactly one error and leaves the target layer
  unchanged.
- File-open failures identify the source asset; reference failures identify
  the source element and ID.
