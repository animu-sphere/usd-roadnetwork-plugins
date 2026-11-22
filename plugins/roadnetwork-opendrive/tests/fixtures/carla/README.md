# CARLA OpenDRIVE test files

These two networks are third-party files. They are **not** covered by this
repository's Apache-2.0 licence. They are licensed under the **MIT License**:

> Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
> de Barcelona (UAB).

The full licence text is in [`LICENSE`](LICENSE), copied unchanged from
upstream. Keep `LICENSE` and this file next to the `.xodr` files whenever they
are copied. The same text is in the bundle's
[`THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md), which the
manifest declares under `notices`, so every package of the bundle carries it.
`ost plugin package` packs only the fixtures named in the manifest's test
lists, so the `.xodr` files enter the package only once they are declared
there, and the notice is already in place when they do.

## Provenance

| Item | Value |
| --- | --- |
| Upstream | [carla-simulator/opendrive-test-files](https://github.com/carla-simulator/opendrive-test-files) |
| Commit | `5929b67cc414b611b701ad251fe1e6d30f465bba` (2019-06-19, head of `master` on 2026-11-22) |
| Upstream paths | `OpenDrive/Town01.xodr`, `OpenDrive/Town02.xodr`, `LICENSE` |
| Producer | RoadRunner 2019.0.0 (`vendor="VectorZero"`), OpenDRIVE 1.4 |
| Modifications | None. The files are byte-identical to upstream. |

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `Town01.xodr` | 498,388 | `15794f0dff5911837e6059ed628618ad666b3526748b698c0be25cdef7b27f70` |
| `Town02.xodr` | 616,712 | `a6f47c763e09cdf34e759540eef8ef9399e2d899d8eaa33054a5203bceb5494b` |
| `LICENSE` | 1,133 | `ac6291ec3b8a559931e2f0e5f5a849561463a0f174adee40bc6e40b853325d4a` |

Do not edit the `.xodr` files. A test that needs a variant uses a
hand-authored fixture instead, so the checksums above stay verifiable.

## Role

This is the real-world compatibility corpus chosen by the
[FMT-01 spike](../../../../../docs/reports/format-spike-opendrive-2026-11-22.md#8-fixtures).
Both networks use only `line` and `arc` plan-view geometry, with elevation,
lane offsets, road and lane links, and 12 and 8 `default` junctions. They
extend the evidence from the hand-authored fixtures and are never the only
evidence for a claim.

The bundle manifest does not declare them yet: the reader is still a
placeholder, so a test on them would prove nothing. The Phase 1 PR that
decodes them adds them to the manifest's test lists.
