# Supported configurations

This document separates what has been exercised from what is only declared or
targeted. Targets are taken from the sibling `usd-3dgs-plugins` workspace.
As of 2026-11-22 the only exercised configuration is a local Windows build of
the scaffolded (empty) `roadnetwork-opendrive` bundle and its two libraries.

## OpenUSD and OpenStrata

| Item | Target | Exercised |
| --- | --- | --- |
| OpenStrata | 0.23.21 | 0.23.21 locally (scaffold; library build and test; bundle build, test L0–L5, package, and package-origin test L0–L5) |
| OpenUSD tolerated range | `>=25.05,<27.0` (scaffold default in the bundle manifest) | only 26.08; the range is narrowed or confirmed before the first release |
| OpenUSD runtime | 26.08 | 26.08 locally (`openstrata-cy2026-windows-x86_64-py313-usd`) |
| OpenStrata platform/profile | `cy2026` / `usd` | `cy2026` / `usd` |
| Python ABI | CPython 3.13 | CPython 3.13 locally |

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
| Windows 2022 / x86_64 / MSVC 143 | `workspace-{pr,main}-windows` | local Windows 11 build; the empty bundle passes `ost plugin test --workspace --up-to 5` and `--from-package --up-to 5` at L0–L5 |
| macOS 15 / arm64 / libc++ | `workspace-{pr,main}-macos` | — |
| Ubuntu 24.04 / x86_64 | `workspace-{pr,main}-linux` | — |

The CI contract is [`openstrata.ci.yaml`](../../openstrata.ci.yaml). Each cell
is a GitHub-hosted workspace cell that bootstraps the pinned `ost`, pulls the
digest-pinned OpenUSD 26.08 runtime for its platform, and runs, in order: the
workspace dependency graph, `ost build`, the composed root's CTest suite
(`ost test`), every bundle's verification pyramid up to L5, and workspace and
product packaging (`verify: package`). The package-origin test
(`--from-package`) is not part of the hosted cells. Pull requests run the
`pr` cells and pushes to `main` run the `main` cells, through the hand-written
[`ci.yml`](../../.github/workflows/ci.yml) caller; changes to documentation
alone do not build. The jobs in
[`ost-source-ci.yml`](../../.github/workflows/ost-source-ci.yml) are generated
from the matrix and are not edited by hand.

Hosted support is claimed only after the declared CI jobs complete; a row is
updated with a run link in the PR that proves it.

## OpenDRIVE input contract

Not yet defined. The [FMT-01 spike](../roadmap/current.md#active-work)
establishes the specification versions in scope; the result is recorded in
[OPENDRIVE_MAPPING.md](OPENDRIVE_MAPPING.md) and summarized here.
