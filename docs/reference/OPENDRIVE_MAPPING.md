# OpenDRIVE mapping contract

- **Status:** Draft (2026-11-23). Sections 1 to 8 give the decoder's
  mapping from OpenDRIVE to `RoadNetworkData`, and section 9 what the shared
  writer authors from it (USD-01). A row's status is *authored* when the
  value reaches the stage. The USD rules follow ADRs 0002, 0003, and 0005,
  accepted on 2026-11-23. Each row is completed in the PR that implements
  it, with the fixtures that prove it, and the matching
  [capability matrix](CAPABILITY_MATRIX.md) row is updated in the same PR.

This page is the normative mapping for the `roadnetwork-opendrive` bundle:
how each ASAM OpenDRIVE element becomes `RoadNetworkData`, and which USD
values the shared writer authors from it. Element names follow the OpenDRIVE
XML schema. The [FMT-01 spike](../reports/format-spike-opendrive-2026-11-22.md#34-differences-that-touch-the-phase-1-elements)
records the differences between versions that touch the Phase 1 elements.

The decoder lives in [`src/io/Decoder.cpp`](../../plugins/roadnetwork-opendrive/src/io/Decoder.cpp).
It checks what only the source syntax shows: required attributes, value
syntax, element placement, unsupported representations, and lane links that
the source leaves ambiguous. Shared validation
([ADR 0001 §6](../adr/0001-roadnetwork-data-model.md#6-validation)) checks
identity, references, lane numbering, and numeric invariants. The codes are
in [DIAGNOSTICS.md](DIAGNOSTICS.md).

## 1. Versions and detection

| Item | Decision |
| --- | --- |
| Specification versions in scope | `revMajor="1"`, `revMinor` 4–9, per the [FMT-01 spike §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy), interpreted by the 1.9.0 rules |
| Version source | `<header revMajor revMinor>`, the mandatory first child of `<OpenDRIVE>`: non-negative decimal integers, surrounding whitespace allowed. A file cannot state its patch level, and none is inferred. |
| Unsupported version | `ODR-E007`; a missing or non-integer `revMajor` or `revMinor` is `ODR-E006`, and a missing `<header>` is `ODR-E005` |
| Encoding | UTF-8 only, with or without a byte-order mark; anything else is `ODR-E003` |
| Root element | `OpenDRIVE` with no prefix and no default namespace; anything else is `ODR-E004` |
| `CanRead()` signature | `.xodr` extension and an `<OpenDRIVE>` root element within the first 64 KiB, after only a BOM, the XML declaration, comments, processing instructions, a DOCTYPE, and whitespace; no body parsing ([ADR 0006 §1](../adr/0006-fileformat-contract.md#1-canread)) |
| Header provenance | `name`, `date`, `vendor`, and `version` are read verbatim as opaque strings (`date` is free text in practice), and authored on `/Asset` when present (§9). |
| Header bounding box | `north`, `south`, `east`, and `west` are not kept and raise no warning, because they are derived from the geometry. |
| Metadata-only read | Decodes the header only: provenance, `<geoReference>`, and `<offset>`, plus the reader's element counts ([ADR 0006 §2](../adr/0006-fileformat-contract.md#2-metadataonly)), and authors `/Asset` and an empty `/Asset/Network` (§9). It does not decode or validate roads, lanes, or junctions, so a fixture that fails a full read can still pass a metadata-only one. |

### Untrusted input

Every asset is treated as untrusted
([design policy §9](../design/DESIGN_POLICY.md#9-strictness)). The reader
parses with pugixml 1.16 ([FMT-01 §6](../reports/format-spike-opendrive-2026-11-22.md#6-xml-parser)).
`roadnetworkOpendrive_file_format_contract` checks each row below.

| Limit | Behavior |
| --- | --- |
| Size | An asset larger than 512 MiB fails with `ODR-E501` before any byte is read, in full and metadata-only reads. Below it, the read holds the document in memory, and the parsed document takes a few times its size again. |
| External references | None are followed. pugixml does not validate and does not process a DOCTYPE: an internal subset is skipped, and its entities, internal or external, are never declared or expanded. Only the five predefined XML entities and character references are expanded. Any other entity reference stays as literal text, such as `&b;` in a header `name`, which is authored verbatim. Nothing is fetched from a file or the network. |
| Depth | Neither the parser nor the decoder recurses on element depth. The decoder reads elements only at the depths OpenDRIVE defines. It counts an ignored element in a warning and does not visit its children, and an unknown element inside the plan view, a profile, or the lane structure fails with `ODR-E206`. A million nested `<userData>` elements open with one `ODR-W204`. |
| Time | Parsing and decoding are linear in the size of the document, and the size limit bounds both. |

### Value syntax

| Kind | Accepted | Otherwise |
| --- | --- | --- |
| Number | `xsd:double` with surrounding whitespace: an optional sign, digits with an optional `.` (`10`, `10.`, `.5`), an optional exponent, or `INF`, `-INF`, and `NaN`. The conversion does not depend on the C locale. A non-finite or overflowing value is kept, and validation rejects it (`RN-E301`). | `ODR-E009` |
| Integer (lane `id`, lane links) | `xsd:int`: an optionally signed decimal within 32 bits | `ODR-E009` |
| Boolean (lane `level`) | `true`, `false`, `1`, `0`; absent is `false` | `ODR-E009` |
| ID (road, junction, connection, link targets) | The attribute text after XML decoding, unchanged ([ADR 0001 §2](../adr/0001-roadnetwork-data-model.md#2-identity)) | missing: `ODR-E008`; empty: `RN-E101` |

On the elements the decoder reads, an attribute that the model does not
represent is counted in `ODR-W207`. The same applies to an attribute that
this decoder does not know, such as the 1.9 `roadworks` attribute.

## 2. Coordinate frames

OpenDRIVE defines a right-handed inertial frame with `z` up, a reference-line
frame `(s, t, h)` along each road, and local frames for objects and signals.
Headings are measured counterclockwise from the inertial `x` axis. Every
coordinate is kept in the file's inertial frame, as written
([ADR 0003](../adr/0003-crs-axes-units.md)).

| Source | IR | Status |
| --- | --- | --- |
| The inertial frame: `z` up, metres | Layer metadata `upAxis = "Z"` and `metersPerUnit = 1`, **source facts** for OpenDRIVE, not importer defaults ([ADR 0003 §1](../adr/0003-crs-axes-units.md#1-axes-and-units)). No axis conversion: source `x`, `y`, `z` are USD `x`, `y`, `z`. | authored |
| `<header>/<geoReference>` | `SourceInfo::geoReference`: the text and CDATA content, with the surrounding XML whitespace trimmed; absent or empty leaves it unset. Never parsed. | authored |
| `<header>/<offset x y z hdg>` | `SourceInfo::offset`, exactly as read; not applied to any coordinate | authored |
| A second `<geoReference>` or `<offset>` | — | `ODR-E010` |

## 3. Roads and reference line

| Source | IR | Status |
| --- | --- | --- |
| `<road id name length junction>` | `Road`: `id` and `name` verbatim, `length` in metres. `junction="-1"` means no junction; any other value is the junction ID. | authored; a missing `id`, `length`, or `junction` is `ODR-E008` |
| `<road rule>` | — | `ODR-W207` (the driving side is not in the Phase 1 model) |
| `<road>/<link>/<predecessor>`, `<successor>` | `RoadLink`: `elementType` `road` or `junction`, `elementId`, and for a road `contactPoint` `start` or `end` (required) | authored; `elementS` or `elementDir` is `ODR-E204`; an unresolved target is `RN-E111` |
| `<road>/<link>/<neighbor>` (1.4) | — | `ODR-W206` |
| `<road>/<type s>/<speed max unit>` | `SpeedRecord { s, max }`, converted to metres per second from `m/s` (also when `unit` is absent), `km/h`, or `mph` (0.44704 m/s) | authored; `max="no limit"` or `"undefined"` is `ODR-W205` and not kept |
| `<road>/<type type country>` | — | `ODR-W207` |
| `<planView>/<geometry s x y hdg length>` | `PlanViewSegment`, in source order | authored; chaining and continuity are validated (`RN-E303`, `RN-E305`, `RN-E306`) |
| `<line>` | `Line` | authored |
| `<arc curvature>` | `Arc`, curvature in 1/m, positive to the left | authored |
| `<spiral curvStart curvEnd>` | `Spiral`, curvatures in 1/m, positive to the left; the curvature is linear in `s` and the two may be equal ([ADR 0010 §1](../adr/0010-plan-view-shapes-and-superelevation.md#1-spiral)) | authored |
| `<paramPoly3 aU bU cU dU aV bV cV dV pRange>` | `ParamPoly3`, the eight coefficients as stated, and `pRange` `arcLength` or `normalized`, kept absent when the source omits it and then read as `normalized`; `p` is linear in `s` over the segment ([ADR 0010 §2](../adr/0010-plan-view-shapes-and-superelevation.md#2-parampoly3)) | authored; a missing coefficient is `ODR-E008`, another `pRange` value `ODR-E009`, a vanishing tangent `RN-E310`, and a curve length more than 1% from `length` the warning `RN-W302` |
| `<poly3>` | — | `ODR-E201` (deprecated since 1.6.0; [backlog](../roadmap/backlog.md)) |
| A `<geometry>` with no shape, or with two | — | `ODR-E008`, `ODR-E010` |
| `<road>/<objects>/*` | — | `ODR-W203`, one count per object |
| `<road>/<signals>/*` | — | `ODR-W202`, one count per signal |
| `<road>/<surface>`, `<railroad>` | — | `ODR-W206` |

A road has at most one `<link>`, `<planView>`, `<elevationProfile>`, and
`<lateralProfile>`; a repeat is `ODR-E010`. A road without a `<planView>` has
no segments (`RN-E303`), and a road without `<lanes>` has no lane sections
(`RN-E304`).

## 4. Profiles

| Source | IR | Status |
| --- | --- | --- |
| `<elevationProfile>/<elevation s a b c d>` | `Road::elevation`, cubic records with `s0 = s` | authored |
| `<lateralProfile>/<superelevation s a b c d>` | — | accepted when `a`, `b`, `c`, and `d` are all zero; otherwise `ODR-E202` ([ADR 0004 §4](../adr/0004-opendrive-geometry-retention.md#4-profiles-in-phase-1)) |
| `<lateralProfile>/<crossfall side s a b c d>` (1.4) | — | as for `<superelevation>` |
| `<lateralProfile>/<shape s t a b c d>` | — | as for `<superelevation>` |
| `<lateralProfile>/<crossSectionSurface>` (1.8) | — | `ODR-E202` |

## 5. Lanes

| Source | IR | Status |
| --- | --- | --- |
| `<lanes layer>` (1.9) | The permanent layer: `layer` absent or `permanent` | authored; `temporary` is `ODR-E205`, and a second permanent `<lanes>` is `ODR-E010` |
| `<lanes>/<laneOffset s a b c d>` | `Road::laneOffset`, cubic records with `s0 = s` | authored |
| `<laneSection s singleSide>` | `LaneSection`, in source order; `singleSide` is not kept, because the lanes show it | authored |
| `<left>`, `<center>`, `<right>` | The lanes of a section: left, then center, then right, each group in descending lane number. A repeated group is `ODR-E010`. | authored |
| `<lane id type level>` | `Lane`: `id` is positive in `<left>`, `0` in `<center>`, and negative in `<right>` (otherwise `ODR-E101`). `type` is kept verbatim and must be one of the 31 lane types of OpenDRIVE 1.4–1.9, deprecated ones included (otherwise `ODR-E009`). | authored; numbering gaps are `RN-E122` |
| `<lane>/<width sOffset a b c d>` | `Lane::width`, cubic records with `s0 = sOffset`, relative to the section start | authored; required on every non-center lane (`ODR-E008`); a `<width>` on the center lane is `ODR-E010` |
| `<lane>/<border>` | — | `ODR-E203` |
| `<lane>/<link>/<predecessor id>`, `<successor id>` | Lane-graph edges (§5.1); several are allowed | authored; `layer="temporary"` is `ODR-E205` |
| `<lane>/<roadMark>` | — | `ODR-W201` |
| `<lane>/<speed>`, `<access>`, `<height>`, `<material>`, `<rule>`, `<visibility>` | — | `ODR-W206` |

An element that this decoder does not know is an error (`ODR-E206`) when it
appears in `<planView>`, `<geometry>`, a profile, `<lanes>`, a
`<laneSection>`, or a lane group, because it could change the geometry or
the lanes. Anywhere else it is counted in `ODR-W208`.

### 5.1 Lane graph

Each lane `<link>` statement becomes one undirected edge between two lane
ends ([ADR 0001 §3](../adr/0001-roadnetwork-data-model.md#3-topology)):

| Statement | Edge |
| --- | --- |
| `<predecessor id="p">` in section `i > 0` | this lane's start, and lane `p`'s end in section `i − 1` |
| `<successor id="q">` in section `i` below the last | this lane's end, and lane `q`'s start in section `i + 1` |
| `<predecessor>` in the first section, or `<successor>` in the last | Follows the road's predecessor or successor. For a road with `contactPoint="start"` it is lane `p` or `q` at the start of the target's first section, and for `end` at the end of its last section. A missing road link is `ODR-E102`. A junction link is `ODR-E103`, because the junction's `<laneLink>` elements state those links. |

A statement made on one lane only gives the same edge as statements made on
both lanes. Edges to lanes that do not exist are kept, so that validation
reports them (`RN-E115`).

## 6. Junctions

| Source | IR | Status |
| --- | --- | --- |
| `<junction id name type>` | `Junction`; `type` absent or `default` | authored; `direct`, `virtual`, and `crossing` are `ODR-E204` |
| `<connection id incomingRoad connectingRoad contactPoint>` | `Connection`, all four attributes required | authored; unresolved roads are `RN-E113`, and a connecting road outside the junction is `RN-E114` |
| `<laneLink from to>` | `Connection::laneLinks` as stated, and one lane-graph edge each | authored |
| `<priority>`, `<controller>`, `<surface>`, `<boundary>`, `<elevationGrid>`, `<crossPath>` | — | `ODR-W206` |
| `<connection>/<predecessor>`, `<successor>` (virtual junctions) | — | `ODR-W206` |

A `<laneLink>` edge joins lane `from` at the incoming road's end at the
junction with lane `to` at the connecting road's `contactPoint` end. That
incoming end is the `contactPoint` of the connecting road's link back to the
incoming road, at its own contact end. When that link names another road, or
is absent, the incoming end is the one at which the incoming road links to the
junction. If the incoming road links to the junction at both ends or at
neither, the read fails with `ODR-E104`.

## 7. Signals and objects

Post-MVP ([design policy §7.2](../design/DESIGN_POLICY.md#72-after-the-mvp)).
Signals, signal references, and the root `<controller>` are counted in
`ODR-W202`. Objects, object references, tunnels, and bridges are counted in
`ODR-W203`. Nothing else is done with them.

## 8. Loss and diagnostics

Every element that is read but not represented produces a counted warning
from [DIAGNOSTICS.md](DIAGNOSTICS.md#odr--decoder). Each code is posted once
per read, with the first five locations, and is mirrored in
`RoadNetworkData::warnings`. A failed read posts no warning.
`<userData>`, `<include>`, and `<dataQuality>` are counted in `ODR-W204`
wherever they appear, and are not authored
([ADR 0002 §4](../adr/0002-usd-schema-strategy.md#4-source-specific-data)).
Elements of the root element other than `<header>`, `<road>`, and
`<junction>` are `ODR-W202` (`<controller>`), `ODR-W206` (`<junctionGroup>`,
`<station>`), or `ODR-W208`.

The codes and their affected-element counts are authored on `/Asset` as
`roadnetwork:import:warningCodes` and `roadnetwork:import:warningCounts`, in
ascending code order. A read without warnings authors two empty arrays.

## 9. USD authoring

The shared writer in
[`libs/roadnetwork-usd`](../../libs/roadnetwork-usd/README.md) authors the
model into the [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract).
The prim tree is [ADR 0005 §3](../adr/0005-prim-path-and-id-stability.md#3-hierarchy),
and the property list is [ADR 0002 §3](../adr/0002-usd-schema-strategy.md#3-phase-1-property-list);
neither is copied here. This section gives only what is specific to
OpenDRIVE.

| Model value | USD | Notes |
| --- | --- | --- |
| `revMajor`, `revMinor` | `/Asset` `roadnetwork:source:formatVersion`, e.g. `"1.7"` | `roadnetwork:source:format` is `"opendrive"` |
| Header `name`, `date`, `vendor`, `version` | `roadnetwork:source:name`, `:date`, `:vendor`, `:version` | not authored when absent or empty |
| `<geoReference>`, `<offset>` | `roadnetwork:source:geoReference`, `:offset`, `:offsetHeading` | verbatim (§2) |
| The importer | `roadnetwork:import:importer` `"roadnetwork-opendrive"` and `:importerVersion`, the bundle version | |
| Element counts | `roadnetwork:import:roadCount`, `:junctionCount`, `:laneCount` | the reader's scan of `<road>`, `<junction>`, and `<lane>` elements, center lanes included, in full and metadata-only reads alike |
| Road, junction, and connection IDs | prim names `road_`, `junction_`, and `connection_` followed by the escaped ID, and the verbatim ID as `roadnetwork:id` | [ADR 0005 §1](../adr/0005-prim-path-and-id-stability.md#1-encoding-source-ids); `junction="-1"` authors no `roadnetwork:junction` |
| Lane section, lane | `section_<n>`; `left_<n>`, `center`, `right_<n>` | [ADR 0005 §2](../adr/0005-prim-path-and-id-stability.md#2-derived-names) |
| `<line>`, `<arc>` | `roadnetwork:referenceLine:kind` `line` or `arc`; `:curvature` is 0 for a line | |
| `<spiral>`, `<paramPoly3>` | `:kind` `spiral` or `paramPoly3`; `:curvature` is `curvStart` for a spiral and 0 for a `paramPoly3`. A road with a spiral adds `:curvatureEnd`, and a road with a `paramPoly3` adds `:aU` … `:dV` and `:pRange` (`unstated` when the source omits it) | [ADR 0010 §5](../adr/0010-plan-view-shapes-and-superelevation.md#5-ir-and-stage) (proposed); a road of lines and arcs authors none of them |
| `<elevation>`, `<laneOffset>`, `<width>` | five `double[]` each: `:s` (`:sOffset` for a width), `:a`, `:b`, `:c`, `:d` | an absent profile is not authored; the center lane has no width |
| Lane `type` | `roadnetwork:laneType`, a `token` with the source spelling | |
| Road `<type>/<speed>` | `roadnetwork:speed:s` and `:max`, in metres per second | `"no limit"` and `"undefined"` are not authored (`ODR-W205`) |
| Road link `contactPoint` | `roadnetwork:predecessorContact`, `:successorContact` | only for a road target |
| Lane links and junction `<laneLink>` | `roadnetwork:startLinks` and `:endLinks` on both lanes of each lane-graph edge (§5.1) | they name the lane's own start and end, not a direction of travel; the target's end is not recorded ([ADR 0005 §4](../adr/0005-prim-path-and-id-stability.md#4-relationships)) |
| Junction `<laneLink from to>` | `roadnetwork:laneLink:from` and `:to` on the connection | source lane numbers in source order, beside the lane relationships |
| Lane borders and centerlines | `/Asset/Visualization/Boundaries` and `/Centerlines`, one linear `BasisCurves` per road | derived; sampled within 1 cm ([ADR 0004 §3](../adr/0004-opendrive-geometry-retention.md#3-tessellation)) and shifted by the origin of [ADR 0003 §5](../adr/0003-crs-axes-units.md#5-precision) |
