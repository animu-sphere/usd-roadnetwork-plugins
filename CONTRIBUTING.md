# Contributing

The project is in Phase 0: the workspace and an empty `roadnetwork-opendrive`
bundle are scaffolded, and the initial ADRs are being drafted.

## Development path

```sh
git clone https://github.com/animu-sphere/usd-roadnetwork-plugins
cd usd-roadnetwork-plugins
ost runtime pull cy2026 --profile usd                       # once
ost plugin build plugins/roadnetwork-opendrive              # build
ost plugin doctor plugins/roadnetwork-opendrive             # L0-L1 diagnostics
ost plugin test plugins/roadnetwork-opendrive --up-to 5     # the gate PRs must pass
ost plugin package plugins/roadnetwork-opendrive
```

Without `ost`, the dual-mode root builds every bundle with plain CMake:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<your-openusd-install>
cmake --build build
```

Confirm the commands against the `ost` version recorded in
[supported configurations](docs/reference/SUPPORTED_CONFIGURATIONS.md).
Pull requests run the same lifecycle on hosted Windows, Ubuntu, and macOS
runners. The CI contract is [`openstrata.ci.yaml`](openstrata.ci.yaml); after
editing it, regenerate the workflow with
`ost ci generate github --reusable-source --out .github/workflows/ost-source-ci.yml --force`
and commit both files. A
plain CMake/CTest path for local unit tests will be documented alongside.

## Where things go

| Change | Where |
| --- | --- |
| Parsing and semantic decoding | the format bundle's `src/io/` — USD-free and unit-tested |
| Format-independent model and validation | `libs/roadnetwork-core/` |
| USD authoring | `libs/roadnetwork-usd/` — shared by every bundle |
| Test inputs | the bundle's fixtures, small and redistributable, with provenance |
| New diagnostics | the bundle's diagnostics header **and** its catalog; codes are never renumbered or reused |
| Behavior claims | `docs/reference/` — a claim without a fixture is a bug |

## Ground rules

- Follow the [design policy](docs/design/DESIGN_POLICY.md); the
  [docs index](docs/README.md) explains which document owns which question.
- A decision that changes a rule needs an [ADR](docs/adr/).
- Every behavior change lands with a deterministic test; malformed input gets
  a fixture and a stable diagnostic.
- No silent fallback: a broken road, lane, or junction relationship is an
  error, not an approximation.
- No external data enters the repository without license and provenance
  review.
- Third-party dependencies are pinned and recorded in
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- Public Markdown is English; keep links relative and resolving.

## Documentation review

| Change | Review |
| --- | --- |
| Reader, decoder, or diagnostics | [Capability matrix](docs/reference/CAPABILITY_MATRIX.md), [OpenDRIVE mapping](docs/reference/OPENDRIVE_MAPPING.md), [diagnostics](docs/reference/DIAGNOSTICS.md), and fixtures |
| Shared model or validation | [ADR 0001](docs/adr/0001-roadnetwork-data-model.md), cross-format tests, and a new ADR if a decision changes |
| USD authoring | [Stage contract](docs/architecture/WORKSPACE.md#5-authored-stage-contract), mappings, stage assertions, and migration impact |
| CI, packaging, or release tooling | [Supported configurations](docs/reference/SUPPORTED_CONFIGURATIONS.md) and the [release gate](docs/releases/README.md#release-gate) |
| New bundle | [Workspace contract](docs/architecture/WORKSPACE.md), bundle README, and the root README's format summary |
| Phase scope or status | [Release plan](docs/roadmap/release-plan.md), [current work](docs/roadmap/current.md), and [backlog](docs/roadmap/backlog.md); each item has one home |
