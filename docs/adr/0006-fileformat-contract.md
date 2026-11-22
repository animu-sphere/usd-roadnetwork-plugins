# ADR 0006 — `SdfFileFormat` metadata-only, reload, and error contract

- **Status:** Accepted (2026-11-22)
- **Scope:** the behavior shared by every bundle's `SdfFileFormat`
  implementation, and the diagnostic numbering scheme

## Context

The design policy ([§10](../design/DESIGN_POLICY.md#10-file-format-plugin-contract),
[§11](../design/DESIGN_POLICY.md#11-diagnostics)) fixes the principles:
read-only, cheap `CanRead()`, honored `metadataOnly`, explicit failure with
stable codes, determinism, and reload consistency verified on the real
runtime. The numbering scheme must be fixed **before** implementation so that
codes never need renumbering.

Reading OpenUSD 26.05 (`pxr/usd/sdf/layer.cpp`, `fileFormat.h`) and probing
the 26.08 runtime established four facts this decision depends on:

1. **Sdf does not call `CanRead()` when it opens a layer.** It selects the
   format by extension, or by an explicit format argument. `CanRead()` is
   called only by tools such as `usddiff` and by usdz packaging, which
   delegates to the packaged file's format.
2. **`metadataOnly` is a hint.** Only `SdfLayer::OpenAsAnonymous(...,
   metadataOnly=true)` requests it; stage composition always performs a full
   read, and reloading such a layer performs a full read too.
3. **`SdfLayer::Reload` calls `Read()` inside an `SdfChangeBlock`**, and
   `SdfLayer::ReloadLayers` opens an outer one around several reloads.
4. **Inside an `SdfChangeBlock`, authoring through a temporary `UsdStage`
   fails** (`Failed to define UsdPrim </Asset>` from `UsdGeom.Xform.Define`
   on an in-memory stage), while authoring Sdf specs (`SdfPrimSpec`,
   `defaultPrim`) succeeds. This is the condition `usd-3dgs-plugins` works
   around by authoring on a worker thread, where the thread-local block is
   not observed.

## Decision

### 1. `CanRead()`

- Returns `true` only when the extension is the bundle's, **and** the first
  element of the document is the format's root element (`OpenDRIVE` for
  `roadnetwork-opendrive`). Before it, only a byte-order mark, the XML
  declaration, comments, processing instructions, a document type
  declaration, and whitespace may appear.
- Reads at most the first **64 KiB**, through `ArResolver::OpenAsset`, so
  resolver-backed assets behave like files.
- Any failure (open, read, decoding, a missing or different root element, or
  an exception) returns `false`. `CanRead()` posts no diagnostics.
- Because Sdf routes by extension, `CanRead()` is advice to tools, not a
  routing guarantee. `Read()` performs the same structural check itself and
  fails with a stable code. A bundle that shares an extension with another
  format cannot rely on `CanRead()` to separate them (see
  [ADR 0008](0008-lanelet2-osm-identification.md)).

### 2. `metadataOnly`

A metadata-only read authors:

- the layer metadata, `defaultPrim`, and the `/Asset` root of the
  [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract);
- import provenance on `/Asset` taken from the source header (source format
  and version) and element counts from a scan that does not decode geometry,
  profiles, or lanes;
- the `/Asset/Network` scope, empty.

It authors no per-element prims and no `/Asset/Visualization`. Errors in the
container, header, or root element are reported as in a full read; deeper
content is not validated, so a successful metadata-only read does not imply
that a full read succeeds. The prim names and provenance keys are those the
stage contract fixes (ADRs 0002 and 0005).

### 3. Errors

- A failed `Read()` posts exactly one `TF_RUNTIME_ERROR` whose message starts
  with the bracketed code (`[ODR-E001] ...`), names the source asset, and
  names the source element and ID when the failure has one. It then returns
  `false`.
- A failed `Read()` leaves the target layer unchanged. The shared writer
  authors into a scratch layer and transfers it into the target only on
  success; on reload, the previous content therefore survives a failed read.
- No exception escapes `Read()` or `CanRead()`.

### 4. Warnings

- A warning is posted with `TF_WARN`, its message starting with the bracketed
  code, and is allowed only for an explicit, bounded loss
  ([diagnostics rules](../reference/DIAGNOSTICS.md#rules)).
- Each code is posted at most once per read, with the number of affected
  elements and the IDs of at most the first five.
- Whether warning codes are also recorded on `/Asset` is decided with the
  stage contract (ADR 0002).

### 5. Reload and authoring thread

- `Read()` authors on the calling thread. There is **no worker-thread
  authoring**.
- The shared writer authors **Sdf specs only**, never through a `UsdStage` or
  a schema API that requires one. `roadnetwork-usd` links the OpenUSD
  `arch tf vt sdf` components and not `usd`. Against a per-component OpenUSD
  build, a `UsdStage` dependency in the writer makes its unit test fail to
  link. Review is the guard otherwise, because the bundle links `usd` itself.
- The reload and change-block tests below are rerun whenever the OpenUSD
  runtime is re-pinned.

### 6. Diagnostic numbering

The [proposed scheme](../reference/DIAGNOSTICS.md) is ratified:
`<PREFIX>-<E|W><NNN>`, with the hundreds digit as the category.

- **`ODR-`** codes belong to the `roadnetwork-opendrive` reader and decoder.
- **`RN-`** codes belong to `roadnetwork-core` validation and `roadnetwork-usd`
  authoring, independent of the source format. They are raised only by
  shared library code; a bundle relays them unchanged. The same condition
  has the same code in every format.
- Each owner keeps one catalog (format `roadnetwork.diagnostics/v1`, below).
  Every bundle ships its own catalog as `diagnostics.json` and a copy of the
  `RN-` catalog as `rn-diagnostics.json` in its plugin resources.
- Codes are never renumbered or reused. A retired code stays in its catalog
  with `"retired": "<version>"`.

Catalog format:

```json
{
    "schema": "roadnetwork.diagnostics/v1",
    "prefix": "ODR",
    "owner": "roadnetwork-opendrive",
    "description": "...",
    "diagnostics": [
        { "code": "ODR-E001", "severity": "error", "summary": "...", "action": "..." }
    ]
}
```

Codes are listed in ascending order; `severity` is `error` for `E` and
`warning` for `W`.

### 7. File-format arguments

Phase 1 exposes **no** file-format arguments. An argument is added only by
the ADR that needs it (for example a tessellation tolerance under ADRs 0004
and 0007) and is documented in the
[capability matrix](../reference/CAPABILITY_MATRIX.md) in the same PR.

## Consequences

- Phase 1 implements the `CanRead()` sniff, the metadata-only path, and the
  first codes. Codes are assigned in the PR that first raises them, together
  with their catalog entries and a negative fixture.
- [DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) is normative. The catalogs
  start empty in Phase 0.
- The design policy's
  [§10.2](../design/DESIGN_POLICY.md#102-canread-is-a-cheap-routing-decision)
  is read with fact 1 above: a "routing decision" means advice to tools.
- ADR 0008 cannot separate Lanelet2 from general OpenStreetMap `.osm` through
  `CanRead()` alone.

## Evidence

Phase 0, on the empty bundle (local Windows, `ost` 0.23.21, OpenUSD 26.08;
the `ost test` CTest suite):

| Test | Checks |
| --- | --- |
| `roadnetworkOpendrive_file_format_contract` ([source](../../plugins/roadnetwork-opendrive/tests/test_file_format_contract.py)) | Registration (`formatId`, target, extension, `CanRead`); read-only (`SupportsWriting()` is false and exporting to `.xodr` fails without leaving a file); open; sublayer and reference composition through the plugin; a metadata-only read agrees with a full read; two reads are identical; forced reload, reload under an outer `Sdf.ChangeBlock`, `ReloadLayers`, skipped unforced reload, and an unforced reload after an on-disk change each succeed and send `LayerDidReloadContent` exactly when Sdf re-reads. |
| `roadnetworkUsd_layer_writer` ([source](../../libs/roadnetwork-usd/tests/test_layer_writer.cpp)) | The writer replaces existing content, is deterministic, and gives the same result inside an `SdfChangeBlock` as outside. |
| `roadnetworkOpendrive_diagnostics_catalog` ([source](../../plugins/roadnetwork-opendrive/tests/test_diagnostics_catalog.py)) | Both catalogs follow the format; every bracketed code used in the bundle or library sources is in its owner's catalog, and each owner raises only its own prefix. A scratch-copy mutation raising `[ODR-E001]` and `[RN-W201]` from the bundle was rejected on both counts. |

Fact 4 was observed with this snippet under the runtime's Python:

```python
with Sdf.ChangeBlock():
    stage = Usd.Stage.CreateInMemory()
    UsdGeom.Xform.Define(stage, "/Asset")      # Tf error: Failed to define UsdPrim </Asset>
    layer = Sdf.Layer.CreateAnonymous(".usda")
    Sdf.PrimSpec(layer, "Asset", Sdf.SpecifierDef, "Xform")   # succeeds
```

Phase 1, the ODR-01 reader (same configuration):

| Test | Checks |
| --- | --- |
| `roadnetworkOpendrive_reader` ([source](../../plugins/roadnetwork-opendrive/tests/test_reader.cpp)) | The §1 sniff on 24 prologue cases and a root beyond 64 KiB; each `ODR-E002` to `ODR-E007` condition; the version range; the header scan. |
| `roadnetworkOpendrive_file_format_contract` | `CanRead()` on the prologue and negative fixtures, and quietly false on a missing asset; each negative fixture fails with exactly one error that starts with its code and names the asset, in full and metadata-only reads; a failed forced reload posts one `ODR-E002` and keeps the previous layer content, and the repaired file reloads. |

Phase 1, the ODR-02 decoder (same configuration):

| Test | Checks |
| --- | --- |
| `roadnetworkOpendrive_decoder` ([source](../../plugins/roadnetwork-opendrive/tests/test_decoder.cpp)) | The minimal network decodes to the expected model and lane graph. Each decoder and validation fixture fails with its code, and a metadata-only read of it, which decodes only the header, succeeds. Number parsing is checked under a comma-decimal C locale. |
| `roadnetworkOpendrive_file_format_contract` | Each decoder fixture fails through the plugin with exactly one error, in a full read only. A successful read posts each warning code once (§4): the network, its metadata-only read, `basic.xodr`, and CARLA `Town01` and `Town02`. A failed read posts no warning. |

The metadata-only `/Asset` and its provenance are checked once the stage
contract (USD-01) authors them.
