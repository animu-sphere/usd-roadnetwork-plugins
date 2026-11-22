# ADR 0001 — `RoadNetworkData` ID, topology, and geometry model

- **Status:** Proposed (2026-11-22). The proposed decision is ready for review.
- **Scope:** the shared intermediate representation in `roadnetwork-core`
  that every format decoder produces and the shared USD writer consumes
- **Phase 1 owner:** IR-01 ([current work](../roadmap/current.md#active-work))

## Context

The design policy ([§5.3](../design/DESIGN_POLICY.md#53-intermediate-representation))
requires one format-independent IR built from standard C++ types, with no USD
type in its public API. It must carry OpenDRIVE first, and later Lanelet2 and
SUMO, without forcing one format into the model of another:

- OpenDRIVE is reference-line based. A road owns a plan-view curve, profiles
  along `s`, and lane sections. A lane is defined by width or border
  functions relative to the reference line.
- Lanelet2 is boundary based. A lanelet is a pair of left and right polylines
  plus regulatory elements.
- SUMO is graph based. It has edges and lanes with shapes, plus connections
  and traffic-light programs.

The [ADR probes](../reports/adr-probes-2026-11-22.md#6-carla-links) found that
real OpenDRIVE files state many lane links on one side only. In CARLA
`Town01`, 144 of 396 statements are one-sided. They are the links from
connecting roads inside junctions, and the specification defines them that
way.

## Questions

1. **Identity.** How is a source ID kept (as a string, qualified by format),
   and how are derived entities without a source ID named? For example, an
   OpenDRIVE lane is identified only by its road, lane section, and lane
   number.
2. **Topology.** Is connectivity stored on lanes, on roads, or as a separate
   edge list? How are contact points (start or end) and directions recorded?
3. **Geometry.** Is geometry analytic (curve segments and polynomials),
   sampled (polylines), or both? See ADR 0004 for the OpenDRIVE specifics.
4. **Extensibility.** How is source-specific data that the shared model does
   not represent kept for provenance? Candidates are typed side tables, a
   key-value bag, or nothing plus a diagnostic.
5. **Validation.** Which invariants does `roadnetwork-core` check after
   decoding? Candidates are unique IDs, resolved references, finite values,
   and monotonic `s`.

## Options considered

- **A. OpenDRIVE-shaped IR.** Roads, lane sections, and analytic curves are
  first-class types, and other formats are lifted into them.
  Simple for Phase 1, but it risks forcing boundary-based formats into a
  reference line they do not have.
- **B. Boundary- or graph-shaped IR.** Lanes are boundary polylines in a lane
  graph, and OpenDRIVE is evaluated into it.
  Fits Lanelet2 and SUMO, but loses OpenDRIVE's analytic geometry, contrary to
  ADR 0004.
- **C. Layered IR.** A common identity and lane-graph layer shared by all
  formats, plus optional geometry representations attached to it (analytic
  reference-line geometry, boundary polylines).

## Proposed decision

Option C.

### 1. Types

- The IR uses standard C++17 types only: `std::string`, `std::vector`,
  `std::optional`, `std::variant`, fixed-width integers, and `double`. No
  OpenUSD type and no parser type appears in it.
- Elements are plain values with no pointers between them. One element
  refers to another by ID, and validation (§6) resolves the reference.
- Lengths are in metres, angles in radians, and speeds in metres per second
  ([ADR 0003](0003-crs-axes-units.md)).
- Containers keep the source document order. The lane graph (§3) is the only
  container in canonical order.

### 2. Identity

| Element | Identity | Rule |
| --- | --- | --- |
| Road, junction | `std::string id` | The source ID as text, after XML decoding, unchanged. It is unique within its kind. |
| Connection | `std::string id` | The source ID, unique within its junction. |
| Lane section | road ID, `std::uint32_t` section index | The 0-based position in source order. |
| Lane | `LaneKey { roadId, sectionIndex, laneNumber }` | `laneNumber` is the signed source number: positive on the left, `0` for the center lane, negative on the right. |

No other ID is synthesized: no hashes and no counters except the section
index. [ADR 0005](0005-prim-path-and-id-stability.md) turns these identities
into prim paths.

### 3. Topology

- **Road links** are kept as the source states them. A road has an optional
  `predecessor` and an optional `successor`. Each is a
  `RoadLink { kind: road | junction, id, contact: optional<start | end> }`,
  read in the reference-line direction. A road inside a junction carries the
  junction's ID.
- **Junctions** hold their connections in source order:
  `Connection { id, incomingRoad, connectingRoad, contact,
  laneLinks: vector<{ from, to }> }`, with the lane numbers as the source
  states them.
- **The lane graph** is a separate edge list, `std::vector<LaneLink>`. Each
  edge joins two lane ends, `LaneEnd { LaneKey, end: start | end }`, where
  `start` is the lane's lowest `s`. An edge has no direction and implies no
  direction of travel.
  - The decoder derives the edges from three sources: lane `<link>` elements
    between sections of the same road, lane `<link>` elements across road
    links (using the road link's contact point), and junction `<laneLink>`
    elements.
  - A link stated on one lane only produces the same edge as a link stated on
    both lanes. Reciprocity is not required, because real files do not have
    it ([probes §6](../reports/adr-probes-2026-11-22.md#6-carla-links)).
  - Each edge is stored once, with its lower end first. Lane ends are ordered
    by road ID (byte order), section index, lane number, then
    `start < end`. The vector is sorted, and duplicate edges are removed.
- Travel direction, driving side, and a directed lane graph for routing are
  not part of the Phase 1 IR. They are decided when the second format is
  mapped ([ADR 0008](0008-lanelet2-osm-identification.md)).

### 4. Geometry representations

- Phase 1 defines one representation, the **reference-line
  representation**. It is analytic, as [ADR 0004](0004-opendrive-geometry-retention.md)
  defines:
  - `ReferenceLine { vector<PlanViewSegment> }`, where
    `PlanViewSegment { s, x, y, hdg, length,
    variant<Line, Arc { curvature }> }`;
  - `PiecewiseCubic { vector<CubicRecord { s0, a, b, c, d }> }` for
    elevation, lane offset, and each lane width. `s0` is measured along the
    road for elevation and lane offset, and from the lane-section start for a
    lane width, as in the source.
- A road always carries a reference line. A boundary representation (left
  and right polylines for each lane) is added with the second format. The
  reference-line types do not change.
- A lane that belongs to no road, such as a Lanelet2 lanelet, cannot be
  named by `LaneKey` without synthesizing an ID, which §2 forbids. When the
  second format is added, `LaneEnd::lane` becomes a variant of lane keys,
  `LaneKey` for a lane in a road section and one key for each new kind
  (for example `LaneletKey { id }`). The canonical order of §3 compares the
  variant index first, then the key. The OpenDRIVE identities of §2, the
  undirected edges of §3, and the reference-line types keep their meaning.
  The IR headers are in the contributor tier
  ([API boundary](../architecture/API_BOUNDARY.md#tiers)), so this is a
  change to in-repo code only.
- A consumer that needs a representation the source does not provide gets an
  explicit diagnostic. It never gets a silently derived approximation.

### 5. Provenance and loss

- `SourceInfo` holds the source format (`"opendrive"`), the version as two
  integers, and the opaque header strings (`name`, `date`, `vendor`, and the
  network `version`). It also holds the verbatim geoReference string and the
  header offset ([ADR 0003](0003-crs-axes-units.md)).
- Phase 1 has no generic key-value bag. Source data that the IR does not
  represent is not carried. The decoder reports it as one counted warning per
  kind of loss, as [ADR 0006 §4](0006-fileformat-contract.md#4-warnings)
  requires. A bag would let vocabulary into the USD contract without review.
  A new representation instead enters the IR through an ADR.
- `RoadNetworkData` carries the code and count of each warning posted during
  the read. The writer records them on `/Asset`
  ([ADR 0002](0002-usd-schema-strategy.md)).

### 6. Validation

`roadnetwork-core` provides one validation function. Every bundle calls it
after decoding and before writing. It checks the invariants in the order
below and returns the first failure, so that a failed read posts exactly one
error ([ADR 0006 §3](0006-fileformat-contract.md#3-errors)).

1. **Identity:** IDs are non-empty. Road IDs and junction IDs are unique, and
   connection IDs are unique within their junction.
2. **References:**
   - every road-link target exists and has the stated kind;
   - a road's junction exists;
   - a connection's incoming and connecting roads exist, and the connecting
     road belongs to the junction;
   - both ends of every lane-graph edge exist.
3. **Lane structure:** each lane section has exactly one center lane. Left
   lanes are numbered `1..n` and right lanes `-1..-m`, without gaps.
4. **Numeric:**
   - every value is finite;
   - road and segment lengths are positive;
   - a road has at least one plan-view segment and one lane section, and
     segment and lane-section `s` values ascend strictly from `0` (within
     the 1e-6 m station tolerance of ADR 0004 §6);
   - the plan view is continuous within the tolerances of ADR 0004;
   - cubic records ascend strictly in `s0`, so that the record in force at
     any `s` is unambiguous.

A condition that can be stated on the IR gets an `RN-` code
([ADR 0006 §6](0006-fileformat-contract.md#6-diagnostic-numbering)). The
same condition then has the same code in every format, and the decoder does
not check it again. A condition that only the source syntax shows gets an
`ODR-` code. Examples are malformed XML, a missing or non-numeric attribute,
an unsupported element, and an unsupported version. Codes are assigned in
the IR-01 PR that first raises them.

### 7. Header sketch

This sketch is the review basis for IR-01. Names may change in review. The
shape may not change without amending this decision.

```cpp
namespace openstrata::rn {

enum class ContactPoint { Start, End };

struct CubicRecord { double s0, a, b, c, d; };
struct PiecewiseCubic { std::vector<CubicRecord> records; };

struct Line {};
struct Arc { double curvature; };
struct PlanViewSegment {
  double s, x, y, hdg, length;
  std::variant<Line, Arc> shape;
};
struct ReferenceLine { std::vector<PlanViewSegment> segments; };

struct Lane {
  std::int32_t number;              // > 0 left, 0 center, < 0 right
  std::string type;                 // source vocabulary, e.g. "driving"
  bool level = false;
  PiecewiseCubic width;             // empty for the center lane
};
struct LaneSection {
  double s;
  std::vector<Lane> lanes;          // descending lane number
};

struct RoadLink {
  enum class Kind { Road, Junction } kind;
  std::string id;
  std::optional<ContactPoint> contact;  // set when kind == Road
};
struct SpeedRecord { double s, maxMetresPerSecond; };
struct Road {
  std::string id, name;
  double length;
  std::optional<std::string> junction;
  std::optional<RoadLink> predecessor, successor;
  ReferenceLine referenceLine;
  PiecewiseCubic elevation, laneOffset;
  std::vector<SpeedRecord> speeds;
  std::vector<LaneSection> sections;
};

struct LaneKey { std::string roadId; std::uint32_t sectionIndex; std::int32_t laneNumber; };
struct LaneEnd { LaneKey lane; ContactPoint end; };
struct LaneLink { LaneEnd a, b; };    // a < b; no direction of travel

struct Connection {
  std::string id, incomingRoad, connectingRoad;
  ContactPoint contact;
  std::vector<std::pair<std::int32_t, std::int32_t>> laneLinks;
};
struct Junction { std::string id, name; std::vector<Connection> connections; };

struct SourceOffset { double x, y, z, hdg; };
struct SourceInfo {
  std::string format;               // "opendrive"
  int versionMajor = 0, versionMinor = 0;
  std::string name, date, vendor, version;
  std::optional<std::string> geoReference;
  std::optional<SourceOffset> offset;
};
struct WarningSummary { std::string code; std::uint32_t count; };

struct RoadNetworkData {
  SourceInfo source;
  std::vector<Road> roads;
  std::vector<Junction> junctions;
  std::vector<LaneLink> laneLinks;  // canonical order
  std::vector<WarningSummary> warnings;
};

} // namespace openstrata::rn
```

## Consequences

- `RoadNetworkData.h` replaces its empty placeholder with these types in
  IR-01. `roadnetwork-core` gains the validation function and the first `RN-`
  codes, with a negative unit test for each invariant.
- The OpenDRIVE decoder (ODR-02) does not check references or numeric
  ranges itself. It leaves them to validation and handles only what the
  source syntax shows.
- The lane-end graph is the input to the relationships of
  [ADR 0005](0005-prim-path-and-id-stability.md).
- Lanelet2 adds a boundary representation and a lanelet identity without
  changing the reference-line types, and turns `LaneEnd::lane` into a
  variant (§4). The paper mapping below checks this.

## Evidence

Gathered in the [ADR probes](../reports/adr-probes-2026-11-22.md):

- CARLA `Town01` and `Town02` resolve every lane link to an existing lane
  with the resolution rule in §3, and 36% and 16% of the statements are
  one-sided (§6 of the probes).
- Both files number their lanes without gaps, which the §6 lane-structure
  invariant requires (§5 of the probes).

- Neither file has two elevation, lane-offset, or width records with the
  same `s0`, nor a first lane section that starts anywhere but `s = 0`, so
  the strict ordering in §6 rejects neither (checked during IR-01).

Gathered in IR-01:

- `RoadNetworkData.h` implements the §7 sketch without a change of shape.
  It adds default member initializers, the lane-end order, and
  `CanonicalizeLaneLinks()`, which produces the canonical lane graph of §3.
- `Validate()` checks §6 and assigns `RN-E101`–`RN-E122` and
  `RN-E301`–`RN-E307` ([diagnostics](../reference/DIAGNOSTICS.md#rn--shared-validation)).
  The `roadnetworkCore_validation` unit test fails each invariant with its
  code, checks the order of the categories, and checks canonicalization of
  reversed, one-sided, and duplicate links.

Gathered in ODR-02, in the `roadnetwork-opendrive` decoder:

- The decoder fills the §7 header without a change of shape, and checks only
  what the source syntax shows (`ODR-E008`–`ODR-E206`). It leaves
  references, lane numbering, and numeric ranges to `Validate()`, which
  every full read runs. Where a reference is unresolved, the decoder adds no
  lane-graph edge for it and lets validation report it.
- The lane graph of §3 is built from the three sources it names.
  `roadnetworkOpendrive_decoder` gets the same six edges for
  `decoder/network.xodr` with and without the connecting road's own links,
  that is from lane links and from the junction's `<laneLink>` elements
  alone. CARLA `Town01` and `Town02` give 270 and 356 canonical edges and
  validate.
- §5 needed one addition: validation can return warnings
  (`ValidationWarning`, ADR 0004 §2). They join the decoder's warnings in
  `RoadNetworkData::warnings`.

Gathered in the [Lanelet2 paper mapping](../reports/adr-0001-lanelet2-mapping-2026-11-23.md)
of Lanelet2's `mapping_example.osm` (BSD-3-Clause):

- Lanelet IDs are unique within their kind, as §2 requires. The 388
  lane-end edges derived from shared bound endpoints include 52 between
  opposing lanelets, and the undirected `LaneEnd` model of §3 holds them
  (§3 and §4 of the mapping).
- `LaneKey` cannot name a lanelet without synthesizing a road, a section,
  and a number, so `LaneEnd` changes with the second format (§3 and §9 of
  the mapping). §4 originally said that the common layer would not change.
  The maintainer chose on 2026-11-23 to amend §4 before acceptance with the
  variant of lane keys.
- Regulatory elements attach to lanes through the same lane reference, and
  to physical elements that OpenDRIVE signals and objects also need
  (§7 of the mapping).

Needed before acceptance:

- Review of the header (§7, as implemented) and of the derived identities,
  together with ADR 0005.
