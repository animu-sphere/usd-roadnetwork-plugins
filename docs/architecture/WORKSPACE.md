# Workspace contract

- **Status:** Proposed (2026-11-22). The workspace root and the
  `roadnetwork-opendrive` bundle were scaffolded with `ost` 0.23.20
  (`usd-plugin-workspace` 1.1.1, `usd-fileformat-cpp` 1.2.1); the paths below
  match that output. The libraries are added in SCF-02, after which this
  document becomes binding.

This is the structural contract for `usd-roadnetwork-plugins`. It fixes
component identities, dependency directions, root responsibilities, and the
authored stage contract. A structural change that contradicts this document
must change this document first.

See the [capability matrix](../reference/CAPABILITY_MATRIX.md) for what the
components support.

## 1. Components

| Identity | Kind | Responsibility | State |
| --- | --- | --- | --- |
| `roadnetwork-opendrive` | OpenStrata plugin bundle (`usd-fileformat`) | Detect and parse ASAM OpenDRIVE `.xodr`; decode it into `RoadNetworkData`. | scaffolded: registers `.xodr` and authors an empty layer |
| `roadnetwork-lanelet2` | plugin bundle | Lanelet2 `.osm`. | future (Phase 4 candidate) |
| `roadnetwork-sumo` | plugin bundle | SUMO `.net.xml`. | future (Phase 4 candidate) |
| `roadnetwork-core` | plain CMake/OpenStrata static library | `RoadNetworkData`, validation, shared geometry math, diagnostics helpers, and a decoder test kit. No OpenUSD types. | planned |
| `roadnetwork-usd` | plain CMake/OpenStrata static library | Author `RoadNetworkData` into the stage contract (§5). Diagnostic codes stay owned by the calling bundle. | planned |

`roadnetwork-core` is not a plugin: it has no `plugInfo.json`, performs no
plugin registration, and exposes no OpenUSD types in its public API.

## 2. Dependency directions

Allowed:

```text
roadnetwork-opendrive -> roadnetwork-core
roadnetwork-opendrive -> roadnetwork-usd
roadnetwork-opendrive -> <XML parser> (private, pinned; selected by FMT-01)
roadnetwork-usd       -> roadnetwork-core
roadnetwork-usd       -> OpenUSD
```

Forbidden:

```text
roadnetwork-core -> any plugin bundle
roadnetwork-core -> OpenUSD
roadnetwork-core -> any format parser
roadnetwork-usd  -> any plugin bundle
roadnetwork-usd  -> any format parser
<bundle>         -> another format bundle
any dependency cycle
```

Libraries declare an OpenStrata descriptor so that bundles can name them in
`requires.libraries` and `ost plugin build/test/package` can resolve them.

## 3. Source boundaries (proposed)

```text
plugins/roadnetwork-opendrive/src/RoadnetworkOpendriveFileFormat.*
    thin SdfFileFormat integration (scaffolded)

plugins/roadnetwork-opendrive/src/io/OpenDriveReader.*
    XML parser isolation; element and scalar parsing

plugins/roadnetwork-opendrive/src/io/OpenDriveDecoder.*
    version recognition, ID resolution, topology, geometry and profile
    decoding into RoadNetworkData; diagnostics

libs/roadnetwork-core/include/openstrata/rn/...
    RoadNetworkData, validation, geometry evaluation, diagnostics helpers

libs/roadnetwork-usd/include/openstrata/rn/usd/...
    shared layer writer
```

## 4. Repository layout

```text
usd-roadnetwork-plugins/
├── README.md
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── openstrata.toml                 # project manifest (platform cy2026, profile usd)
├── openstrata.scaffold.yaml        # scaffold template provenance
├── CMakeLists.txt                  # dual-mode root: plain CMake builds every bundle
├── CMakePresets.json
├── .gitattributes                  # LF for sources, docs, and XML fixtures
├── plugins/
│   ├── roadnetwork-opendrive/
│   │   ├── openstrata.plugin.yaml  # bundle contract and declared test fixtures
│   │   ├── CMakeLists.txt
│   │   ├── cmake/OpenStrataPlugin.cmake
│   │   ├── src/
│   │   ├── plugin/resources/roadnetwork-opendrive/plugInfo.json
│   │   └── tests/fixtures/
│   ├── roadnetwork-lanelet2/       # future
│   └── roadnetwork-sumo/           # future
├── libs/                           # SCF-02
│   ├── roadnetwork-core/
│   └── roadnetwork-usd/
├── tests/                          # cross-format and aggregate package tests, when needed
├── docs/
│   ├── design/
│   ├── architecture/
│   ├── reference/
│   ├── adr/
│   ├── roadmap/
│   ├── reports/
│   └── releases/
└── examples/
```

As in `usd-3dgs-plugins`, tests and fixtures live inside the bundle or
library they exercise; the root `tests/` holds only cross-format and
aggregate package tests. Bundle fixtures are declared in the bundle manifest
so package tests can find them. Build state (`.strata/`, `build/`, `lib/`,
`strata.lock`) is generated and ignored.

## 5. Authored stage contract

*Proposed; settled by [ADR 0002](../adr/0002-usd-schema-strategy.md),
[ADR 0003](../adr/0003-crs-axes-units.md), and
[ADR 0005](../adr/0005-prim-path-and-id-stability.md).*

```text
/Asset                             Xform, defaultPrim
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

Invariants every bundle must satisfy once the contract is accepted:

- one shared writer authors the stage; bundles never author prims directly;
- `defaultPrim` is `Asset`;
- paths derive deterministically from source IDs (ADR 0005);
- `/Network` is complete without `/Visualization`; visualization is derived
  data and never the only carrier of semantics;
- importer-default layer metadata (`upAxis`, `metersPerUnit`) is documented
  as a default, not a source claim (ADR 0003);
- import provenance (source format, version, counts) is recorded on `/Asset`.

## 6. Packaging

Each bundle packages independently. A release requires that a plugin is
discovered and opens a fixture **from the installed package**, not only from
the source tree (see the [release gate](../releases/README.md#release-gate)).
