# ADR 0001 — `RoadNetworkData` ID, topology, and geometry model

- **Status:** Proposed (2026-11-22)
- **Scope:** the shared intermediate representation in `roadnetwork-core`
  that every format decoder produces and the shared USD writer consumes

## Context

The design policy ([§5.3](../design/DESIGN_POLICY.md#53-intermediate-representation))
requires one format-independent IR built from standard C++ types, with no USD
type in its public API. It must carry OpenDRIVE first, and later Lanelet2 and
SUMO, without forcing either into the other's model:

- OpenDRIVE is reference-line based: roads own a plan-view curve, profiles
  along `s`, and lane sections whose lanes are defined by width or border
  functions relative to the reference line.
- Lanelet2 is boundary based: a lanelet is a pair of left and right polylines
  plus regulatory elements.
- SUMO is graph based: edges and lanes with shapes, plus connections and
  traffic-light programs.

## Questions

1. **Identity.** How is a source ID kept (string, format-qualified), and how
   are derived entities without a source ID (for example an OpenDRIVE lane,
   which is identified only by road, section, and lane number) named?
2. **Topology.** Is connectivity stored on lanes, on roads, or as a separate
   edge list? How are contact points (start or end) and directions recorded?
3. **Geometry.** Is geometry analytic (curve segments and polynomials),
   sampled (polylines), or both? See ADR 0004 for the OpenDRIVE specifics.
4. **Extensibility.** How is source-specific data that the shared model does
   not represent kept for provenance (typed side tables, a key-value bag, or
   nothing plus a diagnostic)?
5. **Validation.** Which invariants does `roadnetwork-core` check after
   decoding (unique IDs, resolved references, finite values, monotonic `s`)?

## Options considered

- **A. OpenDRIVE-shaped IR.** Roads, lane sections, and analytic curves as
  first-class types; other formats are lifted into it.
  Simple for Phase 1; risks forcing boundary-based formats into a reference
  line they do not have.
- **B. Boundary/graph-shaped IR.** Lanes as boundary polylines and a lane
  graph; OpenDRIVE is evaluated into it.
  Fits Lanelet2 and SUMO; loses OpenDRIVE's analytic geometry, contrary to
  the initial direction of ADR 0004.
- **C. Layered IR.** A common lane-graph and identity layer shared by all
  formats, plus optional per-representation geometry (analytic reference-line
  geometry, boundary polylines) attached to it.

## Initial direction

Option C: a common identity and topology core, with geometry representations
that a format may or may not populate, and explicit diagnostics when a
consumer needs a representation the source did not provide. Confirm by
sketching the OpenDRIVE line/arc slice and a paper mapping of one Lanelet2
fixture before acceptance.

## Evidence needed before acceptance

- A header sketch of the core types reviewed against one OpenDRIVE fixture
  and one Lanelet2 example.
- The validation invariant list and the diagnostic codes it uses.
- The derived-ID rule, agreed together with ADR 0005.
