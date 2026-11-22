# ADR 0001 Lanelet2 paper mapping — 2026-11-23

This report records the paper mapping that
[ADR 0001](../adr/0001-roadnetwork-data-model.md#evidence) lists as needed
before acceptance: one redistributable Lanelet2 example mapped onto the
common layer of `RoadNetworkData`, showing where its lanelets and regulatory
elements would attach. No Lanelet2 code was written, and nothing here is a
support claim. The [capability matrix](../reference/CAPABILITY_MATRIX.md)
still lists Lanelet2 as unsupported.

## Summary

| Question | Result | Section |
| --- | --- | --- |
| Identity | Lanelet IDs are unique within their kind, as ADR 0001 §2 requires. Way and relation IDs overlap, which kind prefixes already handle. **A lanelet cannot be named by `LaneKey`**, which is road-shaped. | §3 |
| Topology | Shared bound endpoints give 388 undirected lane-end edges: 336 end–start, 31 end–end, and 21 start–start. The undirected `LaneEnd` model holds all of them unchanged. | §4 |
| Geometry | Lanelets fit the boundary representation that ADR 0001 §4 plans. 124 bounds are shared by two lanelets. | §5 |
| Coordinates | Nodes carry WGS 84 `lat`/`lon` only, with no metric frame. **Authoring them needs a projection, which ADR 0003 §2 forbids for every format.** | §6 |
| Regulatory elements | 9 elements in three subtypes. Each attaches to lanes and to physical elements (lights, signs, stop lines) that the Phase 1 IR does not have. | §7 |
| USD | Lanelets fit `/Network/Lanes/lanelet_<E(id)>` (reserved by ADR 0005 §3) and the lane relationships of ADR 0002 §3. `roadnetwork:laneType` keeps the source spelling, so its vocabulary differs between formats. | §8 |

The two findings in bold contradict text that ADRs 0001 and 0003 propose,
and accepted ADRs are not rewritten (§9).

## 1. Source

- `lanelet2_maps/res/mapping_example.osm` from
  [Lanelet2](https://github.com/fzi-forschungszentrum-informatik/Lanelet2),
  commit `de0b1789fa2c8966815dd6f99eeb1d09825f0111` (the `master` head on
  2026-11-23). Lanelet2 is licensed under BSD-3-Clause
  (© 2018 FZI Forschungszentrum Informatik), so the file is redistributable
  with its notice. It is not committed here: a paper mapping does not need
  it, and a Phase 4 fixture goes through the format spike.
- The tagging rules are from the same commit: `LaneletPrimitives.md`,
  `LaneletAndAreaTagging.md`, and `RegulatoryElementTagging.md` in
  `lanelet2_core/doc`. Bound alignment follows `geometry::align` in
  `lanelet2_core/include/lanelet2_core/geometry/impl/LineString.h`, which
  the OSM loader calls for every lanelet.
- Read with the Python standard library `xml.etree.ElementTree` on local
  Windows 11. The scripts were throwaway scripts and are not committed. Each
  section describes its method.

## 2. Contents

| Element | Count |
| --- | --- |
| Nodes | 2,258 |
| Ways | 1,141 |
| Relations | 456 |
| Lanelets (`type=lanelet`) | 371: 337 `road`, 14 `bicycle_lane`, 8 `crosswalk`, 8 `highway`, 2 `rail`, 2 `walkway` |
| Regulatory elements | 9: 6 `traffic_light`, 2 `right_of_way`, 1 `speed_limit` |
| Multipolygon areas | 76 (vegetation, walkway, parking, and others) |

The nodes span about 3.4 km by 1.0 km near Karlsruhe.

## 3. Identity

- IDs are decimal strings from 38992 to 9217047218277094766, all positive.
  IDs are unique within each kind (node, way, relation). Two IDs are used by
  both a way and a relation, and two by both a node and a relation. ADR 0001
  §2 already asks for uniqueness within a kind only, and ADR 0005 §1 prefixes
  each prim name with its kind, so the overlap needs no new rule.
- A lanelet has its own source ID. It belongs to no road, section, or lane
  number. `LaneKey { roadId, sectionIndex, laneNumber }` cannot name it
  without inventing a road, a section, and a number, which ADR 0001 §2
  forbids ("no other ID is synthesized").
- `LaneEnd`, and through it the lane graph of ADR 0001 §3, is built on
  `LaneKey`. So the lane graph type changes when Lanelet2 is added, for
  example to `LaneEnd { variant<LaneKey, LaneletKey> lane; ContactPoint end; }`
  with the variant index first in the canonical order. ADR 0001 §4 says
  instead that "the common layer (§2, §3) does not change". The
  reference-line types do not change. The IR headers are in the contributor
  tier ([API boundary](../architecture/API_BOUNDARY.md#tiers)), so the code
  change is cheap. Only the ADR text is wrong.

## 4. Topology

Lanelet2 states no lane links. The routing graph derives them from
geometry: two lanelets are joined where an end of one has the same two bound
endpoints as an end of the other.

Method: each lanelet's left and right ways were aligned as `geometry::align`
does (a bound is reversed when the middle point of the other bound lies on
its wrong side), in a local equirectangular projection. The lanelet's
`start` is then the pair (first left node, first right node), and its `end`
is the pair of last nodes. Two lane ends whose node pairs are equal as sets
form one edge.

| Edge | Count |
| --- | ---: |
| end–start | 336 |
| end–end | 31 |
| start–start | 21 |
| Total | 388 |

- 233 of the 371 lanelets have at least one bound whose way order the
  alignment reverses, so a lanelet's `start` and `end` come from the aligned
  geometry, not from the way order.
- The 52 end–end and start–start edges join lanelets of opposite direction.
  An undirected `LaneEnd` edge with `start | end` holds them without change,
  which confirms the choice in ADR 0001 §3 to keep the edges undirected.
  This is a fourth source of edges for the decoder (shared endpoints), next
  to the three OpenDRIVE sources.
- Travel direction is not in the Phase 1 IR. Here 241 lanelets are
  `one_way=yes`, 97 are `one_way=no` (77 `road`, 14 `bicycle_lane`,
  4 `crosswalk`, 2 `walkway`), 1 is `one_way=false`, and 32 `road` lanelets
  leave it unset, which Lanelet2 reads as one-way. Until the
  [travel-direction decision](../roadmap/backlog.md#decisions-to-record-later)
  is made, a Lanelet2 decoder would report this as a counted loss.
- Lateral adjacency (lane change) comes from shared bounds: 116
  left–right pairs, plus 3 left–left and 5 right–right pairs between
  opposing lanelets. The common layer has no lateral edge. OpenDRIVE
  implies one through adjacent lane numbers. Phase 1 needs neither.

## 5. Geometry

- Each lanelet has exactly one `left` and one `right` way. No lanelet has a
  `centerline` member. This is the boundary representation that ADR 0001 §4
  plans as a new optional member, and a lanelet carries no reference line.
  Because "a road always carries a reference line", lanelets are not roads.
  They need their own element kind.
- 124 bound ways are shared by two lanelets. ADR 0001 §1 keeps elements as
  plain values that refer to each other by ID, so shared bounds become
  boundary elements with their own IDs that lanelets refer to. Copying the
  polyline into both lanelets would lose the sharing that adjacency is
  derived from.
- Bounds carry a physical type: 231 `curbstone`, 136 `line_thin`,
  129 `virtual`, 111 `road_border`, 79 `line_thick`, and others. OpenDRIVE
  keeps this in road marks, which are out of Phase 1 scope (F-13).
- 4 of the 2,258 nodes carry an `ele` tag. Lanelet2 treats `z` as secondary
  ("2.5D").

## 6. Coordinates

- Nodes have WGS 84 `lat` and `lon` and no local metric coordinates. The
  file has no metric frame, no projection string, and no origin. Lanelet2
  itself projects with a projector and an origin that the application
  supplies, for example UTM.
- [ADR 0003 §2](../adr/0003-crs-axes-units.md#2-frame) authors "all
  coordinates in the inertial frame of the file, as written", and its
  Option C rejects reprojection for import. Its scope is "every format". For
  this file, those rules leave no valid coordinates to author: degrees are
  not metres, and `metersPerUnit = 1` would be false.
- The rest of ADR 0003 fits: the extent is far inside ±8,192 m once
  projected about a local origin, and the georeference stays provenance.

## 7. Regulatory elements

| Element | Subtype | Parameters | Lanelets referencing it |
| --- | --- | --- | ---: |
| 45218, 45224, 45226, 45234 | `traffic_light` | `refers`: 2 `traffic_light` ways; `ref_line`: 1 `stop_line` | 2 each |
| 45222, 45232 | `traffic_light` | `refers`: 1 `traffic_light` way; `ref_line`: 1 `stop_line` | 1 each |
| 45230 | `right_of_way`, `fallback=yes` | `right_of_way`: 5 lanelets; `yield`: 2 lanelets; `refers`: 2 `traffic_sign`; `ref_line`: 1 `stop_line` | 8 |
| 45236 | `right_of_way`, `fallback=yes` | `right_of_way`: 6 lanelets; `yield`: 2 lanelets; `refers`: 2 `traffic_sign`; `ref_line`: 1 `stop_line` | 8 |
| 45390 | `speed_limit` | `refers`: 1 `traffic_sign` | 0 |

- 10 lanelets hold the 26 references: 6 reference three elements and 4
  reference two.
- Where they would attach: a regulatory element becomes a top-level element
  of the common layer with its source ID. Its lane parameters refer to lanes
  through the same generalized lane reference as §3. Its physical
  parameters (lights, signs, stop lines) refer to physical elements, which
  the Phase 1 IR does not have. OpenDRIVE signals and objects (F-14, F-15)
  need the same physical layer, so the two formats would share it there. In
  USD this is a relationship from each lane to the element prim, and from
  the element to `/Network/Signals`, which ADR 0005 §3 reserves.
- Element 45230 is referenced by one lanelet that is not one of its
  parameters, and 45390 is referenced by no lanelet. Element-to-lane and
  lane-to-element references are both kept as stated. As with one-sided
  lane links (ADR 0001 §3), reciprocity is not required.
- No lanelet has a speed tag. Speed comes from the `speed_limit` element or
  from `location=urban` with `region=de` (336 lanelets), which Lanelet2
  turns into a speed through country rules. Under ADR 0001 §4 that is a
  derived value, so no `SpeedRecord` would be authored from it.

## 8. USD layout

- Lanelets go under `/Asset/Network/Lanes/lanelet_<E(id)>`, which ADR 0005
  §3 reserves for lanes that belong to no road. The `lanelet` prefix is an
  addition to the prefix list in ADR 0005 §1, and `E` is unchanged.
- `roadnetwork:predecessor` and `roadnetwork:successor` keep the meaning of
  ADR 0005 §4: the lanes joined at this lane's start and end. For a lanelet
  that is the aligned start and end of §4. For one-way lanelets this matches
  the travel direction, but for the 52 opposing edges it does not, as for
  OpenDRIVE. ADR 0001 §3 and the USD relationships therefore mean the same
  thing in both formats.
- `roadnetwork:laneNumber` has no lanelet value, so a lanelet does not
  author it.
- `roadnetwork:laneType` keeps the source spelling (ADR 0002 §3). Lanelet2
  `road` corresponds to OpenDRIVE `driving`, and `bicycle_lane` to `biking`.
  A consumer that filters for `driving` misses every lanelet, so the
  property is shared but its vocabulary is not. A shared vocabulary belongs
  with the [schema migration trigger](../roadmap/backlog.md#decisions-to-record-later).

## 9. Effect on the Phase 1 ADRs

| Finding | ADR text affected | Blocks acceptance |
| --- | --- | --- |
| `LaneKey` cannot name a lanelet, so `LaneEnd` and the lane graph type change (§3) | ADR 0001 §4: "The common layer (§2, §3) does not change." | Yes. The sentence would be false once accepted. |
| A geodetic-only source has no frame to author "as written" (§6) | ADR 0003 scope ("every format"), §2, and Option C | Yes. The rule would bind Lanelet2 to coordinates it cannot author. |
| No lateral adjacency, shared bounds, regulatory and physical elements, travel direction (§4, §5, §7) | None: ADR 0001 defers them to the second format | No |
| `laneType` vocabulary differs between formats (§8) | None: ADR 0002 §3 keeps the source spelling | No; it is a review choice |
| The `lanelet` prefix and `/Network/Lanes` (§8) | None: ADR 0005 reserves `/Lanes` | No |
