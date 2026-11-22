# ADR 0003 — CRS, axes, units, and large coordinates

- **Status:** Proposed (2026-11-22). The proposed decision is ready for review.
- **Scope:** layer metadata, georeference provenance, and coordinate
  conversion for every format whose source coordinates are in a metric
  frame, such as OpenDRIVE. A format whose coordinates are geodetic only
  is outside this scope until its own ADR decides its projection (§2).
- **Phase 1 owner:** USD-01, with ODR-02 for the header decoding
  ([current work](../roadmap/current.md#active-work))

## Context

The design policy ([§6.3](../design/DESIGN_POLICY.md#63-coordinates-units-and-georeferencing))
sets three rules:

- preserve the source CRS and units;
- never treat an unknown CRS as known;
- document `upAxis` and `metersPerUnit` as importer defaults where the
  source does not define them.

OpenDRIVE uses a right-handed inertial frame with `z` up and the metre as
its unit. A file may carry a georeference, a PROJ string, and a header
`<offset>`. Real networks can sit at projected coordinates hundreds of
kilometres from the origin. In single precision, such coordinates lose
centimetres ([FMT-01 spike §4](../reports/format-spike-opendrive-2026-11-22.md#4-coordinates-and-georeference)).

The [ADR probes](../reports/adr-probes-2026-11-22.md) measured three things:

- the `float` error: at most 0.49 mm up to 8,192 m, 1.6 cm at 500 km, and
  25 cm at 4,500 km (§3 of the probes);
- composition: layer metadata does not reach a referencing stage, but
  `/Asset` attributes do (§4);
- an origin shift on `/Asset/Visualization`, which recovers UTM-scale points
  within 4.5e-8 m through a reference (§8).

## Questions

1. `upAxis`: author `Z` to match OpenDRIVE, or `Y` with a conversion?
2. Where is the CRS recorded (layer `customLayerData`, `/Asset` metadata, or
   an attribute), and in what form (the verbatim source string or a parsed
   EPSG code)?
3. Large coordinates: keep source coordinates in double precision, shift the
   origin of `/Asset`, or both? Which values are single precision in USD (for
   example `points` in `UsdGeom`)?
4. How is the header offset applied: baked into coordinates, or authored as
   a transform?
5. How do two networks with different CRSs or origins compose (ADR 0005)?

## Options considered

- **A. Source frame, `upAxis = "Z"`, no reprojection.** Coordinates stay in
  the projected frame of the source. The CRS is provenance only.
- **B. Local frame with recorded origin.** Coordinates are expressed relative
  to a recorded origin to keep `float` precision. The origin and CRS are
  recorded, so the original values are reproducible.
- **C. Reprojection to a fixed CRS.** Rejected for import from a metric
  source frame. It requires a projection library and makes the importer
  assert geodesy that it cannot verify. A geodetic-only source has no
  metric frame to keep, so this rejection does not decide its case (§2).

## Proposed decision

A for axes, units, and the semantic layer. B for the single-precision
visualization points only. Never reproject during import.

### 1. Axes and units

- Every layer authors `upAxis = "Z"` and `metersPerUnit = 1`. There is no
  axis conversion: mapping the source `x`, `y`, `z` to USD `x`, `y`, `z`
  unchanged is the importer's convention, while the up direction and the
  unit it exposes are source facts.
- A consumer that wants a `Y`-up asset can request one with the `upAxis`
  file-format argument of [ADR 0009](0009-file-format-arguments-up-axis.md),
  which adds a rotation on `/Asset` and changes no authored value. Without
  the argument, nothing in this section changes.
- For OpenDRIVE, both values are **source facts**: the specification defines
  a `z`-up frame in metres. The [OpenDRIVE mapping](../reference/OPENDRIVE_MAPPING.md)
  says so. A later format whose specification defines neither documents them
  as **importer defaults** in its own mapping.
- Layer metadata does not compose through references. A consumer whose stage
  is `Y`-up or uses other units places the asset with its own transform on
  the referencing prim. The importer does not try to compensate.
- The decoder converts other units to SI: speeds become metres per second,
  and angles stay in radians as in the source.

### 2. Frame

All coordinates are authored in the inertial frame of the file, as written.
The header offset is not applied (§4), and nothing is reprojected.

Some formats have no metric frame: Lanelet2 `.osm` nodes carry only WGS 84
latitude and longitude
([Lanelet2 mapping §6](../reports/adr-0001-lanelet2-mapping-2026-11-23.md#6-coordinates)).
Authoring degrees as written would make `metersPerUnit = 1` false. Such a
format needs a projection, and its own ADR decides it before the format is
accepted: the projection, the choice and recording of the origin, and the
loss it reports. That ADR may supersede this section for its format only.
For every format in scope, this section is unchanged.

### 3. Georeference

- The content of `<geoReference>` is authored on `/Asset` as
  `roadnetwork:source:geoReference` (`string`), with only the surrounding
  whitespace trimmed ([ADR 0002](0002-usd-schema-strategy.md)). It is not
  written to `customLayerData`, which a reference does not carry.
- The string is never parsed, validated, or used. Its content raises no
  diagnostic. Incomplete strings, such as CARLA's `+lat_0 … +lon_0` without
  `+proj`, are kept as they are.
- A missing or empty `<geoReference>` leaves the attribute unauthored. No
  default CRS is authored, and the absence makes no claim.

### 4. Header offset

- `<offset x y z hdg>` is authored on `/Asset` as
  `roadnetwork:source:offset` (`double3`) and
  `roadnetwork:source:offsetHeading` (`double`, radians), exactly as read.
- It is not applied to coordinates and produces no `xformOp`. In OpenDRIVE,
  the offset relates the inertial frame of the file to the georeferenced
  frame. Applying it would move coordinates back to the large values that it
  exists to avoid. A consumer that needs projected coordinates applies the
  offset and the georeference itself, under the specification's rules.

### 5. Precision

- **Semantic layer.** Every coordinate and parameter under `/Asset/Network`
  is a `double` ([ADR 0002 §2](0002-usd-schema-strategy.md#2-properties)), so
  no precision is lost there at any magnitude.
- **Visualization.** `points` are `point3f[]`. The writer computes an origin
  `O` from the axis-aligned bounds of all visualization points:
  - If the bounds lie within ±8,192 m on every axis, `O = (0, 0, 0)` and no
    `xformOp` is authored.
  - Otherwise, each component of `O` is the center of the bounds, rounded to
    the nearest multiple of 1,024 m. The rounded value is exact in `float`
    and in `double`, and a small edit to the network does not change it.
  - `O` is authored on `/Asset/Visualization` (an `Xform`) as a
    double-precision `xformOp:translate`, and each point is authored as
    `p − O` in `float`.
- Inside ±8,192 m of `O`, rounding costs at most 0.49 mm. That is twenty
  times below the 1 cm tessellation tolerance of
  [ADR 0004](0004-opendrive-geometry-retention.md). For networks wider than
  16 km, the error grows with the extent as the probes table shows. Tiling
  for such networks belongs to [ADR 0007](0007-geometry-lod-payload-boundary.md).
- The source position of a sample is `p_float + O`, evaluated in `double`.

### 6. Several networks

Each asset carries its own georeference, offset, and visualization origin,
all inside `/Asset`, so a reference carries them along. The importer reads
one file at a time. It does not reconcile CRSs or origins across files and
does not check that two networks agree. Placing several networks in one scene
is the consumer's composition, as
[ADR 0005](0005-prim-path-and-id-stability.md#6-several-networks) describes.

## Consequences

- The [OpenDRIVE mapping §2](../reference/OPENDRIVE_MAPPING.md#2-coordinate-frames)
  takes §1–§4 when this ADR is accepted.
- The [capability matrix](../reference/CAPABILITY_MATRIX.md) lists `upAxis`
  and `metersPerUnit` as source facts for OpenDRIVE, and the georeference
  and offset as verbatim provenance.
- `/Asset/Visualization` is an `Xform`, not a `Scope`.
- Consumers that want projected or geodetic coordinates need their own PROJ
  setup. The importer gives them the exact inputs.

## Evidence

Gathered in the [ADR probes](../reports/adr-probes-2026-11-22.md):

- the `float` error table at magnitudes from 670 m to 10,000 km (§3);
- `/Asset` attributes compose through references, and layer metadata does
  not (§4);
- an origin-shifted point at UTM scale is recovered within 4.5e-8 m through
  a reference, and the layer passes `usdchecker` (§8).

Gathered in USD-01:

- `writer/projected.xodr`, a hand-authored road at about 500 km east and
  5,428 km north with a georeference and an `<offset>`, in the bundle
  manifest. Its line and arc have closed forms.
- Through a reference, `roadnetworkOpendrive_file_format_contract` checks
  that the georeference, the offset, and the offset heading reach the
  referencing prim unchanged; that the origin on `/Asset/Visualization` is a
  multiple of 1,024 m; and that every point, mapped to world space by
  `ComputeLocalToWorldTransform`, lies within 0.5 mm of its closed-form
  curve. The worst point is 0.024 mm away. Rounded to `float` without the
  shift, the same points would be up to 246 mm away. The layer passes
  `usdchecker`.
- The CARLA `Town01` and `Town02` layers author no `xformOp`, and every
  layer authors `upAxis = "Z"` and `metersPerUnit = 1`, in full and
  metadata-only reads alike.
- `roadnetworkUsd_layer_writer` checks the §5 rule on a UTM-scale road
  built in code: the origin is the rounded center of the bounds, and every
  `float` point plus the origin is within 0.5 mm of the tessellator's
  `double` sample.

Gathered in the [Lanelet2 paper mapping](../reports/adr-0001-lanelet2-mapping-2026-11-23.md#6-coordinates):

- Lanelet2's `mapping_example.osm` has WGS 84 `lat` and `lon` on every node
  and no metric frame, projection string, or origin. The scope said "every
  format", and §2 would have left such a file nothing valid to author. The
  maintainer chose on 2026-11-23 to narrow the scope and §2 to metric
  source frames before acceptance, and to leave the projection of
  geodetic-only formats to their own ADR.

Needed before acceptance:

- Review (current work, next action 1).
