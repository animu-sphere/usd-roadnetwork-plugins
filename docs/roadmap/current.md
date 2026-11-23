# Current: Phase 2 — OpenDRIVE fidelity

This page owns the active phase's workstreams, their order, and their
tracking. The scope, the feature list, and the exit criteria are in the
[release plan](release-plan.md#phase-2--opendrive-fidelity); task-level
status is in issues and pull requests.

Phase 2 started on 2026-11-23, when Phase 1 met its
[exit criteria](release-plan.md#phase-1-exit) and
[v0.1.0](../releases/v0.1.0.md) was published (PRs
[#6](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/6)–[#21](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/21)).

## Decisions carried from Phase 1

Each decision is recorded in its owner; this list only points to it.

- ADRs 0001–0006 are accepted ([ADR index](../adr/README.md)). A Phase 2
  change to any of them is a new ADR, not an edit.
- The accepted versions stay `revMajor="1"` with `revMinor` 4–9
  ([OpenDRIVE mapping §1](../reference/OPENDRIVE_MAPPING.md#1-versions-and-detection)).
- An inner border past the center of curvature is `RN-E308`, and an
  outermost one is the warning `RN-W301`
  ([ADR 0004 evidence](../adr/0004-opendrive-geometry-retention.md#evidence)).
- Untrusted input is bounded by the 512 MiB import limit and the rules in
  [mapping §1](../reference/OPENDRIVE_MAPPING.md#untrusted-input); new
  elements are decoded under the same rules.
- The bundle declares OpenUSD `>=26.08,<27.0`, and only 26.08 is exercised
  ([supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md)).
- Releases are cut by the tag workflow and the
  [release process](../releases/README.md#release-process).
- CARLA `Town01` and `Town02` remain the committed real-world corpus
  ([THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md#carla-opendrive-test-files));
  they use only `line` and `arc` and carry no signals or objects
  ([FMT-01 §5](../reports/format-spike-opendrive-2026-11-22.md#5-elements-needed-for-phase-1)),
  so Phase 2 needs more sources.

## Active work

All workstreams are owned by `usd-roadnetwork-plugins` and are listed in the
recommended order. FMT-02 comes first because it declares the scope the
others implement. After it, the geometry line (ADR-10, GEO-02, XS-01), the
semantics line (ADR-11, SEM-01), and UPA-01 may run in parallel. A change to
the IR or the stage contract updates the decoder and writer tests together,
and each implementation updates the capability matrix, the mapping, and the
diagnostics in the same pull request. Add issue or PR links as work starts.

| Order | ID | Work | Features | Component | ADRs | Exit criterion | Tracking |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | FMT-02 | Phase 2 corpus and scope spike | F-12 to F-16 | — | — | A [report](../reports/) counts, by producer and version, which Phase 2 elements real files use; lists redistributable fixtures with provenance that carry spirals, `paramPoly3`, signals, objects, and road marks; and measures the error of VIRES `odrSpiral`. The [release plan](release-plan.md#phase-2--opendrive-fidelity) gives each Phase 2 feature a minimum acceptance, and deferred elements move to the [backlog](backlog.md). | [report](../reports/format-spike-opendrive-2026-11-23.md) |
| 2 | ADR-10 | New ADR: plan-view shape and cross-section evaluation | F-12, F-13 | `roadnetwork-core` | new (0010), [0004](../adr/0004-opendrive-geometry-retention.md) | The ADR fixes the evaluation method, the error bound, and the golden values for `spiral` (including nearly constant curvature, [FMT-02 §4.2–§4.3](../reports/format-spike-opendrive-2026-11-23.md#42-starting-at-non-zero-curvature)) and `paramPoly3` (how `s` maps to `p`, an absent `pRange`, and a length mismatch, [§4.4](../reports/format-spike-opendrive-2026-11-23.md#44-parampoly3)), and how superelevation enters the borders and the tessellation, and is accepted. | — |
| 3 | GEO-02 | Spiral and `paramPoly3` | F-12 | `roadnetwork-core`, `roadnetwork-opendrive` | new (0010) | Numeric tests pass within the ADR bound, the shapes decode and author, and `ODR-E201` remains only for `poly3`. | — |
| 4 | XS-01 | Superelevation; the continuity questions left open by [ADR 0004 §6](../adr/0004-opendrive-geometry-retention.md#6-discontinuities) | F-13 | `roadnetwork-core`, `roadnetwork-opendrive`, `roadnetwork-usd` | new (0010) | Hand-authored fixtures with known results pass within the stated accuracy, and `ODR-E202` remains only for lateral shape, crossfall, and `<crossSectionSurface>`. | — |
| 5 | ADR-11 | New ADR: traffic semantics, road marks, and objects in the model and the stage | F-13, F-14, F-15 | `roadnetwork-core`, `roadnetwork-usd` | new (0011), [0001](../adr/0001-roadnetwork-data-model.md), [0002](../adr/0002-usd-schema-strategy.md), [0005](../adr/0005-prim-path-and-id-stability.md) | The ADR fixes the IR types, prim paths, property vocabulary, and how signals and objects refer to roads and lanes (the part of the [lane-graph question](backlog.md#decisions-to-record-later) that F-14 and F-15 need), and is accepted. | — |
| 6 | SEM-01 | Signals, signal references, road marks, and objects | F-13, F-14, F-15 | `roadnetwork-opendrive`, `roadnetwork-usd` | new (0011) | The declared elements decode, validate, and author with golden semantics, and `ODR-W201` to `ODR-W203` count only what the scope leaves out. | — |
| 7 | UPA-01 | ADR 0009 probe and the `upAxis` argument | F-27 | `roadnetwork-usd`, `roadnetwork-opendrive` | [0009](../adr/0009-file-format-arguments-up-axis.md) | The [probe](../adr/0009-file-format-arguments-up-axis.md#evidence-needed-before-acceptance) is recorded in a report, ADR 0009 is accepted, and `upAxis=Y` through a reference places known points within 0.5 mm. | — |
| 8 | CMP-01 | Compatibility corpus regression | F-16 | `roadnetwork-opendrive` | — | The fixtures [FMT-02 §10](../reports/format-spike-opendrive-2026-11-23.md#10-fixtures) selects are committed with their licences or fetched by pinned checksum, and their decode counts and digests are checked in CI. | — |
| 9 | REL-02 | v0.2.0 | all | all | — | The [Phase 2 exit](release-plan.md#phase-2-exit) is met and v0.2.0 is released. | — |

## Next actions

1. **ADR-10.** Draft ADR 0010 from the
   [FMT-02 measurements](../reports/format-spike-opendrive-2026-11-23.md#4-plan-view-shapes):
   the spiral method for nearly constant curvature and its bound, and the
   `paramPoly3` parameter mapping.
2. **ADR-11.** Draft ADR 0011 for the signal, road-mark, and object
   elements in the [Phase 2 scope](release-plan.md#phase-2--opendrive-fidelity).

Update the capability matrix in the same pull request as each
implementation; a row without test evidence stays unsupported.
