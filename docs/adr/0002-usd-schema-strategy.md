# ADR 0002 — USD schema strategy

- **Status:** Proposed (2026-11-22)
- **Scope:** how the semantic layer under `/Asset/Network` is typed in USD

## Context

The design policy ([§6.1](../design/DESIGN_POLICY.md#61-separate-data-from-visualization))
prefers standard USD schemas, but standard `UsdGeom` types cannot express
road-network topology and semantics: lane connectivity, junction membership,
lane types, speed limits, or rule references. The policy also forbids
adopting a schema before its specification is settled.

## Questions

1. Are semantic prims typeless (`def "Lane"`), standard (`Scope`, `Xform`),
   or of a project-specific typed schema?
2. Are properties namespaced attributes (for example `roadnetwork:laneType`)
   and relationships, or an applied API schema?
3. Where does source-specific data (OpenDRIVE `<userData>`, vendor
   extensions) go: `customData`, namespaced attributes, or nowhere?
4. If a codeless or generated schema is introduced later, what is the
   migration path for already-authored stages?

## Options considered

- **A. Standard prims plus namespaced attributes and relationships.**
  No plugin schema to ship or version; discoverable with ordinary tools.
  Typing is by convention only, and validation lives in tests and a checker.
- **B. Applied API schemas (codeless).** Typed property sets without
  compiled code; `usdchecker` and schema registry aware. Requires a schema
  package in the bundle and a versioning policy.
- **C. Typed IsA schemas (`RoadNetworkRoad`, `RoadNetworkLane`, …).**
  Strongest typing; highest commitment and migration cost.

## Initial direction

Start with A for the Phase 1 vertical slice, with a fixed property namespace
and a written property list, so that a later move to B is a mechanical
re-typing rather than a semantic change. Re-evaluate after the second format
has been mapped on paper (ADR 0008), when the shared vocabulary is known.

## Evidence needed before acceptance

- The property namespace and the Phase 1 property list.
- A sample `.usda` of one road, two lanes, and one junction connection.
- Confirmation that the chosen form survives `usdcat --flatten`, references,
  and `usdchecker` on the target runtime.
