# Vendored pugixml

Provenance of the vendored copy. The consuming build wiring lives in
`plugins/roadnetwork-opendrive`; the choice of parser and the alternatives are
in the [FMT-01 spike §6](../../docs/reports/format-spike-opendrive-2026-11-22.md#6-xml-parser),
and the licence is recorded in [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md#pugixml-116).

## Source

- Upstream: <https://github.com/zeux/pugixml>
- Release: `v1.16` (2026-06-16)
- Archive: `pugixml-1.16.tar.gz` from the GitHub release, SHA-256
  `4cee1ca4aad395170f4c7a07824f3bdd41f28316c6e1e1090a1425b278ec0b4b`
  (checked against the archive on 2026-11-22)
- Licence: MIT (`LICENSE.md`)

## What is vendored

Each file is byte-identical to the release archive.

| File | Archive path | SHA-256 |
| --- | --- | --- |
| `pugixml.cpp` | `src/pugixml.cpp` | `bd94cd9ec6ac67c0723422640bb243eab4a7c2d9fd5584dab77305a50fa31ef4` |
| `pugixml.hpp` | `src/pugixml.hpp` | `0b7c94701df3d50981ffafe57f177c77b940e347c6e5d01073e4161f61299212` |
| `pugiconfig.hpp` | `src/pugiconfig.hpp` | `e6c51fbc9a765608bd214ea47d229d3bbd6b650036cdffc94b96b498c202495c` |
| `LICENSE.md` | `LICENSE.md` | `90974fbb120e966b452654eaae3ba6ee55f3647db3b28918a7649733ee99da97` |

Every upstream build file, document, sample, and script is dropped. No
upstream file is edited; configuration is by build definitions only.

## Build definitions

Set on `roadnetworkOpendriveIo` in `plugins/roadnetwork-opendrive/CMakeLists.txt`:

| Definition or property | Why |
| --- | --- |
| `PUGIXML_NO_XPATH` | The reader walks the DOM; XPath is not needed. |
| `CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN` | Keeps every pugixml symbol inside the plugin library. MaterialX ships its own pugixml in the same `pugi` namespace, and an OpenUSD host can load it into the same process; on ELF and Mach-O the two copies must not bind to each other (FMT-01 §6). |

pugixml is compiled into the bundle's static reader library and never
appears in a public header.

## Updating

Download the new release archive, check its SHA-256, copy the three source
files and `LICENSE.md` unchanged, and update this file, the root
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md), and the bundle's
`THIRD_PARTY_NOTICES.md` in the same pull request.
