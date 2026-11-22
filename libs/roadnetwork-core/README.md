# roadnetwork-core

Format-independent road network model shared by every format bundle in this
workspace. It has no OpenUSD, plugin, or format-parser dependency
([workspace contract §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)).

**State: skeleton.** `RoadNetworkData` has no members until
[ADR 0001](../../docs/adr/0001-roadnetwork-data-model.md) is accepted.
Validation, geometry math, diagnostics helpers, and the decoder test kit
arrive with Phase 1.

| Item | Value |
| --- | --- |
| Library id | `roadnetwork-core` (`openstrata.library.yaml`) |
| CMake | `find_package(roadnetworkCore CONFIG REQUIRED)`, target `openstrata::roadnetworkCore` |
| Kind | static library, C++17 |
| Headers | `openstrata/rn/RoadNetworkData.h`, `openstrata/rn/Version.h` |
| API tier | Contributor ([API boundary](../../docs/architecture/API_BOUNDARY.md#tiers)): no external API or ABI guarantee |

## Build and test

```sh
ost library build libs/roadnetwork-core
ost library test libs/roadnetwork-core
```

`ROADNETWORKCORE_BUILD_TESTS` overrides whether `tests/` is built. Otherwise
it follows the composed root's `ROADNETWORK_BUILD_TESTS`, then
`BUILD_TESTING`, then whether this is the top-level project.
