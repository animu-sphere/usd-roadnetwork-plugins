# Import diagnostics

- **Status:** Normative. The numbering scheme was ratified by
  [ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering)
  on 2026-11-22, before the first code was assigned. Codes are never
  renumbered or reused. The `RN-` validation codes and the `ODR-` reader and
  decoder codes below are assigned.

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
| `0xx` | Input and container: unreadable file, malformed XML, wrong root element, unsupported version, invalid file-format argument | `ODR-E001` XML parse error |
| `1xx` | Identity and references: missing, duplicated, or unresolved IDs; inconsistent links | `ODR-E101` unknown reference |
| `2xx` | Semantic mapping: unsupported element, lossy or ignored data | `RN-W201` lossy mapping |
| `3xx` | Geometry and numeric: non-finite values, invalid curve parameters, discontinuities beyond tolerance | — |
| `4xx` | Coordinates and georeference: unknown or unparsable CRS, conflicting offsets | — |
| `5xx` | Import limits: sizes and counts beyond declared bounds | — |
| `9xx` | Internal: invariant violations that indicate a bug | — |

## Assigned codes

The catalogs are authoritative for summaries and actions; this table lists
what raises each code.

### `RN-` shared validation

`openstrata::rn::Validate()` in `roadnetwork-core` checks a decoded
`RoadNetworkData` against the invariants of
[ADR 0001 §6](../adr/0001-roadnetwork-data-model.md#6-validation) and returns
the first failure: identity, then references, then lane structure, then
numeric values, each in source order. The plan-view tolerances are those of
[ADR 0004 §6](../adr/0004-opendrive-geometry-retention.md#6-discontinuities).
The `roadnetwork-opendrive` bundle calls it after every full read and relays
its error or warnings unchanged. Each code that `.xodr` input can reach has a
negative fixture in
[`tests/fixtures/decoder/`](../../plugins/roadnetwork-opendrive/tests/fixtures/decoder/),
and `roadnetworkCore_validation` fails every code on the model.

| Code | Condition |
| --- | --- |
| `RN-E101` | A road, junction, or junction connection has an empty ID. |
| `RN-E102` | A road or junction ID repeats within its kind, or a connection ID repeats within its junction. |
| `RN-E111` | A road's predecessor or successor names a road or junction that does not exist. |
| `RN-E112` | A road belongs to a junction that does not exist. |
| `RN-E113` | A junction connection names an incoming or connecting road that does not exist. |
| `RN-E114` | A junction connection's connecting road does not belong to that junction. |
| `RN-E115` | A lane-graph edge names a lane that does not exist. |
| `RN-E121` | A lane section does not have exactly one center lane. |
| `RN-E122` | Left lanes are not numbered `1..n`, or right lanes `-1..-m`, without gaps or repeats. |
| `RN-E301` | A numeric value is NaN or infinite. |
| `RN-E302` | A road or plan-view segment length is not positive. |
| `RN-E303` | The plan-view segments do not chain from `s = 0` to the road length (1e-6 m), or there are none. |
| `RN-E304` | A road has no lane section, or the lane-section `s` values do not ascend strictly from 0. |
| `RN-E305` | A plan-view segment starts more than 0.01 m from the end of the previous one. |
| `RN-E306` | A plan-view segment's start heading differs from the previous end heading by more than 0.001 rad. |
| `RN-E307` | Elevation, lane-offset, or lane-width records do not ascend strictly in `s0`. |
| `RN-E308` | On the inside of an arc, a border that is the inner border of a lane reaches the center of curvature (`t · κ ≥ 1`): the center border, or the outer border of a lane with another lane outside it ([ADR 0004 §2](../adr/0004-opendrive-geometry-retention.md#2-evaluation)). |
| `RN-W301` | Warning. On the inside of an arc, the outer border of the outermost lane on its side reaches the center of curvature. The semantics are kept and only that border's sampled outline loops. CARLA `Town01` has one such border and `Town02` two, all sidewalks. |

### `ODR-` reader

The `roadnetwork-opendrive` reader reports the first failure, in both full
and metadata-only reads. It checks the asset, the encoding, the XML, the root
element, the header, and the version, in that order, so a UTF-16 file reports
`ODR-E003` rather than a parse error. Each code has a negative fixture in
[`tests/fixtures/`](../../plugins/roadnetwork-opendrive/tests/fixtures/), named
in the bundle manifest's `negative` list, except `ODR-E001`, which the
contract test produces from a directory named `.xodr`, and `ODR-E901`, which
no input can produce on purpose.

| Code | Condition | Fixture |
| --- | --- | --- |
| `ODR-E001` | The resolver cannot open or read the asset. | a directory (generated) |
| `ODR-E002` | The document is not well-formed XML: a parse error (with line and column), no element, a second top-level element, or text outside the root element. | `invalid.xodr` |
| `ODR-E003` | The bytes are not UTF-8: a UTF-16 or UTF-32 byte-order mark or content, or an XML declaration naming another encoding. | `reader/not-utf8.xodr`; UTF-16 (generated) |
| `ODR-E004` | The root element is not `OpenDRIVE`, has a prefix, or declares a default namespace. | `reader/wrong-root.xodr`, `reader/namespaced-root.xodr` |
| `ODR-E005` | The first child element of `<OpenDRIVE>` is not `<header>`. | `reader/missing-header.xodr` |
| `ODR-E006` | `revMajor` or `revMinor` is missing or not a non-negative decimal integer. | `reader/version-malformed.xodr` |
| `ODR-E007` | The version is not `revMajor` 1 with `revMinor` 4 to 9 ([FMT-01 §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy)). | `reader/version-unsupported.xodr`; 1.0–1.3, 1.10, 1.11, and 2.0 (generated) |
| `ODR-E901` | An internal failure, such as running out of memory, escaped the reader or the decoder. | — |

### `ODR-` decoder

A full read decodes the document after the reader's checks, then runs
shared validation. The decoder reports what only the source syntax shows,
and reports the first failure in document order. Lane-link failures
(`ODR-E102` to `ODR-E104`) come after all of the content has been read. A
metadata-only read decodes the header only, so it raises none of these
codes except `ODR-E008`–`ODR-E010` on `<geoReference>` and `<offset>`. The
[OpenDRIVE mapping](OPENDRIVE_MAPPING.md) gives the element-by-element rules.
Each error has a negative fixture in
[`tests/fixtures/decoder/`](../../plugins/roadnetwork-opendrive/tests/fixtures/decoder/),
listed in the bundle manifest's `negative` list.

| Code | Condition | Fixture |
| --- | --- | --- |
| `ODR-E008` | A required element or attribute is missing: for example a road's `length` or `junction`, a `<geometry>` without `<line>` or `<arc>`, a lane's `type`, or a non-center lane without `<width>`. | `decoder/missing-attribute.xodr` |
| `ODR-E009` | An attribute value is not valid for its type: not an `xsd:double` or a 32-bit integer, or not a value OpenDRIVE 1.4–1.9 defines, such as a lane type, `contactPoint`, or speed unit. | `decoder/invalid-number.xodr`, `decoder/unknown-lane-type.xodr` |
| `ODR-E010` | An element is repeated or misplaced: two `<planView>` elements, two shapes in one `<geometry>`, a second `<header>`, a `<width>` on the center lane, and similar. | `decoder/duplicate-plan-view.xodr` |
| `ODR-E101` | A lane's `id` does not match its group: positive in `<left>`, `0` in `<center>`, negative in `<right>`. | `decoder/lane-group-mismatch.xodr` |
| `ODR-E102` | A lane predecessor or successor at a road end has no road link to follow. | `decoder/lane-link-without-road-link.xodr` |
| `ODR-E103` | A lane predecessor or successor at a road end leads into a junction. The junction's `<laneLink>` elements state these links. | `decoder/lane-link-into-junction.xodr` |
| `ODR-E104` | The end of a connection's incoming road at the junction cannot be determined. | `decoder/connection-unresolved.xodr` |
| `ODR-E201` | A `<spiral>`, `<poly3>`, or `<paramPoly3>` plan-view geometry ([ADR 0004 §5](../adr/0004-opendrive-geometry-retention.md#5-other-plan-view-shapes)). | `decoder/spiral.xodr` |
| `ODR-E202` | A `<superelevation>`, `<crossfall>`, or `<shape>` with a non-zero coefficient, or a `<crossSectionSurface>` ([ADR 0004 §4](../adr/0004-opendrive-geometry-retention.md#4-profiles-in-phase-1)). | `decoder/superelevation.xodr` |
| `ODR-E203` | A lane `<border>`. | `decoder/lane-border.xodr` |
| `ODR-E204` | A `direct`, `virtual`, or `crossing` junction, or a road link with `elementS` or `elementDir`. | `decoder/direct-junction.xodr` |
| `ODR-E205` | A temporary lane layer (`<lanes layer="temporary">`), or a lane link to one. | `decoder/temporary-lane-layer.xodr` |
| `ODR-E206` | An element this decoder does not know, inside the plan view, a profile, or the lane structure, where it could change the geometry or the lanes. | `decoder/unknown-lane-element.xodr` |

Warnings report data that is read but not represented. Each code is posted
once per successful read, with the count and the first five locations. The
model's `warnings` mirrors them. A failed read posts no warning.
`decoder/network.xodr` raises `ODR-W201` to `ODR-W207`.

| Code | Not represented |
| --- | --- |
| `ODR-W201` | `<roadMark>`. |
| `ODR-W202` | `<signal>`, `<signalReference>`, and the root `<controller>`. |
| `ODR-W203` | `<object>`, `<objectReference>`, `<tunnel>`, and `<bridge>`. |
| `ODR-W204` | `<userData>`, `<include>`, and `<dataQuality>`, wherever they appear. |
| `ODR-W205` | A road `<speed>` whose `max` is `"no limit"` or `"undefined"` ([ADR 0002 §3](../adr/0002-usd-schema-strategy.md#3-phase-1-property-list)). |
| `ODR-W206` | Elements that OpenDRIVE defines and Phase 1 does not represent: lane `<speed>`, `<access>`, `<height>`, `<material>`, `<rule>`, and `<visibility>`; road `<surface>`, `<railroad>`, and `<neighbor>`; junction `<priority>`, `<controller>`, `<surface>`, `<boundary>`, `<elevationGrid>`, and `<crossPath>`; connection `<predecessor>` and `<successor>`; header `<license>` and `<defaultRegulations>`; `<junctionGroup>` and `<station>`. |
| `ODR-W207` | Attributes the model does not represent, on the elements it decodes, such as road `rule` and road type `type` and `country`. |
| `ODR-W208` | Elements unknown to this decoder, outside the plan view, the profiles, and the lane structure. |

## Rules

- A warning is allowed only when the loss is explicit and bounded; a broken
  road, lane, or junction relationship is always an error.
- Each warning code is posted at most once per read, with the number of
  affected elements and the IDs of at most the first five.
- A failed read posts exactly one error and leaves the target layer
  unchanged.
- File-open failures identify the source asset; reference failures identify
  the source element and ID.
