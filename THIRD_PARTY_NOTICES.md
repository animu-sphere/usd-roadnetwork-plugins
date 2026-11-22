# Third-party notices

The repository vendors the third-party code and contains the third-party
test data listed below.

Each dependency gets a section here with:

- the project URL;
- the exact pinned version or commit (and archive checksum when vendored from
  a release archive);
- the files included and where they live;
- the license, with the upstream license text retained in the tree;
- any local modifications.

Dependencies stay private to the bundle that needs them; see the
[dependency policy](docs/design/DESIGN_POLICY.md#14-dependency-policy).

## Code

### pugixml 1.16

- **Project:** [zeux/pugixml](https://github.com/zeux/pugixml)
- **Version:** release `v1.16` (2026-06-16), from the release archive
  `pugixml-1.16.tar.gz` (SHA-256
  `4cee1ca4aad395170f4c7a07824f3bdd41f28316c6e1e1090a1425b278ec0b4b`)
- **Files:** `pugixml.cpp`, `pugixml.hpp`, `pugiconfig.hpp`, and `LICENSE.md`
  in [`third_party/pugixml/`](third_party/pugixml/). The per-file checksums and
  build definitions are in its [VENDORING.md](third_party/pugixml/VENDORING.md).
- **Used by:** the `roadnetwork-opendrive` bundle only, compiled into its
  reader with hidden symbols and kept out of every public header. No shared
  library depends on it.
- **License:** MIT License, Copyright (c) 2006-2026 Arseny Kapoulkine. The
  upstream license text is retained unchanged in
  [`third_party/pugixml/LICENSE.md`](third_party/pugixml/LICENSE.md) and
  repeated in the bundle's
  [`THIRD_PARTY_NOTICES.md`](plugins/roadnetwork-opendrive/THIRD_PARTY_NOTICES.md),
  so every bundle package carries it.
- **Modifications:** none. Configuration is by build definitions only.
- **Selection:** [FMT-01 spike §6](docs/reports/format-spike-opendrive-2026-11-22.md#6-xml-parser).

## Test data

### CARLA OpenDRIVE test files

- **Project:** [carla-simulator/opendrive-test-files](https://github.com/carla-simulator/opendrive-test-files)
- **Commit:** `5929b67cc414b611b701ad251fe1e6d30f465bba` (2019-06-19)
- **Files:** `OpenDrive/Town01.xodr` and `OpenDrive/Town02.xodr`, in
  [`plugins/roadnetwork-opendrive/tests/fixtures/carla/`](plugins/roadnetwork-opendrive/tests/fixtures/carla/).
  The SHA-256 of each file is in that directory's
  [README](plugins/roadnetwork-opendrive/tests/fixtures/carla/README.md).
- **License:** MIT License, Copyright (c) 2019 Computer Vision Center (CVC) at
  the Universitat Autonoma de Barcelona (UAB). The upstream license text is
  retained unchanged in
  [`plugins/roadnetwork-opendrive/tests/fixtures/carla/LICENSE`](plugins/roadnetwork-opendrive/tests/fixtures/carla/LICENSE)
  and repeated in the bundle's
  [`THIRD_PARTY_NOTICES.md`](plugins/roadnetwork-opendrive/THIRD_PARTY_NOTICES.md),
  which the bundle manifest declares under `notices`, so every bundle package
  carries it.
- **Modifications:** none. The files are byte-identical to upstream.
- **Selection:** [FMT-01 spike §8](docs/reports/format-spike-opendrive-2026-11-22.md#8-fixtures).
