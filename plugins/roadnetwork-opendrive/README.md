# roadnetwork-opendrive — OpenUSD `xodr` file-format plugin

Scaffolded by `ost plugin new usd-fileformat roadnetwork-opendrive --extension xodr`.

## Layout

```
openstrata.plugin.yaml          bundle contract (identity, runtime range, provides, tests)
CMakeLists.txt                  builds libRoadnetworkOpendriveFileFormat.so into lib/
cmake/OpenStrataPlugin.cmake    pinned, self-contained build/install mechanics
src/RoadnetworkOpendriveFileFormat.{h,cpp}  the SdfFileFormat implementation
src/io/Reader.{h,cpp}           USD-free reader: CanRead() sniff, container, root, header, and version checks
src/io/Decoder.{h,cpp}          USD-free decoder: RoadNetworkData, the lane graph, validation, and warnings
src/io/DocumentImpl.h           the parsed XML, shared by the reader and the decoder only
src/io/Diagnostics.h            the ODR- codes, one constant each
plugin/resources/roadnetwork-opendrive/plugInfo.json   USD plugin registration (supportsWriting: false)
plugin/resources/roadnetwork-opendrive/diagnostics.json  ODR- diagnostic catalog (rn-diagnostics.json is installed beside it)
tests/fixtures/                 basic (valid) + invalid (negative) fixtures, basic golden
tests/fixtures/reader/          prologue and negative fixtures for the reader
tests/fixtures/decoder/         the minimal network and a negative fixture per decoder and validation code
tests/fixtures/writer/          a UTM-scale road for the visualization origin shift (ADR 0003)
tests/fixtures/carla/           third-party CARLA networks (MIT, see its README and LICENSE)
LICENSE                         copy of the root Apache-2.0 LICENSE, shipped in every package (manifest `notices`)
THIRD_PARTY_NOTICES.md          notices shipped in every package (manifest `notices`)
tests/test_reader.cpp           reader unit test, without OpenUSD (CTest)
tests/test_decoder.cpp          decoder unit test, without OpenUSD (CTest)
tests/test_*.py                 file-format contract and catalog tests (CTest)
```

## Workspace libraries

The bundle declares `roadnetwork-core` and `roadnetwork-usd` in
`requires.libraries`; `ost plugin build` builds and installs them first, and
`CMakeLists.txt` finds them with `find_package` unless the composed root build
already defines the targets. `Read()` checks the document with the reader
in `src/io/`, decodes it into `RoadNetworkData` (only the header in a
metadata-only read), runs `openstrata::rn::Validate()`, and passes the result
to `openstrata::rn::usd::WriteLayer()`, the only author of the layer, which
authors the stage contract
([WORKSPACE.md §5](../../docs/architecture/WORKSPACE.md#5-authored-stage-contract)).
The bundle gives the writer its import record: the importer identity, the
version of this CMake project, and the reader's element counts, which a
metadata-only read takes too. A writer failure (`RN-E901`) fails the read
like any other error. The decoder's warnings are posted after a successful
write, each code once.

The reader parses XML with [pugixml](https://github.com/zeux/pugixml) 1.16
(MIT), vendored at the repository root in
[`third_party/pugixml`](../../third_party/pugixml/VENDORING.md) and compiled
into the static `roadnetworkOpendriveIo` library with hidden symbols, so
neither the plugin's exports nor any header expose it. Its licence text is in
the bundle's [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Fixtures

The top-level fixtures are hand-authored for this project, licensed
Apache-2.0, and carry no third-party content. Their header revision (1.8) is
inside the Phase 1 scope set by the
[FMT-01 spike](../../docs/reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy);
they are not validated against the ASAM schema.

| Fixture | Role | Content |
| --- | --- | --- |
| `basic.xodr` | smoke, roundtrip (L3–L5) | one straight 10 m road with one right-hand driving lane |
| `basic.xodr.golden.usda` | L5 oracle | the flattened stage of `basic.xodr` |
| `writer/projected.xodr` | smoke, contract | one road at UTM-scale coordinates with a georeference and an `<offset>`; its points must recover their closed-form curves within 0.5 mm (ADR 0003 §5) |
| `invalid.xodr` | negative | XML that is not well-formed (`ODR-E002`) |
| `reader/prologue-bom.xodr` | reader | a UTF-8 byte-order mark before the XML declaration |
| `reader/prologue-doctype.xodr` | reader | comments, a processing instruction, and a DOCTYPE with an internal subset before the root |
| `reader/not-utf8.xodr` | negative | an XML declaration naming ISO-8859-1 (`ODR-E003`) |
| `reader/wrong-root.xodr` | negative | an `<OpenSCENARIO>` root (`ODR-E004`) |
| `reader/namespaced-root.xodr` | negative | a default namespace on `<OpenDRIVE>` (`ODR-E004`) |
| `reader/missing-header.xodr` | negative | a `<road>` where the `<header>` must be (`ODR-E005`) |
| `reader/version-malformed.xodr` | negative | `revMinor="8.0"` (`ODR-E006`) |
| `reader/version-unsupported.xodr` | negative | OpenDRIVE 1.10 (`ODR-E007`) |

The contract test generates the remaining cases in a temporary directory: a
root beyond the 64 KiB sniff window, UTF-16, a directory named `.xodr`
(`ODR-E001`), and every `revMinor` from 0 to 11.

`carla/` holds two **third-party** networks, `Town01.xodr` and `Town02.xodr`,
from [carla-simulator/opendrive-test-files](https://github.com/carla-simulator/opendrive-test-files).
They are licensed under the **MIT License**, copyright (c) 2019 Computer
Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB), not under
Apache-2.0. The upstream licence text is in
[`carla/LICENSE`](tests/fixtures/carla/LICENSE), and the pinned commit and
checksums are in [`carla/README.md`](tests/fixtures/carla/README.md). The
files are unmodified. The licence text is also in the bundle's
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), declared under `notices`
in the manifest, so every package carries it. The networks are not yet named
in the manifest's test lists, so `ost plugin package` does not pack them yet.

Regenerate the golden after an intended output change:

```sh
ost plugin run plugins/roadnetwork-opendrive -- usdcat --flatten \
  plugins/roadnetwork-opendrive/tests/fixtures/basic.xodr \
  --out plugins/roadnetwork-opendrive/tests/fixtures/basic.xodr.golden.usda
```

`ost plugin test` drops the absolute path that `usdcat` writes into the
golden's `doc` before comparing.

## Contract tests

The file-format contract is fixed by
[ADR 0006](../../docs/adr/0006-fileformat-contract.md). Three CTest tests,
built when `ROADNETWORKOPENDRIVE_BUILD_TESTS` is on (it follows the composed
root's `ROADNETWORK_BUILD_TESTS`, then `BUILD_TESTING`, then whether this is the
top-level project), check it; run them with `ost test` from the
repository root:

| Test | Checks |
| --- | --- |
| `roadnetworkOpendrive_reader` | the `CanRead()` sniff, each reader failure and its code, the version range, and the header scan, without OpenUSD |
| `roadnetworkOpendrive_file_format_contract` | registration, read-only export, open, sublayer, reference, and payload composition, two references of one network with their own relationship targets, flattening, the metadata-only `/Asset`, the import record, the origin shift on `writer/projected.xodr`, `usdchecker` on the generated layers, determinism, reload with unchanged paths and targets (also under an outer `SdfChangeBlock`), `CanRead()` and the rejection of every negative fixture through the plugin, the warnings, and a failed reload that keeps the previous content |
| `roadnetworkOpendrive_diagnostics_catalog` | both catalogs are well-formed and every bracketed code used in source is catalogued by its owner |

The contract test loads the bundle from this source tree (`lib/` and
`plugin/resources/`), so build the bundle first; the reader test needs only a
compiler.

## Diagnostics

`Read()` reports failures as one `TF_RUNTIME_ERROR` whose message starts with
a bracketed code, and leaves the layer unchanged. Add each new `ODR-` code to
`src/io/Diagnostics.h` and `diagnostics.json` in the PR that first raises
it, with a negative fixture; format-independent conditions use `RN-` codes
from `roadnetwork-core`. The assigned codes are listed in
[DIAGNOSTICS.md](../../docs/reference/DIAGNOSTICS.md#odr--reader).

The copied CMake helper is versioned with this scaffold and requires neither an
OpenStrata checkout nor `ost` at build time. Keep bundle-specific targets,
components, resources, and tests in `CMakeLists.txt`; update helper mechanics by
reviewing a newer template rather than linking to the generator source tree.

## plugInfo.json and the library name

Edit `plugin/resources/roadnetwork-opendrive/plugInfo.json.in`; `plugInfo.json` beside it is
generated. Its `LibraryPath` is `@OPENSTRATA_PLUGIN_LIBRARY_PATH@`, which the
helper expands to `../../../lib/$<TARGET_FILE_NAME:RoadnetworkOpendriveFileFormat>` and
CMake evaluates at generate time. The registered filename is therefore always
the one the target actually builds, including any `OUTPUT_NAME`, `PREFIX`,
`SUFFIX` or `DEBUG_POSTFIX` you set on the target.

- `ost plugin new` writes a host-specific preview (`libRoadnetworkOpendriveFileFormat` plus
  this host's suffix) so `inspect` and `doctor` work before the first build.
  CMake overwrites it at the first configure.
- Each configuration also gets
  `<build>/openstrata/RoadnetworkOpendriveFileFormat/<config>/plugInfo.json`. The
  source-tree file holds the primary configuration (the build type, or
  `Release` for multi-config generators; set `OPENSTRATA_PLUGIN_PRIMARY_CONFIG`
  to change it). `cmake --install --config <config>` installs the matching
  copy.
- The installed `plugin/resources/roadnetwork-opendrive/` must stay at the same depth as the
  source one, so the same relative `LibraryPath` resolves in both trees.
  Configure fails if it does not.

## Workflow

```sh
ost plugin inspect roadnetwork-opendrive     # Level 0: bundle structure
ost plugin build roadnetwork-opendrive       # build the shared library into lib/
ost plugin doctor roadnetwork-opendrive      # staged diagnostics (L0–L1; L2+ need a real runtime)
ost plugin test roadnetwork-opendrive        # orchestrate the levels and write a report
```

`RoadnetworkOpendriveFileFormat::Read` checks, decodes, and validates the
file, fails with one stable code otherwise, and authors the result through the
shared writer.

## Co-hosting a Typed Schema

To compile a generated API schema into this same plugin, add `schema.usda` at the
bundle root and add `usd-schema:<TypeName>` to `provides` in
`openstrata.plugin.yaml`. `ost plugin build` runs `usdGenSchema` in the resolved
runtime environment, links the generated C++ API sources into this library, and
merges the generated `Types` into `plugin/resources/roadnetwork-opendrive/plugInfo.json`.
