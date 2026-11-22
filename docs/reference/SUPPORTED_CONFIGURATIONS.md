# Supported configurations

This document separates what has been exercised from what is only declared or
targeted. As of 2026-11-22 nothing has been built or exercised in this
repository; every row below is a **target**, taken from the sibling
`usd-3dgs-plugins` workspace, and is confirmed by the Phase 0 scaffold and CI.

## OpenUSD and OpenStrata

| Item | Target | Exercised |
| --- | --- | --- |
| OpenStrata | 0.23.20 (version used by `usd-3dgs-plugins`) | — |
| OpenUSD | 26.08 runtime, tolerated range to be declared | — |
| OpenStrata platform/profile | `cy2026` / `usd` | — |
| Python ABI | CPython 3.13 | — |

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
| Windows 2022 / x86_64 / MSVC 143 | to be declared in Phase 0 | — |
| macOS 15 / arm64 / libc++ | to be declared in Phase 0 | — |
| Ubuntu 24.04 / x86_64 | to be declared in Phase 0 | — |

Hosted support is claimed only after the declared CI jobs complete; a row is
updated with a run link in the PR that proves it.

## OpenDRIVE input contract

Not yet defined. The [FMT-01 spike](../roadmap/current.md#active-work)
establishes the specification versions in scope; the result is recorded in
[OPENDRIVE_MAPPING.md](OPENDRIVE_MAPPING.md) and summarized here.
