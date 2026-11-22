# Current: Phase 1 — OpenDRIVE vertical slice

This page owns the active phase's workstreams, their order, and their
tracking. The scope, the feature list, and the exit criteria are in the
[release plan](release-plan.md#phase-1--opendrive-vertical-slice); task-level
status is in issues and pull requests.

Phase 1 started on 2026-11-22, when Phase 0 met its exit criteria (PRs
[#1](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/1)–[#5](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/5)).

## Decisions carried from Phase 0

Each decision is recorded in its owner; this list only points to it.

- OpenDRIVE is the first format, and the plugins are read-only
  ([design policy §4](../design/DESIGN_POLICY.md#4-format-strategy), [§12](../design/DESIGN_POLICY.md#12-write-support)).
- The project is licensed under Apache-2.0, like its sibling OpenStrata
  projects ([LICENSE](../../LICENSE)).
- The file-format contract and diagnostic numbering are accepted
  ([ADR 0006](../adr/0006-fileformat-contract.md)); it replaces the
  `usd-3dgs-plugins` worker-thread reload workaround with Sdf-only authoring.
- Phase 1 accepts OpenDRIVE `revMajor="1"` with `revMinor` 4–9
  ([FMT-01 §3.3](../reports/format-spike-opendrive-2026-11-22.md#33-proposed-phase-1-version-policy)).
- CARLA `Town01.xodr` and `Town02.xodr` are the real-world compatibility
  corpus, committed unmodified under MIT
  ([THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md#carla-opendrive-test-files)).

## Active work

All workstreams are owned by `usd-roadnetwork-plugins` and are listed in the
recommended order. They may run in parallel, but a change to the IR or the
stage contract updates the decoder and writer tests together. An ADR is
accepted in the workstream that produces the last of its evidence, or
earlier if review finds the evidence sufficient. Add issue or PR links as
work starts.

| Order | ID | Work | Features | Component | ADRs | Exit criterion | Tracking |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | IR-01 | `RoadNetworkData` types and validation | F-03, F-06 | `roadnetwork-core` | [0001](../adr/0001-roadnetwork-data-model.md) | The ADR 0001 types replace the placeholder. Each validation invariant fails a unit test with its `RN-` code. | merged ([#6](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/6)) |
| 2 | GEO-01 | Line, arc, and cubic evaluation; lane borders; tessellation | F-04, F-05, F-08 | `roadnetwork-core` | [0004](../adr/0004-opendrive-geometry-retention.md) | Closed-form position and heading tests pass, and sampled borders stay within the tolerance on a hand-authored fixture. | merged ([#8](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/8)) |
| 3 | ODR-01 | Reader: pugixml 1.16, the `CanRead()` sniff, version recognition, and the metadata-only header scan | F-01, F-02, F-10 | `roadnetwork-opendrive` | [0006](../adr/0006-fileformat-contract.md) | The prologue, version, and malformed-input fixtures pass with stable `ODR-` codes, and a failed reload keeps the previous content. | merged ([#9](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/9)) |
| 4 | ODR-02 | Decoder: roads, lane sections, lanes, junctions, and profiles | F-05, F-06, F-09, F-11 | `roadnetwork-opendrive` | [0001](../adr/0001-roadnetwork-data-model.md), [0003](../adr/0003-crs-axes-units.md), [0004](../adr/0004-opendrive-geometry-retention.md) | The minimal fixture set and CARLA `Town01` and `Town02` decode and validate, and each Phase 1 rejection has a negative fixture. | merged ([#10](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/10)) |
| 5 | USD-01 | Stage contract and the shared writer | F-07, F-08, F-09 | `roadnetwork-usd` | [0002](../adr/0002-usd-schema-strategy.md), [0003](../adr/0003-crs-axes-units.md), [0005](../adr/0005-prim-path-and-id-stability.md) | Generated layers pass `usdchecker` and semantic assertions. Paths are stable across reloads, and two referenced networks keep their own relationships. | merged ([#11](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/11)) |
| 6 | REL-01 | First release | F-10, F-11 | all | — | The [Phase 1 exit](release-plan.md#phase-1-exit) is met and v0.1.0 is released. | — |

## Next actions

1. **Review ADRs 0001–0005 for acceptance.** Each ADR's evidence section
   now lists what IR-01 to USD-01 gathered, and the
   [Lanelet2 paper mapping](../reports/adr-0001-lanelet2-mapping-2026-11-23.md)
   completed ADR 0001's last evidence item. It led to two amendments made
   before acceptance: ADR 0001 §4 (a variant of lane keys) and ADR 0003's
   scope and §2 (metric source frames only). Review ADRs 0001 and 0002
   together: the undirected lane-end links and the USD
   `predecessor`/`successor` relationships must mean the same thing, and
   both must extend to a second format. Review at the same time ADR 0004
   §2's `RN-E308`/`RN-W301` split, found while decoding CARLA in ODR-02,
   and the choices USD-01 made where ADR 0002 is silent
   ([ADR 0002 evidence](../adr/0002-usd-schema-strategy.md#evidence)).
2. **Inspect the fixtures in `usdview`**, as the
   [Phase 1 exit](release-plan.md#phase-1-exit) requires, and record it for
   REL-01 (`ost plugin test --up-to 6` adds the `usdview` level).
3. Update the capability matrix in the same pull request as each
   implementation; a row without test evidence stays unsupported.
