# Format spike: OpenDRIVE Phase 2 corpus and scope — 2026-11-23

This is the FMT-02 spike in [current work](../roadmap/current.md#active-work).
It records which Phase 2 elements real OpenDRIVE files use, by producer and
version, which of those files may be redistributed, and the measured error
of VIRES `odrSpiral`. It is not a support claim: the
[capability matrix](../reference/CAPABILITY_MATRIX.md) is unchanged. The
specification versions, licensing, and Phase 1 elements are in the
[FMT-01 spike](format-spike-opendrive-2026-11-22.md); this report does not
repeat them.

## Summary

| Check | Result |
| --- | --- |
| Corpus | 20 repositories at pinned commits; 559 distinct files. 222 are networks written by a tool or by hand in OpenDRIVE 1.4 to 1.9, from at least 11 producers (§2). |
| Plan view | Spirals in 37 files from 5 producers; `paramPoly3` in 21 files from 4 producers. No file in scope uses `poly3` (§3). |
| `odrSpiral` | The Fresnel core is accurate to a relative 5.3e-16. The usual way to start a spiral at non-zero curvature (libOpenDRIVE) divides by zero when `curvStart == curvEnd` and is wrong by up to 11.9 m when they differ by 1e-16. Both occur in real files (§4). |
| `paramPoly3` | `@length` differs from the curve's arc length by up to 0.44% in tool-written `normalized` files. 113 records omit `pRange`, which 1.9.0 makes mandatory (§4.4). |
| Cross-section | Non-zero superelevation in 11 files: 5 from 3 producers, 6 unattributed. Non-zero lateral shape in one RoadRunner file. Lane `<border>` and non-zero `<crossfall>` in none (§5). |
| Road marks, signals, objects | Road marks are in 206 of 222 files. Signals in 47 files from 6 producers and objects in 63 files from 4 (§6–§8). |
| Data quality | Four networks have plan-view gaps of 0.12 to 64 m, beyond the ADR 0004 §6 tolerance of 0.01 m (§9). |
| Fixtures | Ten files from four producers and versions 1.4, 1.6, 1.7, and 1.8 may be committed under MIT, BSD-3-Clause, or Apache-2.0. Larger and copyleft files can be fetched by checksum (§10). |
| Recommendation | Support spiral and `paramPoly3`; superelevation; road marks; signals, references, controllers, and lane speed; objects with outlines. Defer `poly3`, lateral shape, lane `<border>`, and the rarely used sub-elements (§11). |

## 1. Sources and method

| Source | Commit read | Licence | Files |
| --- | --- | --- | ---: |
| [esmini/esmini](https://github.com/esmini/esmini) | `6ce663611b73` | MPL-2.0 | 83 |
| [maliput/maliput_malidrive](https://github.com/maliput/maliput_malidrive) | `9f9a0ca7735f` | BSD-3-Clause | 82 |
| [asam-ev/qc-opendrive](https://github.com/asam-ev/qc-opendrive) | `8baf0a5ccd8d` | MPL-2.0 | 181 |
| [Robomous/RoadMaker](https://github.com/Robomous/RoadMaker) | `c7a3f5acc457` | Apache-2.0 | 126 |
| [eclipse-sumo/sumo](https://github.com/eclipse-sumo/sumo) `tests/netconvert/import/openDRIVE` | `22f87a6fede1` | EPL-2.0 | 26 |
| [ISET/isetauto](https://github.com/ISET/isetauto) | `82208c8cd8ac` | Apache-2.0 | 24 |
| [truevisionai/designer](https://github.com/truevisionai/designer) | `451e9ce9f6da` | not SPDX (custom) | 18 |
| [uoe-agents/IGP2](https://github.com/uoe-agents/IGP2) | `42068004e4d9` | not SPDX (custom) | 14 |
| [bark-simulator/bark-ml](https://github.com/bark-simulator/bark-ml) | `ea2ee58be406` | MIT | 9 |
| [uoe-agents/cema](https://github.com/uoe-agents/cema) | `86196b13ba99` | MIT | 8 |
| [carla-simulator/opendrive-test-files](https://github.com/carla-simulator/opendrive-test-files) | `5929b67cc414` | MIT | 7 |
| [ika-rwth-aachen/omega-prime](https://github.com/ika-rwth-aachen/omega-prime) | `98c7d036d5a9` | MPL-2.0 | 5 |
| [xmssp/PCG-OpenDRIVE](https://github.com/xmssp/PCG-OpenDRIVE) | `242c288a3289` | MIT | 5 |
| [carla-simulator/carla](https://github.com/carla-simulator/carla) `ue4-dev`, `ue5-dev` | `bab23b62ef09`, `1360bb9aff0f` | MIT | 4 |
| [huawei-noah/SMARTS](https://github.com/huawei-noah/SMARTS) | `c113dd7425e2` | MIT | 3 |
| [DLR-TS/OSTAR-Quickstart](https://github.com/DLR-TS/OSTAR-Quickstart) | `5c39c3973c24` | EPL-2.0 | 3 |
| [tum-gis/rtron](https://github.com/tum-gis/rtron), [RI-SE/ATOS](https://github.com/RI-SE/ATOS) | `9211766efb04`, `2e5d80328010` | Apache-2.0, MPL-2.0 | 2 each |
| [asam-ev/ASAM-OpenX-Examples](https://github.com/asam-ev/ASAM-OpenX-Examples), [pageldev/libOpenDRIVE](https://github.com/pageldev/libOpenDRIVE) | `669b7403a0b2`, `c3a5c8c8a3a1` | none declared, Apache-2.0 | 1 each |
| [ASAM OpenDRIVE 1.9.0 specification](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/index.html) §9.4, §9.6, §9.7, §10.5 | 1.9.0 | ASAM license terms | — |

The repositories were found with GitHub code search for `.xodr` files that
contain `spiral`, `paramPoly3`, `<signal`, `<object`, `superelevation`, or
`<border`, and kept when they had a licence or many files. Every `.xodr` file
in each repository was fetched at the pinned commit and parsed with the
Python standard library. Files were deduplicated by SHA-256. A file was
attributed to a producer by, in order: RoadRunner or VIRES user data
(`vectorScene`, `viStyleDef`) or the VIRES header attributes `maxRoad` and
`maxJunc`; a `netconvert` comment; the header `vendor`; and the header layout
of the Python generator `scenariogeneration` (a `datetime` date with
fractional seconds and `north="0.0"`), which is an inference, not a
declaration. Files with none of these are *unattributed*.

The probe scripts were throwaway scripts and are not committed. No file from
these sources is committed by this report.

## 2. Corpus

| Set | Files |
| --- | ---: |
| Fetched (with duplicates) | 604 |
| Distinct by SHA-256 | 559 |
| Not well-formed, or not an `OpenDRIVE` root | 15 |
| Synthetic: the `qc-opendrive` rule tests and the RoadMaker fuzz corpus | 262 |
| Networks | 282 |
| Networks outside the accepted versions (54 at 1.1, 6 at 1.2) | 60 |
| **Networks in scope, `revMinor` 4 to 9** | **222** |

The synthetic files test a checker or a fuzzer, so they say nothing about
what producers write; §3 to §9 count only the 222 networks in scope. The 45
duplicates include CARLA `Town01` to `Town07` copied into maliput and
TrueVision, and the esmini `fabriksgatan.xodr` copied into omega-prime.

## 3. Elements by producer and version

Each cell is the element count, with the number of files in brackets.
Superelevation and shape count records with a non-zero coefficient. ¹ is
inferred from the header layout (§1).

| Producer | Version | Files | spiral | paramPoly3 | superelev. | shape | roadMark | signal | signalRef. | object | lane speed |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Libram IntelRoad RoadPlanner | 1.8 | 1 | — | — | — | — | 9 (1) | 5 (1) | — | — | — |
| RoadMaker (Robomous) | 1.7 | 4 | 2 (1) | — | 1 (1) | — | 8 (4) | — | — | 18 (1) | — |
| RoadMaker (Robomous) | 1.8 | 16 | 2 (2) | — | — | — | 115 (14) | 28 (4) | — | 77 (7) | — |
| RoadRunner (MathWorks) | 1.4 | 14 | 91 (2) | — | — | 4,201 (1) | 1,134 (14) | 13 (4) | — | 56 (11) | 630 (12) |
| RoadRunner (MathWorks) | 1.6 | 18 | 92 (1) | — | — | — | 2,964 (18) | 22 (9) | — | 60 (10) | 1,682 (18) |
| RoadRunner (MathWorks) | 1.7 | 8 | — | — | — | — | 2,177 (8) | 60 (4) | — | 448 (7) | 1,156 (8) |
| RoadRunner (VectorZero 2019) | 1.4 | 32 | — | — | — | — | 49,611 (32) | 723 (6) | 811 (6) | 912 (6) | 8 (1) |
| SUMO netconvert | 1.4 | 2 | — | 20 (2) | — | — | 68 (2) | — | — | — | 45 (2) |
| TRI (maliput) | 1.4 | 1 | — | — | — | — | 5 (1) | — | — | — | — |
| TrueVision Designer | 1.4 | 1 | — | — | — | — | 12 (1) | — | — | — | — |
| VIRES ROD | 1.4 | 23 | 184 (5) | 99 (6) | 4 (2) | — | 1,011 (23) | 390 (6) | 18 (1) | 43 (7) | — |
| VIRES ROD | 1.5 | 1 | — | 16 (1) | — | — | 8 (1) | — | — | — | — |
| VIRES ROD | 1.7 | 2 | — | 32 (2) | — | — | 14 (2) | — | — | — | — |
| VIRES ROD | 1.8 | 3 | — | 2 (1) | — | — | 33 (3) | 3 (1) | — | 3 (1) | — |
| dSPACE (ModelDesk) | 1.6 | 1 | 4 (1) | 14 (1) | — | — | 31 (1) | 16 (1) | — | — | — |
| scenariogeneration ¹ | 1.5 | 27 | 83 (13) | — | 7 (2) | — | 485 (26) | — | — | 8 (1) | — |
| scenariogeneration ¹ | 1.6 | 16 | 97 (9) | — | — | — | 402 (16) | 3 (2) | — | 13 (5) | — |
| scenariogeneration ¹ | 1.7 | 2 | 6 (1) | — | — | — | 16 (2) | — | — | 14 (2) | — |
| scenariogeneration ¹ | 1.8 | 1 | — | — | — | — | 9 (1) | — | — | — | — |
| scenariogeneration ¹ | 1.9 | 1 | — | — | — | — | 84 (1) | — | — | 10 (1) | — |
| unattributed | 1.4 | 30 | — | 5,068 (7) | 9 (3) | — | 6,869 (18) | 162 (8) | 371 (3) | 27 (2) | 3,487 (8) |
| unattributed | 1.5 | 2 | — | — | — | — | 5 (1) | — | — | — | — |
| unattributed | 1.6 | 8 | 1 (1) | — | — | — | 67 (8) | — | — | 12 (2) | — |
| unattributed | 1.7 | 3 | 1 (1) | — | — | — | 23 (3) | — | — | — | 8 (1) |
| unattributed | 1.8 | 4 | — | — | 4 (3) | — | 27 (4) | 6 (1) | — | — | — |
| xodr_extract App | 1.4 | 1 | — | 4 (1) | — | — | 7 (1) | — | — | — | — |
| **All** | | **222** | **563** (37) | **5,255** (21) | **25** (11) | **4,201** (1) | **65,194** (206) | **1,431** (47) | **1,200** (10) | **1,701** (63) | **7,016** (50) |

Elements that no or almost no file in scope uses:

| Element | Count in the 222 networks | Note |
| --- | ---: | --- |
| `<poly3>` | 0 | Deprecated since 1.6.0 (§9.7). |
| Lane `<border>` | 0 | Only in 8 `qc-opendrive` tests and 1 RoadMaker fuzz file. |
| `<crossfall>` | 533, all zero | All in RoadRunner 2019 files. The 1.9.0 element list has no `<crossfall>`. |
| `<crossSectionSurface>` | 0 | New in 1.8. |
| `direct` junction | 3 | VIRES ROD 1.7 (`soderleden.xodr`, twice) and one unattributed esmini test. |
| `virtual` junction | 0 | Only in RoadMaker fuzz files. |
| Temporary lane layer | 1 | One esmini file (1.9, `roadworks_permanent_temporary_layers.xodr`). |
| `<roadMark>/<explicit>`, `<sway>` | 4, 0 | |
| `<objectReference>`, `<tunnel>`, `<bridge>` | 0, 4, 22 | `<bridge>` in four RoadRunner files. |
| Signal `<dependency>`, `<reference>`, `<semantics>` | 7, 3, 5 | `<semantics>` (1.8) only from Libram. |

## 4. Plan-view shapes

### 4.1 `odrSpiral` core

VIRES `odrSpiral(s, cDot)` computes the clothoid that starts at the origin
with curvature 0 and curvature rate `cDot`, from the Cephes Fresnel
integrals. It was read from libOpenDRIVE `c3a5c8c`
(`src/Geometries/Spiral/odrSpiral.cpp`, Apache-2.0, VIRES 2010). A build
with MSVC was not available in this session, so the file was transliterated
line for line into Python with the same operation order in IEEE `double`.
The reference is the same clothoid from mpmath 1.4.1 Fresnel integrals at
50 significant digits, checked against direct quadrature (agreement 4e-47
m).

| `cDot` (1/m²) | `s` up to (heading 2π) | Largest position error | Largest relative error |
| ---: | ---: | ---: | ---: |
| 1e-8 | 35,449 m | 1.1e-11 m | 3.4e-16 |
| 1e-6 | 3,545 m | 7.7e-13 m | 2.9e-16 |
| 1e-4 | 354 m | 6.0e-14 m | 2.6e-16 |
| 1e-2 | 35 m | 1.2e-14 m | 4.4e-16 |
| 1 | 3.5 m | 9.2e-16 m | 3.3e-16 |

Each row samples 400 stations for both signs of `cDot`; the nine rows run
from 1e-8 to 1 by factors of 10, and the largest relative error of any is
5.3e-16. Around the switch between the series and the asymptotic form
(`|s/a| = 1.6`), the relative error stays below 4e-16. The core needs no
correction.

### 4.2 Starting at non-zero curvature

A spiral from `curvStart` to `curvEnd` over `length` starts part way along a
standard clothoid. libOpenDRIVE (and esmini, with the guard below) finds
that point at `s0 = curvStart / cDot`, with `cDot = (curvEnd − curvStart) /
length`, and subtracts and rotates two `odrSpiral` results. The error of
this composition grows with `|s0|`:

| `curvStart` | `curvEnd − curvStart` | `length` | `|s0|` | Largest error |
| ---: | ---: | ---: | ---: | ---: |
| 0.01 | 1e-3 | 100 m | 1e3 m | 1.9e-13 m |
| 0.01 | 1e-7 | 100 m | 1e7 m | 1.8e-9 m |
| 0.1 | 1e-9 | 100 m | 1e10 m | 2.4e-6 m |
| 0.1 | 1e-11 | 100 m | 1e12 m | 20 m |
| any | 0 | any | — | division by zero |

The 1.9.0 rule `road.geometry.spiral.curvature_change` says only that
`curvStart` and `curvEnd` *should not* be equal. In the corpus they are: of
298 distinct spirals in scope, four have `curvStart == curvEnd` exactly and
two differ by about 1e-16 (rounding). All six are in `scenariogeneration`
files in esmini (`road_straight_curve_junction.xodr`,
`route_strategy_test_road.xodr` and its left-hand-traffic copy,
`parking_demo.xodr`, `Junction_with_building0.xodr`). On them the
composition fails or is wrong by 11.9 m. On the other 292 it is within 1.0e-7
m (median 7e-15 m).

esmini evaluates a spiral with `|curvEnd − curvStart| < 1e-6` as an arc of
curvature `curvStart`. That removes the failure, but the arc is wrong by up
to 2.6 cm for a 500 m spiral just under the threshold.

### 4.3 A single switch

A spiral can be evaluated as an arc of the mean curvature when
`|curvEnd − curvStart| · length²` is below a threshold `T`, and by the
composition above it. The arc error is at most 0.083 · `|Δκ| · length²`.
Over a grid of start curvatures 0 to 0.5 1/m, lengths 5 to 500 m, and
curvature changes 1e-14 to 1e-2 1/m of both signs, the largest position
error at eight stations was:

| `T` (m) | Largest error | Where |
| ---: | ---: | --- |
| 1e-6 | 200 m | composition, `κ0 = 0.01`, `Δκ = 1e-11`, 500 m |
| 1e-5 | 1.7e-5 m | composition, `κ0 = 0.01`, `Δκ = 1e-10`, 500 m |
| **1e-4** | **3.3e-6 m** | arc, `κ0 = 0`, `Δκ = 1e-9`, 200 m |
| 1e-3 | 3.3e-5 m | arc |

No single switch keeps both branches below 1 µm. A bound tighter than a few
micrometres needs another method for nearly constant curvature, such as a
series in `Δκ` about the arc. ADR 0010 chooses the method and the bound.

### 4.4 `paramPoly3`

Spec §9.6 makes `pRange` mandatory, with `arcLength` (`p` from 0 to
`@length`) and `normalized` (`p` from 0 to 1). It states that `p` is not
proportional to the arc length, and rule `length_match` says the curve's
length *should* match `@length`.

| Measure, over the 5,255 records in scope | Result |
| --- | --- |
| `pRange="normalized"` | 4,993 records |
| `pRange="arcLength"` | 149 records (VIRES ROD 1.4 to 1.8) |
| `pRange` absent | 113 records (SUMO `netconvert` and unattributed 1.4 files) |
| `|arc length − @length|`, `normalized` or absent | median 7.5e-6 m; 99th percentile 2.6 cm; largest 18 cm, 0.44% of 62 m (unattributed, `french-big.xodr`) |
| `|arc length − @length|`, `arcLength` | median 2.1e-6 m; largest 48.5 m in a maliput test file whose `@length` is 100 m for a 148.5 m curve |
| Position gap after a `paramPoly3` | largest 0.75 mm (3,771 joints) |
| Heading gap after a `paramPoly3` | 99th percentile 3.1e-8 rad |

So ADR 0010 has to state how the road's `s` maps to `p` (linearly, as most
tools assume, or by arc length), what an absent `pRange` means in 1.4 files,
and whether a length mismatch is diagnosed. The arc lengths were computed
with 8 × 32-point Gauss–Legendre quadrature.

### 4.5 Continuity with the new shapes

With spirals evaluated by the composition and the switch at `T = 1e-4`, and
`paramPoly3` in closed form, the files that use these shapes meet the
[ADR 0004 §6](../adr/0004-opendrive-geometry-retention.md#6-discontinuities)
tolerances after them: the largest gap after a spiral is 0.36 mm and 2.0e-4
rad (444 joints), and after a `paramPoly3` 0.75 mm. Larger gaps in the same
files follow lines or arcs (§9).

### 4.6 `poly3`

No network in scope uses `poly3`. Its section is headed *deprecated* since
1.6.0, and 1.8.0 deprecates its attributes. Support would also need an
arc-length inversion, because `poly3` is parameterized by the local `u`, not
by `s`.

## 5. Cross-section

- **Superelevation.** 7,189 records, 25 of them non-zero, in 11 files from
  VIRES ROD, `scenariogeneration`, RoadMaker, and hand-written esmini tests.
  The non-zero ones are in small test roads (banked curves, a velodrome,
  rolled ramps); none is in a large tool-written network.
- **Lateral shape.** 7,033 records. The 4,201 non-zero ones are all in one
  RoadRunner (MathWorks) 1.4 file in the TrueVision repository
  (`tongji.xodr`), whose licence is not SPDX-identified. 1.9.0 §10.5.3
  defines a shape relative to the reference plane, interpolates it linearly
  along `s`, and deprecates shape combined with superelevation.
- **Crossfall.** 533 records, all zero, all in RoadRunner 2019 files. The
  1.9.0 element list has no `<crossfall>`; the current rule (zero accepted,
  non-zero `ODR-E202`) needs no change.
- **Lane `<border>`.** No network uses it.
- **Lane height.** 1,665 `<height>` records in 27 files from five producers;
  counted under `ODR-W206` today.

## 6. Road marks

Road marks are in 206 of the 222 networks and in every producer. Of 65,194
`<roadMark>` records, the `type` attribute is `none` 36,007 times, then
`solid` (12,277), `broken` (8,301), `curb` (5,422), `solid solid`,
`broken solid`, `solid broken`, and five rarer values. 29,489 have a
non-zero `width`. 2,219 carry a
`<type>` with `<line>` children (VIRES ROD, `scenariogeneration`,
RoadRunner MathWorks, dSPACE, Libram), 4 an `<explicit>`, and none a
`<sway>`.

## 7. Signals and controllers

- 1,431 `<signal>` in 47 files from 6 producers, and 1,200
  `<signalReference>` in 10 files, almost all RoadRunner 2019.
- `country` is `OpenDRIVE` for 1,264 of the signals, with
  numeric `type` codes such as `1000001` (traffic light), `205`, `206`, and
  `294`. Country codes appear in both cases (`de` and `DE`, `se` and `SE`).
  The importer should keep `country`, `type`, `subtype`, and `value`
  verbatim, not interpret them.
- 1,058 `<validity>`, 222 `<positionInertial>` (RoadRunner), 7
  `<dependency>`, 3 `<reference>`, and 5 `<semantics>` (1.8, Libram).
- 847 `<controller>` with 881 `<control>` in 11 files from four producers;
  212 junction `<priority>` in 13 files (RoadRunner, Libram).
- Lane `<speed>`: 7,016 records in 50 files, mostly RoadRunner MathWorks.
  `dynamic="yes"` on 802 signals.

## 8. Objects

1,701 `<object>` in 63 files from 4 producers and two unattributed files. The `type` is often outside
the enumeration: RoadRunner writes `-1` 744 times. Common values are `tree`,
`crosswalk`, `roadMark`, `parkingSpace`, `none`, `pole`, and `barrier`.
Outlines are common: 551 `<outline>` or `<outlines>` with 3,066
`<cornerLocal>` and 59 `<cornerRoad>`. Rarer children: 77 `<parkingSpace>`,
41 `<repeat>`, 14 `<markings>`, 4 `<validity>`, 4 `<material>`. No file uses
`<objectReference>`; 22 `<bridge>` and 4 `<tunnel>`.

## 9. Data quality

Four networks have a plan-view gap larger than the
[ADR 0004 §6](../adr/0004-opendrive-geometry-retention.md#6-discontinuities)
tolerance of 0.01 m, all after a line or an arc:

| File | Producer | Largest gap |
| --- | --- | ---: |
| isetauto `scenarios/aeb/peab export.xodr` (and a copy) | RoadRunner (MathWorks) 1.4 | 0.12 m |
| TrueVision `src/assets/open-drive/tongji.xodr` | RoadRunner (MathWorks) 1.4 | 11.2 m |
| SMARTS `scenarios/open_drive/od_newmarket/map.xodr` | unattributed 1.4 | 64.4 m |
| SUMO `tests/.../imprecise_geometry/opendrive.xodr` | unattributed 1.4 (a deliberate test) | 3.7 m |

The importer rejects these with `RN-` errors today, as ADR 0004 intends. A
compatibility corpus should not include them unless a later decision changes
the tolerance.

## 10. Fixtures

**Candidates to commit.** Each is small, carries a permissive licence, was
written by its repository's authors with the named tool, and decodes within
the scope in §11 apart from counted warnings.

| File | Source and licence | Producer, version | Bytes | Uses | SHA-256 |
| --- | --- | --- | ---: | --- | --- |
| `assets/samples/curved_road.xodr` | RoadMaker, Apache-2.0 (© 2026 Robomous) | RoadMaker 1.7 | 2,067 | spiral, superelevation | `173518c6083aef39e510a8f2f1f3ba98374bae1c0d87dffff2807eee49f0d456` |
| `assets/samples/park_road.xodr` | as above | RoadMaker 1.8 | 1,692 | spiral | `7ac1d936df7025635454d5518ee982e7494eaf4fa9df253f4ec291985edd85e6` |
| `assets/samples/tree_avenue.xodr` | as above | RoadMaker 1.8 | 3,651 | spiral, 16 objects | `02669673f33ca85e68ade188391717f55b110ecd1c4eddf05f6263d07f78852b` |
| `assets/samples/sign_pack.xodr` | as above | RoadMaker 1.8 | 3,686 | 14 signals | `4010a1009b2763ab41a459be71d79ed1272790f0cc6bf5a1714362d246db70a4` |
| `assets/samples/gs1_urban_intersection.xodr` | as above | RoadMaker 1.8 | 30,053 | 6 signals, 36 objects, road marks | `b81c2b88242c1d6db4605073583fac40ff3e9225d93063dede3ffd9bdb292981` |
| `scenarios/maps/scenario1.xodr` | cema, MIT (© 2022 Bálint Gyevnár) | RoadRunner (MathWorks) 1.6 | 65,571 | 2 signals, 4 objects, lane speed | `188561ac00318a2043116fc0554157e48bfaad5d9afea2b44cb487ad8654a94c` |
| `data/scenes/road/road_006/road_006.xodr` | isetauto, Apache-2.0 | RoadRunner (MathWorks) 1.4 | 23,856 | 2 signals, 4 objects | `6ab8b32a9341d501540b22df7e9baff5aa53c2604750e0bcc706b673a88733d7` |
| `resources/Road2_20.xodr` | maliput_malidrive, BSD-3-Clause (© TRI, Woven Planet) | RoadRunner (MathWorks) 1.4 | 17,244 | 9 spirals | `67dde3e6805bd4fe731652a3691df8bfcadfdedeeab4137c3910a9785c3e8e72` |
| `resources/ParametricCubicCurveComplexRoad.xodr` | as above | xodr_extract App 1.4 | 7,948 | 4 `paramPoly3` (`normalized`) | `ef0f119113feeb52c00f91ecdcaad8a642d7cc743151adbc515d62c8c5a235db` |
| `scenarios/open_drive/od_4lane/map.xodr` | SMARTS, MIT (© Huawei) | SUMO `netconvert` 1.4 | 41,734 | 8 `paramPoly3` without `pRange` | `79205642aa135ae53841b417b690f374c48bd1f0cbeb90edb1e49e37beebee58` |

Together with CARLA `Town01` and `Town02` they cover RoadRunner 2019 and
MathWorks, RoadMaker, SUMO `netconvert`, and `xodr_extract`, at versions 1.4,
1.6, 1.7, and 1.8, in about 197 kB. RoadMaker's `NOTICE` and each licence
text would go beside the files, as the CARLA files are handled. The RoadMaker
samples are original files in an Apache-2.0 repository; its asset ledger
covers media, not the samples.

**Fetched by checksum, not committed.**

- CARLA `Town03` to `Town07` (MIT; checksums in
  [FMT-01 §8](format-spike-opendrive-2026-11-22.md#8-fixtures)) and
  `PythonAPI/util/opendrive/TownBig.xodr` (MIT, 15,536,627 bytes, SHA-256
  `69dcd7e2fcfa29c4e3af1bcf907b858d9d57e9d1243852f4fb8c4919196f2ffc`; 557
  signals, 809 objects).
- esmini VIRES ROD and `scenariogeneration` files (MPL-2.0 repository, no
  per-file notice; FMT-01 did not adopt them for that reason). They hold
  the near-constant-curvature spirals of §4.2 and `pRange="arcLength"`.
- dSPACE `map.xodr` from OSTAR-Quickstart and the Libram file from SUMO
  (EPL-2.0).

**Not adopted.** `qc-opendrive` tests (synthetic, and some derive from
vendor files with an "all rights reserved" header); TrueVision and IGP2
(non-standard licences); the ASAM example (no licence); the 1.1 and 1.2
files.

**Still hand-authored.** A spiral with `curvStart == curvEnd` and one
differing by rounding; spirals and `paramPoly3` with closed-form or
independently computed values; superelevation with known border heights;
road marks with every `type`; and each element §11 rejects.

## 11. Proposed Phase 2 scope

| Feature | Proposed scope | Deferred, with the reason |
| --- | --- | --- |
| F-12 | `spiral` (including equal or nearly equal end curvatures) and `paramPoly3` (both `pRange` values and the absent case) | `poly3`: no use in the corpus, deprecated. `ODR-E201` stays for it. |
| F-13 | Non-zero superelevation; road marks with their attributes and `<type>`/`<line>` | Lateral shape: one file from one producer, not redistributable, and deprecated with superelevation. Lane `<border>`: no use. Non-zero crossfall and `<crossSectionSurface>`: no use. `<explicit>` and `<sway>`: 4 and 0 uses. |
| F-14 | Signals with their position (road or inertial), identity, and `<validity>`; `<signalReference>`; root `<controller>` and junction `<controller>`; junction `<priority>`; lane `<speed>` | Signal `<dependency>`, `<reference>`, and `<semantics>`: 15 uses together. Lane `<access>` and `<rule>`. |
| F-15 | Objects with position, orientation, dimensions, `type` kept verbatim, `<validity>`, `<outline>`/`<outlines>` with both corner kinds, and `<repeat>` kept as a record | `<markings>`, `<material>`, `<parkingSpace>`, `<borders>`, `<skeleton>`; `<objectReference>`, `<tunnel>`, `<bridge>`. |
| F-16 | The ten files of §10 committed, and the fetched set checked by checksum | — |

Deferred elements stay counted (`ODR-W2xx`) or rejected (`ODR-E2xx`) as today.
The `direct` junction and the temporary lane layer still have no committable
fixture, so their backlog rows stay open.

## 12. Not examined

- Running the v0.1.0 importer over the corpus. The element counts above
  predict its diagnostics, but were not checked against it.
- `odrSpiral` compiled with a C++ compiler. The Python transliteration keeps
  the operation order, but a compiler may contract or reorder operations.
- Heading and curvature errors of the spiral composition; only position was
  measured.
- Producers with no files in public repositories (for example Hexagon VTD
  exports beyond the VIRES ROD files, or 4ward), and SUMO `netconvert`
  output beyond what the SUMO and SMARTS repositories contain.
- The OpenDRIVE 1.4 text on the default `pRange`.
