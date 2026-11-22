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

Phase 1 started on 2026-11-22. Its workstreams (IR-01, GEO-01, ODR-01,
ODR-02, USD-01, and REL-01), with their ADRs and exit criteria, are in
[current work](current.md#active-work).

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
