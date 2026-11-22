# ADR 0009 — File-format arguments: up-axis presentation, static and dynamic

- **Status:** Proposed (2026-11-22)
- **Scope:** the file-format arguments every bundle accepts, starting with
  `upAxis`; how they reach `Read()` from an asset path and from payload
  metadata
- **Needed by:** Phase 2 (F-27, static argument) and Phase 3 (F-28, dynamic
  payload) in the [release plan](../roadmap/release-plan.md)

## Context

[ADR 0003](0003-crs-axes-units.md#1-axes-and-units) authors every layer
`Z`-up with `metersPerUnit = 1`, because OpenDRIVE's inertial frame is
`z`-up in metres, and applies no axis conversion. Most consumers of road
networks (simulators, robotics, GIS) are `Z`-up, but many DCC tools and
pipelines are `Y`-up.

Layer metadata does not reach a referencing stage
([ADR probes §4](../reports/adr-probes-2026-11-22.md#4-sample-stage)), so
setting `upAxis` alone does not help a `Y`-up consumer; only a transform on
the referenced prims does. Today that consumer writes the transform itself.

[ADR 0006 §7](0006-fileformat-contract.md#7-file-format-arguments) exposes
no file-format arguments in Phase 1 and adds one only through the ADR that
needs it. This is that ADR.

OpenUSD offers two ways to pass arguments to an `SdfFileFormat`:

- **Static:** arguments embedded in the asset path
  (`road.xodr:SDF_FORMAT_ARGS:upAxis=Y`), or passed to
  `SdfLayer::FindOrOpen(path, args)`. They work for sublayers, references,
  and payloads, and they are part of the layer identifier.
- **Dynamic:** a file format that implements
  `SdfDynamicFileFormatInterface` computes arguments from metadata on the
  prim that holds a **payload**, during composition. Editing that metadata
  recomposes the payload with new arguments. It does not apply to references
  or sublayers.

## Questions

1. Is a `Y`-up presentation offered by the importer at all, or left to the
   consumer?
2. What does the argument change, and what must it leave untouched?
3. Static, dynamic, or both; and if both, which wins when they disagree?
4. Who parses and validates arguments, and how are invalid values reported?

## Options considered

- **A. No argument.** The consumer adds the transform. Nothing to maintain,
  but every `Y`-up consumer repeats the same, easy-to-get-wrong rotation.
- **B. Static argument only.** Cheap and works everywhere, but switching the
  presentation means editing asset paths.
- **C. Dynamic payload only.** Switchable from prim metadata, but payloads
  only, and it costs every payload to the format a composition hook.
- **D. Both, over one argument.** The dynamic interface only turns metadata
  into the same argument that the static path carries.

## Proposed decision

Option D, delivered in stages: the static argument first (F-27), the
dynamic interface on top of it later (F-28). Phase 1 still exposes no
arguments, so ADR 0006 §7 holds for v0.1.0.

### 1. The `upAxis` argument

| Argument | Values | Default |
| --- | --- | --- |
| `upAxis` | `Z`, `Y` | `Z` (the argument is absent) |

With `upAxis=Y` the shared writer authors two things, and nothing else:

- the layer metadata `upAxis = "Y"`;
- on `/Asset`, `xformOp:rotateX = -90` with `xformOpOrder = ["xformOp:rotateX"]`.
  It maps the source `+z` to `+y` and the source `+y` to `−z`, and keeps the
  frame right-handed.

Everything else is unchanged:

- every value under `/Asset/Network` stays in the source frame, as
  [ADR 0004 §1](0004-opendrive-geometry-retention.md#1-retention) requires;
- `/Asset/Visualization` keeps its points and its origin shift
  ([ADR 0003 §5](0003-crs-axes-units.md#5-precision)), which now compose
  under the rotation;
- `metersPerUnit`, prim paths, and relationships do not change.

Because the rotation sits on `/Asset`, a reference or payload carries it
into the consuming stage. `roadnetwork:import:upAxis` (`token`) records the
value on `/Asset` when the argument is given, and is not authored otherwise,
so a layer read without arguments is unchanged from ADR 0002 §3.

### 2. Parsing and validation

- `roadnetwork-usd` owns argument parsing, so that every bundle accepts the
  same arguments with the same meaning.
- An invalid value of a known argument (for example `upAxis=X`) fails the
  read with one `RN-` error, under the
  [error contract](0006-fileformat-contract.md#3-errors); the code is
  assigned in the PR that implements it.
- Unknown keys are ignored, because OpenUSD itself passes arguments such as
  `target`.
- A metadata-only read applies the argument too, so that both reads agree.
- Each argument and its values are documented in the
  [capability matrix](../reference/CAPABILITY_MATRIX.md#file-format-arguments)
  in the PR that implements it.

### 3. Static arguments (F-27)

- Accepted from the asset path and from `FindOrOpen` arguments.
- Layers opened with different arguments are different layers, cached and
  reloaded independently. Paths and values are a pure function of the
  source and the arguments ([ADR 0005 §7](0005-prim-path-and-id-stability.md#7-determinism-and-reload)).

### 4. Dynamic payload (F-28)

- The bundle's file format implements `SdfDynamicFileFormatInterface`.
- One prim metadata field, a `token` registered in the bundle's
  `plugInfo.json`, carries the up axis. Its name is fixed by the probe
  (below); the candidate is `roadnetworkUpAxis`.
- `ComposeFieldsForFileFormatArguments` reads only that field and sets
  `upAxis`. It does no I/O, is thread-safe, and returns quickly.
  `CanFieldChangeAffectFileFormatArguments` returns `true` only for that
  field and only when the value changes.
- When an asset path already carries `upAxis`, the asset path wins, so that
  an explicit argument is never overridden silently. If OpenUSD composes the
  two differently, this ADR follows OpenUSD and documents it.
- References and sublayers do not see the metadata; this is stated in the
  capability matrix row.

## Consequences

- ADR 0003 keeps `Z`-up as the authored default and links here for a
  `Y`-up presentation.
- Argument names and values join the public behavior tier
  ([API boundary](../architecture/API_BOUNDARY.md#tiers)); renaming one is a
  breaking change.
- Later arguments (for example a tessellation tolerance under ADR 0004 and
  ADR 0007) follow §2 and are added by amending this ADR or by their own ADR.
- Switching the dynamic metadata re-reads and re-parses the source for the
  new arguments; the cost is that of a full read.

## Evidence needed before acceptance

A probe on OpenUSD 26.08, recorded in [reports/](../reports/):

- `SDF_FORMAT_ARGS` reaches `Read()` through a sublayer, a reference, and a
  payload, and different arguments produce separately cached layers that
  reload independently.
- A `Y`-up layer referenced into a `Y`-up stage places a known point at the
  expected world position, and the origin shift of ADR 0003 still recovers
  source positions within 0.5 mm.
- The naming rules for a registered prim metadata field, and whether a
  namespaced name is accepted.
- How Pcp combines asset-path arguments with dynamically composed ones, so
  that §4's precedence can be implemented or corrected.
- Recomposition when the metadata changes, and the composition cost of the
  dynamic hook on a stage with many payloads.
