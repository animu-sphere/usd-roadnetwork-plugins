# Workspace contract

- **Status:** Proposed (2026-11-22). Paths and identities are adjusted to what
  the OpenStrata scaffold actually generates in Phase 0; this document is then
  updated in the same PR and becomes binding.

This is the structural contract for `usd-roadnetwork-plugins`. It fixes
component identities, dependency directions, root responsibilities, and the
authored stage contract. A structural change that contradicts this document
must change this document first.

See the [capability matrix](../reference/CAPABILITY_MATRIX.md) for what the
components support.

## 1. Components

| Identity | Kind | Responsibility | State |
| --- | --- | --- | --- |
| `roadnetwork-opendrive` | OpenStrata plugin bundle (`usd-fileformat`) | Detect and parse ASAM OpenDRIVE `.xodr`; decode it into `RoadNetworkData`. | planned (Phase 0 empty bundle) |
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
plugins/roadnetwork-opendrive/src/OpenDriveFileFormat.*
    thin SdfFileFormat integration

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
├── openstrata.toml                 # name per scaffold
├── plugins/
│   ├── roadnetwork-opendrive/
│   ├── roadnetwork-lanelet2/       # future
│   └── roadnetwork-sumo/           # future
├── libs/
│   ├── roadnetwork-core/
│   └── roadnetwork-usd/
├── tests/
│   ├── fixtures/
│   ├── unit/
│   ├── integration/
│   └── package/
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

Whether tests live at the root or inside each bundle and library (as in
`usd-3dgs-plugins`) is decided with the scaffold; bundle-specific fixtures
must be declared in the bundle manifest either way so package tests can find
them.

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
