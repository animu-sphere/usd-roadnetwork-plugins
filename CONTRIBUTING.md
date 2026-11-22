# Contributing

This page owns the procedures: building, testing, CI, and which documents a
change must review. What is being built now is in
[current work](docs/roadmap/current.md).

Participation follows the [Code of Conduct](.github/CODE_OF_CONDUCT.md).
Report suspected vulnerabilities privately as described in the
[security policy](.github/SECURITY.md), never in a public issue. Bug reports
and feature requests use the [issue templates](.github/ISSUE_TEMPLATE/), and
pull requests start from the [pull request template](.github/PULL_REQUEST_TEMPLATE.md).

## Development path

```sh
git clone https://github.com/animu-sphere/usd-roadnetwork-plugins
cd usd-roadnetwork-plugins
ost runtime pull cy2026 --profile usd                              # once
ost plugin inspect plugins/roadnetwork-opendrive                   # L0 bundle structure
ost plugin build plugins/roadnetwork-opendrive                     # build, with its libraries
ost plugin doctor plugins/roadnetwork-opendrive                    # L0-L1 diagnostics
ost plugin test plugins/roadnetwork-opendrive --up-to 5            # the gate PRs must pass
ost test                                                           # the composed root's CTest suite
ost plugin package plugins/roadnetwork-opendrive
ost plugin test plugins/roadnetwork-opendrive --from-package --up-to 5   # from the installed package
python tools/check_docs.py                                         # documentation drift check
```

Confirm the commands against the `ost` version recorded in
[supported configurations](docs/reference/SUPPORTED_CONFIGURATIONS.md); the
kinds of test each phase requires are in the
[design policy §15](docs/design/DESIGN_POLICY.md#15-test-strategy).

### Without `ost`

Plain CMake 3.23 or newer and an OpenUSD install are enough. The dual-mode
root builds both libraries and every bundle, and CTest runs the same suite
as `ost test`; on Windows, run these from a Visual Studio developer prompt so
Ninja finds the compiler:

```sh
cmake --preset default -DCMAKE_PREFIX_PATH=<your-openusd-install>
cmake --build --preset default
ctest --preset default
```

The tests put the OpenUSD that `find_package(pxr)` found on `PATH` and
`PYTHONPATH` themselves. The contract tests need a Python whose version
matches the one OpenUSD was built against; pass `-DPython3_EXECUTABLE=<path>`
if CMake finds another.

Each library and bundle also configures on its own. Install the libraries
first, in dependency order, and add their prefix to `CMAKE_PREFIX_PATH`:

```sh
cmake -S libs/roadnetwork-core -B build/core -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/core && cmake --install build/core --prefix <prefix>
cmake -S libs/roadnetwork-usd -B build/usd -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<your-openusd-install>;<prefix>"
cmake --build build/usd && cmake --install build/usd --prefix <prefix>
cmake -S plugins/roadnetwork-opendrive -B build/opendrive -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<your-openusd-install>;<prefix>"
cmake --build build/opendrive && ctest --test-dir build/opendrive
```

`ROADNETWORK_BUILD_TESTS=OFF` turns every member's tests off; each member's
own `<MEMBER>_BUILD_TESTS` option overrides it for that member. The shared
build conventions (C++ standard, MSVC flags, OpenUSD target discovery, the
test environment) are in [`cmake/RoadnetworkBuild.cmake`](cmake/RoadnetworkBuild.cmake),
which every member includes; the bundle's `cmake/OpenStrataPlugin.cmake` is
the pinned scaffold helper and is not edited.

### CI

Pull requests run the same lifecycle on hosted Windows, Ubuntu, and macOS
runners. The CI contract is [`openstrata.ci.yaml`](openstrata.ci.yaml); after
editing it, regenerate the workflow with
`ost ci generate github --reusable-source --out .github/workflows/ost-source-ci.yml --force`
and commit both files. The documentation check runs separately, on every
change, from [`docs.yml`](.github/workflows/docs.yml).

## Where things go

| Change | Where |
| --- | --- |
| Parsing and semantic decoding | the format bundle's `src/io/` — USD-free and unit-tested |
| Format-independent model and validation | `libs/roadnetwork-core/` |
| USD authoring | `libs/roadnetwork-usd/` — shared by every bundle |
| Test inputs | the bundle's fixtures, small and redistributable, with provenance |
| Vendored third-party source | `third_party/<name>/`, unmodified, with a `VENDORING.md` (archive, checksums, build definitions); compiled only into the bundle that needs it |
| New diagnostics | the bundle's diagnostics header (`src/io/Diagnostics.h`) **and** its catalog; codes are never renumbered or reused |
| Behavior claims | `docs/reference/` — a claim without a fixture is a bug |
| A new or changed feature | the [release plan](docs/roadmap/release-plan.md) feature list; cite its `F-` ID from the workstream and the capability row |

## Ground rules

- Follow the [design policy](docs/design/DESIGN_POLICY.md); the
  [docs index](docs/README.md) explains which document owns which question
  and how documents are kept in sync.
- A decision that changes a rule needs an [ADR](docs/adr/).
- Every behavior change lands with a deterministic test; malformed input gets
  a fixture and a stable diagnostic.
- No silent fallback: a broken road, lane, or junction relationship is an
  error, not an approximation.
- No external data enters the repository without license and provenance
  review.
- Third-party dependencies are pinned and recorded in
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- Public Markdown is English; keep links relative and resolving.

## Documentation review

| Change | Review |
| --- | --- |
| Reader, decoder, or diagnostics | [Capability matrix](docs/reference/CAPABILITY_MATRIX.md), [OpenDRIVE mapping](docs/reference/OPENDRIVE_MAPPING.md), [diagnostics](docs/reference/DIAGNOSTICS.md), and fixtures |
| Shared model or validation | [ADR 0001](docs/adr/0001-roadnetwork-data-model.md), cross-format tests, and a new ADR if a decision changes |
| USD authoring | [Stage contract](docs/architecture/WORKSPACE.md#5-authored-stage-contract), mappings, stage assertions, and migration impact |
| CI, packaging, or release tooling | [Supported configurations](docs/reference/SUPPORTED_CONFIGURATIONS.md) and the [release gate](docs/releases/README.md#release-gate) |
| New bundle | [Workspace contract](docs/architecture/WORKSPACE.md), bundle README, the [format strategy](docs/design/DESIGN_POLICY.md#4-format-strategy), and the [docs index](docs/README.md#component-documentation) |
| Phase scope, features, or status | [Release plan](docs/roadmap/release-plan.md), [current work](docs/roadmap/current.md), and [backlog](docs/roadmap/backlog.md); each item has one home |
| ADR status | The ADR's `Status` line and its [index](docs/adr/README.md) row, in the same PR |
| Documentation structure | The [ownership table](docs/README.md#document-ownership) and `tools/check_docs.py` |
