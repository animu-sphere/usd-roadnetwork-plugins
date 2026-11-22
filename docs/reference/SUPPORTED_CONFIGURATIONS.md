# Supported configurations

This document separates what has been exercised from what is only declared or
targeted. Targets are taken from the sibling `usd-3dgs-plugins` workspace.
As of 2026-11-22 the only exercised configuration is a local Windows build of
the scaffolded (empty) `roadnetwork-opendrive` bundle.

## OpenUSD and OpenStrata

| Item | Target | Exercised |
| --- | --- | --- |
| OpenStrata | 0.23.21 | 0.23.21 locally (scaffold, build, doctor, test L0–L4) |
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
| Windows 2022 / x86_64 / MSVC 143 | to be declared in Phase 0 | local Windows 11 build; scaffold bundle passes `ost plugin test --up-to 5` at L0–L4 |
| macOS 15 / arm64 / libc++ | to be declared in Phase 0 | — |
| Ubuntu 24.04 / x86_64 | to be declared in Phase 0 | — |

Hosted support is claimed only after the declared CI jobs complete; a row is
updated with a run link in the PR that proves it.

## OpenDRIVE input contract

Not yet defined. The [FMT-01 spike](../roadmap/current.md#active-work)
establishes the specification versions in scope; the result is recorded in
[OPENDRIVE_MAPPING.md](OPENDRIVE_MAPPING.md) and summarized here.
