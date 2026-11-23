# Release plan

This page owns the phases, the **feature list** (the `F-` IDs that other
documents cite), each phase's exit criteria, and the format acceptance gate.
The active phase's workstreams are in [current work](current.md); what the
tree supports now is in the [capability matrix](../reference/CAPABILITY_MATRIX.md).
A feature listed here is a plan, not a claim.

Work is organized in phases. A version number is assigned when a phase is
scheduled for release; the first tagged release is planned at the end of
Phase 1.

| Phase | Goal | Features | Exit criteria | Version |
| --- | --- | --- | --- | --- |
| **0 — Scaffold / ADR** | OpenStrata workspace, basic CI, initial decisions | — | An empty `.xodr` bundle and the core and USD libraries pass the package test; ADR 0006 numbering is accepted. | untagged (met 2026-11-22) |
| **1 — OpenDRIVE vertical slice** | Simple roads, lanes, and junctions into USD | F-01 to F-11 | [Phase 1 exit](#phase-1-exit) | v0.1.0 |
| **2 — OpenDRIVE fidelity** | Wider coverage of curves, cross-sections, rules, and objects | F-12 to F-16, F-27 | Capability matrix, diagnostics, golden semantics, and compatibility tests are complete for the declared scope. | unassigned |
| **3 — Geometry / scale** | Surfaces, LOD, tiling, and loading measured and designed | F-17 to F-21, F-28 | Measured performance and quality metrics can be published; ADR 0007 is accepted. | unassigned |
| **4 — Multi-format** | Add Lanelet2 or SUMO | F-22 to F-24 | The same IR and USD writer are reused and the differences are explained; ADR 0008 is accepted for Lanelet2. | unassigned |
| **5 — Runtime integration** | Composition with the runtime projects | F-25, F-26 | Deployable and tested independently of other USD assets and runtimes. | unassigned |

This repository owns import and USD authoring. See the
[API boundary](../architecture/API_BOUNDARY.md) for integrated runtime
responsibilities.

## Phase 1 — OpenDRIVE vertical slice

The minimum useful implementation. The scope is frozen at F-01 to F-11:
LOD, a USD schema, and a query API are not added to it and never block it
([design policy §7.1](../design/DESIGN_POLICY.md#71-minimum-useful-implementation-mvp)).

| ID | Feature | Minimum acceptance | Decided by |
| --- | --- | --- | --- |
| F-01 | `SdfFileFormat` plugin | `.xodr` opens through `SdfLayer::FindOrOpen` and `UsdStage::Open`. Malformed XML fails; it never opens as an empty layer. | [ADR 0006](../adr/0006-fileformat-contract.md) |
| F-02 | XML and version recognition | `CanRead()` is a cheap structural sniff, `Read()` validates fully. `revMajor="1"` with `revMinor` 4–9 is accepted; any other version fails with a stable code. | [ADR 0006](../adr/0006-fileformat-contract.md), [FMT-01 §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy) |
| F-03 | IR and validation | Duplicate IDs, unresolved references, non-finite values, and station or lane-structure violations fail with stable `RN-` codes. | [ADR 0001](../adr/0001-roadnetwork-data-model.md) |
| F-04 | Analytic plan view | Line and arc are kept and evaluated in `double`, and pass closed-form numeric tests. | [ADR 0004](../adr/0004-opendrive-geometry-retention.md) |
| F-05 | Profiles and lane shape | Lane sections, lane offset, lane width, and elevation are kept. Non-zero superelevation and lateral shape are rejected explicitly. | [ADR 0004](../adr/0004-opendrive-geometry-retention.md) |
| F-06 | Topology | Road links, lane-end links, and `default` junction connections are kept and validated; links stated on one side only are normalized. | [ADR 0001](../adr/0001-roadnetwork-data-model.md) |
| F-07 | Semantic writer | `/Asset/Network` with road, lane, and junction prims, typed properties, and relationships, authored with stable paths. | [ADR 0002](../adr/0002-usd-schema-strategy.md), [ADR 0005](../adr/0005-prim-path-and-id-stability.md) |
| F-08 | Derived visualization | Centerlines and lane boundaries, with a measured approximation error, kept apart from the semantics. | [ADR 0004](../adr/0004-opendrive-geometry-retention.md), [ADR 0005](../adr/0005-prim-path-and-id-stability.md) |
| F-09 | CRS, units, and origin | The georeference and offset are kept verbatim; nothing unknown is presented as known; large coordinates keep their precision. | [ADR 0003](../adr/0003-crs-axes-units.md) |
| F-10 | USD lifecycle | References, reload, `metadataOnly`, `usdchecker`, and plugin discovery from the installed package are verified. | [ADR 0006](../adr/0006-fileformat-contract.md) |
| F-11 | Diagnostics and loss reporting | Stable `ODR-` and `RN-` errors and warnings with element counts, a loss summary on `/Asset`, and the capability matrix updated with evidence. | [ADR 0006](../adr/0006-fileformat-contract.md), [ADR 0002](../adr/0002-usd-schema-strategy.md) |

Explicit limits of Phase 1:

- `<spiral>`, `<poly3>`, and `<paramPoly3>` are errors; the
  [FMT-01 spike](../reports/format-spike-opendrive-2026-11-22.md#5-elements-needed-for-phase-1)
  found that the first fixtures need only line and arc.
- `direct` and `virtual` junctions, lane `<border>`, and the 1.9 lane layers
  are errors ([FMT-01 §3.4](../reports/format-spike-opendrive-2026-11-22.md#34-differences-that-touch-the-phase-1-elements)).
- Signals, road marks, objects, and other out-of-scope elements are not
  authored; their presence is a counted warning. Nothing outside the scope is
  presented as supported.

### Phase 1 exit

- A small `.xodr` fixture yields roads, lanes, and junctions as semantics,
  and its centerlines and lane boundaries can be inspected in `usdview`.
- Generated layers pass `usdchecker`, composition, reload, the installed
  package tests, and the negative fixtures.
- ADRs 0001–0005 are accepted.
- The [format acceptance gate](#format-acceptance-gate) passes for OpenDRIVE,
  the [release gate](../releases/README.md#release-gate) passes for v0.1.0,
  and the capability matrix cites the evidence.

## Phase 2 — OpenDRIVE fidelity

| ID | Feature | Direction |
| --- | --- | --- |
| F-12 | Spiral, poly3, paramPoly3 | A new ADR fixes the evaluation method, numeric tolerance, and golden tests ([ADR 0004 §5](../adr/0004-opendrive-geometry-retention.md#5-other-plan-view-shapes)). |
| F-13 | Cross-section extensions | Superelevation, lateral shape, lane `<border>`, and road marks with stated accuracy. |
| F-14 | Traffic semantics | Signs, signals, speeds, priorities, and other rules, represented so that they can be extended. |
| F-15 | Road objects | Semantics, reference position, and orientation of barriers, guardrails, and similar; mesh generation stays separate. |
| F-16 | Real-world compatibility | Regression tests across versions and producers on a corpus whose redistribution terms are met, such as CARLA. |
| F-27 | `upAxis` file-format argument | `upAxis=Y` in the asset path or `FindOrOpen` arguments presents the asset `Y`-up through a rotation on `/Asset`; no authored value changes ([ADR 0009 §1–§3](../adr/0009-file-format-arguments-up-axis.md#1-the-upaxis-argument)). |

## Phase 3 — Geometry and scale

| ID | Feature | Direction |
| --- | --- | --- |
| F-17 | Surface geometry | Road meshes derived from the semantics; seams, normals, UVs, and materials verified independently. |
| F-18 | LOD | Several visualization accuracies from the same semantics, with quantified error. |
| F-19 | Tiling and spatial partitioning | Geometry load units separate from stable IDs; relationships across tiles keep working. |
| F-20 | Lazy loading and payloads | Adopted only after measuring USD composition and `SdfFileFormat` constraints. |
| F-21 | Large-scale performance tests | Parse, authoring, reload, peak RSS, layer size, and time to display, compared across changes. |
| F-28 | Dynamic payload arguments | Prim metadata on a payload sets `upAxis` through `SdfDynamicFileFormatInterface`, reusing F-27 ([ADR 0009 §4](../adr/0009-file-format-arguments-up-axis.md#4-dynamic-payload-f-28)). |

The constraints that Phase 1 must already respect are listed in
[ADR 0007](../adr/0007-geometry-lod-payload-boundary.md#constraints-for-earlier-phases).

## Phase 4 — Multi-format

| ID | Feature | Direction |
| --- | --- | --- |
| F-22 | Lanelet2 | Adds a boundary representation and regulatory elements to the IR and reuses the shared writer. |
| F-23 | SUMO | Maps the graph and traffic-light control with the differences stated; no forced equivalence of meaning. |
| F-24 | `.osm` routing | Lanelet2 and general OSM are told apart structurally ([ADR 0008](../adr/0008-lanelet2-osm-identification.md)). |

## Phase 5 — Runtime integration

| ID | Feature | Direction |
| --- | --- | --- |
| F-25 | Query API (candidate) | A thin read-only interface over the semantics; the boundary with runtime algorithms is decided first ([backlog](backlog.md#decisions-to-record-later)). |
| F-26 | Runtime composition | Tested with `usd-stage-runner`, `usd-http-resolver`, and a future `usd-mobility-runtime`, each separately ([API boundary](../architecture/API_BOUNDARY.md#repository-ownership)). |

## Format acceptance gate

A format is claimed as supported only when:

- the specification versions, licensing, fixture provenance, and supported
  and rejected variants are documented in a spike report
  ([design policy §4.1](../design/DESIGN_POLICY.md#41-format-acceptance-prerequisites));
- `CanRead()` routing is cheap and structural, and separate from `Read()`
  validation; routing for formats already supported is preserved;
- a format reader and semantic decoder produce `RoadNetworkData`, and the
  stage is authored only through the shared writer;
- coordinate, unit, and georeference policy is documented, and any loss is
  stated and diagnosed;
- malformed and hostile input is bounded and diagnosed; the metadata-only
  behavior is verified;
- deterministic fixtures, numeric geometry tests, and (from the second
  format) cross-format contract tests pass; new fixtures are declared in the
  bundle manifest so source and installed packages are verified with them;
- the capability matrix and the affected mapping and diagnostics references
  cite the implementation evidence, and the
  [release gate](../releases/README.md#release-gate) passes.
