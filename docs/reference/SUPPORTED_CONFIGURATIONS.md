# Supported configurations

This page owns the runtimes, toolchains, and platforms the project targets,
and separates what has been exercised from what is only declared. Targets
are taken from the sibling `usd-3dgs-plugins` workspace. What is exercised is
the build and test lifecycle of the current tree; what the tree *does* is in
the [capability matrix](CAPABILITY_MATRIX.md).

## OpenUSD and OpenStrata

| Item | Target | Exercised |
| --- | --- | --- |
| OpenStrata | 0.23.21 | 0.23.21 locally (scaffold; library build and test; bundle build, test L0–L5, package, and package-origin test L0–L5) and on the hosted `pr` and `main` cells (bootstrapped from the pinned release) |
| OpenUSD tolerated range | `>=26.08,<27.0` (the bundle manifest) | 26.08, the lower bound. Later 26.x releases are declared but not exercised |
| OpenUSD runtime | 26.08 | 26.08 locally (`openstrata-cy2026-windows-x86_64-py313-usd`; the `-lookdev` runtime for the L6 `usdview` level, [REL-01 report](../reports/rel-01-inspection-2026-11-23.md#2-usdview-launch-l6)) and on the hosted `pr` and `main` cells (the Windows, Linux, and macOS runtimes pinned in `openstrata.ci.yaml`) |
| OpenStrata platform/profile | `cy2026` / `usd` | `cy2026` / `usd`; `cy2026` / `lookdev` locally for L6 only |
| Python ABI | CPython 3.13 | CPython 3.13 locally |

The lower bound of the range is the oldest OpenUSD the build and tests
exercise; the scaffold default `>=25.05` was narrowed to it because no
earlier runtime is built or tested. The upper bound stays below 27.0, the
next major release. Lowering the bound, or claiming a later 26.x release,
needs a CI cell or a local run on that runtime, recorded in this table.

OpenUSD does not promise C++ ABI compatibility across versions. Rebuild the
plugins against the target OpenUSD runtime even when the version falls inside
the declared range.

## Build toolchain

| Item | Target |
| --- | --- |
| CMake | 3.23 or newer |
| C++ | C++17, compiler extensions disabled |
| Build type | Release by default |
| Windows compiler | MSVC toolset 143 |

## Platforms

| OS / architecture | CI contract | Observed |
| --- | --- | --- |
| Windows 2022 / x86_64 / MSVC 143 | `workspace-{pr,main}-windows`, `opendrive-{pr,main}-windows` | hosted `pr` and `main` cells pass (2026-11-22, [run 45006974286](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45006974286) and [run 45007612429](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45007612429)): CTest 4 of 4; pyramid L0–L5 (12 pass, 4 skip); bundle packaged. Local Windows 11 also passes `--from-package --up-to 5` at L0–L5 |
| macOS 15 / arm64 / libc++ | `workspace-{pr,main}-macos`, `opendrive-{pr,main}-macos` | hosted `pr` and `main` cells pass (2026-11-22, [run 45006974286](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45006974286) and [run 45007612429](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45007612429)): CTest 4 of 4; pyramid L0–L5 (12 pass, 4 skip); bundle packaged |
| Ubuntu 24.04 / x86_64 | `workspace-{pr,main}-linux`, `opendrive-{pr,main}-linux` | hosted `pr` and `main` cells pass (2026-11-22, [run 45006974286](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45006974286) and [run 45007612429](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45007612429)): CTest 4 of 4; pyramid L0–L5 (12 pass, 4 skip); bundle packaged |

The CI contract is [`openstrata.ci.yaml`](../../openstrata.ci.yaml). Every
cell runs on a GitHub-hosted runner, bootstraps the pinned `ost`, and pulls the
digest-pinned OpenUSD 26.08 runtime for its platform. The cells differ in what
they verify:

- `workspace-*` checks the dependency graph, builds the composed root with
  `ost build`, and runs its CTest suite with `ost test`.
- `opendrive-*` builds the `roadnetwork-opendrive` bundle with its required
  libraries, runs its verification pyramid up to L5, and packages it.

Both kinds are needed because of an `ost` 0.23.21 generator gap
([open-strata#304](https://github.com/animu-sphere/open-strata/issues/304)):
a workspace cell cannot run the pyramid or packaging on a clean checkout.
Neither kind runs the package-origin test (`--from-package`).

Pull requests run the `pr` cells, and pushes to `main` run the `main` cells.
Both go through the hand-written
[`ci.yml`](../../.github/workflows/ci.yml) caller. Changes to Markdown,
`docs/`, or `LICENSE` alone do not build. The jobs in
[`ost-source-ci.yml`](../../.github/workflows/ost-source-ci.yml) are generated
from the matrix; do not edit them by hand.

Hosted support is claimed only after the declared CI jobs complete; a row is
updated with a run link in the PR that proves it.

## OpenDRIVE input contract

The accepted specification versions and the per-element behavior are owned
by the [OpenDRIVE mapping](OPENDRIVE_MAPPING.md#1-versions-and-detection);
they are not repeated here.
