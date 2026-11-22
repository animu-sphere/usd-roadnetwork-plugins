# ADR 0006 — `SdfFileFormat` metadata-only, reload, and error contract

- **Status:** Proposed (2026-11-22)
- **Scope:** the behavior shared by every bundle's `SdfFileFormat`
  implementation, and the diagnostic numbering scheme

## Context

The design policy ([§10](../design/DESIGN_POLICY.md#10-file-format-plugin-contract),
[§11](../design/DESIGN_POLICY.md#11-diagnostics)) fixes the principles:
read-only, cheap `CanRead()`, honored `metadataOnly`, explicit failure with
stable codes, determinism, and reload consistency verified on the real
runtime. The numbering scheme must be fixed **before** implementation so that
codes never need renumbering.

`usd-3dgs-plugins` authors on a worker thread to avoid an outer
`SdfChangeBlock` during reload. The policy forbids copying that workaround
without reproducing the problem here.

## Questions

1. **`CanRead()`:** which bytes may it read (extension plus XML prolog and
   root element), and what is its failure behavior?
2. **`metadataOnly`:** which structure and metadata does a metadata-only read
   author (for example `/Asset`, `/Asset/Network`, counts from the header or
   a cheap scan, version, CRS)?
3. **Errors:** how is a failed `Read()` reported (TF error with a bracketed
   code, return value), and is a partially authored layer ever left behind?
4. **Warnings:** how are warnings aggregated and surfaced?
5. **Reload:** does direct authoring under the reload change block preserve
   the stage contract on the target OpenUSD version?
6. **Numbering:** is the [proposed scheme](../reference/DIAGNOSTICS.md)
   accepted, including the split between bundle prefixes (`ODR-`) and a
   shared `RN-` prefix?
7. **File-format arguments:** which, if any, are exposed in Phase 1 (for
   example a tessellation tolerance)?

## Initial direction

- `CanRead()` reads at most the XML prolog and root element and returns
  `false` on any failure.
- A failed `Read()` leaves the target layer unchanged.
- Adopt the proposed numbering scheme, ratified before the first code.
- Write a reload test first; adopt a workaround only if that test fails.

## Evidence needed before acceptance

- Reload, reference, and `metadataOnly` tests on the empty Phase 0 bundle.
- The catalog format and the test that cross-checks codes against source.
