# Design and development policy

- **Status:** Proposal (2026-11-22; revised the same day to take in the
  feature and implementation-order policy). This document states intent and
  rules, not behavior. Where an ADR decides a question, the ADR is the rule
  and this document only links to it.

This document owns the product intent and the engineering rules of
`usd-roadnetwork-plugins`: why the project exists, what it will and will not
do, and the principles that format selection, decoding, USD authoring, and
verification follow. It deliberately does not repeat material owned
elsewhere:

| For | See |
| --- | --- |
| What the tree supports now | [Capability matrix](../reference/CAPABILITY_MATRIX.md) |
| Phases, the feature list, and exit criteria | [Release plan](../roadmap/release-plan.md) |
| Components, dependency directions, and the stage contract | [Workspace contract](../architecture/WORKSPACE.md) |
| Who owns which API and compatibility promise | [API boundary](../architecture/API_BOUNDARY.md) |
| Individual decisions and their status | [ADR index](../adr/README.md) |
| Build, test, and review procedure | [CONTRIBUTING.md](../../CONTRIBUTING.md) |

## 1. Project overview

`usd-roadnetwork-plugins` is an OpenStrata (`ost`) project that provides
OpenUSD file-format plugins for road-network formats. Like
[`usd-3dgs-plugins`](https://github.com/animu-sphere/usd-3dgs-plugins), it
keeps each format's reading in a thin `SdfFileFormat` bundle and concentrates
shared work in a USD-independent model and one shared USD authoring library.

**The first format is ASAM OpenDRIVE (`.xodr`).** Names, boundaries, and
verification methods are chosen so that Lanelet2, SUMO, OpenStreetMap, and
other formats can be added later without duplicating the USD writer.

## 2. Vision and purpose

Treat a road network not as a road mesh but as a **semantic asset** that
OpenUSD can compose, inspect, visualize, and reuse, opened directly from its
source format without an external conversion step.

### Principles

1. **Semantic first.** Roads, lanes, connectivity, and rules are represented
   correctly before anything is drawn. An import whose geometry looks right
   but whose topology is wrong is a failure.
2. **Read-only first.** Import comes first; export to a source format needs
   its own ADR (§12).
3. **One model, one writer.** Every format decodes into `RoadNetworkData`,
   and one shared writer authors every stage. No bundle authors prims
   directly.
4. **USD-independent core.** The public API of `roadnetwork-core` uses
   standard C++17 types only; no USD or parser type appears in it.
5. **Source fidelity and explicit loss.** Analytic geometry, source IDs,
   frames, and units are kept. What cannot be represented is diagnosed,
   never silently approximated or dropped.
6. **Stable contract.** Prim paths, property names, relationships, and
   diagnostic codes are public behavior and change only with compatibility
   review ([API boundary](../architecture/API_BOUNDARY.md#tiers)).
7. **Test before claiming support.** A capability is claimed only with
   deterministic tests and package verification
   ([capability matrix](../reference/CAPABILITY_MATRIX.md)).
8. **Optimize after measuring.** LOD, tiling, and streaming are added only
   when measurements show the need (§13).

### Definition of success

- A `.xodr` file opens through ordinary USD APIs such as
  `SdfLayer::FindOrOpen` and `UsdStage::Open`, with no conversion step.
- Roads, lanes, junctions, connectivity, the coordinate reference, and the
  source provenance are available as machine-readable, verifiable meaning.
- Visual geometry (curves, boundaries, surfaces) is regenerated from the
  semantics and is never their only carrier.
- The asset composes with other scenes through references and payloads
  without breaking relationships or stable IDs.
- A second format is added without duplicating the USD writer.
- Supported scope, rejections, information loss, and performance are
  explained by tests and the capability matrix.
- The project passes OpenStrata's build, inspection, and package
  verification flow.

## 3. Scope and responsibilities

| This repository owns | Delegated to other projects by default |
| --- | --- |
| Detecting, parsing, and validating road-network formats | Running traffic, vehicle, or pedestrian simulation |
| A shared semantic model of roads, lanes, and junctions | Vehicle controllers, AI, route-planning engines |
| A stable mapping into OpenUSD | Renderer-specific drawing |
| Preserving source alignment, boundaries, elevation, and superelevation | Full physics and collision systems |
| Minimal inspection geometry and alignment curves (introduced in stages) | Large-scale road-asset or city generation |
| Provenance, diagnostics, tests, and per-format capability tables | Terrain fusion and runtime world integration |
| The **data contract** that runtimes consume | The runtime algorithms that consume it |

`usd-mobility-runtime` is a candidate **integration target**; none of the
runtime projects is a dependency of this repository. The
[API boundary](../architecture/API_BOUNDARY.md) records which repository owns
what and the contract notes for each integration.

## 4. Format strategy

| Priority | Format | Main representation | Introduction policy |
| --- | --- | --- | --- |
| **P0** | ASAM OpenDRIVE `.xodr` | reference line, lane sections, junctions, signals, objects | First implementation. Maintain a per-version support table. |
| **P1** | Lanelet2 `.osm` | lanelet boundaries, regulatory elements | Second candidate, chosen to validate the shared model. |
| **P1–P2** | SUMO `.net.xml` | edges, lanes, connections, traffic-light programs | Validates traffic topology and runtime integration. Differences in meaning are stated, not forced into OpenDRIVE's. |
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
signatures, never by loosening one bundle until it accepts the other's files.

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
       OpenUSD layer: /Asset/Network (required)
                      /Asset/Visualization (derived)
```

Principles:

1. The `SdfFileFormat` entry point stays thin and contains no parsing logic.
2. Syntactic parsing and semantic conversion are separate responsibilities
   within a bundle.
3. There is one USD writer, and every format satisfies the same stage
   contract.
4. When the source meaning cannot be represented, the importer emits a
   diagnostic rather than inventing a value.
5. Parsers and SDKs stay inside the bundle that needs them; third-party
   versions and licenses are pinned (§14).

What each component owns, and what it must not take on, is fixed in the
[workspace contract §1](../architecture/WORKSPACE.md#1-components).

### 5.1 Format reader

Owns syntax: XML (or other container) parsing, element enumeration, scalar
parsing, and the source's own structural errors. Third-party parser types do
not escape the reader.

### 5.2 Semantic decoder

Owns meaning: version recognition, required-element checks, ID resolution,
topology construction, unit and frame conversion, and lossy-mapping
diagnostics. It produces `RoadNetworkData` and leaves the invariants that can
be stated on the IR to shared validation
([ADR 0001 §6](../adr/0001-roadnetwork-data-model.md#6-validation)).

### 5.3 Intermediate representation

Conceptually the IR has five concerns: **identity**, **geometry**,
**topology**, **semantics** (types, speeds, rules), and **provenance**.
[ADR 0001](../adr/0001-roadnetwork-data-model.md) proposes a layered model
(a common identity and lane-graph layer plus attached geometry
representations); the concerns are not split into separate types beyond
what Phase 1 needs. Rules that hold regardless of that ADR's details:

- Do not force one format's geometry model onto another: OpenDRIVE's
  reference lines are not imposed on Lanelet2's boundary polylines, nor the
  reverse. A new representation is **added** beside the existing ones; it
  does not replace them.
- Connectivity is stored as its own data. It is never inferred from curves,
  meshes, or geometric contact.
- A lane-end link has no direction and does not mean "drivable from A to B".
  Travel direction and routability are separate, later decisions
  ([backlog](../roadmap/backlog.md#decisions-to-record-later)).
- Analytic information is kept where possible; discretization for
  visualization and LOD is a separate, explicit step
  ([ADR 0004](../adr/0004-opendrive-geometry-retention.md)).
- There is no untyped metadata bag. A new representation enters through an
  ADR with its specification, validation, and compatibility.

### 5.4 Shared USD writer

Owns prim and property authoring, relationships, value types, layer and
asset metadata, `defaultPrim`, deterministic path naming, and provenance. It
contains no XML parsing and no geometry math of its own; derived geometry
comes from `roadnetwork-core`.

## 6. USD representation

### 6.1 Separate data from visualization

- **Semantic layer (required):** stable IDs; road, lane, and junction
  relationships; speeds, priorities, and rules; source parameters in
  `double` precision.
- **Visualization layer (derived):** centerlines, lane boundaries, and later
  road surfaces as `UsdGeomBasisCurves`, `UsdGeomMesh`, or similar.
- **Runtime layer (out of scope):** driving, walking, collision, and traffic
  flow belong to integration projects.

Standard USD prims are preferred wherever they can express the data.
Standard `UsdGeom` types alone cannot carry road-network topology, so
[ADR 0002](../adr/0002-usd-schema-strategy.md) decides how semantics are
typed. Policy for that decision:

- **No schema is adopted before its specification is settled.** A
  project-specific schema is considered only once a second format has
  stabilized the shared vocabulary.
- The first candidate is an applied API schema that keeps the already
  authored property names and types, so that adopting it re-types prims
  without changing values. A typed (IsA) schema waits until its benefit is
  shown.
- Until then, the name, type, meaning, and absence behavior of every
  property are tested, which keeps a later migration mechanical.

### 6.2 Stage contract

The invariants every bundle's stage satisfies are owned by the
[workspace contract §5](../architecture/WORKSPACE.md#5-authored-stage-contract);
the proposed prim tree and naming rules are in
[ADR 0005](../adr/0005-prim-path-and-id-stability.md#3-hierarchy). Naming is
deterministic so that reloads and new formats do not move paths, and every
relationship stays inside the asset so that it is remapped under references.

### 6.3 Coordinates, units, and georeferencing

[ADR 0003](../adr/0003-crs-axes-units.md) proposes the rules. Policy:

- Preserve the source CRS, georeference, origin, axis directions, and units
  as far as the source provides them. Never reproject during import.
- Never treat an unknown CRS as known. Distinguish **source facts** from
  **importer defaults**: each format's mapping states which `upAxis` and
  `metersPerUnit` values its specification defines and which are importer
  conventions.
- Keep semantic coordinates in `double`. Where USD stores single-precision
  values (visualization `points`), use a recorded local origin so that large
  world coordinates do not lose precision.
- Make every transform between source and authored values reproducible, and
  test the convention and its tolerances.

## 7. OpenDRIVE initial scope

### 7.1 Minimum useful implementation (MVP)

The MVP is Phase 1; its features (F-01 to F-11) and acceptance criteria are
listed in the [release plan](../roadmap/release-plan.md#phase-1--opendrive-vertical-slice).
Rules for it:

- The MVP stays small. Scope beyond it (LOD, schemas, query APIs) never
  blocks it.
- Elevation, superelevation, and other profiles have a defined policy; none
  is silently omitted.
- Representations outside the scope (for example spiral, poly3, and
  paramPoly3 in Phase 1) are rejected with a stable error, not approximated.

### 7.2 After the MVP

**A feature is not described as supported until it is implemented and
tested.** The MVP may be narrow, but a **silent fallback that only looks
right is forbidden** wherever road, lane, or junction relationships, or the
position of a border, would be wrong. Elements read but not represented are
counted and reported. The per-element mapping is maintained in
[OPENDRIVE_MAPPING.md](../reference/OPENDRIVE_MAPPING.md).

## 8. Naming

Bundle identities are format-oriented and share the `roadnetwork-` family
prefix (`roadnetwork-opendrive`, later `roadnetwork-lanelet2`,
`roadnetwork-sumo`). The library identities, CMake packages, and targets are
fixed in the [workspace contract §2](../architecture/WORKSPACE.md#2-dependency-directions);
C++ code lives in `openstrata::rn` and its `odr` and `usd` sub-namespaces.

## 9. Strictness

The load fails when the structure needed to build a correct network is
missing or inconsistent: malformed XML, unknown or duplicated IDs, unresolved
references, non-finite values, or a representation the decoder does not
support. The load may continue with a warning only when the loss is
explicit, bounded, and recorded in the authored stage or diagnostics, for
example an ignored optional element.

Input is treated as untrusted: malformed or very large XML, external
entities, reference cycles, unresolved IDs, and extreme values are expected.
Each reader documents, for the parser it uses, how external references are
disabled and which size, depth, and time limits apply; limits that are hit
fail the load with a stable code (category `5xx` in the
[diagnostics reference](../reference/DIAGNOSTICS.md)).

## 10. File-format plugin contract

[ADR 0006](../adr/0006-fileformat-contract.md) is accepted and is the
normative contract; the subsections state the intent behind it.

### 10.1 Read-only first

Exporting to the source format is out of scope (§12). `WriteToString` may
expose the imported layer as USDA for inspection and testing; it is not
export.

### 10.2 `CanRead()` is a cheap routing decision

It checks the extension and a minimal structural signature and reads no more
than needed; full validation belongs to `Read()`. **`CanRead()` returning
`true` does not mean the asset is valid.** OpenUSD does not call `CanRead()`
when it opens a layer, so the routing decision is advice to tools, and
`Read()` repeats the structural check
([ADR 0006 §1](../adr/0006-fileformat-contract.md#1-canread)).

### 10.3 Respect `metadataOnly`

A metadata-only read returns header-derived information and minimal
structure, and generates no geometry for the whole network.

### 10.4 Explicit failure

Every failure has a stable diagnostic code (§11) and leaves the target layer
unchanged.

### 10.5 Determinism

The same input with the same settings yields the same prim paths, values,
and relationships. No environment-dependent value, such as an absolute local
path, is authored into the stage.

### 10.6 Reload consistency

Reload behavior is verified on the supported runtime rather than assumed.
The `usd-3dgs-plugins` worker-thread workaround is not copied: the shared
writer authors Sdf specs only, which work inside the reload's
`SdfChangeBlock`
([ADR 0006 §5](../adr/0006-fileformat-contract.md#5-reload-and-authoring-thread)).

## 11. Diagnostics

Every error and warning has a stable code: `ODR-` (and later other format
prefixes) for format-specific conditions, `RN-` for conditions stated on the
shared model. Codes are never renumbered or reused, and each owner ships a
machine-readable catalog. The numbering is fixed by
[ADR 0006 §6](../adr/0006-fileformat-contract.md#6-diagnostic-numbering);
the codes and their rules are in [DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).
A failure message identifies enough (asset, element, ID) to find the cause.

## 12. Write support

The plugins are read-only. Export to OpenDRIVE or any other road-network
format requires a separate ADR that defines losslessness, version targeting,
and compatibility before implementation. Full lossless cross-conversion
between OpenDRIVE, Lanelet2, and SUMO is a non-goal (§18).

## 13. Performance and large data

Semantic precision comes first; do not add complex optimization before
measuring.

- Metrics: source size, road and lane counts, parse and authoring time, peak
  RSS, generated layer size, reload time, and time until the stage can be
  displayed.
- Centerlines and surface meshes are derived data and must be regenerable.
- **LOD, tiling, streaming, and lazy loading** are designed in
  [ADR 0007](../adr/0007-geometry-lod-payload-boundary.md), which also lists
  the constraints that earlier phases must not break.
- `UsdProcGenerativeProcedural`, Hydra, and runtime geometry generation are
  research topics and never a dependency of a basic read.
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

| Kind | Checks | Required from |
| --- | --- | --- |
| Core unit | IDs, references, numeric and station checks; canonicalization of one-sided lane links | Phase 1 |
| Geometry numeric | Line and arc position and heading against closed-form values; border positions; sampling error | Phase 1 |
| Reader and decoder | Versions, malformed input, unknown elements, each rejection and its code | Phase 1 |
| USD semantic | Prim paths, property names, types and precision, relationships, `/Asset` provenance | Phase 1 |
| Composition | Two references of one network, flattening, reload, `metadataOnly` | Phase 1 |
| Packaging | Plugin discovery and opening a real fixture **from the installed package** | Phase 1 |
| Profile fidelity | Spiral, superelevation, and other Phase 2 shapes against reference values and rejection rules | Phase 2 |
| Performance | Parse, author, peak RSS, reload, and layer size on a large licensed network | Phase 3 |
| Cross-format contract | The same meaning, and the stated differences, read from two formats at the IR and USD level | Phase 4 |
| Runtime contract | Independent deployment, dependencies, and coordinate agreement with an integrating runtime | Phase 5 |

The commands that run these tests are in
[CONTRIBUTING.md](../../CONTRIBUTING.md#development-path).

**Golden-test policy:** goldens check stable stage semantics, not incidental
formatting. Prefer semantic assertions (paths, types, values, relationship
targets) over full USDA text comparisons.

### 15.1 Test data

Fixtures are small, deterministic, redistributable after license review, and
hand-checkable. Large real-world networks may extend a compatibility and
performance corpus after license and provenance review; they are never the
only evidence for a claim.

## 16. Decisions

Decisions are recorded as ADRs; the [ADR index](../adr/README.md) owns their
list, status, and the phase that needs each. Questions not yet ready for an
ADR are in the [backlog](../roadmap/backlog.md#decisions-to-record-later). A
proposal that differs from an existing ADR becomes a new ADR; it never
changes a decision by being written down elsewhere.

## 17. Workspace structure

The [workspace contract](../architecture/WORKSPACE.md) owns the source layout,
dependency directions, and packaging boundaries. Keep format code in its
bundle, USD-independent shared code in `roadnetwork-core`, and shared OpenUSD
authoring in `roadnetwork-usd`.

## 18. Initial non-goals

- Vehicle control, pedestrian AI, traffic-flow simulation, route-planning
  engines, collision physics, city generation, and renderer-specific
  features.
- Covering every OpenDRIVE version and element in the first release.
- Fully lossless conversion between OpenDRIVE, Lanelet2, and SUMO.
- Requiring a road mesh to be generated and displayable for an `SdfFileFormat`
  read to succeed.
- Fixing a custom schema or renderer first and bending format semantics to
  fit it.

## 19. One-sentence policy

*Start with OpenDRIVE; build on a shared IR that preserves road-network
meaning and a stable OpenUSD representation; and deliver format additions,
USD composition, and verifiable distribution through OpenStrata.*
