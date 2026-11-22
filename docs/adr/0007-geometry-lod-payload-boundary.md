# ADR 0007 — Geometry generation, LOD and tiles, payload boundary

- **Status:** Proposed (2026-11-22)
- **Scope:** where generated geometry lives, and how large networks are split
  into loadable units

## Context

The design policy ([§13](../design/DESIGN_POLICY.md#13-performance-and-large-data))
treats centerlines and surfaces as regenerable derived data and defers LOD,
tiling, streaming, and lazy loading until load units, topology boundaries, and
composition constraints are understood. Mesh generation must not be required
for a read to succeed ([§18](../design/DESIGN_POLICY.md#18-initial-non-goals)).

This ADR is not needed for the MVP; it is recorded now so that Phase 1
choices do not close off its options.

## Questions

1. Is `/Asset/Visualization` authored in the same layer as `/Asset/Network`,
   behind a payload, or on request through a file-format argument?
2. What is a load unit for a large network (road, tile, junction cluster),
   and how are relationships across unit boundaries kept valid?
3. Do LOD variants use variant sets, payloads, or separate layers?
4. Is runtime generation (`UsdProcGenerativeProcedural`, Hydra) a better home
   for surface meshes than the file format?

## Constraints for earlier phases

- Phase 1 keeps semantic and visualization data in separate subtrees so
  either can later move behind a payload.
- No semantic data may live only in visualization prims.

## Evidence needed before acceptance

- Performance measurements (§13 metrics) on at least one large, licensed
  network.
- A composition prototype showing cross-unit relationships surviving
  partial loading.
