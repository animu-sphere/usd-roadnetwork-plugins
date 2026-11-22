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
- The diagnostic numbering scheme is fixed before the first code is assigned
  ([ADR 0006](../adr/0006-fileformat-contract.md)).
- The `usd-3dgs-plugins` worker-thread reload workaround is not copied unless
  a reload test reproduces the need.

## Active work

All workstreams are owned by `usd-roadnetwork-plugins`. Add issue or PR links
as work starts.

| ID | Work | Exit criterion | Tracking |
| --- | --- | --- | --- |
| DOC-01 | Initial documentation set | Design policy, architecture, reference skeletons, ADR drafts, and roadmap are in the tree and linked from the [index](../README.md). | this PR |
| SCF-01 | Scaffold the OpenStrata workspace | `ost` scaffold output committed; [WORKSPACE.md](../architecture/WORKSPACE.md) paths and identities updated to match; `ost plugin inspect` and `ost plugin doctor` pass. | — |
| SCF-02 | Empty bundle and libraries pass the package lifecycle | The empty `.xodr` bundle registers, `ost plugin test --workspace --up-to 5`, `ost plugin package`, and `ost plugin test --from-package` pass on the local host. | — |
| CI-01 | Source CI | Hosted build and test run on the declared platforms; [supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md) updated with run links. | — |
| FMT-01 | OpenDRIVE format spike | A [report](../reports/) records the specification versions and how to recognize them, specification access and licensing, coordinate and georeference conventions, the elements needed for Phase 1, candidate XML parsers and existing OpenDRIVE libraries with licenses, and redistributable fixtures with provenance. | — |
| ADR-06 | Accept ADR 0006 | Numbering scheme, `CanRead()`, `metadataOnly`, and error behavior accepted; reload test on the empty bundle written. | — |
| ADR-01..05 | Draft ADRs 0001–0005 to acceptance-ready | Each ADR lists the evidence it needs and a Phase 1 owner. Acceptance may land early in Phase 1. | — |

## Phase exit

- The empty bundle and both libraries pass the package lifecycle from source
  and from the installed package.
- ADR 0006 is accepted; ADRs 0001–0005 are ready for review.
- The FMT-01 report fixes the Phase 1 OpenDRIVE versions and fixtures.
