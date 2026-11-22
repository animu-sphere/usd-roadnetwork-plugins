# Design and development policy

- **Status:** Proposal (2026-11-22). Nothing in this document describes
  implemented behavior. Rules marked *open* are settled by the ADRs listed in
  §16 before the corresponding code lands.

This document defines the product intent and engineering policy for
`usd-roadnetwork-plugins`. It governs format selection, semantic conversion,
scene authoring, and verification. Current behavior lives in
[reference/](../reference/); release scope and work order live in the
[roadmap](../roadmap/).

## 1. Project overview

`usd-roadnetwork-plugins` is an OpenStrata (`ost`) project that provides
OpenUSD file-format plugins for road-network formats.

The planned repository identity is:

```text
animu-sphere/usd-roadnetwork-plugins
```

Like [`usd-3dgs-plugins`](https://github.com/animu-sphere/usd-3dgs-plugins),
the project keeps each format's reading in a thin `SdfFileFormat` bundle and
concentrates shared work in a USD-independent model and one shared USD
authoring layer.

**The first format is ASAM OpenDRIVE (`.xodr`).** Names, boundaries, and
verification methods are chosen so that Lanelet2, SUMO, OpenStreetMap, and
other formats can be added later without duplicating the USD writer.

## 2. Vision and purpose

Open road-network formats as OpenUSD layers without an external conversion
step, and reuse them for composition, inspection, visualization, and
simulation.

The governing principles are:

- implement each road-network format as an OpenUSD `SdfFileFormat` plugin;
- separate syntactic parsing, semantic decoding, and USD authoring;
- share a format-independent intermediate representation, `RoadNetworkData`;
- author through one shared USD writer that every format satisfies;
- never discard unrecoverable source information silently;
- keep import read-only; require a separate design for export (§12);
- optimize for semantic fidelity and testability before performance (§13).

### Definition of success

- A `.xodr` file opens through ordinary USD APIs such as
  `SdfLayer::FindOrOpen` and `UsdStage::Open`.
- Roads, lanes, junctions, and their connectivity are available in
  machine-readable form without loss of meaning.
- Differences between formats are normalized explicitly; information that
  cannot be represented is diagnosed, not dropped.
- The result composes with other scenes through ordinary USD references and
  payloads.
- The project passes OpenStrata's build, inspection, and package verification
  flow.
- A second format can be added without duplicating the USD writer.

## 3. Scope and responsibilities

| This repository owns | Delegated to other projects by default |
| --- | --- |
| Detecting, parsing, and validating road-network formats | Running traffic, vehicle, or pedestrian simulation |
| A shared semantic model of roads, lanes, and junctions | Vehicle controllers, AI, route-planning engines |
| A stable mapping into OpenUSD | Renderer-specific drawing |
| Preserving source alignment, boundaries, elevation, and superelevation | Full physics and collision systems |
| Minimal inspection geometry and alignment curves (introduced in stages) | Large-scale road-asset or city generation |
| Provenance, diagnostics, tests, and per-format capability tables | Terrain fusion and runtime world integration |

`usd-mobility-runtime` is a candidate **integration target**. Runtime
composition with `usd-stage-runner`, `usd-http-resolver`, and geospatial
plugins is anticipated, but none of them is a required dependency of this
repository. The [API boundary](../architecture/API_BOUNDARY.md) records
ownership.

## 4. Format strategy

| Priority | Format | Main representation | Introduction policy |
| --- | --- | --- | --- |
| **P0** | ASAM OpenDRIVE `.xodr` | reference line, lane sections, junctions, signals, objects | First implementation. Maintain a per-version support table. |
| **P1** | Lanelet2 `.osm` | lanelet boundaries, regulatory elements | Second candidate, chosen to validate the shared model. |
| **P1–P2** | SUMO `.net.xml` | edges, lanes, connections, traffic-light programs | Validates traffic topology and runtime integration. |
| **P2** | OpenStreetMap `.osm` / `.pbf` | ways, nodes, tags, road graph | Must be distinguished strictly from Lanelet2 (§4.2). |
| **Evaluate** | CityGML Transportation, GDF, others | urban-traffic or navigation models | Individual ADR after demand, specification, and license review. |
| **Separate** | OpenCRG | high-resolution road-surface properties | Not a road network; evaluate as auxiliary data. |

### 4.1 Format acceptance prerequisites

Before implementing a format, document:

- the specification versions in scope and how each is recognized;
- detectable dialects and producers;
- the coordinate system, georeferencing, and units;
- the topology model and how it maps onto `RoadNetworkData`;
- redistributable fixtures and their provenance;
- the supported and rejected scope.

The record is a format spike report in [reports/](../reports/); the release
plan's [format acceptance gate](../roadmap/release-plan.md#format-acceptance-gate)
lists what must be true before a format is claimed.

### 4.2 Shared extensions

**A file extension alone never identifies a format.** Lanelet2 and general
OpenStreetMap both use `.osm`; ADR 0008 defines how they are told apart and
which bundle owns the extension. Ambiguity is resolved by structural
signatures in `CanRead()` (§10.2), never by loosening one bundle until it
accepts the other's files.

## 5. Architecture

```text
.xodr        .osm (Lanelet2)     .net.xml       .osm/.pbf
  |               |                 |               |
  v               v                 v               v
OpenDRIVE       Lanelet2           SUMO             OSM
SdfFileFormat   SdfFileFormat     SdfFileFormat   SdfFileFormat
  |               |                 |               |
  +---------------+-----------------+---------------+
                          |
                 Format-specific readers
                          |
                  Semantic decoders
                          |
                   RoadNetworkData
                (USD-independent IR)
                          |
                  Shared USD writer
                          |
                OpenUSD layer / stage
```

Principles:

1. The `SdfFileFormat` entry point stays thin and contains no parsing logic.
2. Syntactic parsing and semantic conversion are separate responsibilities
   within a bundle.
3. `RoadNetworkData` uses standard C++ types; no USD type appears in its
   public API.
4. There is one USD writer, and every format satisfies the same stage
   contract.
5. When the source meaning cannot be represented, the importer emits a
   diagnostic and keeps the necessary provenance or source-specific metadata.
6. Parsers and SDKs stay inside the bundle that needs them; third-party
   versions and licenses are pinned (§14).

### 5.1 Format reader

Owns syntax: XML (or other container) parsing, element enumeration, scalar
parsing, and the source's own structural errors. Third-party parser types do
not escape the reader.

### 5.2 Semantic decoder

Owns meaning: version recognition, required-element validation, ID resolution,
topology construction, unit and frame conversion, non-finite checks, and
lossy-mapping diagnostics. It produces `RoadNetworkData`.

### 5.3 Intermediate representation

The conceptual model is settled by ADR 0001:

```text
RoadNetworkData
├── metadata / source provenance / CRS
├── roads
│   ├── reference geometry (line / arc / spiral / paramPoly3 ...)
│   ├── elevation / superelevation / lateral profile
│   └── lane sections
├── lanes
│   ├── boundary / width / type / direction
│   └── predecessor / successor / adjacency
├── junctions / connections / lane links
├── traffic controls / regulatory rules
├── signals / signs
├── road objects (e.g. barriers, guardrails)
└── coordinate transforms / source IDs
```

*Open (ADR 0004):* whether OpenDRIVE's analytic curves and cross-section
functions are kept in the IR or converted to sampled geometry. The initial
direction is **to keep analytic information where possible** and treat
discretization for visualization and LOD as a separate step. Any lossy
normalization is explicit.

### 5.4 Shared USD writer

Owns prim and schema creation, relationships, attribute values and types,
layer and asset metadata, `defaultPrim`, deterministic path naming, and
writer-side diagnostics. Diagnostic codes for format-specific failures stay
owned by the calling bundle (§11).

## 6. USD representation

### 6.1 Separate data from visualization

- **Semantic layer (required):** stable IDs; road, lane, and junction
  relationships; speed, priority, and traffic rules; source parameters.
- **Visualization layer (introduced in stages):** centerlines, lane
  boundaries, and road surfaces as `UsdGeomBasisCurves`, `UsdGeomMesh`, or
  similar.
- **Runtime layer (out of scope):** driving, walking, collision, and traffic
  flow belong to integration projects.

Standard USD schemas are preferred wherever they can express the data.
Standard `UsdGeom` types alone cannot carry road-network topology and
semantics, so *ADR 0002 evaluates* whether to introduce custom typed or API
schemas. **No schema is adopted before its specification is settled.**

### 6.2 Stage contract (proposed)

```text
/Asset                             Xform / defaultPrim
  /Network                         semantic root
    /Roads
    /Lanes
    /Junctions
    /Signals
    /Objects
  /Visualization                   optional generated geometry
    /Centerlines
    /Boundaries
    /Surfaces
```

This is a **design example, not a fixed schema.** ADR 0005 settles source-ID to
prim-path mapping, name collisions, relationship direction and typing, and how
multiple road networks compose. Naming is deterministic so that reloads and
new formats do not move paths unnecessarily. The binding contract will live in
[WORKSPACE.md §5](../architecture/WORKSPACE.md#5-authored-stage-contract).

### 6.3 Coordinates, units, and georeferencing

Settled by ADR 0003. Policy:

- Preserve the source CRS, georeference, origin, axis directions, and units
  as far as the source provides them.
- Never treat an unknown CRS as known. Default `upAxis` and `metersPerUnit`
  values are documented as **importer defaults**, not source-derived claims.
- Avoid float precision loss on large geographic coordinates; evaluate local
  frames and origin shifts.
- Make the pre- and post-normalization transform reproducible; test the
  convention and its numeric tolerances.
- Treat georeference-versus-scene transforms and consistency across multiple
  referenced files as design problems, not implementation details.

## 7. OpenDRIVE initial scope

### 7.1 Minimum useful implementation (MVP)

1. `.xodr` detection, XML parsing, specification-version recognition, and
   stable errors.
2. Road geometry: line and arc as the first vertical slice, then spiral,
   poly3, and paramPoly3 in stages.
3. Lane sections, lane width, lane type, road and lane links, and junctions.
4. A defined policy for elevation, superelevation, and other profiles, which
   are not silently omitted.
5. An inspectable USD representation of centerlines and lane boundaries.
6. Verification of `UsdStage::Open`, references, reload, `metadataOnly`, and
   packaged distribution.

### 7.2 After the MVP

Signals, road marks, objects (guardrails, obstacles), advanced junctions,
surface meshes, LOD and tiling, and partial loading. **A feature is not
described as supported until it is implemented and tested.**

The MVP stays small, but a **silent fallback that only looks right is
forbidden** wherever road, lane, or junction relationships would break.

The per-version mapping is maintained in
[OPENDRIVE_MAPPING.md](../reference/OPENDRIVE_MAPPING.md).

## 8. Naming

Bundle identities are format-oriented and share a family prefix:

```text
roadnetwork-opendrive
roadnetwork-lanelet2      (future)
roadnetwork-sumo          (future)
```

Shared libraries live in `libs/roadnetwork-core` and `libs/roadnetwork-usd`.
Their CMake/OpenStrata identities and the C++ namespaces are fixed during the
Phase 0 scaffold; the proposed defaults, following `usd-3dgs-plugins`, are
`roadnetworkCore` / `roadnetworkUsd` and `openstrata::rn`,
`openstrata::rn::odr`, `openstrata::rn::usd`.

## 9. Strictness

The load fails when the structure needed to build a correct network is
missing or inconsistent: malformed XML, unknown or duplicated IDs, unresolved
references, non-finite values, or a representation the decoder does not
support. The load may continue with a warning only when the loss is
explicit, bounded, and recorded in the authored stage or diagnostics, for
example an ignored optional element. ADR 0006 fixes the exact error contract.

## 10. File-format plugin contract

Settled by ADR 0006.

### 10.1 Read-only first

Exporting to the source format (`WriteToFile`) is out of initial scope.
`WriteToString` may expose the imported layer as USDA for inspection and
testing; it is not export. See §12.

### 10.2 `CanRead()` is a cheap routing decision

It checks the extension and the minimal structural signature (for example the
XML root element) and reads no more than needed. Full validation belongs to
`Read()`. **`CanRead()` returning `true` does not mean the asset is valid.**
Any failure inside `CanRead()` returns `false` rather than propagating.
OpenUSD selects a layer's format by extension and does not call `CanRead()`
when it opens a layer, so the routing decision serves tools; `Read()` repeats
the structural check ([ADR 0006](../adr/0006-fileformat-contract.md#1-canread)).

### 10.3 Respect `metadataOnly`

A metadata-only read returns header-derived information and the minimal
structure, and avoids generating geometry for the whole network.

### 10.4 Explicit failure

Malformed XML, missing IDs, non-finite values, reference inconsistencies, and
unsupported representations each receive a stable diagnostic code (§11).

### 10.5 Determinism

The same input with the same settings yields the same IDs, relationships, and
attributes.

### 10.6 Reload consistency

Verify OpenUSD's `Sdf` change handling and `TransferContent` behavior on the
supported runtime. **Do not copy the `usd-3dgs-plugins` worker-thread
workaround unconditionally;** adopt it only if a test reproduces the problem
it solves here. ADR 0006 avoids the problem instead: the shared writer
authors Sdf specs only, which work inside the reload's `SdfChangeBlock`
([§5 of the ADR](../adr/0006-fileformat-contract.md#5-reload-and-authoring-thread)).

## 11. Diagnostics

Every error and warning has a stable code. Codes are never renumbered or
reused, and each bundle ships a machine-readable catalog. **The numbering
scheme is fixed before implementation**; the proposal is in
[DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) and is ratified by ADR 0006.

## 12. Write support

The plugins are read-only. Export to OpenDRIVE or any other road-network
format requires a separate ADR that defines losslessness, version targeting,
and compatibility before implementation. Full lossless cross-conversion
between OpenDRIVE, Lanelet2, and SUMO is a non-goal (§18).

## 13. Performance and large data

Semantic precision comes first; do not add complex optimization before
measuring.

- Metrics: source size, road and lane counts, parse and authoring time, peak
  RSS, generated layer size, reload time.
- Centerlines and surface meshes are derived data and should be regenerable.
- **LOD, tiling, streaming, and lazy loading** are designed (ADR 0007) only
  after load units, topology boundaries, and USD composition constraints are
  understood.
- `UsdProcGenerativeProcedural`, Hydra, and runtime geometry generation are
  exploration topics, not MVP assumptions.
- Remote assets through an HTTP resolver require relative-reference,
  security, cache, and failure behavior to be defined first.

## 14. Dependency policy

- Pin every third-party library to an exact version or commit.
- Record its license and provenance in
  [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) and keep upstream
  license text.
- Keep parsers private to the bundle that needs them; shared libraries depend
  on no format parser.
- An update re-runs parser, integration, OpenStrata, and package
  verification.

## 15. Test strategy

1. **Parser and IR unit tests:** curves, lane width, cross-sections,
   connectivity, XML errors, unknown IDs, boundary values.
2. **Geometry numeric tests:** positions and tangents of lines, arcs, and
   spirals; boundaries; elevation; superelevation — against known solutions.
3. **USD authoring tests:** prim paths, metadata, relationships, types,
   values, `defaultPrim`, coordinate conventions.
4. **File-format integration tests:** `SdfLayer::FindOrOpen`,
   `UsdStage::Open`, references and reload, `metadataOnly`.
5. **OpenStrata lifecycle and package tests:** plugin discovery after install,
   runtime linking, missing-dependency checks.
6. **Cross-format contract tests (from the second format):** read the same
   road semantics from different formats and verify equality, and the
   unavoidable differences, at the IR and USD level.

The supported lifecycle is:

```sh
ost plugin inspect
ost plugin build
ost plugin doctor
ost plugin test --workspace --up-to 5
ost plugin package
ost plugin test --from-package
```

Confirm each command against the `ost` version in use. Plain CMake/CTest
builds and unit tests stay supported for local work.

**Golden-test policy:** golden outputs check stable stage semantics, not
incidental formatting. Prefer semantic assertions over full USDA text
comparisons.

### 15.1 Test data

Fixtures are small, deterministic, redistributable after license review, and
hand-checkable. Large real-world networks may extend a compatibility and
performance corpus after license and provenance review; they are never the
only evidence for a claim.

## 16. Decisions to record first

| ADR | Question |
| --- | --- |
| [0001](../adr/0001-roadnetwork-data-model.md) | `RoadNetworkData` ID, topology, and geometry model |
| [0002](../adr/0002-usd-schema-strategy.md) | Existing schemas with attributes and relationships, or a custom schema |
| [0003](../adr/0003-crs-axes-units.md) | CRS, `upAxis`, units, and large coordinates |
| [0004](../adr/0004-opendrive-geometry-retention.md) | Retaining OpenDRIVE curves and cross-sections; tessellation conventions |
| [0005](../adr/0005-prim-path-and-id-stability.md) | Prim-path and source-ID stability; composing multiple networks |
| [0006](../adr/0006-fileformat-contract.md) | `SdfFileFormat` `metadataOnly`, reload, error contract, and diagnostic numbering |
| [0007](../adr/0007-geometry-lod-payload-boundary.md) | Geometry generation, LOD and tiles, payload boundaries |
| [0008](../adr/0008-lanelet2-osm-identification.md) | Distinguishing Lanelet2 from OSM; sharing regulatory information |

## 17. Workspace structure

The [workspace contract](../architecture/WORKSPACE.md) owns the source layout,
dependency directions, and packaging boundaries. Keep format code in its
bundle, USD-independent shared code in `roadnetwork-core`, and shared OpenUSD
authoring in `roadnetwork-usd`.

## 18. Initial non-goals

- Covering every OpenDRIVE version and element in the first release.
- Fully lossless conversion between OpenDRIVE, Lanelet2, and SUMO.
- Executing traffic-signal control, traffic-flow simulation, or vehicle
  physics.
- Requiring a road mesh to be generated and displayable for an `SdfFileFormat`
  read to succeed.
- Fixing a custom schema or renderer first and bending format semantics to
  fit it.

## 19. One-sentence policy

*Start with OpenDRIVE; build on a shared IR that preserves road-network
meaning and a stable OpenUSD representation; and deliver format additions,
USD composition, and verifiable distribution through OpenStrata.*
