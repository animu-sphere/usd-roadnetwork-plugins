# Current: Phase 1 — OpenDRIVE vertical slice

## Goal

Import simple OpenDRIVE roads, lanes, and junctions into USD, with meaning
and geometry that the small fixtures can verify, and accept ADRs 0001–0005.
The scope is in the [release plan](release-plan.md#phase-1--opendrive-vertical-slice);
v0.1.0 is planned at the end of the phase.

Phase 0 met its exit criteria on 2026-11-22 (PRs
[#1](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/1)–[#5](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/5)):
the empty bundle and both libraries pass the package lifecycle from source
and from the installed package, ADR 0006 is accepted, ADRs 0001–0005 are
ready for review, and the [FMT-01 report](../reports/format-spike-opendrive-2026-11-22.md)
fixes the Phase 1 versions and fixtures.

## Decisions

Carried from Phase 0:

- OpenDRIVE is the first format (design policy, 2026-11-22).
- The plugins are read-only.
- The project is licensed under Apache-2.0, like its sibling OpenStrata
  projects (2026-11-22).
- ADR 0006 is accepted (2026-11-22): `RN-` shared plus `ODR-` bundle codes,
  a metadata-only read authors the header-derived root only, `CanRead()`
  sniffs the root element within 64 KiB, and the writer authors Sdf specs on
  the calling thread ([ADR 0006](../adr/0006-fileformat-contract.md)).
- Phase 1 accepts OpenDRIVE `revMajor="1"` with `revMinor` 4–9 and rejects
  any other version, including a newer minor version, with a stable error
  (2026-11-22, [FMT-01 spike §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy)).
- CARLA `Town01.xodr` and `Town02.xodr` are committed unmodified as the
  real-world compatibility corpus, under their MIT license, which is stated
  beside the files, in the bundle README, and in
  [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) (2026-11-22).
- The `usd-3dgs-plugins` worker-thread reload workaround is not copied: the
  problem it solves (authoring through a `UsdStage` inside the reload's
  `SdfChangeBlock`) was reproduced on OpenUSD 26.08 and is avoided by
  authoring Sdf specs only.

## Active work

All workstreams are owned by `usd-roadnetwork-plugins`. Each names the ADRs
whose remaining evidence it produces. An ADR is accepted in the workstream
that produces the last of its evidence, or earlier if review finds the
evidence sufficient. Add issue or PR links as work starts.

| ID | Work | Component | ADRs | Exit criterion | Tracking |
| --- | --- | --- | --- | --- | --- |
| IR-01 | `RoadNetworkData` types and validation | `roadnetwork-core` | [0001](../adr/0001-roadnetwork-data-model.md) | The ADR 0001 types replace the placeholder. Each validation invariant fails a unit test with its `RN-` code. | in review: the types, `Validate()`, `CanonicalizeLaneLinks()`, and `RN-E101`–`RN-E307` land with a negative unit test per invariant (`roadnetworkCore_validation`) |
| GEO-01 | Line, arc, and cubic evaluation; lane borders; tessellation | `roadnetwork-core` | [0004](../adr/0004-opendrive-geometry-retention.md) | Closed-form position and heading tests pass, and sampled borders stay within the tolerance on a hand-authored fixture. | — |
| ODR-01 | Reader: pugixml 1.16, the `CanRead()` sniff, version recognition, and the metadata-only header scan | `roadnetwork-opendrive` | [0006](../adr/0006-fileformat-contract.md) | The prologue, version, and malformed-input fixtures pass with stable `ODR-` codes, and a failed reload keeps the previous content. | — |
| ODR-02 | Decoder: roads, lane sections, lanes, junctions, and profiles | `roadnetwork-opendrive` | [0001](../adr/0001-roadnetwork-data-model.md), [0003](../adr/0003-crs-axes-units.md), [0004](../adr/0004-opendrive-geometry-retention.md) | The hand-authored fixtures and CARLA `Town01` and `Town02` decode and validate, and each Phase 1 rejection has a negative fixture. | — |
| USD-01 | Stage contract and the shared writer | `roadnetwork-usd` | [0002](../adr/0002-usd-schema-strategy.md), [0003](../adr/0003-crs-axes-units.md), [0005](../adr/0005-prim-path-and-id-stability.md) | Generated layers pass `usdchecker` and semantic assertions. Paths are stable across reloads, and two referenced networks keep their own relationships. | — |
| REL-01 | First release | all | — | The [release gate](../releases/README.md#release-gate) passes for v0.1.0. | — |

## Phase exit

- The meaning and geometry of the small fixtures are verifiable in USD.
- ADRs 0001–0005 are accepted.
- The [format acceptance gate](release-plan.md#format-acceptance-gate) passes
  for OpenDRIVE, and the [release gate](../releases/README.md#release-gate)
  passes for v0.1.0.
