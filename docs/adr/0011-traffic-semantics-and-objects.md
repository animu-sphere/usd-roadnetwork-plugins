# ADR 0011 — Traffic semantics, road marks, and objects in the model and the stage

- **Status:** Proposed (2026-11-28)
- **Scope:** how `<roadMark>`, lane `<speed>`, `<signal>`,
  `<signalReference>`, `<controller>`, junction `<priority>` and
  `<controller>`, and `<object>` are kept in `RoadNetworkData`, validated,
  and authored under `/Asset/Network`; how they refer to roads, lanes,
  signals, and controllers
- **Needed by:** Phase 2, SEM-01 (F-13 road marks, F-14, F-15), in
  [current work](../roadmap/current.md#active-work)
- **Extends:** the types of [ADR 0001 §7](0001-roadnetwork-data-model.md#7-header-sketch),
  the property list of [ADR 0002 §3](0002-usd-schema-strategy.md#3-phase-1-property-list),
  and the names and tree of [ADR 0005 §1–§3](0005-prim-path-and-id-stability.md#3-hierarchy),
  which reserves `/Signals` and `/Objects` for Phase 2. Nothing they decide
  for roads, lanes, and junctions changes.

## Context

Phase 1 counts road marks, signals, and objects and authors none of them
([ADR 0002 §4](0002-usd-schema-strategy.md#4-source-specific-data),
[mapping §7](../reference/OPENDRIVE_MAPPING.md#7-signals-and-objects)). The
[FMT-02 spike](../reports/format-spike-opendrive-2026-11-23.md) found them
in most real files and put them in the Phase 2 scope
([§11](../reports/format-spike-opendrive-2026-11-23.md#11-proposed-phase-2-scope)):

- 65,194 road marks in 206 of 222 networks, from every producer; 2,219 with
  a `<type>` and `<line>` children
  ([§6](../reports/format-spike-opendrive-2026-11-23.md#6-road-marks));
- 1,431 signals in 47 networks from 6 producers, 1,200 signal references
  (almost all RoadRunner 2019), 847 controllers, 212 junction priorities,
  and 7,016 lane speed records. `country` and `type` are written in several
  conventions (`OpenDRIVE` with numeric types, `de` and `DE`), so they are
  kept verbatim
  ([§7](../reports/format-spike-opendrive-2026-11-23.md#7-signals-and-controllers));
- 1,701 objects in 63 networks. RoadRunner writes `type="-1"` 744 times,
  outside the enumeration. Outlines are common (3,066 local and 59 road
  corners), `<repeat>` rarer (41)
  ([§8](../reports/format-spike-opendrive-2026-11-23.md#8-objects)).

These elements need what the Phase 1 IR does not have: elements that are
not roads, lanes, or junctions, and references from them to lanes. The
[Lanelet2 mapping §7](../reports/adr-0001-lanelet2-mapping-2026-11-23.md#7-regulatory-elements)
found that Lanelet2 regulatory elements refer to the same kind of physical
elements (lights, signs, stop lines), so this ADR decides the part of the
[lane-graph question](../roadmap/backlog.md#decisions-to-record-later) that
F-14 and F-15 need and leaves the rest to the second format.

The ASAM OpenDRIVE 1.9.0 text that the decision relies on is summarized in
the [Evidence](#evidence) section.

## Questions

1. Where do signals, controllers, and objects live in the prim tree, and
   how are they named?
2. How does a signal, signal reference, or object refer to the lanes it is
   valid for, and what does an absent `<validity>` mean?
3. How are records with optional values (a road mark without `width`, a
   repeat without `heightStart`) authored, when
   [ADR 0002 §2](0002-usd-schema-strategy.md#2-properties) forbids a
   fallback value?
4. Which values are closed vocabularies and which are opaque source text?
5. Does a dangling reference or an out-of-range position fail the read, as
   it does for roads, or drop the element with a warning?
6. Is any position, pose, or shape derived, in the semantic layer or in
   `/Asset/Visualization`?

## Options considered

Placement (Question 1):

- **A. Containers under `/Network`, named by source ID:**
  `/Network/Signals/signal_<E(id)>`, as ADR 0005 §3 reserves. Signal IDs are
  unique in the file (1.9.0 §14.1), and `<signalReference>` and `<control>`
  refer to a signal by ID alone, so a flat container matches how the source
  resolves them. Lanelet2 traffic signs and lights would join the same
  container. **Chosen** for signals, controllers, and objects.
- **B. Under the road that holds them:** robust to IDs that repeat across
  roads, but a signal reference from another road would point into a third
  party's subtree, and Lanelet2 signs belong to no road. Declined; it is the
  fallback for objects if the evidence finds object IDs that repeat across
  roads (below).
- **C. Under the lane:** a signal valid for several lanes or sections has no
  single parent. Declined.

Lane references (Question 2):

- **A. Keep the source lane numbers, and resolve them to lane
  relationships** in the lane section where the element applies. The
  numbers stay as stated, as for a junction `<laneLink>`
  ([ADR 0002 §2](0002-usd-schema-strategy.md#2-properties)). **Chosen.**
- **B. Numbers only:** a consumer would repeat the section lookup, and the
  reference would not be remapped as a relationship. Declined.
- **C. Also derive the lanes of an absent `<validity>` from `orientation`:**
  the spec's default is "all lanes for traffic in the direction of
  `orientation`", which needs the driving side. The driving side is not in
  the model ([backlog](../roadmap/backlog.md#decisions-to-record-later)).
  Declined; nothing is derived.

Records with optional values (Question 3):

- **A. Parallel arrays with a sentinel:** the empty token works for an
  absent token, but a `double` has no sentinel (non-finite values are
  forbidden), and `0` is a valid width. Declined.
- **B. Parallel arrays with a presence mask per optional value:** doubles
  the property count, and every reader must apply the mask. Declined.
- **C. One child prim per record, which authors only what the source
  states.** **Chosen** for records with optional values; records whose
  values are all required stay parallel arrays, as in ADR 0002.

Strictness (Question 5):

- **A. Same as roads:** a missing required attribute, a value outside its
  vocabulary, an unresolved reference, or a position off its road fails the
  read with a stable code. Consistent with
  [design policy §9](../design/DESIGN_POLICY.md#9-strictness) and ADR 0001
  §6. **Proposed**, subject to the corpus evidence below.
- **B. Drop the element with a counted warning:** a bounded, recorded loss,
  but a stage would silently lack a signal that a consumer depends on, and
  the rule differs from roads. Kept as the fallback for any rule that the
  evidence shows real files break; such a change is made here before
  acceptance.

## Proposed decision

### 1. Common rules

- The elements are kept as **source records**. The semantic layer holds
  their stated values, in metres and radians. No inertial position, pose,
  outline, or mesh is derived, and nothing is authored under
  `/Asset/Visualization` for them; where derived placement belongs is
  [ADR 0007](0007-geometry-lod-payload-boundary.md)'s question.
- **Vocabulary.** A value from a closed OpenDRIVE enumeration is a `token`
  with the source spelling, checked against the union of the 1.4–1.9 values;
  any other value is `ODR-E009`, as for lane types. The closed values are:
  road-mark `type`, `weight`, `color`, and `laneChange`; line `color` and
  `rule`; `orientation`; signal `unit`; outline `fillType` and `laneType`;
  validity `layer`. A value that the spec leaves open, or that producers
  write outside the enumeration, is a `string`, verbatim and unchecked:
  signal `type`, `subtype`, `country`, `countryRevision`, `text`, and
  `name`; object `type`, `subtype`, and `name`; road-mark `material` and
  type `name`; control and junction-controller `type`.
- **Flags.** `dynamic` (`yes`/`no`) is a `bool`, and any other value is
  `ODR-E009`. The `t_bool` attributes follow the boolean syntax of the
  [mapping](../reference/OPENDRIVE_MAPPING.md#value-syntax).
- **Optional values.** An absent optional attribute is absent in the IR
  (`std::optional`) and not authored. The spec's defaults (`laneChange`
  `both`, `material` `standard`, an omitted `temporary` or `invalidated`
  meaning `false`) are not written in its place.
- **Records.** A kind of record with any optional value is authored as one
  child prim per record, named `<kind>_<n>` with `n` its 0-based position in
  source order within its parent (§9). A kind whose values are all required
  is authored as parallel arrays on its parent. This keeps the ADR 0002 rule
  that an absent value is never filled in.
- **References** to roads, lanes, signals, and controllers are
  relationships. Each relationship is authored as the source states it, on
  the element that states it; no reverse relationship is added (a lane does
  not list its signals), as reciprocity is not required elsewhere
  ([ADR 0001 §3](0001-roadnetwork-data-model.md#3-topology)).
- **No meaning is derived** from `orientation`, `type`, `country`, or
  `value`: no speed limit from a sign, no travel direction from an
  orientation, no priority from signals. Those belong to the consumer, or to
  the [travel-direction decision](../roadmap/backlog.md#decisions-to-record-later).

### 2. Lane validity

A `<validity fromLane toLane>` on a signal, a signal reference, or an
object restricts it to a range of lane numbers. It is the one reference
from these elements to lanes.

- Every `<validity>` is kept, in source order, as
  `LaneValidity { fromLane, toLane }`.
- The validity **applies to the lane sections** of the element's road that
  contain its position: the section `i` with `s_i ≤ s < s_{i+1}` (the last
  section up to and including the road's end). For an object with
  `validLength > 0` it applies to every section that `[s, s + validLength]`
  overlaps.
- In each of those sections, the **valid lanes** are the non-center lanes
  whose number `n` satisfies `fromLane ≤ n ≤ toLane` for some validity. The
  center lane carries no traffic and is never a valid lane.
- `fromLane > toLane`, or a validity that names no non-center lane in any
  section it applies to, is the error `RN-E123` (§10).
- `layer="temporary"` (1.9) is `ODR-E205`, as temporary lane layers are
  ([mapping §5](../reference/OPENDRIVE_MAPPING.md#5-lanes)); `permanent` or
  absent is accepted.
- **No `<validity>`** keeps the source's omission: nothing is resolved and
  no relationship is authored. The spec's default (all lanes in the
  direction of `orientation`) needs the driving side, so it is left to the
  consumer (Option C, declined).
- The rule of 1.9.0 §14.2 that ties the sign of the lanes to `orientation`
  under right- or left-hand traffic is not checked, for the same reason.

This settles, for F-14 and F-15, the part of the lane-graph question that
signals and objects need: they refer to lanes by `LaneKey`
([ADR 0001 §2](0001-roadnetwork-data-model.md#2-identity)) through their
road, position, and lane numbers. Lateral adjacency, shared boundaries, and
regulatory elements stay with the second format.

### 3. Road marks

- A lane keeps its `<roadMark>` records in source order. Each holds
  `sOffset` (from the lane-section start), `type`, `color`, and the optional
  `weight`, `width`, `height`, `material`, and `laneChange`.
- A `<type name width>` is kept on its road mark with its `<line>` children
  in source order: `length`, `space`, `tOffset`, `sOffset` (from the road
  mark's start), and the optional `width`, `color`, and `rule`. A `<line>`
  value that is absent is not replaced by the road mark's value, although
  the spec says the line's value supersedes it when present.
- The mark lies on the lane's outer border, and the center lane's on the
  center border (1.9.0 §11.9); this is a meaning for consumers, and no
  position is computed.
- `<explicit>` and `<sway>` stay counted in `ODR-W201`
  ([release plan](../roadmap/release-plan.md#phase-2--opendrive-fidelity)).
- Records ascend strictly in `sOffset` (1.9.0 rule `elem_asc_order`;
  `RN-E307`).

### 4. Lane speed

- A lane keeps its `<speed sOffset max unit>` records in source order as
  `SpeedRecord`, with `s` measured from the lane-section start and `max`
  converted to metres per second as for a road speed
  ([mapping §3](../reference/OPENDRIVE_MAPPING.md#3-roads-and-reference-line)).
- A `max` of `"no limit"` or `"undefined"` is not kept and is counted in
  `ODR-W205`, as on a road.
- Records ascend strictly in `sOffset` (`RN-E307`).
- Lane `<access>` and `<rule>` stay counted in `ODR-W206`.

### 5. Signals and signal references

- A **signal** keeps: `id`; the road that holds it; `s`, `t`, `zOffset`;
  `orientation`; `dynamic`; `type` and `subtype`; and when present `name`,
  `country`, `countryRevision`, `value`, `unit`, `text`, `hOffset`, `pitch`,
  `roll`, `height`, `width`, `length` (1.8), `temporary`, and `invalidated`
  (1.9). `value` is the stated number and `unit` its stated unit; nothing is
  converted, because the unit's meaning depends on `type`.
- A `<positionRoad roadId s t zOffset hOffset pitch roll>` or
  `<positionInertial x y z hdg pitch roll>` child is kept as the signal's
  physical position, a variant of the two; both are deprecated since 1.8.0
  but RoadRunner writes the inertial one (222 in FMT-02). Both on one signal
  is `ODR-E010`. `positionRoad`'s road is a reference (§10).
- Its `<validity>` elements follow §2.
- `<dependency>`, `<reference>`, `<semantics>`, and any other child of a
  signal stay counted in `ODR-W202`.
- A **signal reference** belongs to the road whose `<signals>` holds it and
  keeps the referenced signal's ID, `s`, `t`, `orientation`, and its own
  `<validity>` elements, which apply on the referencing road (§2).

### 6. Controllers and junction priorities

- A root **`<controller>`** keeps `id`, and when present `name` and
  `sequence`, and its `<control>` children in source order, each with the
  controlled signal's ID and an optional `type`.
- A junction **`<priority high low>`** keeps the two road IDs in source
  order. The spec defines `high` and `low` as road IDs; which roads they
  may name (connecting or incoming) is not restricted beyond existence.
- A junction **`<controller id type sequence>`** keeps the controller's ID,
  and when present `type` and `sequence`, in source order.
- No signal cycle, phase, or priority is derived; signal programs are
  outside OpenDRIVE.

### 7. Objects

- An **object** keeps: `id`; the road that holds it; `s`, `t`, `zOffset`;
  and when present `type`, `subtype`, `name`, `validLength`, `orientation`,
  `hdg`, `pitch`, `roll`, `length`, `width`, `height`, `radius`, `dynamic`,
  `perpToRoad` (1.7), `temporary`, and `invalidated` (1.9). Whether
  `radius` or `length` and `width` describe the bounding volume is the
  source's choice; both are kept as stated and neither is checked against
  the other.
- **Outlines.** Every `<outline>` is kept in source order, whether inside
  `<outlines>` or, before 1.5, directly in the object (both forms on one
  object is `ODR-E010`). An outline keeps the optional `id`, `fillType`,
  `outer`, `closed`, and `laneType`, and either its `<cornerRoad s t dz
  height>` or its `<cornerLocal u v z height>` corners in source order. A
  corner's optional `id`, which only `<markings>` uses, is counted in
  `ODR-W207`. Mixing the two corner kinds in one outline is `ODR-E010`. An
  outline made of 1.9 `<curveLocal>` elements is not kept and is counted in
  `ODR-W203`; the object keeps its bounding volume.
- **Repeats.** Every `<repeat>` is kept in source order as a record: `s`,
  `length`, `distance`, `tStart`, `tEnd`, `zOffsetStart`, `zOffsetEnd`, and
  when present `heightStart`, `heightEnd`, `widthStart`, `widthEnd`,
  `lengthStart`, `lengthEnd`, `radiusStart`, `radiusEnd`, `bT`, `cT`, `dT`
  (1.9), and `detachFromReferenceLine` (1.8). No instance is generated;
  `distance = 0`, a continuous object, is kept as stated.
- Its `<validity>` elements follow §2.
- `<markings>`, `<material>`, `<parkingSpace>`, `<borders>`, `<skeleton>`,
  and any other child stay counted in `ODR-W203`, as do `<objectReference>`,
  `<tunnel>`, and `<bridge>` on the road.

### 8. IR

The additions follow [ADR 0001 §1](0001-roadnetwork-data-model.md#1-types):
plain values, references by ID, source order. Names may change in review;
the shape may not without amending this decision.

```cpp
namespace openstrata::rn {

enum class Orientation { Positive, Negative, None };  // "+", "-", "none"
struct LaneValidity { std::int32_t fromLane, toLane; };

struct RoadMarkLine {
  double length, space, tOffset, sOffset;
  std::optional<double> width;
  std::optional<std::string> color, rule;            // tokens
};
struct RoadMarkType { std::string name; double width; std::vector<RoadMarkLine> lines; };
struct RoadMark {
  double sOffset;
  std::string type, color;                            // tokens
  std::optional<std::string> weight, laneChange;      // tokens
  std::optional<std::string> material;
  std::optional<double> width, height;
  std::optional<RoadMarkType> lineType;
};
// Lane gains:  std::vector<RoadMark> roadMarks;
//              std::vector<SpeedRecord> speeds;      // s from the section start

struct RoadPosition {
  std::string roadId; double s, t, zOffset, hOffset;
  std::optional<double> pitch, roll;
};
struct InertialPosition { double x, y, z, hdg; std::optional<double> pitch, roll; };
struct Signal {
  std::string id, roadId;
  double s, t, zOffset;
  Orientation orientation;
  bool dynamic;
  std::string type, subtype;
  std::optional<std::string> name, country, countryRevision, unit, text;
  std::optional<double> value, hOffset, pitch, roll, height, width, length;
  std::optional<bool> temporary, invalidated;
  std::vector<LaneValidity> validity;
  std::optional<std::variant<RoadPosition, InertialPosition>> physicalPosition;
};
struct SignalReference {
  std::string signalId; double s, t; Orientation orientation;
  std::vector<LaneValidity> validity;
};
// Road gains:  std::vector<SignalReference> signalReferences;

struct Control { std::string signalId; std::optional<std::string> type; };
struct Controller {
  std::string id;
  std::optional<std::string> name;
  std::optional<std::uint32_t> sequence;
  std::vector<Control> controls;
};
struct JunctionPriority { std::string high, low; };   // road IDs
struct JunctionController {
  std::string controllerId;
  std::optional<std::string> type;
  std::optional<std::uint32_t> sequence;
};
// Junction gains:  std::vector<JunctionPriority> priorities;
//                  std::vector<JunctionController> controllers;

struct CornerRoad { double s, t, dz, height; };
struct CornerLocal { double u, v, z, height; };
struct Outline {
  std::optional<std::uint32_t> id;
  std::optional<std::string> fillType, laneType;      // tokens
  std::optional<bool> outer, closed;
  std::variant<std::vector<CornerRoad>, std::vector<CornerLocal>> corners;
};
struct ObjectRepeat {
  double s, length, distance, tStart, tEnd, zOffsetStart, zOffsetEnd;
  std::optional<double> heightStart, heightEnd, widthStart, widthEnd,
      lengthStart, lengthEnd, radiusStart, radiusEnd, bT, cT, dT;
  std::optional<bool> detachFromReferenceLine;
};
struct RoadObject {
  std::string id, roadId;
  double s, t, zOffset;
  std::optional<std::string> type, subtype, name;
  std::optional<Orientation> orientation;
  std::optional<double> validLength, hdg, pitch, roll, length, width, height, radius;
  std::optional<bool> dynamic, perpToRoad, temporary, invalidated;
  std::vector<Outline> outlines;
  std::vector<ObjectRepeat> repeats;
  std::vector<LaneValidity> validity;
};

// RoadNetworkData gains, in source order:
//   std::vector<Signal> signals;
//   std::vector<Controller> controllers;
//   std::vector<RoadObject> objects;

} // namespace openstrata::rn
```

`roadId` on a signal or object is set by the decoder from the `<road>` that
holds it. When Lanelet2 adds traffic signs and lights, a signal gains a
geometry representation beside the road position, as a lane does in
[ADR 0001 §4](0001-roadnetwork-data-model.md#4-geometry-representations);
the OpenDRIVE fields keep their meaning.

### 9. Stage

The tree of [ADR 0005 §3](0005-prim-path-and-id-stability.md#3-hierarchy)
gains these prims, all of type `Scope`
([ADR 0002 §1](0002-usd-schema-strategy.md#1-prim-types)):

```text
/Asset/Network
  /Roads/road_<E(id)>
    /section_<n>/<lane>
      /roadMark_<n>                 one per <roadMark>, within the lane
        /line_<n>                   one per <type>/<line>
    /signalReference_<n>            one per <signalReference>, within the road
  /Junctions/junction_<E(id)>
    /connection_<E(id)>
    /priority_<n>                   one per <priority>
    /controllerRef_<n>              one per junction <controller>
  /Signals/signal_<E(id)>
  /Controllers/controller_<E(id)>
    /control_<n>                    one per <control>
  /Objects/object_<E(id)>
    /outline_<n>                    one per <outline>
    /repeat_<n>                     one per <repeat>
```

- `signal`, `controller`, and `object` join the prefixes of
  [ADR 0005 §1](0005-prim-path-and-id-stability.md#1-encoding-source-ids),
  with `E` unchanged. `/Controllers` is a new container. The derived names
  follow [ADR 0005 §2](0005-prim-path-and-id-stability.md#2-derived-names):
  `n` is the 0-based source-order index within the parent, so inserting a
  record renumbers only its later siblings.
- Children follow source order. A road lists its sections, then its signal
  references; a junction its connections, then its priorities, then its
  controller references. `/Network` lists `Roads`, `Junctions`, `Signals`,
  `Controllers`, and `Objects`, each authored only when it has children.
- Every relationship target is inside `/Asset`
  ([ADR 0005 §4](0005-prim-path-and-id-stability.md#4-relationships)).
  `roadnetwork:validLanes` targets are sorted by path.

The properties follow [ADR 0002 §2](0002-usd-schema-strategy.md#2-properties):
`uniform`, `custom`, `double` for every length and angle, and authored only
when the source states them, except the relationships of §2, which are
authored when they resolve to at least one lane. Kind-specific
vocabularies take the kind's name, as `roadnetwork:laneType` does, so that
each property group can become one applied API schema
([ADR 0002 §5](0002-usd-schema-strategy.md#5-migration-to-applied-api-schemas)).

Lane, in addition to ADR 0002 §3:

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:speed:sOffset`, `roadnetwork:speed:max` | `double[]` | lane speed records, `max` in metres per second |

Road mark (`roadMark_<n>`) and line (`line_<n>`):

| Property | Type | On | Value |
| --- | --- | --- | --- |
| `roadnetwork:sOffset` | `double` | both | from the section start (mark) or from the mark's start (line) |
| `roadnetwork:roadMarkType` | `token` | mark | the source spelling, e.g. `solid solid` |
| `roadnetwork:color` | `token` | both | |
| `roadnetwork:weight`, `roadnetwork:laneChange` | `token` | mark | |
| `roadnetwork:material` | `string` | mark | verbatim |
| `roadnetwork:width` | `double` | both | |
| `roadnetwork:height` | `double` | mark | |
| `roadnetwork:lineTypeName` | `string` | mark | `<type name>` |
| `roadnetwork:lineTypeWidth` | `double` | mark | `<type width>` |
| `roadnetwork:length`, `roadnetwork:space`, `roadnetwork:tOffset` | `double` | line | |
| `roadnetwork:rule` | `token` | line | `no passing`, `caution`, or `none` |

Signal (`signal_<E(id)>`):

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:id`, `roadnetwork:name` | `string` | source ID; name when present |
| `roadnetwork:road` | `rel` | the road that holds the signal |
| `roadnetwork:s`, `roadnetwork:t`, `roadnetwork:zOffset` | `double` | |
| `roadnetwork:orientation` | `token` | `+`, `-`, or `none` |
| `roadnetwork:dynamic` | `bool` | |
| `roadnetwork:signalType`, `roadnetwork:signalSubtype`, `roadnetwork:country`, `roadnetwork:countryRevision`, `roadnetwork:text` | `string` | verbatim |
| `roadnetwork:value` | `double` | as stated |
| `roadnetwork:unit` | `token` | as stated, not converted |
| `roadnetwork:hOffset`, `roadnetwork:pitch`, `roadnetwork:roll`, `roadnetwork:height`, `roadnetwork:width`, `roadnetwork:length` | `double` | |
| `roadnetwork:temporary`, `roadnetwork:invalidated` | `bool` | |
| `roadnetwork:validity:fromLane`, `roadnetwork:validity:toLane` | `int[]` | one entry per `<validity>`, as stated |
| `roadnetwork:validLanes` | `rel` | the valid lanes of §2 |
| `roadnetwork:position:kind` | `token` | `road` or `inertial`, when a physical position is stated |
| `roadnetwork:position:road` | `rel` | the `positionRoad` road |
| `roadnetwork:position:s`, `:t`, `:zOffset`, `:hOffset` or `:x`, `:y`, `:z`, `:hdg`; and `:pitch`, `:roll` | `double` | the physical position |

Signal reference (`signalReference_<n>`): `roadnetwork:signal` (`rel`),
`roadnetwork:s`, `roadnetwork:t` (`double`), `roadnetwork:orientation`
(`token`), and the validity properties of a signal.

Controller (`controller_<E(id)>`): `roadnetwork:id`, `roadnetwork:name`
(`string`), and `roadnetwork:sequence` (`int`). Control (`control_<n>`):
`roadnetwork:signal` (`rel`) and `roadnetwork:controlType` (`string`).

Junction priority (`priority_<n>`): `roadnetwork:high` and
`roadnetwork:low` (`rel`, one road each). Junction controller
(`controllerRef_<n>`): `roadnetwork:controller` (`rel`),
`roadnetwork:controlType` (`string`), and `roadnetwork:sequence` (`int`).

Object (`object_<E(id)>`):

| Property | Type | Value |
| --- | --- | --- |
| `roadnetwork:id`, `roadnetwork:name` | `string` | source ID; name when present |
| `roadnetwork:road` | `rel` | the road that holds the object |
| `roadnetwork:objectType`, `roadnetwork:objectSubtype` | `string` | verbatim, `-1` included |
| `roadnetwork:s`, `roadnetwork:t`, `roadnetwork:zOffset`, `roadnetwork:validLength` | `double` | |
| `roadnetwork:orientation` | `token` | `+`, `-`, or `none` |
| `roadnetwork:hdg`, `roadnetwork:pitch`, `roadnetwork:roll` | `double` | radians, as stated: `hdg` relative to the reference line at `s` (1.9.0 §13.1) |
| `roadnetwork:length`, `roadnetwork:width`, `roadnetwork:height`, `roadnetwork:radius` | `double` | the bounding volume |
| `roadnetwork:dynamic`, `roadnetwork:perpToRoad`, `roadnetwork:temporary`, `roadnetwork:invalidated` | `bool` | |
| validity properties | | as on a signal |

Outline (`outline_<n>`): `roadnetwork:outlineId` (`int`),
`roadnetwork:fillType` and `roadnetwork:laneType` (`token`),
`roadnetwork:outer` and `roadnetwork:closed` (`bool`),
`roadnetwork:cornerKind` (`token`, `road` or `local`), and one `double[]`
per corner value: `roadnetwork:corner:s`, `:t`, `:dz`, `:height` for road
corners, or `roadnetwork:corner:u`, `:v`, `:z`, `:height` for local ones.

Repeat (`repeat_<n>`): one `double` per stated value, named as the source
attribute (`roadnetwork:s`, `roadnetwork:length`, `roadnetwork:distance`,
`roadnetwork:tStart`, … `roadnetwork:dT`), and
`roadnetwork:detachFromReferenceLine` (`bool`).

`/Asset` gains `roadnetwork:import:signalCount` and
`roadnetwork:import:objectCount` (`int`): the reader's scan of `<signal>`
and `<object>` elements, in full and metadata-only reads alike, as for the
other counts. A metadata-only read authors no signal, controller, or
object prim.

### 10. Validation and diagnostics

The decoder checks what the syntax shows (`ODR-`); shared validation checks
identity, references, and ranges (`RN-`), as
[ADR 0001 §6](0001-roadnetwork-data-model.md#6-validation) divides them.
Validation keeps its category order; within a category, the new checks come
after the Phase 1 ones.

| Code | Change |
| --- | --- |
| `ODR-E008` | Also a missing required attribute of the elements above. |
| `ODR-E009` | Also a value outside a closed vocabulary of §1, or a `dynamic` other than `yes` or `no`. |
| `ODR-E010` | Also both physical positions on a signal, both outline forms on an object, or both corner kinds in an outline. |
| `ODR-E205` | Also a `<validity layer="temporary">`. |
| `ODR-W201` | Only road-mark `<explicit>` and `<sway>`. |
| `ODR-W202` | Only signal `<dependency>`, `<reference>`, `<semantics>`, and other children of a signal. |
| `ODR-W203` | Only `<objectReference>`, `<tunnel>`, `<bridge>`, the object children of §7, and `<curveLocal>` outlines. |
| `ODR-W205` | Also a lane speed whose `max` is `"no limit"` or `"undefined"`. |
| `ODR-W206` | No longer lane `<speed>`, junction `<priority>`, or junction `<controller>`. |
| `ODR-W207` | Also the corner `id` of an outline. |
| `RN-E101`, `RN-E102` | Also an empty or repeated signal, controller, or object ID, each within its kind. The condition is the same, so the code is. |
| `RN-E116` (new) | A signal's `positionRoad`, or a junction priority, names a road that does not exist. |
| `RN-E117` (new) | A signal reference or a controller's control names a signal that does not exist. |
| `RN-E118` (new) | A junction controller names a controller that does not exist. |
| `RN-E123` (new) | A lane validity has `fromLane > toLane`, or names no non-center lane in the lane sections it applies to (§2). |
| `RN-E307` | Also road-mark and lane-speed records that do not ascend strictly in `sOffset`. |
| `RN-E311` (new) | A signal, signal reference, object, repeat range `[s, s + length]`, or road corner lies outside its road's `[0, length]`, beyond the 1e-6 m station tolerance. |

`RN-E309`, `RN-E310`, and the warnings `RN-W303` and `RN-W304` that
[ADR 0010 §8](0010-plan-view-shapes-and-superelevation.md#8-diagnostics)
may take are left to it. The codes are assigned in the
[diagnostics reference](../reference/DIAGNOSTICS.md) and the catalogs by the
pull request that implements them.

## Consequences

- SEM-01 adds §8 to `roadnetwork-core` with the checks of §10, decodes the
  elements in `roadnetwork-opendrive`, and authors §9 in `roadnetwork-usd`,
  with a golden semantic layer for a hand-authored fixture that carries
  each element and each optional value, and the FMT-02 fixtures that carry
  signals and objects
  ([§10](../reports/format-spike-opendrive-2026-11-23.md#10-fixtures)).
- Layers of files without road marks, signals, objects, lane speeds,
  priorities, or controllers do not change apart from the two new `/Asset`
  counts. Files with them gain prims and properties; a file that v0.1.0
  opened may fail where §10 adds an error, which the evidence below bounds.
- The [OpenDRIVE mapping §5–§7](../reference/OPENDRIVE_MAPPING.md#7-signals-and-objects),
  the [diagnostics reference](../reference/DIAGNOSTICS.md), the
  [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract),
  and the [capability matrix](../reference/CAPABILITY_MATRIX.md) change in
  the pull request that implements each part.
- When this ADR is accepted, the status lines of ADRs 0001, 0002, and 0005
  gain a link to it as the ADR that extends them, and the lane-graph row of
  the [backlog](../roadmap/backlog.md#decisions-to-record-later) keeps only
  what the second format decides.

## Evidence needed before acceptance

- **Corpus counts.** Over the 222 FMT-02 networks, per producer: signal,
  controller, and object IDs that repeat in the file, and object IDs that
  repeat only across roads; signal references, controls, junction
  controllers, priorities, and `positionRoad` roads that do not resolve;
  validity ranges that would raise `RN-E123`; positions that would raise
  `RN-E311`; road-mark and lane-speed records that do not ascend strictly;
  values outside the closed vocabularies of §1; and missing required
  attributes. A network that v0.1.0 opens and §10 would reject moves the
  rule that rejects it to Option B before acceptance, or is listed here
  with the reason to keep it. Object IDs that repeat across roads in more
  than one producer move objects to Option B of the placement question.
- **Stage.** A sample layer with every prim kind of §9 passes `usdchecker`
  on OpenUSD 26.08, keeps its relationships under two references and
  `Flatten()`, and, for the largest committed fixture
  (`gs1_urban_intersection.xodr`) and one RoadRunner 2019 network, reports
  the prim count and layer size, so that a prim per record (§1) is measured
  rather than assumed.
- **Lane validity.** In the corpus, the number of validities that apply to
  more than one lane section, and whether any names a lane only in one of
  them.

## Evidence

Gathered from the ASAM OpenDRIVE 1.9.0 specification
([§11.8–§11.9](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/11_lanes/11_09_lane_road_markings.html),
[§12.4.1](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/12_junctions/12_04_connecting_roads.html),
[§12.14](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/12_junctions/12_14_signal_synchronization_groups.html),
[§13](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/13_objects/13_01_introduction.html),
[§14](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/14_signals/14_01_introduction.html),
read on 2026-11-28):

- A road mark describes the lane's outer border, the center lane's mark the
  line between left and right lanes, and its `sOffset` is measured from the
  lane-section start; road marks shall ascend in `s`. A `<line>`'s `width`
  and `color` supersede the road mark's. `laneChange` defaults to `both`
  and `material` to `standard` (§3, §1).
- A lane speed's `sOffset` is from the section start, its unit defaults to
  `m/s`, and its records ascend (§4).
- A signal's `id` is unique within the file, `type` and `subtype` are
  unique only with `country` and `countryRevision`, and `value` requires
  `unit`. `<signalReference>` refers to a signal by its ID from another
  road (§5, Option A of placement).
- Signals and objects are valid for all lanes of the road when they have no
  `<validity>`; for signals, for traffic in the direction of
  `orientation`, and the sign of the lanes depends on right- or left-hand
  traffic. `fromLane ≤ toLane` (§2).
- `<positionRoad>` is deprecated since 1.8.0, and the signal positioning
  section as a whole is marked deprecated (§5).
- A junction `<priority>`'s `high` and `low` are road IDs; a junction
  `<controller>` names a controller for a synchronization group; a root
  `<controller>`'s `<control>` names a signal (§6).
- An object's `zOffset` is relative to the reference-line elevation, `hdg`
  is relative to the road direction, and its local frame rotates by
  heading, then pitch, then roll. A repeat with `distance = 0` is a
  continuous object. An outline uses road corners, local corners, or (1.9)
  local curves, never mixed (§7).
