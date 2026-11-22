# ADR 0001–0005 probes — 2026-11-22

This report records the measurements gathered for the ADR-01..05 work item in
[current work](../roadmap/current.md): bringing ADRs 0001–0005 to a state
where they are ready for review. The ADRs cite these results. Nothing here is
a support claim. The [capability matrix](../reference/CAPABILITY_MATRIX.md)
still lists every OpenDRIVE capability as unsupported.

## Summary

| Probe | Result | Used by |
| --- | --- | --- |
| Prim-name validity | `road_1`, `road__2d1`, `section_0`, `right_1`, and `center` are valid prim names. `1`, `-1`, `a.b`, and `a-b` are not. Unicode names are valid prim names but not `Tf` identifiers, and the NFC and NFD spellings of `é` are two different valid names (§2). | [ADR 0005](../adr/0005-prim-path-and-id-stability.md) |
| Single-precision error | Rounding to `float` costs at most 0.03 mm at 670 m (the CARLA extent), 0.49 mm at 8,192 m, 1.6 cm at 500 km, and 25 cm at 4,500 km (§3). | [ADR 0003](../adr/0003-crs-axes-units.md) |
| Sample stage | A hand-written layer with the proposed layout passes `usdchecker`. Referenced twice into one stage, its relationships are remapped to each reference, and they survive flattening. `/Asset` attributes compose; layer metadata does not (§4). | [ADR 0002](../adr/0002-usd-schema-strategy.md), [ADR 0003](../adr/0003-crs-axes-units.md), [ADR 0005](../adr/0005-prim-path-and-id-stability.md) |
| CARLA continuity | Consecutive plan-view segments in `Town01` and `Town02` meet within 0.35 mm and 4.6e-5 rad, and their `s` values within 3e-14 m (§5). | [ADR 0004](../adr/0004-opendrive-geometry-retention.md) |
| CARLA links | 36% (`Town01`) and 16% (`Town02`) of lane-link statements have no matching statement on the other lane. All of them start on a connecting road inside a junction (§6). | [ADR 0001](../adr/0001-roadnetwork-data-model.md), [ADR 0005](../adr/0005-prim-path-and-id-stability.md) |
| Tessellation size | At a 1 cm chord tolerance, the reference lines of `Town01` need 1,428 intervals and those of `Town02` need 1,112 (§7). | [ADR 0004](../adr/0004-opendrive-geometry-retention.md) |
| Origin shift | A point at UTM-scale coordinates is authored as a `float` relative to a double-precision `xformOp:translate` on `/Asset/Visualization`. Through a reference, its world position comes back within 4.5e-8 m (§8). | [ADR 0003](../adr/0003-crs-axes-units.md) |

## 1. Environment

- Local Windows 11, the OpenStrata runtime
  `openstrata-cy2026-windows-x86_64-py313-usd` (OpenUSD 26.08, CPython 3.13),
  activated with `ost env cy2026 --profile usd`.
- The CARLA files at
  [`plugins/roadnetwork-opendrive/tests/fixtures/carla/`](../../plugins/roadnetwork-opendrive/tests/fixtures/carla/),
  read with the Python standard library `xml.etree.ElementTree`.
- The probe scripts were throwaway scripts and are not committed. The
  methods are described in each section, so the results can be reproduced.

## 2. Prim-name validity

Checked with `Sdf.Path.IsValidPrimName` and `Tf.IsValidIdentifier` on
OpenUSD 26.08.

| Name | Prim name | `Tf` identifier |
| --- | --- | --- |
| `1`, `-1` | no | no |
| `a.b`, `a-b`, `a b` | no | no |
| `road_1`, `_1`, `road__2d1`, `road_a_5fb` | yes | yes |
| `section_0`, `right_1`, `center` | yes | yes |
| `道路`, `road_道路` | yes | no |
| `é` (U+00E9, NFC) | yes | no |
| `é` (U+0065 U+0301, NFD) | yes | no |

OpenDRIVE IDs are mostly decimal integers, which are not valid prim names on
their own. A Unicode ID could be used as a prim name, but two canonically
equivalent spellings would give two different paths that look identical.

## 3. Single-precision error

`UsdGeomBasisCurves` stores `points` as `point3f[]`. For each magnitude, 2,000
values `x + k · 0.001237 m` were rounded to IEEE-754 single precision. The
table shows the largest absolute error.

| Magnitude | `float` spacing | Largest error |
| --- | --- | --- |
| 670.4 m (largest CARLA coordinate) | 0.061 mm | 0.031 mm |
| 8,191 m | 0.49 mm | 0.49 mm |
| 65,535 m | 3.9 mm | 3.9 mm |
| 100 km | 7.8 mm | 3.9 mm |
| 500 km (a UTM easting) | 3.1 cm | 1.6 cm |
| 1,000 km | 6.3 cm | 3.1 cm |
| 4,500 km (a UTM northing) | 50 cm | 25 cm |
| 10,000 km | 1 m | 50 cm |

Up to 8,192 m (2¹³), the error stays below 0.5 mm. That is twenty times below
the 1 cm tessellation tolerance that ADR 0004 proposes. A network in projected
coordinates written without an `<offset>` would be visibly wrong in single
precision.

## 4. Sample stage

A hand-written `network.usda` followed the layout proposed in ADRs 0002 and
0005. It had:

- `/Asset`, an `Xform` with `kind = "component"`;
- two roads with a lane section each, as `Scope` prims;
- lanes linked by `roadnetwork:predecessor` and `roadnetwork:successor`
  relationships;
- one junction with one connection;
- `uniform` namespaced attributes, including parallel `double[]` arrays for
  the plan view;
- one `BasisCurves` under `/Asset/Visualization/Centerlines`, with a
  `roadnetwork:represents` relationship to two lanes.

Results on OpenUSD 26.08:

- `usdchecker network.usda` reports `Success!`.
- An in-memory stage referenced the layer under both `/World/CityA` and
  `/World/CityB`. Each lane's `roadnetwork:successor` resolved to the lane in
  its own reference (for example
  `/World/CityB/Network/Roads/road_2/section_0/right_1`). Each
  `roadnetwork:represents` resolved to the lanes in its own reference.
- `Stage.Flatten()` followed by an export and a reopen kept the remapped
  targets. Attribute variability was still `uniform`.
- `roadnetwork:source:geoReference` on `/Asset` was readable on both
  referencing prims. The referenced layer's `upAxis = "Z"` and
  `metersPerUnit = 1` did not reach the composed stage, which reported its
  own fallbacks (`Y`, 0.01). Layer metadata does not compose through a
  reference, so provenance that must survive a reference belongs on `/Asset`.

The flattened stage failed `usdchecker` only on the metadata that the probe's
own wrapper stage did not set (`metersPerUnit`, `upAxis`, `defaultPrim`).

## 5. CARLA plan-view continuity

The end of each `line` and `arc` segment was evaluated in closed form and
compared with the start of the next segment on the same road.

| File | Segment pairs | Largest position gap | Largest heading gap | Largest `s` gap | Last segment end vs road `length` |
| --- | --- | --- | --- | --- | --- |
| `Town01.xodr` | 254 | 0.347 mm | 1.8e-12 rad | 2.8e-14 m | 0 |
| `Town02.xodr` | 342 | 0.312 mm | 4.6e-5 rad | 1.4e-14 m | 0 |

No position gap exceeds 1 mm. The smallest arc radius is 6.50 m in `Town01`
and 3.62 m in `Town02`. In both files every `<elevation>` and `<laneOffset>`
record is zero, every `<speed>` is in `mph`, and every lane group is numbered
without gaps (`1..n` on the left, `-1..-m` on the right).

## 6. CARLA links

Lane links were resolved the way the decoder will resolve them:

- a link inside a road connects to the previous or next lane section;
- a link at the end of a road follows the road link and its `contactPoint`.

Each statement was then checked for a matching statement on the target lane.

| File | Roads / junctions / lanes | Lane-link statements | Reciprocated | One-sided | Unresolved |
| --- | --- | --- | --- | --- | --- |
| `Town01.xodr` | 98 / 12 / 482 | 396 | 252 | 144 | 0 |
| `Town02.xodr` | 68 / 8 / 660 | 616 | 520 | 96 | 0 |

At road level, every road link without a matching link back starts on a
connecting road inside a junction (`Town01`: 144, `Town02`: 96). The incoming
road links to the junction instead, as OpenDRIVE specifies, and the junction
`<connection>`/`<laneLink>` elements carry the other direction (72 and 48
lane links). Every road link that is not a junction link has a `contactPoint`.

A decoder that required both sides of every link to be stated would reject
both files. The lane graph therefore has to be built from the union of the
statements.

## 7. Tessellation size

A per-segment estimate of the subdivision that ADR 0004 proposes was made.
Lines take one interval. Arcs take `ceil(|κ| L / Δθ)` intervals, with
`Δθ = 2 acos(1 − ε / R)`, where `R` is the arc radius plus the widest lane
extent of the road (the outermost boundary).

| File | Reference-line length | Intervals at ε = 1 cm | Intervals at ε = 1 mm |
| --- | --- | --- | --- |
| `Town01.xodr` | 3,923 m | 1,428 | 3,646 |
| `Town02.xodr` | 1,758 m | 1,112 | 2,551 |

Every lane boundary on a road shares the road's stations, so these counts
multiply by the number of boundaries per section. Even then, the layer stays
small for networks of this size.

## 8. Origin shift

A layer was authored with `/Asset/Visualization` as an `Xform` carrying a
double-precision `xformOp:translate` of `(500736, 5427712, 0)`, which is a
multiple of 1,024 m on each axis. Under it, a `BasisCurves` point was stored
as the `float` difference between the source point
`(500737.123456, 5427713.654321, 12.5)` and that origin.

- `usdchecker` reports `Success!` on the layer.
- The layer was referenced under `/World/CityA` in another stage.
  `ComputeLocalToWorldTransform` applied to the stored point returned the
  source point within 4.5e-8 m.
- The authored translate op kept `PrecisionDouble` through the reference.

Without the shift, the same point would have been rounded to `float` at a
magnitude of 5.4 × 10⁶ m, with an error of up to 25 cm (§3).
