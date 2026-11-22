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
  either can later move behind a payload: global semantics and geometry
  tiles must be able to become separate load units.
- No semantic data may live only in visualization prims, so topology stays
  usable while geometry is not loaded.
- The semantic ID of a road or lane that spans several tiles must not depend
  on the tile, and an element must be able to refer to a connection target
  outside its tile.
- Geometry error and IDs must not depend on tile boundaries or on the order
  in which tiles are generated.
- A change to the stage hierarchy or load boundaries updates
  [ADR 0005](0005-prim-path-and-id-stability.md) or this ADR and states the
  compatibility of layers authored before.

A conceptual target, not the Phase 1 stage contract:

```text
RoadNetwork asset
  global semantic network
  derived geometry tiles
    tile_000
    tile_001
    ...
```

## Evidence needed before acceptance

- Performance measurements (§13 metrics) on at least one large, licensed
  network.
- A composition prototype showing cross-unit relationships surviving
  partial loading, payload unloading, and reloading.
- Measured trade-offs between mesh subdivision accuracy, tile granularity,
  and the size of any global index.
