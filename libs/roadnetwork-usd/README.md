# roadnetwork-usd

The shared writer that authors `RoadNetworkData` into a USD layer. Every
format bundle authors its stage only through this library
([workspace contract §5](../../docs/architecture/WORKSPACE.md#5-authored-stage-contract)).
It depends on `roadnetwork-core` and OpenUSD, never on a plugin bundle or a
format parser.

**State: skeleton.** `WriteLayer()` replaces the target layer with an empty
one (no prims, no layer metadata). It authors the stage contract once ADRs
[0002](../../docs/adr/0002-usd-schema-strategy.md),
[0003](../../docs/adr/0003-crs-axes-units.md), and
[0005](../../docs/adr/0005-prim-path-and-id-stability.md) are accepted.

| Item | Value |
| --- | --- |
| Library id | `roadnetwork-usd` (`openstrata.library.yaml`), requires `roadnetwork-core` `>=0.1,<0.2` |
| CMake | `find_package(roadnetworkUsd CONFIG REQUIRED)`, target `openstrata::roadnetworkUsd` |
| Kind | static library, C++17; links OpenUSD `arch tf vt sdf` (or `usd_ms`) publicly |
| Header | `openstrata/rn/usd/LayerWriter.h` |
| API tier | Contributor ([API boundary](../../docs/architecture/API_BOUNDARY.md#tiers)): no external API or ABI guarantee |

## Writer contract

`bool WriteLayer(const RoadNetworkData&, SdfLayer&, std::string* error)`
authors into a scratch layer and transfers it into the target only on
success, so a failure leaves the target layer unchanged. The unit test checks
that existing content is replaced, that two writes of the same data export
identical text, and that a write inside an `SdfChangeBlock` matches one
outside.

The writer authors **Sdf specs only**, on the calling thread
([ADR 0006 §5](../../docs/adr/0006-fileformat-contract.md#5-reload-and-authoring-thread)).
`SdfLayer::Reload` runs `Read()` inside an `SdfChangeBlock`, where authoring
through a `UsdStage` fails on OpenUSD 26.08. The library therefore links no
`usd` component. Against a per-component OpenUSD build, writer code that
uses a `UsdStage` makes the library's own test executable fail to link; the
bundle links `usd` itself, so this is not enforced there.

## Build and test

```sh
ost library build libs/roadnetwork-usd   # builds roadnetwork-core first
ost library test libs/roadnetwork-usd
```

`ROADNETWORKUSD_BUILD_TESTS` follows the same precedence as
`ROADNETWORKCORE_BUILD_TESTS`.
