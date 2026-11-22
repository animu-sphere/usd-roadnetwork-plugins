# roadnetwork-usd

The shared writer that authors `RoadNetworkData` into a USD layer. Every
format bundle authors its stage only through this library
([workspace contract §5](../../docs/architecture/WORKSPACE.md#5-authored-stage-contract)).
It depends on `roadnetwork-core` and OpenUSD, never on a plugin bundle or a
format parser.

**State: stage contract.** `WriteLayer()` authors the decisions of
ADRs [0002](../../docs/adr/0002-usd-schema-strategy.md) (prim types and the
`roadnetwork:` properties), [0003](../../docs/adr/0003-crs-axes-units.md)
(`Z`-up metres and the visualization origin shift), and
[0005](../../docs/adr/0005-prim-path-and-id-stability.md) (names, hierarchy,
and relationships), accepted on 2026-11-23.

| Item | Value |
| --- | --- |
| Library id | `roadnetwork-usd` (`openstrata.library.yaml`), requires `roadnetwork-core` `>=0.1,<0.2` |
| CMake | `find_package(roadnetworkUsd CONFIG REQUIRED)`, target `openstrata::roadnetworkUsd` |
| Kind | static library, C++17; links OpenUSD `arch tf vt sdf` (or `usd_ms`) publicly |
| Headers | `openstrata/rn/usd/LayerWriter.h` (`WriteLayer()`, `ImportRecord`), `openstrata/rn/usd/PrimNames.h` (the ADR 0005 name encoding `E` and its inverse, section and lane names; no OpenUSD types) |
| API tier | Contributor ([API boundary](../../docs/architecture/API_BOUNDARY.md#tiers)): no external API or ABI guarantee |

## Writer contract

`bool WriteLayer(const RoadNetworkData&, const ImportRecord&, SdfLayer&, Diagnostic* error)`
authors validated data into a scratch layer and transfers it into the target
only on success, so a failure leaves the target layer unchanged. The data
must have passed `Validate()`; an invariant that does not hold anyway, such
as a lane edge to a missing lane, fails the write with `RN-E901` instead of
authoring a dangling relationship. The output is a pure function of the data
and the record. The visualization points come from
`openstrata::rn::TessellateRoad()`; the writer's only arithmetic is the
origin shift and the curve extents.

`roadnetworkUsd_layer_writer` checks the name encoding and its inverse (the
ADR 0005 table, the underscore, punctuation, multi-byte UTF-8, NUL, and the
empty ID), each ADR 0002 property group on a hand-built network with a
junction, the sorted lane relationships, the absence of single-precision and
non-uniform values under `/Asset/Network`, a header-only write, the origin
shift at UTM scale (points within 0.5 mm of their samples), a failure that
leaves the layer unchanged, determinism, and a write inside an
`SdfChangeBlock` that matches one outside.

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
