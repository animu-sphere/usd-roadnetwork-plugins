# ADR 0004 — OpenDRIVE curve and cross-section retention; tessellation

- **Status:** Proposed (2026-11-22)
- **Scope:** how OpenDRIVE plan-view geometry, profiles, and lane width
  functions are kept in `RoadNetworkData` and how derived geometry is sampled

## Context

OpenDRIVE describes geometry analytically: plan-view segments (line, arc,
spiral, poly3, paramPoly3), cubic elevation and superelevation along `s`, and
cubic lane width or border functions per lane section. USD visualization needs
sampled curves and meshes. The design policy's initial direction
([§5.3](../design/DESIGN_POLICY.md#53-intermediate-representation)) is to keep
analytic information where possible and treat discretization as a separate,
explicit step.

## Questions

1. Does the IR store analytic parameters only, samples only, or both?
2. Are the analytic parameters also authored to USD (semantic layer), so that
   consumers can re-evaluate them, or only the samples?
3. Tessellation: by fixed `s` step, by chord or angular tolerance, or
   adaptively? Is the tolerance a file-format argument?
4. Which numeric method evaluates spirals (Fresnel integrals, series,
   numeric integration), and to what tolerance against reference values?
5. How are discontinuities between segments or lane sections detected and
   reported?

## Options considered

- **A. Analytic in the IR and in USD; samples derived on demand.** Lossless;
  consumers need an evaluator to use the semantic data.
- **B. Analytic in the IR; samples in USD.** Simple consumers; source
  parameters are kept only as provenance.
- **C. Samples only.** Rejected: lossy by construction and contrary to the
  policy's initial direction.

## Initial direction

Analytic in the IR (A or B for USD, decided with ADR 0002). Tessellation is a
`roadnetwork-core` function with a documented tolerance, tested against
closed-form solutions for line and arc and reference values for spirals.

## Evidence needed before acceptance

- Numeric tests for line and arc position and tangent at known `s`.
- A spiral evaluation method with a stated error bound.
- The tessellation tolerance default and its effect on a small fixture.
