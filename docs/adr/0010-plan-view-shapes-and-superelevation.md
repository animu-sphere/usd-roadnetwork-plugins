# ADR 0010 — Spiral and paramPoly3 evaluation; superelevation

- **Status:** Proposed (2026-11-24)
- **Scope:** how `<spiral>` and `<paramPoly3>` are kept, evaluated, and
  sampled; how a non-zero `<superelevation>` places the lane borders; the
  error bounds and golden values the implementation is tested against
- **Needed by:** Phase 2, GEO-02 (F-12) and XS-01 (F-13), in
  [current work](../roadmap/current.md#active-work)
- **Extends:** [ADR 0004](0004-opendrive-geometry-retention.md) §2–§5,
  which leaves these shapes and superelevation to a Phase 2 ADR; the types of
  [ADR 0001 §4](0001-roadnetwork-data-model.md#4-geometry-representations)
  and the property list of
  [ADR 0002 §3](0002-usd-schema-strategy.md#3-phase-1-property-list) gain the
  additions in §5. Nothing they decide for lines, arcs, and the Phase 1
  profiles changes.

## Context

[ADR 0004](0004-opendrive-geometry-retention.md) keeps the plan view and the
profiles analytic in the IR and in the semantic layer, evaluates them in
`double`, and samples them for visualization within a chord tolerance `ε`.
Phase 1 supports lines and arcs only, and rejects non-zero superelevation
because ignoring it would place every border at the wrong height
([ADR 0004 §4](0004-opendrive-geometry-retention.md#4-profiles-in-phase-1)).

The [FMT-02 spike](../reports/format-spike-opendrive-2026-11-23.md) put
spirals, `paramPoly3`, and superelevation in the Phase 2 scope and found:

- spirals in 37 networks from 5 producers; the usual way to start a spiral
  at non-zero curvature from VIRES `odrSpiral` divides by zero when
  `curvStart == curvEnd`, and is wrong by up to 11.9 m when they differ by
  rounding. Both occur in real files
  ([§4.2](../reports/format-spike-opendrive-2026-11-23.md#42-starting-at-non-zero-curvature)).
  Switching to an arc for nearly constant curvature cannot keep both methods
  below a few micrometres
  ([§4.3](../reports/format-spike-opendrive-2026-11-23.md#43-a-single-switch));
- `paramPoly3` in 21 networks from 4 producers, 113 records without the
  mandatory `pRange`, and curve lengths that differ from `@length` by up to
  0.44% in tool-written files and 48% in a test file
  ([§4.4](../reports/format-spike-opendrive-2026-11-23.md#44-parampoly3));
- non-zero superelevation in 11 networks, all small test roads
  ([§5](../reports/format-spike-opendrive-2026-11-23.md#5-cross-section)).

The [ADR 0010 probes](../reports/adr-0010-probes-2026-11-24.md) measured the
method chosen below, the meaning of an absent `pRange`, the tessellation
bounds, and the golden values of §6.

## Questions

1. Which method evaluates a spiral, including nearly constant curvature, and
   to what bound?
2. How does the road's `s` map to the `paramPoly3` parameter `p`? What does
   an absent `pRange` mean? Is a length mismatch diagnosed?
3. How does superelevation move the borders, and how do `level` lanes take
   part?
4. How do the new shapes and superelevation enter the tessellation bound of
   ADR 0004 §3, so that every sample stays within `ε`?
5. How are the new shapes and records kept in the IR and authored?
6. What remains open from [ADR 0004 §6](0004-opendrive-geometry-retention.md#6-discontinuities)?

## Options considered

Spiral evaluation:

- **A. `odrSpiral` composition** (libOpenDRIVE): evaluate the standard
  clothoid at `s0 = curvStart / κ̇` and at `s0 + u`, then subtract and
  rotate. Fails at `κ̇ = 0` and loses precision as `|s0|` grows. Declined.
- **B. A with an arc below a threshold** (esmini): no threshold keeps both
  branches below 3.3 µm (FMT-02 §4.3). Declined.
- **C. A with a series in `κ̇` near constant curvature:** accurate, but two
  methods, two error analyses, and a switch to test on both sides. Declined
  in favor of D.
- **D. Piecewise Gauss–Legendre quadrature of the heading:** one method for
  every curvature rate, `κ̇ = 0` included, accurate to rounding (probes §2).
  Its cost grows with the heading change, as the number of samples does.
  **Chosen.**

`s` to `p` for `paramPoly3`:

- **A. Linear:** `p = (s − s_segment) · p_max / length`. What libOpenDRIVE
  and esmini do, and what the `length_match` rule assumes. `s` is then the
  stated length along the road, not the exact arc length of the curve.
  **Chosen.**
- **B. By arc length:** invert the arc length for each `s`. Every `s` would
  be a true distance, but the result would disagree with other tools by up
  to the length mismatch, and the curve's end would no longer be at
  `s_segment + length` when the lengths disagree. Declined.

Superelevation:

- **A. Roll the cross section about the reference line,** with `t` and the
  widths measured in the rolled plane, as 1.9.0 §8.3 and §10.5.2 state.
  **Chosen.**
- **B. Keep `t` horizontal and add `t · tan φ` to the height:** changes the
  projected widths the specification keeps. Declined.

## Proposed decision

### 1. Spiral

A spiral starts at `(x0, y0)` with heading `h0` and has the curvature
`κ(u) = κ0 + κ̇u` along `u = s − s_segment`, where `κ0 = curvStart` and
`κ̇ = (curvEnd − curvStart) / length`. Its heading is the closed form

```text
hdg(u) = h0 + κ0·u + κ̇·u²/2
```

and its position is `(x0, y0)` plus the rotation by `h0` of

```text
(X, Y)(u) = ∫₀ᵘ (cos φ(v), sin φ(v)) dv        φ(v) = κ0·v + κ̇·v²/2
```

- The segment is split into `m` pieces of equal length `h = length / m`,
  with `m = max(1, ceil(length · (K + sqrt|κ̇|)))` and `K` the larger of
  `|curvStart|` and `|curvEnd|`, so that the phase of each piece changes by
  at most about 1 rad.
- A piece is integrated with 8-point Gauss–Legendre about its midpoint `c`:
  the phase is `φ(c) + w·(κ(c) + κ̇w/2)` for `w` in `[−h/2, h/2]`, and the
  piece's sum is rotated by `φ(c)`. A point at `u` adds the full pieces
  before it, which the evaluator may compute once per segment, and the part
  of its own piece up to `u`, with the same 8 points.
- There is no special case: equal end curvatures integrate an arc within
  the same bound.
- **Bound:** the position relative to the segment start is within
  `max(1e-9 m, 1e-13 · |u|)` of the exact clothoid. The probes measured at
  most 6.2e-12 m and 9.2e-16 · `|u|` over 616 spirals, so the bound has a
  margin of more than 100 for compiler differences. The absolute position
  adds the rounding of the start coordinates.

### 2. `paramPoly3`

- The local curve is `u(p) = aU + bU·p + cU·p² + dU·p³` and
  `v(p) = aV + bV·p + cV·p² + dV·p³`. The point is `(x0, y0)` plus the
  rotation by `h0` of `(u, v)`, and the heading is
  `h0 + atan2(v'(p), u'(p))`. All coefficients are used as stated, including
  `aU`, `aV`, and `bV` that 1.9.0 says *should* be zero; the continuity
  checks of ADR 0004 §6 compare the evaluated end and start poses.
- `p = (s − s_segment) · p_max / length`, with `p_max = 1` for
  `pRange="normalized"` and `p_max = length` for `pRange="arcLength"`.
- **An absent `pRange` means `normalized`.** The specification states no
  default; SUMO `netconvert`, the producer that omits it, writes normalized
  coefficients: read so, its curves meet the roads they join within 7.3e-8 m,
  and read as `arcLength`, 10.6 km away (probes §3).
- **Length mismatch.** The curve's length, by 8 × 32-point Gauss–Legendre
  over `[0, p_max]`, is compared with `@length`. A difference of more than
  1% of `@length` is the counted warning `RN-W302`, one count per segment.
  The segment is still evaluated by the linear mapping. Tool-written files
  reach 0.44% (FMT-02 §4.4); 1% flags a curve whose stations along `s` are
  no longer close to distances, such as the 48% test file.
- A tangent that vanishes inside `[0, p_max]` (`u' = v' = 0`, a cusp)
  leaves the heading undefined and is the error `RN-E310`.
- **Bound:** the evaluation is closed form; position within 1e-9 m and
  heading within 1e-12 rad of the exact values at the coordinates of §6.

### 3. Superelevation

- A road keeps its `<superelevation>` records as a piecewise cubic `φ(s)`
  in radians, positive when the road falls to the right, like the elevation
  ([ADR 0004 §2](0004-opendrive-geometry-retention.md#2-evaluation)).
- The cross section rolls about the reference line. Going outward from the
  reference line, each lateral step `Δt` moves the border by
  `Δt · cos φ` along the horizontal left normal and by `Δt · sin φ` up:
  - the lane offset is one step, from the reference line to the center
    border;
  - each lane is one step of its signed width, positive on the left and
    negative on the right;
  - a lane with `level="true"` steps by its width horizontally and keeps
    the height of its inner border (1.9.0 §11.7.4).

  With `φ = 0` this is the border of ADR 0004 §2. A lane centerline stays
  the midpoint of its two borders.
- `|φ| ≥ π/2` anywhere on the road, a vertical or overturned cross section,
  is the error `RN-E309`. Between two stations `φ` is one cubic, so the
  check uses its exact extremum.
- The fold checks of ADR 0004 §2 (`RN-E308`, `RN-W301`) keep using the
  offset `t` in the rolled plane. The horizontal offset is never larger, so
  no fold is missed; a lane that folds only before rolling is rejected,
  which no file in the corpus approaches.
- A non-zero `<shape>` or `<crossfall>` and any `<crossSectionSurface>` stay
  `ODR-E202`, and lane `<border>` stays `ODR-E203`, as the
  [release plan](../roadmap/release-plan.md#phase-2--opendrive-fidelity)
  states.

### 4. Tessellation

[ADR 0004 §3](0004-opendrive-geometry-retention.md#3-tessellation) holds,
with these additions. `ε` stays 0.01 m.

- **Stations** also include the start of every superelevation record, the
  point inside a spiral where its curvature changes sign, and the points
  inside a `paramPoly3` where `u'v'' − v'u''` changes sign (the roots of a
  quadratic in `p`, mapped to `s`). Between two stations the curvature of
  the reference line keeps one sign.
- **Plan-view term** `E_plan`, per part of length `h`:
  - line and arc: `E_arc` of ADR 0004, unchanged;
  - spiral: `E_arc` with `κ` replaced by `K`, the larger `|κ|` at the two
    ends of the part, and `R_out = 1/K + t_max` (largest measured ratio to
    the bound 0.998, probes §4);
  - `paramPoly3`: `E_tri = (c + 2·t_max·sin(Θ/2)) / 2 · tan(Θ/2)`, with `c`
    the chord of the reference line between the part's ends, `Θ` its
    heading change, and `t_max` the largest `|t|` of a border on the part
    (largest ratio 0.786). A part with `Θ ≥ π/2` is refused, as ADR 0004
    refuses an arc step over π.
- **Cross-section term.** ADR 0004's `E_cubic` holds. Where `φ` is not zero
  on the part, it adds

  ```text
  E_super = h²/8 · ( S·Σ max|f''| + (1 + S)·(2·max|F'|·max|φ'| + max|F|·max|φ''|) + 2·max|F|·max|φ'|² )
  ```

  where the sum runs over the rolled contributors (the lane offset and the
  widths of lanes that are not level), `F` is the rolled offset of a border
  (the largest of each maximum over the borders), and `S` the largest
  `|sin φ|` on the part. Every maximum is that of a cubic or its
  derivatives on the part, which is exact (largest ratio 0.969, probes §5).
  With `φ = 0` the term is 0, so a Phase 1 road samples exactly as before.
- `n` for an interval is the smallest number of equal parts for which every
  part meets `E_plan + E_cubic + E_super ≤ ε`. For a spiral or a
  `paramPoly3` the bound is checked per part, because it is not a closed
  function of `n`.

### 5. IR and stage

- `PlanViewSegment::shape` gains two alternatives:
  `Spiral { curvatureStart, curvatureEnd }` and
  `ParamPoly3 { aU, bU, cU, dU, aV, bV, cV, dV, pRange }`, where
  `pRange` is `optional<{Normalized, ArcLength}>` and stays empty when the
  source omits it. `Road` gains `PiecewiseCubic superelevation`. The
  existing types are unchanged.
- The semantic layer follows the rules of
  [ADR 0002 §2](0002-usd-schema-strategy.md#2-properties). On a road:

  | Property | Type | Value |
  | --- | --- | --- |
  | `roadnetwork:referenceLine:kind` | `token[]` | adds `spiral` and `paramPoly3` |
  | `roadnetwork:referenceLine:curvature` | `double[]` | `curvStart` for a spiral, 0 for a `paramPoly3` |
  | `roadnetwork:referenceLine:curvatureEnd` | `double[]` | authored when the road has a spiral: `curvEnd` for a spiral, the curvature for a line or an arc, 0 for a `paramPoly3` |
  | `roadnetwork:referenceLine:aU`, `:bU`, `:cU`, `:dU`, `:aV`, `:bV`, `:cV`, `:dV` | `double[]` | authored when the road has a `paramPoly3`: its coefficients, 0 for other segments |
  | `roadnetwork:referenceLine:pRange` | `token[]` | authored with the coefficients: `normalized`, `arcLength`, or `unstated` for a `paramPoly3` without `pRange` (read as `normalized`, §2); the empty token for other segments |
  | `roadnetwork:superelevation:s`, `:a`, `:b`, `:c`, `:d` | `double[]` | cubic records, authored when the source has any, zero records included |

  `unstated` keeps the source's omission visible, as ADR 0002 §2 requires,
  while §2 fixes its meaning. A road with only lines and arcs and no
  superelevation records is authored exactly as in Phase 1.
- `/Asset/Visualization` samples come from §4 only. The writer contains no
  geometry math, as before.

### 6. Golden values

GEO-02 and XS-01 test against these values, computed independently with
mpmath at 40 digits ([probes §7](../reports/adr-0010-probes-2026-11-24.md#7-golden-values)).
Position must agree within 1e-9 m and heading within 1e-12 rad.

| Case | Input | `s − s_segment` | `x` | `y` | `hdg` |
| --- | --- | ---: | ---: | ---: | ---: |
| S1 | spiral from (0, 0, 0), `curvStart` 0, `curvEnd` 0.02, `length` 100 | 50 | 49.688402921479471 | 4.1481024268547482 | 0.25000000000000001 |
| S1 | as above | 100 | 90.452423790027208 | 31.026830172338111 | 1.0 |
| S2 | spiral, `curvStart` = `curvEnd` = 0.01, `length` 100 | 100 | 84.14709848078965 | 45.969769413186029 | 1.0 |
| S3 | spiral, `curvStart` 0.01, `curvEnd` 0.0100000000000001, `length` 100 | 100 | 84.147098480789538 | 45.969769413186149 | 1.0000000000000051 |
| S4 | spiral, `curvStart` 0.01, `curvEnd` 0.01000000001, `length` 500 | 500 | −95.892427285179128 | 71.633781261491015 | 5.0000000024999999 |
| S5 | spiral, `curvStart` −0.05, `curvEnd` 0.05, `length` 100 | 50 | 31.145313202640919 | −34.681149738592169 | −1.2500000000000001 |
| S5 | as above | 100 | 62.290626405281839 | −69.362299477184339 | 0.0 |
| S6 | spiral from (512000, 4100000, 2.5), `curvStart` 0.08, `curvEnd` 0.02, `length` 40 | 40 | 511971.81988245631 | 4099981.8084150394 | 4.5 |
| P1 | `paramPoly3` without `pRange`, from (150, −45.6, −1.57079633), `length` 23.35459442, `bU` 28.8, `cU` −14.4, `cV` −14.4 | 11.67729721 | 146.39999996538488 | −56.399999988461629 | −2.3561944933974484 |
| P1 | as above | 23.35459442 | 135.59999995384651 | −59.999999953846512 | −3.1415926567948967 |
| P2 | `paramPoly3`, `pRange="arcLength"`, from (0, 0, 0), `length` 10, `bU` 1, `dU` −0.0001, `cV` 0.01, `dV` 0.0002 | 5 | 4.9875 | 0.27500000000000001 | 0.11535461685276674 |
| P2 | as above | 10 | 9.9 | 1.2 | 0.26188526468051904 |

S2 and S3 are the cases on which the composition of option A divides by zero
or loses 11.9 m; S4 is the FMT-02 §4.3 case on which a switch at `T = 1e-6`
loses 200 m. P1 is a SUMO junction road (curve length 23.3744434580193 m,
no `RN-W302`); a P2 with `dU` −0.002 and `dV` 0.0005 has a curve length of
8.26536626148177 m and raises `RN-W302`.

Superelevation (SE): a straight road along `+x`, elevation `a = 10`, lane
offset `a = 0.5`, superelevation `a = 0.05`, `b = 0.001`, lanes 1 and −1
3.5 m wide and lane −2 2.0 m wide with `level="true"`. At `s = 50`
(`φ = 0.1`), the borders are at `(y, z)`: center (0.49750208263901288,
10.049916708323414), lane 1 (3.9800166611121031, 10.399333666587313), lane
−1 (−2.9850124958340773, 9.7004997500595155), and lane −2
(−4.9850124958340773, 9.7004997500595155).

### 7. Continuity left open by ADR 0004

ADR 0004 §6 left three questions for Phase 2. This ADR proposes the rule;
the evidence below decides it before acceptance.

- **Jumps between profile records.** A record of the elevation, the lane
  offset, a width, or the superelevation that starts more than 0.01 m
  (0.001 rad for superelevation) from where the previous one ends is a
  counted warning, one count per jump. The tessellator places a sample on
  each side of the station, so `ε` holds on both sides.
- **Width across lane sections** and **position across road links:** a
  border, or the reference line, that does not continue within 0.01 m into
  the linked section or road is a counted warning, one count per border.
- Neither is an error: the stored data does not contradict itself, and real
  files may model a step on purpose.

### 8. Diagnostics

| Code | Change |
| --- | --- |
| `ODR-E201` | Raised only for `<poly3>`. |
| `ODR-E202` | No longer raised for `<superelevation>`. |
| `RN-E308`, `RN-W301` | Apply on the inside of any curve, not only an arc. For a spiral, `t·κ` between two stations is a quartic, and its exact extremum is used. For a `paramPoly3`, `t·κ` is checked at every tessellation sample and at the midpoint of every part; a fold shorter than one part may go undetected, and it leaves no loop in the samples. |
| `RN-E309` (new) | `|superelevation| ≥ π/2` on a road (§3). |
| `RN-E310` (new) | A `paramPoly3` tangent vanishes in its parameter range (§2). |
| `RN-W302` (new) | A `paramPoly3` curve length differs from `@length` by more than 1% (§2). |
| two new `RN-W30x` | The jumps and the cross-section and link discontinuities of §7, if accepted. |

`RN-` codes come from shared validation, because the checks apply to the IR
whatever the source format. The codes are assigned in the
[diagnostics reference](../reference/DIAGNOSTICS.md) and the catalogs by the
pull request that implements them.

## Consequences

- GEO-02 adds the spiral and `paramPoly3` evaluation, the new stations and
  plan-view bounds, `RN-E310`, `RN-W302`, and the fold checks to
  `roadnetwork-core`, decodes both shapes in `roadnetwork-opendrive`, and
  authors §5 in `roadnetwork-usd`, with the golden values of §6 as tests.
- XS-01 adds superelevation to the borders, the tessellation term,
  `RN-E309`, the decoder, and the writer, with SE as a test, and implements
  §7 once it is accepted.
- Layers of files with only lines, arcs, and no superelevation records do
  not change. Layers of files with all-zero superelevation records gain the
  `roadnetwork:superelevation:*` arrays.
- The [OpenDRIVE mapping §3–§4](../reference/OPENDRIVE_MAPPING.md#3-roads-and-reference-line),
  the [diagnostics reference](../reference/DIAGNOSTICS.md), the
  [stage contract](../architecture/WORKSPACE.md#5-authored-stage-contract),
  and the [capability matrix](../reference/CAPABILITY_MATRIX.md) change in
  the pull requests that implement each part.
- When this ADR is accepted, ADR 0004's status line gains a link to it as
  the ADR that extends §4 and §5.

## Evidence needed before acceptance

- GEO-02 runs §1 and §2 in C++ in `roadnetwork-core` and meets the golden
  values of §6 within the stated tolerances, including S2, S3, and S4.
- GEO-02 or XS-01 samples a hand-authored road with a spiral, a
  `paramPoly3` with an inflection, and a varying superelevation, and every
  border and centerline stays within `ε` of the analytic curve at the
  midpoint of every interval, as ADR 0004's evidence did for lines and arcs.
- For §7: the number of networks in the FMT-02 corpus that would raise each
  continuity warning, and the largest jump of each kind, so that the
  tolerances are set from data.

## Evidence

Gathered in the [ADR 0010 probes](../reports/adr-0010-probes-2026-11-24.md):

- §2: the quadrature of §1 against 40-digit references over 616 spirals,
  including every failing case of FMT-02 §4.2–§4.3, and the truncation
  margin;
- §3: the absent `pRange` in SUMO `netconvert` output;
- §4–§5: the plan-view and cross-section bounds of §4 against sampled
  borders;
- §6: the specification text on superelevation, `level`, and `pRange`;
- §7: the golden values of §6.
