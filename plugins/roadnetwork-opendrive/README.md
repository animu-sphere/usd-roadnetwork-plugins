# roadnetwork-opendrive — OpenUSD `xodr` file-format plugin

Scaffolded by `ost plugin new usd-fileformat roadnetwork-opendrive --extension xodr`.

## Layout

```
openstrata.plugin.yaml          bundle contract (identity, runtime range, provides, tests)
CMakeLists.txt                  builds libRoadnetworkOpendriveFileFormat.so into lib/
cmake/OpenStrataPlugin.cmake    pinned, self-contained build/install mechanics
src/RoadnetworkOpendriveFileFormat.{h,cpp}  the SdfFileFormat implementation
plugin/resources/roadnetwork-opendrive/plugInfo.json   USD plugin registration (supportsWriting: false)
plugin/resources/roadnetwork-opendrive/diagnostics.json  ODR- diagnostic catalog (rn-diagnostics.json is installed beside it)
tests/fixtures/                 basic (valid) + invalid (negative) fixtures, basic golden
tests/test_*.py                 file-format contract and catalog tests (CTest)
```

## Workspace libraries

The bundle declares `roadnetwork-core` and `roadnetwork-usd` in
`requires.libraries`; `ost plugin build` builds and installs them first, and
`CMakeLists.txt` finds them with `find_package` unless the composed root build
already defines the targets. `Read()` passes an empty `RoadNetworkData` to
`openstrata::rn::usd::WriteLayer()`, which is the only author of the layer.
OpenDRIVE is not parsed yet.

## Fixtures

Both fixtures are hand-authored for this project and carry no third-party
content. Their header revision (1.8) is illustrative until the FMT-01 spike
fixes the versions in scope; they are not validated against the ASAM schema.

| Fixture | Role | Content |
| --- | --- | --- |
| `basic.xodr` | smoke, roundtrip (L3–L5) | one straight 10 m road with one right-hand driving lane |
| `basic.xodr.golden.usda` | L5 oracle | the flattened stage of `basic.xodr` (currently empty) |
| `invalid.xodr` | negative | XML that is not well-formed; not yet rejected, because nothing parses it |

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
[ADR 0006](../../docs/adr/0006-fileformat-contract.md). Two CTest tests,
built when `ROADNETWORK_BUILD_TESTS` is on (the default for the composed root
and a standalone configure), check it; run them with `ost test` from the
repository root:

| Test | Checks |
| --- | --- |
| `roadnetworkOpendrive_file_format_contract` | registration, read-only export, open, sublayer and reference composition, metadata-only read, determinism, and reload (also under an outer `SdfChangeBlock`) |
| `roadnetworkOpendrive_diagnostics_catalog` | both catalogs are well-formed and every bracketed code used in source is catalogued by its owner |

The contract test loads the bundle from this source tree (`lib/` and
`plugin/resources/`), so build the bundle first.

## Diagnostics

`Read()` reports failures as one `TF_RUNTIME_ERROR` whose message starts with
a bracketed code, and leaves the layer unchanged. Add each new `ODR-` code to
`diagnostics.json` in the PR that first raises it; format-independent
conditions use `RN-` codes from `roadnetwork-core`
([DIAGNOSTICS.md](../../docs/reference/DIAGNOSTICS.md)). No code is assigned
yet.

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

`RoadnetworkOpendriveFileFormat::Read` does not parse OpenDRIVE yet; it authors an empty
layer through the shared writer so a `.xodr` file opens as a valid (if empty)
stage.

## Co-hosting a Typed Schema

To compile a generated API schema into this same plugin, add `schema.usda` at the
bundle root and add `usd-schema:<TypeName>` to `provides` in
`openstrata.plugin.yaml`. `ost plugin build` runs `usdGenSchema` in the resolved
runtime environment, links the generated C++ API sources into this library, and
merges the generated `Types` into `plugin/resources/roadnetwork-opendrive/plugInfo.json`.
