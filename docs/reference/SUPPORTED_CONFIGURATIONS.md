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
| Windows 2022 / x86_64 / MSVC 143 | `workspace-{pr,main}-windows`, `opendrive-{pr,main}-windows` | local Windows 11 build; the empty bundle passes `ost plugin test --workspace --up-to 5` and `--from-package --up-to 5` at L0–L5 |
| macOS 15 / arm64 / libc++ | `workspace-{pr,main}-macos`, `opendrive-{pr,main}-macos` | — |
| Ubuntu 24.04 / x86_64 | `workspace-{pr,main}-linux`, `opendrive-{pr,main}-linux` | — |

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

Not yet defined. The [FMT-01 spike](../roadmap/current.md#active-work)
establishes the specification versions in scope; the result is recorded in
[OPENDRIVE_MAPPING.md](OPENDRIVE_MAPPING.md) and summarized here.
