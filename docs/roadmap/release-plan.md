# Release plan

Work is organized in phases. A version number is assigned when a phase is
scheduled for release; the first tagged release is planned at the end of
Phase 1.

| Phase | Goal | Exit criteria | Version |
| --- | --- | --- | --- |
| **0 — Scaffold / ADR** | OpenStrata workspace, basic CI, initial decisions | An empty `.xodr` bundle and the core and USD libraries pass the package test; ADR 0006 numbering is accepted. | untagged |
| **1 — OpenDRIVE vertical slice** | Simple roads, lanes, and junctions into USD | The meaning and geometry of small fixtures are verifiable; ADRs 0001–0005 are accepted. | v0.1.0 (planned) |
| **2 — OpenDRIVE fidelity** | Wider coverage of curves, cross-sections, connectivity, signals, and objects | Capability matrix, diagnostics, golden semantics, and compatibility tests are complete for the declared scope. | unassigned |
| **3 — Geometry / scale** | Boundaries, surfaces, LOD and tiling design and tests | Measured performance and quality metrics can be published; ADR 0007 is accepted. | unassigned |
| **4 — Multi-format** | Add Lanelet2 or SUMO | The same IR and USD writer are reused and the differences are explained; ADR 0008 is accepted for Lanelet2. | unassigned |
| **5 — Runtime integration** | Composition checks in `usd-mobility-runtime` | Deployable independently of other USD assets and runtimes. | unassigned |

This repository owns import and USD authoring. See the
[API boundary](../architecture/API_BOUNDARY.md) for integrated runtime
responsibilities. [Current work](current.md) tracks the active phase.

## Phase 1 — OpenDRIVE vertical slice

Scope, from [design policy §7.1](../design/DESIGN_POLICY.md#71-minimum-useful-implementation-mvp):

1. Detection, XML parsing, version recognition, and stable errors.
2. Line and arc plan-view geometry.
3. Lane sections, lane offset, width, type, road and lane links, and
   `default` junctions.
4. A defined policy for elevation and superelevation.
5. Inspectable centerlines and lane boundaries in USD.
6. `UsdStage::Open`, references, reload, `metadataOnly`, and package
   verification.

Spiral, poly3, and paramPoly3 follow in Phase 2: the
[FMT-01 spike](../reports/format-spike-opendrive-2026-11-22.md#5-elements-needed-for-phase-1)
found that the first fixtures need only line and arc. The same spike puts
lane offset in scope, because the real-world files examined use it on almost
every road, and proposes accepting `revMajor="1"` with `revMinor` 4–9.

### Phase 1 workstreams

These workstreams are proposed. Each names the ADRs whose remaining evidence
it produces. They move to [current work](current.md) when Phase 1 starts. An
ADR is accepted in the workstream that produces the last of its evidence, or
earlier if review finds the evidence sufficient.

| ID | Work | Component | ADRs | Exit criterion |
| --- | --- | --- | --- | --- |
| IR-01 | `RoadNetworkData` types and validation | `roadnetwork-core` | [0001](../adr/0001-roadnetwork-data-model.md) | The ADR 0001 types replace the placeholder. Each validation invariant fails a unit test with its `RN-` code. |
| GEO-01 | Line, arc, and cubic evaluation; lane borders; tessellation | `roadnetwork-core` | [0004](../adr/0004-opendrive-geometry-retention.md) | Closed-form position and heading tests pass, and sampled borders stay within the tolerance on a hand-authored fixture. |
| ODR-01 | Reader: pugixml 1.16, the `CanRead()` sniff, version recognition, and the metadata-only header scan | `roadnetwork-opendrive` | [0006](../adr/0006-fileformat-contract.md) | The prologue, version, and malformed-input fixtures pass with stable `ODR-` codes, and a failed reload keeps the previous content. |
| ODR-02 | Decoder: roads, lane sections, lanes, junctions, and profiles | `roadnetwork-opendrive` | [0001](../adr/0001-roadnetwork-data-model.md), [0003](../adr/0003-crs-axes-units.md), [0004](../adr/0004-opendrive-geometry-retention.md) | The hand-authored fixtures and CARLA `Town01` and `Town02` decode and validate, and each Phase 1 rejection has a negative fixture. |
| USD-01 | Stage contract and the shared writer | `roadnetwork-usd` | [0002](../adr/0002-usd-schema-strategy.md), [0003](../adr/0003-crs-axes-units.md), [0005](../adr/0005-prim-path-and-id-stability.md) | Generated layers pass `usdchecker` and semantic assertions. Paths are stable across reloads, and two referenced networks keep their own relationships. |
| REL-01 | First release | all | — | The [release gate](../releases/README.md#release-gate) passes for v0.1.0. |

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
