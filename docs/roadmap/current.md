# Current: Phase 0 — Scaffold and ADRs

## Goal

Stand up the OpenStrata workspace with an empty `roadnetwork-opendrive`
bundle and the `roadnetwork-core` and `roadnetwork-usd` libraries, passing
build, test, and package verification; and settle the decisions Phase 1
needs before any decoder code is written.

## Decisions

- OpenDRIVE is the first format (design policy, 2026-11-22).
- The plugins are read-only.
- The project is licensed under Apache-2.0, like its sibling OpenStrata
  projects (2026-11-22).
- ADR 0006 is accepted (2026-11-22): `RN-` shared plus `ODR-` bundle codes,
  a metadata-only read authors the header-derived root only, `CanRead()`
  sniffs the root element within 64 KiB, and the writer authors Sdf specs on
  the calling thread ([ADR 0006](../adr/0006-fileformat-contract.md)).
- The `usd-3dgs-plugins` worker-thread reload workaround is not copied: the
  problem it solves (authoring through a `UsdStage` inside the reload's
  `SdfChangeBlock`) was reproduced on OpenUSD 26.08 and is avoided by
  authoring Sdf specs only.

## Active work

All workstreams are owned by `usd-roadnetwork-plugins`. Add issue or PR links
as work starts.

| ID | Work | Exit criterion | Tracking |
| --- | --- | --- | --- |
| DOC-01 | Initial documentation set | Design policy, architecture, reference skeletons, ADR drafts, and roadmap are in the tree and linked from the [index](../README.md). | this PR |
| SCF-01 | Scaffold the OpenStrata workspace | `ost` scaffold output committed; [WORKSPACE.md](../architecture/WORKSPACE.md) paths and identities updated to match; `ost plugin inspect` and `ost plugin doctor` pass. | done 2026-11-22 (local Windows): root and bundle regenerated with `ost` 0.23.21; inspect and doctor pass after build; `ost plugin test --up-to 5` passes L0–L4, L5 skipped (no golden) |
| SCF-02 | Empty bundle and libraries pass the package lifecycle | `roadnetwork-core` and `roadnetwork-usd` added with library descriptors and wired into the bundle's `requires.libraries`; the scaffold placeholder fixtures replaced by minimal OpenDRIVE XML; `ost plugin test --workspace --up-to 5` (including an L5 golden), `ost plugin package`, and `ost plugin test --from-package` pass on the local host. | done 2026-11-22 (local Windows, `ost` 0.23.21, OpenUSD 26.08): `--graph-only` valid (1 bundle, 2 libraries, 3 library edges); workspace test L0–L5 pass; package, then `--from-package --up-to 5` L0–L5 pass; `ost library test` and the composed root `ost test` pass |
| CI-01 | Source CI | Hosted build and test run on the declared platforms; [supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md) updated with run links. | done 2026-11-22 ([#2](https://github.com/animu-sphere/usd-roadnetwork-plugins/pull/2)): the `pr` cells pass on hosted Windows, Ubuntu, and macOS ([run 45006974286](https://github.com/animu-sphere/usd-roadnetwork-plugins/actions/runs/45006974286)); workspace cells stop at `test` until [open-strata#304](https://github.com/animu-sphere/open-strata/issues/304) is fixed |
| FMT-01 | OpenDRIVE format spike | A [report](../reports/) records the specification versions and how to recognize them, specification access and licensing, coordinate and georeference conventions, the elements needed for Phase 1, candidate XML parsers and existing OpenDRIVE libraries with licenses, and redistributable fixtures with provenance. | — |
| ADR-06 | Accept ADR 0006 | Numbering scheme, `CanRead()`, `metadataOnly`, and error behavior accepted; reload test on the empty bundle written. | done 2026-11-22: ADR accepted; contract, writer, and catalog tests pass under `ost test` (local Windows, OpenUSD 26.08) |
| ADR-01..05 | Draft ADRs 0001–0005 to acceptance-ready | Each ADR lists the evidence it needs and a Phase 1 owner. Acceptance may land early in Phase 1. | — |

## Phase exit

- The empty bundle and both libraries pass the package lifecycle from source
  and from the installed package.
- ADR 0006 is accepted (done); ADRs 0001–0005 are ready for review.
- The FMT-01 report fixes the Phase 1 OpenDRIVE versions and fixtures.
