# ADR 0008 — Lanelet2 versus OSM identification; shared regulatory model

- **Status:** Proposed (2026-11-22)
- **Scope:** routing of `.osm` files between bundles, and the shared
  representation of regulatory information

## Context

Lanelet2 maps are stored as OpenStreetMap XML with Lanelet2-specific tags, so
Lanelet2 and general OSM share the `.osm` extension. The design policy
([§4.2](../design/DESIGN_POLICY.md#42-shared-extensions)) forbids treating them
as the same format by extension. OpenUSD registers file formats by extension,
so two bundles cannot both own `.osm` without a routing decision.

Regulatory information differs by format: Lanelet2 regulatory elements,
OpenDRIVE signals and their references, and SUMO traffic-light programs and
connection rules.

## Questions

1. Which bundle registers `.osm`, and how does it route between Lanelet2 and
   general OSM (one bundle with two dialects, or a router that delegates)?
2. Which structural signature identifies a Lanelet2 file cheaply in
   `CanRead()` (for example relations tagged `type=lanelet`), and what
   happens to OSM data that is neither?
3. What common regulatory vocabulary does `RoadNetworkData` share across
   OpenDRIVE, Lanelet2, and SUMO, and what stays source-specific?
4. Which diagnostic prefixes do the new bundles take?

## Options considered

- **A. One `.osm` bundle with dialect detection**, as `usd-3dgs-plugins`
  hosts two PLY dialects in one bundle.
- **B. Separate bundles with a file-format argument or explicit target** to
  choose the dialect.

## Initial direction

None yet. Decide after the Phase 4 format spike, informed by the shared model
from ADR 0001.

## Evidence needed before acceptance

- Representative Lanelet2 and general OSM fixtures with a signature check
  that separates them.
- A paper mapping of one regulatory element from each format into the
  shared model.
