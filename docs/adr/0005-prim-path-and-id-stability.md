# ADR 0005 — Prim-path and source-ID stability; multiple networks

- **Status:** Proposed (2026-11-22)
- **Scope:** how source IDs become prim paths and relationships, and how
  several road networks compose in one stage

## Context

USD prim names are restricted identifiers; OpenDRIVE IDs are arbitrary
strings (often numeric) and some entities, such as lanes, are identified only
by their position (road, lane section, lane number). The design policy
([§6.2](../design/DESIGN_POLICY.md#62-stage-contract-proposed),
[§10.5](../design/DESIGN_POLICY.md#105-determinism)) requires deterministic
naming so that reloads and new formats do not move paths unnecessarily, and
so that overrides authored in a stronger layer keep targeting the same prims.

## Questions

1. Encoding: how is an arbitrary source ID turned into a valid prim name
   (prefix plus escaping, hashing, or both), and is the original ID always
   kept as an attribute?
2. Collisions: what happens when two IDs encode to the same name?
3. Derived IDs: how are lanes and lane sections named (for example
   `road_<id>/section_<n>/lane_<m>`), and does a lane-section index remain
   stable if an earlier section is inserted in the source?
4. Hierarchy: are lanes nested under their road or kept flat under
   `/Network/Lanes` with relationships to the road?
5. Relationships: which direction (successor, predecessor, or both), and what
   do they target?
6. Multiple networks: how are two `.xodr` files referenced into one stage
   without path collisions or cross-file references?

## Options considered

- **A. Escaped source IDs with a type prefix.** Readable and stable;
  requires a reversible escaping rule and a collision diagnostic.
- **B. Hashed names.** Always valid and collision-resistant in practice;
  unreadable, and debugging needs the attribute lookup.
- **C. Index-based names.** Rejected: paths move when the source order
  changes.

## Initial direction

A, with the verbatim source ID always authored as an attribute, a collision
being an error rather than a silent suffix, and each referenced network
kept under its own `/Asset` so composition is by ordinary references.

## Evidence needed before acceptance

- The escaping rule with tests for numeric, Unicode, and punctuation IDs.
- A reload test showing unchanged paths for an unchanged file.
- A two-network composition example.
