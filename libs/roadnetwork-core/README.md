# roadnetwork-core

Format-independent road network model shared by every format bundle in this
workspace. It has no OpenUSD, plugin, or format-parser dependency
([workspace contract §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)).

**State: model, validation, geometry, and tessellation.** `RoadNetworkData`
holds the types of
[ADR 0001 §7](../../docs/adr/0001-roadnetwork-data-model.md#7-header-sketch)
(still Proposed), and `Validate()` checks its §6 invariants with the `RN-`
codes listed in the [diagnostics reference](../../docs/reference/DIAGNOSTICS.md#rn--shared-validation).
The evaluator, lane borders, and tessellator follow
[ADR 0004 §2–§3](../../docs/adr/0004-opendrive-geometry-retention.md#2-evaluation)
(still Proposed). No decoder produces the model yet.

| Item | Value |
| --- | --- |
| Library id | `roadnetwork-core` (`openstrata.library.yaml`) |
| CMake | `find_package(roadnetworkCore CONFIG REQUIRED)`, target `openstrata::roadnetworkCore` |
| Kind | static library, C++17 |
| Headers | `openstrata/rn/RoadNetworkData.h` (the model, lane-end order, `CanonicalizeLaneLinks()`), `openstrata/rn/Validation.h` (`Validate()`, `Diagnostic`), `openstrata/rn/Geometry.h` (reference-line and profile evaluation, lane borders and centerlines), `openstrata/rn/Tessellation.h` (`TessellateRoad()`: per-section stations, borders, and centerlines within a tolerance), `openstrata/rn/Version.h` |
| Resources | `resources/diagnostics.json`: the `RN-` diagnostic catalog, installed to `share/roadnetworkCore/` and exposed as `roadnetworkCore_DIAGNOSTICS_CATALOG`; every bundle ships a copy as `rn-diagnostics.json` |
| API tier | Contributor ([API boundary](../../docs/architecture/API_BOUNDARY.md#tiers)): no external API or ABI guarantee |

## Build and test

```sh
ost library build libs/roadnetwork-core
ost library test libs/roadnetwork-core
```

`ROADNETWORKCORE_BUILD_TESTS` overrides whether `tests/` is built. Otherwise
it follows the composed root's `ROADNETWORK_BUILD_TESTS`, then
`BUILD_TESTING`, then whether this is the top-level project.
