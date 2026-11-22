# ADR 0003 — CRS, axes, units, and large coordinates

- **Status:** Proposed (2026-11-22)
- **Scope:** layer metadata, georeference provenance, and coordinate
  conversion for every format

## Context

The design policy ([§6.3](../design/DESIGN_POLICY.md#63-coordinates-units-and-georeferencing))
requires preserving the source CRS and units, never treating an unknown CRS
as known, and documenting `upAxis` and `metersPerUnit` as importer defaults
where the source does not define them.

OpenDRIVE uses a right-handed inertial frame with `z` up and meters as the
unit, and may carry a georeference string and a header offset. Real networks
can sit at projected coordinates of hundreds of kilometres, where single
precision loses centimetre accuracy.

## Questions

1. `upAxis`: author `Z` to match OpenDRIVE, or `Y` with a conversion?
2. Where is the CRS recorded (layer `customLayerData`, `/Asset` metadata, an
   attribute), and in what form (verbatim source string, parsed EPSG code)?
3. Large coordinates: keep source coordinates in double precision, apply an
   origin shift to `/Asset`, or both? Which values are single precision in
   USD (for example `points` in `UsdGeom`)?
4. How is the header offset applied: baked into coordinates, or authored as
   a transform?
5. How do two networks with different CRSs or origins compose (ADR 0005)?

## Options considered

- **A. Source frame, `upAxis = "Z"`, no reprojection.** Coordinates stay in
  the source projected frame; the CRS is provenance only.
- **B. Local frame with recorded origin.** Coordinates are expressed
  relative to a recorded origin to keep float precision; the origin and CRS
  are recorded so the original values are reproducible.
- **C. Reprojection to a fixed CRS.** Rejected for import: it requires a
  projection library and makes the importer assert geodesy it cannot verify.

## Initial direction

A for axes and units, combined with B for precision if measurements on a
large fixture show visible loss in single-precision geometry. Never reproject
during import.

## Evidence needed before acceptance

- A precision test at large projected coordinates for the attributes the
  writer authors in single precision.
- The exact metadata keys and their documentation as importer defaults or
  source claims.
- A round-trip test that reconstructs source coordinates within a stated
  tolerance.
