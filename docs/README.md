# USD Road Network Plugins documentation

The project is in **Phase 0 (scaffold and ADRs)**. The workspace and an
empty OpenDRIVE bundle are scaffolded, but no format is implemented yet; documents marked *Proposal* or *Proposed* describe intent, not behavior.

Use the document that owns the question you are answering. Summaries should
link to the source; once code exists, implementation and tests are the
evidence for current behavior. If either changes, review the affected
documentation in the same PR.

| Type | Owns | Start here |
| --- | --- | --- |
| Entry | Project purpose and first use | [README](../README.md) |
| Index | Navigation and document ownership | This page |
| Normative | Design principles and accepted decisions | [Design policy](design/DESIGN_POLICY.md), [ADRs](adr/) |
| Architecture | Workspace structure, dependency directions, and API audiences | [Workspace](architecture/WORKSPACE.md), [API boundary](architecture/API_BOUNDARY.md) |
| Reference | Current behavior and semantic or stage contracts | [Capability matrix](reference/CAPABILITY_MATRIX.md), [OpenDRIVE mapping](reference/OPENDRIVE_MAPPING.md), [diagnostics](reference/DIAGNOSTICS.md), [supported configurations](reference/SUPPORTED_CONFIGURATIONS.md) |
| Plan | Release criteria and open work | [Roadmap](roadmap/) |
| Record | Published releases, measured results, and completed work | [Release records](releases/), [reports](reports/), [changelog](../CHANGELOG.md) |

## Where status belongs

| Question | Source of truth |
| --- | --- |
| What does the tree support now? | [Capability matrix](reference/CAPABILITY_MATRIX.md). Nothing is claimed until a deterministic test backs it. |
| How is an OpenDRIVE element mapped? | [OpenDRIVE mapping](reference/OPENDRIVE_MAPPING.md). |
| What does a diagnostic code mean? | [Diagnostics](reference/DIAGNOSTICS.md) and the bundle's catalog. |
| Which decision is open or accepted? | The [ADR index](adr/README.md). |
| What does the next release aim to ship? | [Release plan](roadmap/release-plan.md). |
| What work is assigned now? | [Current work](roadmap/current.md), with issues or PRs for task-level status. |
| What remains unscheduled? | [Backlog](roadmap/backlog.md), with an owner and a trigger to revisit each item. |
| What shipped and what passed for a tag? | Its [release record](releases/). The changelog describes user-visible changes. |

## Maintenance

- Agree on changes to normative rules during review; record a new ADR when a
  decision changes.
- Update references with the implementation and tests that support the claim.
  Keep plans and version promises out of reference descriptions.
- Keep procedures reproducible and name the runtime and platform they require.
- Move finished work out of the roadmap. Keep release records and measured
  results as evidence for their original version; use an erratum for a
  factual correction.
- Route evidence from other repositories to its owner and link to a pinned
  record when available.
- Public Markdown is English; keep links relative and resolving.

## Component documentation

| Component | Kind | Documentation |
| --- | --- | --- |
| `roadnetwork-opendrive` | plugin bundle | [Plugin README](../plugins/roadnetwork-opendrive/README.md) (scaffold) |
| `roadnetwork-core` | library | planned: `libs/roadnetwork-core/README.md` |
| `roadnetwork-usd` | library | planned: `libs/roadnetwork-usd/README.md` |
