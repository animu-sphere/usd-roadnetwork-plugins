# API boundary

- **Status:** Proposed (2026-11-22).

This document defines the audience of each surface the repository exposes and
which repository owns compatibility commitments.

## Repository ownership

| Repository | Owns |
| --- | --- |
| `usd-roadnetwork-plugins` | Format readers and decoders, `RoadNetworkData` and its validation, USD layer authoring, diagnostics, and the packaging and verification of its plugin artifacts. |
| `usd-mobility-runtime` (candidate) | Integrated runtime composition: traffic, vehicles, pedestrians, and collision built on the authored stages. Not a dependency of this repository. |
| [`open-strata`](https://github.com/animu-sphere/open-strata) | Build, inspection, test, and packaging tooling and the runtime matrix. |
| [`usd-stage-runner`](https://github.com/animu-sphere/usd-stage-runner) | Interactive runtime hosting (integration candidate). |
| [`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) | Remote-asset resolution (composition candidate). |

## Tiers

| Tier | Audience | Installed? | Change policy |
| --- | --- | --- | --- |
| **Public behavior** | Users and applications opening or composing a USD stage. | Plugin binaries and `plugInfo.json`; no C++ header. | Changes to stage output, file-format arguments, or diagnostics need compatibility review. |
| **Contributor** | Developers adding a format bundle in this workspace or a fork. | `roadnetwork-core` and `roadnetwork-usd` headers, as a build necessity. | Changes update every in-repo bundle together. No external API or ABI guarantee. |
| **Plugin-private** | One bundle's translation units. | No. | Free to change. |

## Observable plugin behavior

Once accepted, the public contract covers:

- the **authored stage contract** in [WORKSPACE.md §5](WORKSPACE.md#5-authored-stage-contract);
- the **file-format arguments**, documented in the
  [capability matrix](../reference/CAPABILITY_MATRIX.md);
- the **stable diagnostic codes**, each with a machine-readable catalog
  shipped in the bundle's resources; codes are never renumbered or reused.

Integrated compatibility claims belong to the runtime that integrates the
plugins and must identify the component versions they cover.
