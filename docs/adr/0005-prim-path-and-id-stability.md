# ADR 0005 — Prim-path and source-ID stability; multiple networks

- **Status:** Proposed (2026-11-22). The proposed decision is ready for review.
- **Scope:** how source IDs become prim paths and relationships, and how
  several road networks compose in one stage
- **Phase 1 owner:** USD-01 ([current work](../roadmap/current.md#active-work))

## Context

USD prim names are restricted identifiers. OpenDRIVE IDs are arbitrary
strings, often numeric, and some entities, such as lanes, are identified only
by their position: road, lane section, and lane number. The design policy
([§6.2](../design/DESIGN_POLICY.md#62-stage-contract),
[§10.5](../design/DESIGN_POLICY.md#105-determinism)) requires deterministic
naming, so that reloads and new formats do not move paths unnecessarily and
overrides authored in a stronger layer keep targeting the same prims.

The [ADR probes](../reports/adr-probes-2026-11-22.md) found three things on
OpenUSD 26.08:

- A decimal ID such as `1` or `-1` is not a valid prim name. Unicode names
  are valid prim names, but the NFC and NFD spellings of `é` are two
  different valid names, and neither is a `Tf` identifier (§2 of the probes).
- A network referenced twice into one stage keeps two separate sets of
  correctly remapped relationship targets, and flattening keeps them too
  (§4).
- In real files, many lane links are stated on one side only (§6).

## Questions

1. Encoding: how is an arbitrary source ID turned into a valid prim name
   (prefix plus escaping, hashing, or both)? Is the original ID always kept as
   an attribute?
2. Collisions: what happens when two IDs encode to the same name?
3. Derived IDs: how are lanes and lane sections named (for example
   `road_<id>/section_<n>/lane_<m>`)? Does a lane-section index remain stable
   if an earlier section is inserted in the source?
4. Hierarchy: are lanes nested under their road, or kept flat under
   `/Network/Lanes` with relationships to the road?
5. Relationships: in which direction (successor, predecessor, or both), and
   what do they target?
6. Multiple networks: how are two `.xodr` files referenced into one stage
   without path collisions or cross-file references?

## Options considered

- **A. Escaped source IDs with a type prefix.** Readable and stable. It
  requires a reversible escaping rule and a collision diagnostic.
- **B. Hashed names.** Always valid, and collision-resistant in practice.
  Unreadable, and debugging needs the attribute lookup.
- **C. Index-based names.** Rejected: paths move when the source order
  changes.

## Proposed decision

Option A, with an injective escaping rule, so that collisions cannot occur.

### 1. Encoding source IDs

- `E(id)` takes the bytes of the UTF-8 ID in order. It keeps every ASCII
  letter and digit and writes every other byte as `_` followed by two
  lowercase hexadecimal digits. The underscore itself is escaped, as `_5f`.
- A prim name is `<prefix>_` followed by `E(id)`. The prefixes are `road`,
  `junction`, and `connection`.

| Source ID | Prim name |
| --- | --- |
| `1` | `road_1` |
| `-1` | `road__2d1` |
| `a_b` | `road_a_5fb` |
| `North 2` | `road_North_202` |
| `道` (UTF-8 `e9 81 93`) | `road__e9_81_93` |

- `E` is injective, and its inverse is defined: a `_` is always followed by
  two hexadecimal digits. Two distinct IDs therefore never share a name, and
  no collision rule is needed. Duplicate and empty IDs are validation errors
  before naming ([ADR 0001 §6](0001-roadnetwork-data-model.md#6-validation)).
- Names are ASCII only, although OpenUSD 26.08 accepts Unicode prim names.
  Byte-level escaping keeps canonically equivalent spellings, which look
  identical, from becoming different paths. It also gives names that are
  valid `Tf` identifiers for every tool.
- The verbatim ID is always authored as `roadnetwork:id`. The attribute is
  normative, and `E⁻¹` is a convenience for tools.

### 2. Derived names

| Element | Name |
| --- | --- |
| Lane section | `section_<n>`: `n` is the 0-based source-order index, in decimal |
| Lane | `left_<n>` for lane number `n > 0`, `center` for `0`, and `right_<n>` for lane number `−n` |

The lane names follow OpenDRIVE's own `<left>`, `<center>`, and `<right>`
groups, and they read better than an escaped minus sign.

Inserting a lane section renumbers the later sections of the same road, and
paths in other roads do not move. This is accepted because lane sections
have no source ID. Naming a section by its start `s` would move the path
whenever that value is edited, and would put formatted floating-point
numbers in names.

### 3. Hierarchy

```text
/Asset                                Xform, defaultPrim
  /Network                            Scope
    /Roads/road_<E(id)>               Scope
      /section_<n>                    Scope
        /left_<n> | center | right_<n>    Scope
    /Junctions/junction_<E(id)>       Scope
      /connection_<E(id)>             Scope
  /Visualization                      Xform (ADR 0003 origin shift)
    /Centerlines/road_<E(id)>         BasisCurves
    /Boundaries/road_<E(id)>          BasisCurves
```

- Lanes are nested under their road and section. Their identity comes from
  the road and the section, so the path is the identity, and no composite key
  needs escaping.
- `/Network/Lanes` is reserved for formats whose lanes belong to no road,
  such as Lanelet2. `/Signals` and `/Objects` are reserved for Phase 2. None
  of the three is authored in Phase 1.
- A container is authored only when it has children. A metadata-only read
  therefore authors an empty `/Asset/Network`
  ([ADR 0006 §2](0006-fileformat-contract.md#2-metadataonly)).
- Children follow the source document order: roads, junctions, connections.
  Sections follow their index. Lanes follow descending lane number, from
  `left_<n>` through `center` to `right_<m>`.

### 4. Relationships

| Relationship | On | Targets |
| --- | --- | --- |
| `roadnetwork:predecessor`, `roadnetwork:successor` | lane | Every lane joined to this lane's start or end in the lane graph ([ADR 0001 §3](0001-roadnetwork-data-model.md#3-topology)), sorted by path |
| `roadnetwork:predecessor`, `roadnetwork:successor` | road | The one road or junction that the source names |
| `roadnetwork:junction` | road | The junction the road belongs to |
| `roadnetwork:incomingRoad`, `roadnetwork:connectingRoad` | connection | The roads |
| `roadnetwork:represents` | visualization curve | The semantic prims, in curve order |

- Each lane-graph edge appears on both of its lanes. A one-sided source
  statement is therefore traversable in both directions, as on CARLA's
  connecting roads.
- Lane `predecessor` and `successor` mean the start and end of the lane's
  own `s` range. They do not mean the direction of travel.
- Every target is inside the same `/Asset`. No relationship leaves the
  asset.

### 5. Visualization

- `Centerlines/road_<E(id)>` holds one curve for each non-center lane of each
  section, in section order and then lane order.
- `Boundaries/road_<E(id)>` holds one curve for each section's center border
  (the lane-offset line), followed by the outer border of each non-center
  lane, in the same order.
- `roadnetwork:represents` lists one target for each curve: the lane, or the
  center lane for a center border.

### 6. Several networks

Each file is its own layer, with `defaultPrim = "Asset"`. A consumer
references each layer under a prim of its own choosing, for example
`/World/CityA`, so paths cannot collide. Every relationship target is inside
`/Asset`, so each reference remaps its own targets. OpenDRIVE has no link
from one file to a road in another, and the importer creates none.

### 7. Determinism and reload

Paths are a pure function of `RoadNetworkData`. No counter, hash, address,
or global state takes part. An unchanged file therefore yields unchanged
paths on every read and reload.

## Consequences

- §3 is the only copy of the tree. The
  [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract)
  fixes only the top level and links here, so accepting this ADR changes no
  other document's tree. Lanes live under their road, and `/Lanes`,
  `/Signals`, and `/Objects` are reserved.
- `roadnetwork-usd` owns `E` and its inverse, so every format names prims
  the same way.
- Renaming a path rule after the first release breaks every override
  authored against the old paths. It needs a new ADR and a migration note.

## Evidence

Gathered in the [ADR probes](../reports/adr-probes-2026-11-22.md):

- the validity of the proposed names, and of their raw and Unicode
  alternatives, on OpenUSD 26.08 (§2 of the probes);
- the composition of two references of one network, with remapped
  relationships that survive flattening (§4);
- the one-sided lane links that make §4 write each edge on both lanes (§6).

Gathered in USD-01, in `roadnetwork-usd`
([`PrimNames.h`](../../libs/roadnetwork-usd/include/openstrata/rn/usd/PrimNames.h)):

- `roadnetworkUsd_layer_writer` checks `E` against the §1 table and for the
  underscore, an escape-like ID (`_5f`), punctuation, spaces and tabs,
  multi-byte UTF-8, NUL and `0xff` bytes, and the empty ID, which
  `PrimName()` rejects. It checks that `E⁻¹(E(id)) = id` and that every name
  is ASCII and a valid identifier. `E⁻¹` rejects strings that `E` cannot
  produce, such as an uppercase escape or an escape of a letter or digit.
- The same test checks the §3 order of roads, sections, and lanes, the §4
  relationships on both lanes of an edge, sorted by path, and the §5 curve
  order with `roadnetwork:represents`.
- `roadnetworkOpendrive_file_format_contract` reloads `decoder/network.xodr`
  and finds the same prim paths and relationship targets. It references the
  same network twice into one stage, finds every relationship target inside
  its own reference, and finds the same targets after `Flatten()`.
- One case §5 does not state: a road whose sections hold only a center lane
  has no centerline, so it has no `Centerlines` prim. Its `Boundaries` prim
  still holds the center border.

Needed before acceptance:

- Review together with ADR 0001's derived identities (current work, next
  action 1).
