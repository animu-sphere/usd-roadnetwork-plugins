# Third-party notices

No third-party code is included yet. The repository contains the third-party
test data listed below.

Each dependency added later gets a section here with:

- the project URL;
- the exact pinned version or commit (and archive checksum when vendored from
  a release archive);
- the files included and where they live;
- the license, with the upstream license text retained in the tree;
- any local modifications.

Dependencies stay private to the bundle that needs them; see the
[dependency policy](docs/design/DESIGN_POLICY.md#14-dependency-policy).

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
