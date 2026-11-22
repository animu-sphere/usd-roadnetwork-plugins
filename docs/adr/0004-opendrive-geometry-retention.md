# ADR 0004 — OpenDRIVE curve and cross-section retention; tessellation

- **Status:** Proposed (2026-11-22). The proposed decision is ready for review.
- **Scope:** how OpenDRIVE plan-view geometry, profiles, and lane width
  functions are kept in `RoadNetworkData`, how they are evaluated, and how
  derived geometry is sampled
- **Phase 1 owner:** GEO-01 ([release plan](../roadmap/release-plan.md#phase-1-workstreams))

## Context

OpenDRIVE describes geometry analytically:

- plan-view segments: line, arc, spiral, poly3, and paramPoly3;
- cubic elevation and superelevation along `s`;
- cubic lane width or border functions for each lane section.

USD visualization needs sampled curves and meshes. The design policy
([§5.3](../design/DESIGN_POLICY.md#53-intermediate-representation)) keeps
analytic information where possible and treats discretization as a separate,
explicit step.

The [FMT-01 spike](../reports/format-spike-opendrive-2026-11-22.md#5-elements-needed-for-phase-1)
puts line, arc, lane offset, and elevation in Phase 1, and spiral, poly3, and
paramPoly3 in Phase 2. The [ADR probes](../reports/adr-probes-2026-11-22.md)
measured two things in CARLA `Town01` and `Town02`:

- consecutive segments meet within 0.35 mm and 4.6e-5 rad (§5 of the
  probes);
- a 1 cm chord tolerance needs 1,428 and 1,112 reference-line intervals
  (§7).

## Questions

1. Does the IR store analytic parameters only, samples only, or both?
2. Are the analytic parameters also authored to USD (semantic layer), so that
   consumers can evaluate them again, or only the samples?
3. Tessellation: by a fixed `s` step, by a chord or angular tolerance, or
   adaptively? Is the tolerance a file-format argument?
4. Which numeric method evaluates spirals (Fresnel integrals, series, or
   numeric integration), and to what tolerance against reference values?
5. How are discontinuities between segments or lane sections detected and
   reported?

## Options considered

- **A. Analytic in the IR and in USD; samples derived on demand.** Lossless.
  Consumers need an evaluator to use the semantic data.
- **B. Analytic in the IR; samples in USD.** Simple for consumers. The source
  parameters are kept only as provenance.
- **C. Samples only.** Rejected: lossy by construction, and contrary to the
  initial direction of the policy.

## Proposed decision

Analytic in the IR. In USD, the analytic parameters are the normative
semantic data, as in A. Phase 1 also authors samples, as derived
visualization data.

### 1. Retention

- The IR stores analytic parameters only: plan-view segments and piecewise
  cubics ([ADR 0001 §4](0001-roadnetwork-data-model.md#4-geometry-representations)).
  It stores no samples.
- The semantic layer authors the same parameters as `double` arrays
  ([ADR 0002 §3](0002-usd-schema-strategy.md#3-phase-1-property-list)). A
  consumer can evaluate them without the visualization.
- `/Asset/Visualization` holds samples generated from those parameters. They
  are derived data that the importer can regenerate, and never the only
  carrier of a value
  ([stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract)).

### 2. Evaluation

`roadnetwork-core` evaluates the reference-line representation in `double`.

- **Line and arc.** A point at distance `u = s − s_segment` along a segment
  that starts at `(x0, y0)` with heading `h0` and curvature `κ` (0 for a
  line) is:

  ```text
  θ   = h0 + κu / 2
  c   = u · sinc(κu / 2)        sinc(z) = sin(z) / z, sinc(0) = 1
  x   = x0 + c · cos θ
  y   = y0 + c · sin θ
  hdg = h0 + κu
  ```

  This chord form is exact for both shapes. It avoids the cancellation of
  `(sin(h0 + κu) − sin h0) / κ` as `κ` tends to 0. `sinc` uses its Taylor
  series for `|z| < 1e-4`.
- **Piecewise cubic.** The value at `s` uses the last record with
  `s0 ≤ s`, as `a + b·d + c·d² + d·d³` with `d = s − s0`. Lane widths use
  `s` relative to the section start.
- **Lane borders.** The lateral position of a border is the lane offset plus
  the widths of the lanes between it and the center lane. Widths are added
  for left lanes and subtracted for right lanes. A border point is the
  reference point moved by that amount along the left normal `(−sin hdg,
  cos hdg)`, with `z` set to the elevation at `s`. A lane centerline is the
  midpoint of the lane's two borders at each `s`.
- A border on the inside of an arc at or beyond the center of curvature
  (`t · κ ≥ 1`) is an `RN-` geometry error. The offset curve folds back on
  itself there.

### 3. Tessellation

- Sampling works on one lane section at a time. Its stations are:
  - the section start and end;
  - every plan-view segment boundary;
  - the start of every elevation, lane-offset, and width record inside it.
- Each interval between two stations is split into `n` equal parts. `n` is
  the smallest value for which the error bound is at most the tolerance
  `ε`, using a step `h` equal to the interval length divided by `n`:

  ```text
  E_arc   = R_out · (1 − cos(|κ| h / 2))     R_out = 1/|κ| + t_max
  E_cubic = h² / 8 · Σ max |f''|             over the lane offset, the widths, and the elevation
  E_arc + E_cubic ≤ ε
  ```

  `t_max` is the outermost border on the interval. `f''` of a cubic is
  linear, so its maximum is at an end of the interval. A line with constant
  widths and elevation needs one part.
- All lanes of a section share its stations, so the borders of adjacent
  lanes share vertices.
- The default is `ε = 0.01 m`. It is fixed in Phase 1, because
  [ADR 0006 §7](0006-fileformat-contract.md#7-file-format-arguments) exposes
  no file-format arguments. An argument may be added under
  [ADR 0007](0007-geometry-lod-payload-boundary.md).

### 4. Profiles in Phase 1

| Element | Phase 1 behavior |
| --- | --- |
| `<elevation>` | Supported. Authored as cubic arrays and applied to the `z` of every sample. |
| `<laneOffset>` | Supported. Authored as cubic arrays and applied to every border. |
| `<superelevation>`, `<shape>` | An error (`ODR-`, unsupported representation) when any record has a non-zero coefficient. All-zero records are accepted and change nothing. |

Ignoring a non-zero superelevation would place every border at the wrong
height. No relationship would break, and the result would look right, which
the [MVP rule](../design/DESIGN_POLICY.md#72-after-the-mvp) forbids. An
error can later become support without breaking any file that loaded. A
warning cannot later become an error without that risk.

### 5. Other plan-view shapes

Phase 1 rejects `<spiral>`, `<poly3>`, and `<paramPoly3>` with an `ODR-`
error that names the road. Accepted ADRs are not rewritten, so the
evaluation method and error bound for these shapes go into a new ADR in
Phase 2. The candidate for spirals is VIRES `odrSpiral`
([FMT-01 spike §7](../reports/format-spike-opendrive-2026-11-22.md#7-existing-opendrive-libraries)).

### 6. Discontinuities

Validation ([ADR 0001 §6](0001-roadnetwork-data-model.md#6-validation))
checks each road's reference line. Every violation is an `RN-` error, because
the stored geometry would otherwise contradict itself.

| Check | Tolerance | CARLA maximum |
| --- | --- | --- |
| The first segment starts at `s = 0`, each segment starts where the previous one ends (`s + length`), and the last ends at the road `length` | 1e-6 m | 2.8e-14 m |
| Position gap between the end of a segment and the start of the next | 0.01 m | 0.35 mm |
| Heading gap at the same point (wrapped to ±π) | 0.001 rad | 4.6e-5 rad |

Phase 1 checks neither width continuity across lane sections nor geometric
continuity between linked roads. Both questions stay open for Phase 2.

## Consequences

- GEO-01 adds the evaluator, the border computation, and the tessellator to
  `roadnetwork-core`, with numeric tests.
- USD-01 authors the samples from the tessellator only. The writer contains
  no geometry math.
- The [OpenDRIVE mapping §3–§5](../reference/OPENDRIVE_MAPPING.md#3-roads-and-reference-line)
  takes the Phase 1 behavior in §4 and §5 when this ADR is accepted.
- A Phase 2 ADR covers spiral, poly3, and paramPoly3 evaluation.

## Evidence

Gathered in the [ADR probes](../reports/adr-probes-2026-11-22.md):

- the CARLA continuity maxima, which the §6 tolerances clear by factors of 30
  or more (§5 of the probes);
- the interval counts at 1 cm and 1 mm (§7).

Needed before acceptance:

- GEO-01 tests of line and arc position and heading at known `s` against
  closed-form values. They include `|κ| < 1e-8` and an arc that turns by
  more than π.
- A test that a hand-authored fixture with non-zero `<laneOffset>`,
  `<elevation>`, and width `b`, `c`, `d` terms stays within `ε` of the
  analytic border at the midpoint of every tessellation interval.
- Negative fixtures for a non-zero `<superelevation>`, a `<spiral>`, a
  position gap of 0.02 m, and an inner border beyond the center of
  curvature.
