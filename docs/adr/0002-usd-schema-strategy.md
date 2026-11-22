# ADR 0002 — USD schema strategy

- **Status:** Accepted (2026-11-23)
- **Scope:** how the semantic layer under `/Asset/Network` is typed in USD,
  and which properties the shared writer authors in Phase 1
- **Phase 1 owner:** USD-01 ([current work](../roadmap/current.md#active-work))

## Context

The design policy ([§6.1](../design/DESIGN_POLICY.md#61-separate-data-from-visualization))
prefers standard USD schemas. Standard `UsdGeom` types cannot express
road-network topology and semantics: lane connectivity, junction membership,
lane types, speed limits, or rule references. The policy also forbids
adopting a schema before its specification is settled.

The [ADR probes](../reports/adr-probes-2026-11-22.md#4-sample-stage) checked
a hand-written stage in the proposed layout on OpenUSD 26.08. It passes
`usdchecker`, and its relationships are remapped correctly when the layer is
referenced twice and when the stage is flattened. Layer metadata does not
reach a referencing stage, but attributes on `/Asset` do.

## Questions

1. Are semantic prims typeless (`def "Lane"`), of a standard type (`Scope`,
   `Xform`), or of a typed schema specific to this project?
2. Are properties namespaced attributes (for example
   `roadnetwork:laneType`) and relationships, or an applied API schema?
3. Where does source-specific data go (OpenDRIVE `<userData>`, vendor
   extensions): `customData`, namespaced attributes, or nowhere?
4. If a codeless or generated schema is introduced later, what is the
   migration path for stages that were already authored?

## Options considered

- **A. Standard prims plus namespaced attributes and relationships.**
  There is no plugin schema to ship or version, and ordinary tools can find
  the data. Typing is by convention only, and tests and a checker carry the
  validation.
- **B. Applied API schemas (codeless).** Typed property sets without compiled
  code, which `usdchecker` and the schema registry know about. Requires a
  schema package in the bundle and a versioning policy.
- **C. Typed IsA schemas (`RoadNetworkRoad`, `RoadNetworkLane`, …).**
  The strongest typing, with the highest commitment and migration cost.

## Decision

Option A for Phase 1. The property names are fixed as if they were schema
properties, so that a later move to B is a re-typing, not a semantic change.

### 1. Prim types

| Prim | Type |
| --- | --- |
| `/Asset` | `Xform`, `kind = "component"`, the layer's `defaultPrim` |
| `/Asset/Network` and its containers (`Roads`, `Junctions`) | `Scope` |
| Roads, lane sections, lanes, junctions, connections | `Scope` |
| `/Asset/Visualization` | `Xform`, which carries the origin shift of [ADR 0003](0003-crs-axes-units.md) |
| Its containers (`Centerlines`, `Boundaries`) | `Scope` |
| Generated curves | `BasisCurves`, `type = "linear"` |

The bundle ships no schema: no `generatedSchema.usda` and no schema
registration in `plugInfo.json`. [ADR 0005](0005-prim-path-and-id-stability.md)
fixes the prim names and the hierarchy.

### 2. Properties

- Every property the project defines is in the `roadnetwork:` namespace.
  Properties of standard schemas, such as `points` on `BasisCurves`, keep
  their standard names.
- `roadnetwork:source:*` holds source header data, verbatim, and appears on
  `/Asset` only. `roadnetwork:import:*` holds the importer's record of the
  read, also on `/Asset` only.
- Every semantic attribute is `uniform`. No attribute has time samples.
- Value types:
  - `double` for every length, angle, and coefficient, so that `/Network`
    contains no single-precision value;
  - `int` for lane numbers and counts;
  - `bool` for source flags, such as `level`;
  - `token` for closed vocabularies;
  - `string` for source IDs and opaque source text.
- A piecewise function is authored as parallel arrays of equal length, one
  per coefficient. The plan view is authored as parallel arrays with one
  entry per segment. These arrays are not child prims, which keeps segments
  ordered and cheap.
- A reference to another element is a relationship, so it is remapped under
  references and flattening. The ID of the target is not also authored as a
  string. The one exception is the lane numbers of a junction `<laneLink>`,
  which are kept as the source states them, next to the resolved lane
  relationships.
- An optional source value that is absent is not authored. No fallback value
  is written in its place.

### 3. Phase 1 property list

`/Asset`:

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:source:format` | `string` | `"opendrive"` |
| `roadnetwork:source:formatVersion` | `string` | `"<revMajor>.<revMinor>"`, for example `"1.4"` |
| `roadnetwork:source:name`, `:date`, `:vendor`, `:version` | `string` | header attributes, verbatim, when present |
| `roadnetwork:source:geoReference` | `string` | [ADR 0003](0003-crs-axes-units.md) |
| `roadnetwork:source:offset` | `double3` | [ADR 0003](0003-crs-axes-units.md) |
| `roadnetwork:source:offsetHeading` | `double` | [ADR 0003](0003-crs-axes-units.md) |
| `roadnetwork:import:importer` | `string` | the bundle identity, `"roadnetwork-opendrive"` |
| `roadnetwork:import:importerVersion` | `string` | the bundle version |
| `roadnetwork:import:roadCount`, `:junctionCount`, `:laneCount` | `int` | element counts; `laneCount` counts every lane prim, center lanes included |
| `roadnetwork:import:warningCodes` | `string[]` | the codes posted during the read, in ascending order |
| `roadnetwork:import:warningCounts` | `int[]` | the affected-element count for each code |

The absolute source path is not recorded: it differs between machines and
would make the output non-deterministic. A metadata-only read authors the
`roadnetwork:source:*` properties, the counts, and the warnings from the
header scan ([ADR 0006 §2](0006-fileformat-contract.md#2-metadataonly)).

Road:

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:id`, `roadnetwork:name` | `string` | source ID; name when present |
| `roadnetwork:length` | `double` | metres |
| `roadnetwork:junction` | `rel` | the junction, when the road belongs to one |
| `roadnetwork:predecessor`, `roadnetwork:successor` | `rel` | one road or junction, as the source states the road link, in the reference-line direction |
| `roadnetwork:predecessorContact`, `roadnetwork:successorContact` | `token` | `start` or `end`, when the target is a road |
| `roadnetwork:referenceLine:kind` | `token[]` | `line` or `arc` |
| `roadnetwork:referenceLine:s`, `:x`, `:y`, `:hdg`, `:length`, `:curvature` | `double[]` | one entry per segment; `curvature` is 0 for a line |
| `roadnetwork:elevation:s`, `:a`, `:b`, `:c`, `:d` | `double[]` | cubic records |
| `roadnetwork:laneOffset:s`, `:a`, `:b`, `:c`, `:d` | `double[]` | cubic records |
| `roadnetwork:speed:s`, `roadnetwork:speed:max` | `double[]` | road speed records, in metres per second |

Lane section: `roadnetwork:s` (`double`).

Lane:

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:laneNumber` | `int` | the signed source number |
| `roadnetwork:laneType` | `token` | the source spelling, for example `driving` or `bidirectional` |
| `roadnetwork:level` | `bool` | the source `level` attribute |
| `roadnetwork:width:sOffset`, `:a`, `:b`, `:c`, `:d` | `double[]` | cubic records, relative to the section start |
| `roadnetwork:startLinks` | `rel` | the lanes joined at this lane's start, from the lane graph |
| `roadnetwork:endLinks` | `rel` | the lanes joined at this lane's end |

A road's `predecessor` and `successor` keep OpenDRIVE's names, because
they record the road link as the source states it: a link stated on one
road only is authored on that road only. A lane's `startLinks` and
`endLinks` come from the undirected lane graph, normalized, and name the
lane's own ends. They do not use the road names, which a reader takes for a
direction of travel
([ADR 0005 §4](0005-prim-path-and-id-stability.md#4-relationships)).

Junction: `roadnetwork:id` and `roadnetwork:name` (`string`).

Connection:

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:id` | `string` | source ID |
| `roadnetwork:incomingRoad`, `roadnetwork:connectingRoad` | `rel` | the roads |
| `roadnetwork:contactPoint` | `token` | `start` or `end` |
| `roadnetwork:laneLink:from`, `roadnetwork:laneLink:to` | `int[]` | source lane numbers, in source order |

Visualization curves add `roadnetwork:represents`, a `rel` whose targets name
the semantic prim of each curve in curve order
([ADR 0005 §5](0005-prim-path-and-id-stability.md#5-visualization)).

Lane-type tokens are checked against the union of the values defined by
OpenDRIVE 1.4–1.9. An unknown value is an `ODR-` error. A speed whose maximum
is a string (`"no limit"`, `"undefined"`) is not authored in Phase 1 and is
reported as one counted warning.

### 4. Source-specific data

`<userData>`, vendor extensions, and the elements that are out of Phase 1
scope (`<roadMark>`, `<signal>`, `<object>`, lane `<speed>`, and others) are
not authored. The decoder counts them and reports them with one warning per
kind. The warnings are recorded on `/Asset` (§3). `customData` is not used
for road-network data.

This settles the question that
[ADR 0006 §4](0006-fileformat-contract.md#4-warnings) left open: warning codes
are also recorded on `/Asset`.

### 5. Migration to applied API schemas

Each property group in §3 corresponds to one future applied API schema, for
example `RoadNetworkRoadAPI`, with the same property names and types. The
migration applies the schema to prims that already carry the properties, so
authored values do not change. The trigger is the
[backlog](../roadmap/backlog.md#usd-contract-and-tooling) condition: the
shared vocabulary is stable across two formats.

## Consequences

- USD-01 implements §1–§4 in `roadnetwork-usd`, with one authoring test per
  property group.
- The [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract)
  and the [OpenDRIVE mapping](../reference/OPENDRIVE_MAPPING.md) take the
  property list when this ADR is accepted.
- Renaming a property after the first release is a breaking change to the
  public behavior tier ([API boundary](../architecture/API_BOUNDARY.md)).
- The Lanelet2 mapping (Phase 4) reuses the common properties and adds its
  own property groups. The vocabulary is reviewed against both formats before
  B is considered.

## Evidence

Gathered in the [ADR probes §4](../reports/adr-probes-2026-11-22.md#4-sample-stage):

- A sample `.usda` in this layout, with two roads, three lanes, one junction
  connection, and a visualization curve, passes `usdchecker` on OpenUSD
  26.08.
- Its relationships are remapped under two references and survive
  `Flatten()`, and `uniform` variability survives too.
- `/Asset` attributes reach a referencing stage. Layer metadata does not.

Gathered in USD-01, in the shared writer
([`libs/roadnetwork-usd`](../../libs/roadnetwork-usd/README.md)):

- `WriteLayer()` authors §1–§3 from `RoadNetworkData` with Sdf specs only.
  `roadnetworkUsd_layer_writer` checks each §3 property group on a
  hand-built network with two lane sections, an escaped road ID, and a
  junction. It also checks that every property under `/Asset/Network` is
  custom, `uniform`, without time samples, and not single precision.
- `usdchecker` passes on the generated layers of `basic.xodr`,
  `decoder/network.xodr`, `writer/projected.xodr`, and CARLA `Town01` and
  `Town02` (`roadnetworkOpendrive_file_format_contract`).
- Each OpenDRIVE value in Phase 1 scope is mapped to its property in
  [OpenDRIVE mapping §9](../reference/OPENDRIVE_MAPPING.md#9-usd-authoring).
- Choices the text above leaves open, made in USD-01 and open to review:
  - `roadnetwork:` properties are authored `custom`, and the standard
    properties of `Xform` and `BasisCurves` are not.
  - `roadnetwork:import:warningCodes` and `:warningCounts` are authored
    also when they are empty, so that a layer states that nothing was lost.
    They are importer records, not optional source values.
  - Each curve prim also authors the standard `extent`, computed from its
    `float` points.
  - A lane with no lane-graph edge at one end has no relationship for that
    end, and an absent junction or road link authors no relationship.
  - A writer failure, which validation should make impossible, is the new
    `RN-E901`, so that the one error of a failed read still starts with a
    code ([ADR 0006 §3](0006-fileformat-contract.md#3-errors)).

Gathered in the acceptance review of ADRs 0001–0005 (2026-11-23):

- The property list, the choices above, and the
  [OpenDRIVE mapping §9](../reference/OPENDRIVE_MAPPING.md#9-usd-authoring)
  were reviewed together with ADR 0001. The choices above are kept.
- The lane relationships were `roadnetwork:predecessor` and
  `roadnetwork:successor`. They mean the lane's own `s` range, so on an
  OpenDRIVE left lane, and on the 52 end–end and start–start edges between
  opposing Lanelet2 lanelets
  ([Lanelet2 mapping §4](../reports/adr-0001-lanelet2-mapping-2026-11-23.md#4-topology)),
  a `successor` is not downstream. Renaming after the first release breaks
  the public behavior tier, so the maintainer chose on 2026-11-23 to rename
  them `roadnetwork:startLinks` and `roadnetwork:endLinks` now. Road links
  keep their names. `roadnetworkUsd_layer_writer` and
  `roadnetworkOpendrive_file_format_contract` check the new names.
- §2 now lists `bool`, which `roadnetwork:level` already used, and §3 says
  that `laneCount` includes center lanes, as the writer counts them.

Accepted by the maintainer on 2026-11-23, after the review above.
