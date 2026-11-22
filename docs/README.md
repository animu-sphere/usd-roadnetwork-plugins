# USD Road Network Plugins documentation

Every question has **one owning document**. Other documents link to the
owner instead of restating it, so a fact changes in one place. The active
phase and its work are in [current work](roadmap/current.md); what the tree
supports is in the [capability matrix](reference/CAPABILITY_MATRIX.md).
Documents marked *Proposal* or *Proposed* describe intent, not behavior.

## Document ownership

| Type | Document | Owns | Does not own (link instead) |
| --- | --- | --- | --- |
| Entry | [README](../README.md) | Project purpose and where to start | Status, format priorities, scope details |
| Index | This page | Navigation, ownership, and the sync rules | Any project fact |
| Normative | [Design policy](design/DESIGN_POLICY.md) | Vision, principles, scope, non-goals, and engineering rules | Decisions an ADR makes, feature lists, current state |
| Normative | [ADRs](adr/) | One decision each; the [index](adr/README.md) owns their status | Work tracking |
| Architecture | [Workspace contract](architecture/WORKSPACE.md) | Components and their responsibilities, dependency directions, layout, stage-contract invariants | Implementation state; the prim tree (ADR 0005) |
| Architecture | [API boundary](architecture/API_BOUNDARY.md) | Repository ownership, API tiers, integration contract notes | Runtime design |
| Reference | [Capability matrix](reference/CAPABILITY_MATRIX.md) | What the tree supports, with evidence | Plans |
| Reference | [OpenDRIVE mapping](reference/OPENDRIVE_MAPPING.md) | Per-element mapping and versions | Rationale (ADRs) |
| Reference | [Diagnostics](reference/DIAGNOSTICS.md) | Code format, prefixes, assigned codes and what raises them | Summaries and actions (the catalogs) |
| Reference | [Supported configurations](reference/SUPPORTED_CONFIGURATIONS.md) | Target and exercised runtimes, toolchains, platforms, CI cells | Format behavior |
| Plan | [Release plan](roadmap/release-plan.md) | Phases, the `F-` feature list, exit criteria, the format acceptance gate | Task status |
| Plan | [Current work](roadmap/current.md) | Active workstreams, their order, and tracking | Scope and exit criteria |
| Plan | [Backlog](roadmap/backlog.md) | Unscheduled work and decisions to record later, each with an owner and a trigger | Scheduled work |
| Record | [Release records](releases/), [reports](reports/), [changelog](../CHANGELOG.md) | What shipped, what was measured, user-visible changes | Current state |
| Procedure | [CONTRIBUTING.md](../CONTRIBUTING.md) | Build, test, CI, and review steps | Policy |
| Component | Bundle and library READMEs (below) | The component's current state, layout, and local commands | Cross-component rules |

## Where status belongs

| Question | Source of truth |
| --- | --- |
| What does the tree support now? | [Capability matrix](reference/CAPABILITY_MATRIX.md). Nothing is claimed until a deterministic test backs it. |
| How is an OpenDRIVE element mapped? | [OpenDRIVE mapping](reference/OPENDRIVE_MAPPING.md). |
| What does a diagnostic code mean? | [Diagnostics](reference/DIAGNOSTICS.md) and the owner's catalog. |
| Which decision is open or accepted? | The ADR file's own `Status` line, mirrored by the [ADR index](adr/README.md). |
| What does a phase deliver, and when is it done? | [Release plan](roadmap/release-plan.md). |
| What work is assigned now? | [Current work](roadmap/current.md), with issues or PRs for task-level status. |
| What remains unscheduled? | [Backlog](roadmap/backlog.md). |
| What shipped and what passed for a tag? | Its [release record](releases/). The changelog describes user-visible changes. |

## Keeping documents in sync

- **Link, do not copy.** A summary names its owner and links to it. Copy a
  value only when a reader needs it in place, and then make it checkable
  (below).
- **No dated status prose outside the owners.** Pages such as this index,
  the root README, and CONTRIBUTING describe where status lives, not what it
  is.
- **IDs connect the plan to the evidence.** The release plan defines each
  feature ID (`F-01` …) once; workstreams and capability rows cite it.
- **Change the owner in the same PR as the code.** The
  [documentation review table](../CONTRIBUTING.md#documentation-review)
  lists which owners a change touches.
- **Records are not rewritten.** Release records and reports stay as
  evidence for their date; use an erratum or a later report for a
  correction. When a report disagrees with a current-state document, the
  current-state document wins.
- **A proposal changes nothing by itself.** A rule changes when its owner
  (usually an ADR) changes; review drafts and external proposals are folded
  into the owners, not kept beside them.
- Public Markdown is English; keep links relative and resolving.

[`tools/check_docs.py`](../tools/check_docs.py) enforces the mechanical part
of these rules on every pull request and push to `main`
([`docs.yml`](../.github/workflows/docs.yml)):

| Check | Fails when |
| --- | --- |
| Links | A relative link, a link to this repository on GitHub, or a heading anchor does not resolve. |
| ADR index | An ADR is missing from the index, or its title or status differs from the ADR file. |
| Feature IDs | An `F-` ID is cited but not defined in the release plan, or is defined twice. |
| Capability matrix | A row's status is not in the vocabulary, or a supported row cites no evidence. |
| Diagnostics | The codes listed in the diagnostics reference differ from the codes in the catalogs. |

Run it locally with `python tools/check_docs.py`.

## Component documentation

| Component | Kind | Documentation |
| --- | --- | --- |
| `roadnetwork-opendrive` | plugin bundle | [Plugin README](../plugins/roadnetwork-opendrive/README.md) |
| `roadnetwork-core` | library | [Library README](../libs/roadnetwork-core/README.md) |
| `roadnetwork-usd` | library | [Library README](../libs/roadnetwork-usd/README.md) |
