# Format spike: ASAM OpenDRIVE — 2026-11-22

This is the FMT-01 spike in [current work](../roadmap/current.md). It records
what was observed on 2026-11-22 against the named specification pages,
schemas, repositories, and files. It is not a support claim: the
[capability matrix](../reference/CAPABILITY_MATRIX.md) still lists every
OpenDRIVE capability as unsupported. It covers the items in
[design policy §4.1](../design/DESIGN_POLICY.md#41-format-acceptance-prerequisites).

## Summary

| Check | Result |
| --- | --- |
| Versions | 1.9.0 is current (released 2026-05-19). Eight releases from 1.4.0 (2015) to 1.9.0 share one root element, no XML namespace, and the `<header revMajor revMinor>` version source (§1, §3). |
| Phase 1 version scope | Accept `revMajor="1"` with `revMinor` 4 to 9. Reject other values with a stable error (§3.3). Accepted by the maintainer on 2026-11-22. |
| Specification access | The 1.9.0 specification is readable online without registration. The downloadable package is free but needs registration (§2). |
| Licensing | The specification, schemas, and ASAM example files are under the ASAM license terms, which do not grant a right to republish them here. Do not vendor them (§2). |
| Coordinates | The inertial frame is right-handed and `z`-up (ISO 8855) and uses metres and radians. A georeference is a PROJ string, recorded verbatim. A header `<offset>` exists from 1.5 (§4). |
| Phase 1 elements | The available real-world files use only `line` and `arc` geometry, so spirals can stay in Phase 2. `<laneOffset>` and `<elevation>` are on almost every road, so Phase 1 needs both (§5). |
| XML parser | pugixml 1.16 (MIT), private to the bundle. It is a non-validating DOM parser that does not expand DTD entities (§6). |
| Existing libraries | libOpenDRIVE (Apache-2.0) and esmini (MPL-2.0) are useful as reference evaluators, not as dependencies. VIRES `odrSpiral` (Apache-2.0) is the spiral candidate for Phase 2 (§7). |
| Fixtures | Hand-authored fixtures stay the unit evidence. CARLA's `opendrive-test-files` (MIT, 7 networks, 1.4) is the real-world compatibility corpus; `Town01` and `Town02` are committed with their MIT licence (§8). |
| Recommendation | No blocker for Phase 1. The proposed scope is in §9. |

## 1. Sources

| Source | Revision or date read | Licence |
| --- | --- | --- |
| [ASAM OpenDRIVE standard page](https://www.asam.net/standards/detail/opendrive/) and [older versions](https://www.asam.net/standards/detail/opendrive/older/) | 2026-11-22 | — |
| [ASAM OpenDRIVE 1.9.0 specification](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.9.0/specification/index.html): §5 backward compatibility, §6.3 root element, §6.4 header, §8 coordinate systems, §9 geometries, §11.7 lane geometry, Annex F map rules, Annex G revision history | 1.9.0, dated 2026-05-08 | ASAM license terms |
| [ASAM license terms](https://www.asam.net/license/) | dated 2017-10-26 | — |
| [asam-ev/qc-opendrive](https://github.com/asam-ev/qc-opendrive): `qc_opendrive/schema/` (XSDs 1.1 to 1.8.1), `schema/schema_files.py`, `base/utils.py`, `checks/schema/valid_schema.py` | `main` on 2026-11-22 (last push 2026-03-09) | MPL-2.0 for the code; the XSDs carry the ASAM notice |
| [asam-ev/ASAM-OpenX-Examples](https://github.com/asam-ev/ASAM-OpenX-Examples): `ASAM_OpenDRIVE/Signalized_T_Intersection.xodr` | `main` on 2026-11-22 (last push 2026-03-23) | none declared |
| [carla-simulator/opendrive-test-files](https://github.com/carla-simulator/opendrive-test-files): `OpenDrive/Town01.xodr` to `Town07.xodr` | commit `5929b67cc414` (2019-06-19), the head of `master` | MIT |
| [esmini/esmini](https://github.com/esmini/esmini): `resources/xodr/`, `odr_coverage.txt`, `externals/pugixml/` | `master` on 2026-11-22; latest release `v3.11.0` (2026-11-05) | MPL-2.0 |
| [pageldev/libOpenDRIVE](https://github.com/pageldev/libOpenDRIVE): `README.md`, `CMakeLists.txt`, `src/Geometries/`, `tests/test.xodr` | `main` on 2026-11-22 (last push 2026-10-15); latest release `0.5.0` (2023-09-13) | Apache-2.0 |
| [zeux/pugixml](https://github.com/zeux/pugixml): `docs/manual.adoc`, `src/pugiconfig.hpp` | `master`; latest release `v1.16` (2026-06-16) | MIT |
| [leethomason/tinyxml2](https://github.com/leethomason/tinyxml2) | latest release `11.0.0` (2025-03-15) | Zlib |
| [libexpat/libexpat](https://github.com/libexpat/libexpat) | latest release `R_2_10_0` (2026-11-02) | MIT |

Files were fetched with `curl` and inspected with `grep`, `awk`, and
`sha256sum`. No file from these sources is committed by this report.

## 2. Specification access and licensing

**Versions.** ASAM lists these releases. 1.4.0 and 1.5.0 were published by
VIRES Simulationstechnologie before ASAM took over the standard.

| Version | Release date | Publisher |
| --- | --- | --- |
| 1.4.0 | 2015-11-04 | VIRES |
| 1.5.0 | 2019-02-17 | VIRES |
| 1.6.0 | 2020-03-12 | ASAM |
| 1.6.1 | 2021-03-04 | ASAM |
| 1.7.0 | 2021-08-03 | ASAM |
| 1.8.0 | 2023-11-22 | ASAM |
| 1.8.1 | 2024-11-21 | ASAM |
| 1.9.0 | 2026-05-19 | ASAM |

The standard page says ASAM OpenX is in maintenance mode, with change
requests still collected. The datasheet also lists `.xodrz` as a file format.
This spike did not examine `.xodrz`.

**Access.** The HTML specification for 1.8.0, 1.8.1, and 1.9.0 is readable
at `publications.pages.asam.net` without an account. The download package
(specification and "XML schemata") is free of charge, but ASAM asks for an
account or contact details and acceptance of its terms, and sends it by email.
The 1.9.0 HTML specification links a `ASAM_OpenDRIVE_v1-9-0_xsd_schema_files.zip`
deliverable. This spike read the 1.4 to 1.8.1 schemas from the ASAM
quality-checker repository instead (§3).

**Licence.** The [ASAM license terms](https://www.asam.net/license/) define
"Standards" to include schemata and sample files. Non-member licensees "are not
entitled to pass standards on to third parties". Members may share them only
unchanged, with the copyright notices and a copy of the terms. The 1.7.0 and
1.8.1 XSD headers say the file "is distributable in accordance with the ASAM
license terms", and ASAM's own MPL-2.0 checker repository ships them. The
terms nonetheless give no general right to republish them in an Apache-2.0
repository. The ASAM example repository declares no licence. This is a
reading of the terms, not legal advice.

Consequences for this repository:

- Do not vendor the specification, the XSDs, or ASAM example files.
- Name elements, attributes, and rules by their specification names, and cite
  the HTML specification by section. Do not copy its text or figures.
- The reader does not validate against an XSD at run time (§6), so it does not
  need the schemas.
- A development-time schema check of hand-authored fixtures can use the
  qc-opendrive package as an external tool without committing its schemas.

## 3. Versions and recognition

### 3.1 Root element and namespace

Every schema examined (1.4H, 1.5M, 1.6.1, 1.7.0, 1.8.1) declares a global
element `OpenDRIVE` with no `targetNamespace`. Specification §6.3 gives the
root element no attributes. None of the 18 distinct real files examined in §8
declares a namespace or root attribute; all use a bare `<OpenDRIVE>`. The
[ADR 0006 `CanRead()`](../adr/0006-fileformat-contract.md#1-canread) rule
(first element named `OpenDRIVE` in no namespace) therefore holds for all
versions in scope.

Two prologue cases from ADR 0006 occur in real files. The ASAM example starts
with a UTF-8 byte-order mark. The libOpenDRIVE copy of CARLA `Town07.xodr`
starts with an XML comment that holds the MIT licence text; the root element
starts at byte 1,352. All ten esmini files use `standalone="yes"` in the XML
declaration. No UTF-16 file was found.

### 3.2 Version source

The version is the integer pair `<header revMajor revMinor>`. The header is the
mandatory first child of `<OpenDRIVE>` (§6.4). Both attributes are optional
in the 1.4H schema and required from 1.5M. The 1.5M to 1.8.1 schemas fix
`revMajor` to `1`; 1.5M also fixes `revMinor` to `5`. ASAM's own checker derives
the version as `revMajor.revMinor.0` and uses it to select a schema. It treats
1.6.0 and 1.6.1, and 1.8.0 and 1.8.1, as the same schema version. A file
therefore cannot state its patch level, and the reader should not infer one.

The other header attributes are not reliable:

- `version` is the network's own version, not the format version. It is a
  float in the 1.4 schema and a string from 1.6.
- `date` is free text in practice. CARLA files use ISO 8601. One esmini file
  uses `Tue Jun 30 14:55:47 2020`. Keep it as an opaque string.
- `north`, `south`, `east`, and `west` are often all zero (esmini) or omitted
  (the ASAM example).

### 3.3 Proposed Phase 1 version policy

| Header | Behaviour |
| --- | --- |
| `revMajor="1"`, `revMinor` 4 to 9 | Accept. Interpret by the 1.9.0 rules, except where §3.4 lists a difference for an element in scope. |
| `revMinor` below 4 | Error: unsupported version. No fixture or need exists, and the 1.1 to 1.3 schemas differ more. |
| `revMinor` above 9 | Error: unsupported version. ASAM promises that a newer version reads older files, not the reverse, and the [strictness rule](../design/DESIGN_POLICY.md#9-strictness) forbids guessing. Revisit when 1.10 is published. |
| `revMajor` not `1`, a missing header, or a non-integer value | Error. |

The maintainer accepted this policy on 2026-11-22, including the error for
`revMinor` above 9.

The version is authored as provenance in both full and metadata-only reads.
The codes are assigned in the implementing PR under
[ADR 0006](../adr/0006-fileformat-contract.md#6-diagnostic-numbering).

### 3.4 Differences that touch the Phase 1 elements

| Element | Difference |
| --- | --- |
| `<header>/<offset>` | Not in the 1.4H schema; present from 1.5M. |
| `<junction type>` | `default` and `virtual` from 1.5; `direct` from 1.7. Phase 1 supports `default` only, because direct and virtual junctions change the connectivity model. |
| `<lane type>` | 1.8.0 deprecates several values, including `bidirectional`, `bus`, and `HOV`. Older files use them (CARLA has 12 `bidirectional` lanes), so they are accepted for every version. |
| `<poly3>` | Deprecated (the 1.9.0 page gives both 1.6.0 and 1.8.0 as the date). Out of Phase 1 scope in any case. |
| `<width>` and `<border>` | Exclusive in the same lane group since 1.4. 1.9.0 adds a rule ID for it. `<border>` is also exclusive with `<laneOffset>`. |
| Lane layers, roadworks, `@temporary`, `@invalidated` | New in 1.9.0. Not in Phase 1 scope. Their presence must be diagnosed, not ignored. |

1.9.0 §5 states that 1.9.0 is backwards compatible with 1.8.1. The
revision histories for 1.8.0 and 1.9.0 list no change to the header,
`geoReference`, or `offset`.

## 4. Coordinates and georeference

From 1.9.0 §8:

- The inertial frame is right-handed per ISO 8855, with `z` up. With a
  georeference, `x` is east and `y` is north.
- A heading is a rotation about `z`, counterclockwise from `+x`. Heading 0
  points along `+x`.
- The reference-line frame is `(s, t, h)`: `s` along the reference line in
  metres, `t` positive to the left, `h` up. Superelevation is a roll about `s`.
- Lengths are in metres and angles in radians.

**Georeference.** `<geoReference>` holds a PROJ string, marked as CDATA. EPSG
is mentioned only as a source of parameter sets. No EPSG code or WKT form is
defined. 1.9.0 adds the rule that there is at most one projection, and that a
missing one means a local Cartesian frame. Real files show why the importer
must not interpret the string:

- All seven CARLA towns carry `+lat_0=4.9000000000000000e+1 +lon_0=8.0000000000000000e+0`,
  with no `+proj`. It is not a complete projection.
- Five esmini files carry a `+proj=tmerc` string with
  `+geoidgrids=egm96_15.gtx`, which needs an external grid file.
- The same five files put whitespace between `<geoReference>` and the CDATA
  section.

This supports the [ADR 0003](../adr/0003-crs-axes-units.md) direction: record
the string verbatim, with only the surrounding whitespace trimmed, and never
reproject.

**Offset.** `<offset x y z hdg>` (all required) translates and then rotates by
`hdg`. Positions must have the offset applied before PROJ is used. The
specification advises against large coordinates and against a rotation. None
of the 18 real files uses `<offset>`. The largest plan-view coordinate in the
CARLA towns is 670.4 m. No available fixture exercises large coordinates, so
the ADR 0003 precision test needs a hand-authored one.

## 5. Elements needed for Phase 1

Element counts over the seven CARLA towns, which RoadRunner 2019.0.0
(`vendor="VectorZero"`) wrote as OpenDRIVE 1.4:

| Element | Count | Note |
| --- | --- | --- |
| `<road>` | 1,347 | 98 to 279 per town |
| `<geometry>` | 6,460 | `<line>` 4,545, `<arc>` 1,915; no `spiral`, `poly3`, or `paramPoly3` |
| `<elevation>` | 7,052 | On every road |
| `<superelevation>`, `<shape>` | 0 | No `<lateralProfile>` |
| `<laneSection>` | 4,052 | No `singleSide` |
| `<laneOffset>` | 4,077 | On almost every road |
| `<lane>` | 13,448 | Mostly `driving` and `none`, then `shoulder`, `sidewalk`, `parking`, `median`, `bidirectional`, `border` |
| `<width>` | 9,488 | No `<border>` |
| `<predecessor>`, `<successor>` | 7,005 / 7,159 | Road and lane links |
| `<junction>` | 157 | No `type` attribute (default) |
| `<connection>`, `<laneLink>` | 954 / 1,213 | |
| `<type>` with `<speed>` | 393 | Road type and speed |
| `<roadMark>` | 15,665 | Post-MVP |
| `<userData>` | 10,311 | `vectorScene` and `vectorLane` (RoadRunner) |
| `<signal>`, `<object>` | 0 | |

The ASAM 1.8 example (`Signalized_T_Intersection.xodr`) adds six `<signal>`
elements to `line` and `arc` roads. The esmini files show what Phase 1 will
reject: 4 of 10 use `spiral`, 3 use `paramPoly3`, and 9 carry VIRES
`viStyleDef` user data.

Findings for the [Phase 1 scope](../roadmap/release-plan.md#phase-1--opendrive-vertical-slice):

- **Spiral, poly3, and paramPoly3 can stay in Phase 2.** The hand-authored
  fixtures and the CARLA corpus need only `line` and `arc`. Many tool-written
  files elsewhere use spirals, and Phase 1 rejects them with an
  unsupported-geometry error.
- **`<laneOffset>` must be in Phase 1.** It shifts every lane boundary. If it
  were dropped, the boundaries would be wrong with no error, which the
  [MVP rule](../design/DESIGN_POLICY.md#71-minimum-useful-implementation-mvp)
  forbids. The release-plan scope does not list it today.
- **Elevation is on every real road.** The Phase 1 profile policy has to
  handle it, not only reject it.
- **`<border>`, non-`default` junctions, and lane layers** are rejected with an
  error in Phase 1. Each changes geometry or connectivity, so dropping it is
  not a bounded loss.
- **`<roadMark>`, `<userData>`, `<signal>`, and `<object>`** are counted and
  reported with one warning each, as
  [OPENDRIVE_MAPPING.md §7–8](../reference/OPENDRIVE_MAPPING.md#7-signals-and-objects)
  requires.

## 6. XML parser

| Candidate | Licence | Latest release | Model | Notes |
| --- | --- | --- | --- | --- |
| **pugixml** | MIT | 1.16 (2026-06-16) | DOM, in memory | Non-validating. Does not expand entities declared in a DOCTYPE; only the five predefined entities and character references are expanded. Bounded stack on malformed DOCTYPE since 1.6. One `.cpp` and two headers. esmini vendors it, and libOpenDRIVE fetches 1.14. |
| tinyxml2 | Zlib | 11.0.0 (2025-03-15) | DOM, in memory | Also small. Slower and less widely used by OpenDRIVE tools. |
| Expat | MIT | 2.10.0 (2026-11-02) | Streaming (SAX) | Bounded memory for huge files. More reader code. The project page warns of unfixed security issues. |
| libxml2 | MIT | not examined | DOM or streaming, validating | Can validate XSD 1.0 only. The 1.8.1 schemas use XSD 1.1 `xs:alternative`, as qc-opendrive notes. Large dependency with DTD features to disable. |

**Recommendation: pugixml 1.16**, vendored into the bundle as source, compiled
statically, and kept out of every public header, following the
[dependency policy](../design/DESIGN_POLICY.md#14-dependency-policy). The
largest CARLA town is 2.9 MB, so a DOM parse is not a memory concern in
Phase 1. A byte limit before parsing belongs to the import limits (category
`5xx`).

One risk needs a test. MaterialX ships its own pugixml (1.9 in a local
MaterialX install, in the same `pugi` namespace), and OpenUSD builds with
MaterialX can load it into the same process. The bundle must build pugixml
with hidden symbols on Linux and macOS so the two copies cannot bind to each
other. A package-origin test with `usdMtlx` loaded can confirm this.

## 7. Existing OpenDRIVE libraries

| Library | Licence | State | Fit |
| --- | --- | --- | --- |
| [libOpenDRIVE](https://github.com/pageldev/libOpenDRIVE) | Apache-2.0 | C++17. Release 0.5.0 (2023-09-13); `main` active in 2026-10. README "targets OpenDRIVE version 1.4". Has line, arc, spiral, paramPoly3, and cubic-polynomial geometry, and builds road meshes. | Reference only. It mixes parsing with mesh generation, has no diagnostic model, and targets 1.4. Useful as an independent numeric cross-check in a development tool. |
| [esmini RoadManager](https://github.com/esmini/esmini) | MPL-2.0 | C++. Release v3.11.0 (2026-11-05). `odr_coverage.txt` claims OpenDRIVE 1.5 and every plan-view geometry. Vendors pugixml. | Reference only. It is part of a scenario player, not a library with a stable API, and MPL-2.0 files would need separate handling. |
| [VIRES `odrSpiral`](https://github.com/pageldev/libOpenDRIVE/tree/main/src/Geometries/Spiral) | Apache-2.0 (VIRES, 2010) | C. Fresnel integrals after the Cephes library. Its notice says it is not the only way to compute OpenDRIVE spirals. | Phase 2 candidate for spiral evaluation under [ADR 0004](../adr/0004-opendrive-geometry-retention.md). Its error bound still has to be measured. |
| [qc-opendrive](https://github.com/asam-ev/qc-opendrive) | MPL-2.0 | Python. ASAM's quality checker, with per-version schema and map-rule checks. | Development tool for validating hand-authored fixtures. Not a dependency. |

The design policy keeps the decoder in this repository so that strictness,
diagnostics, and the IR are under the project's control. Nothing above
changes that.

## 8. Fixtures

| Source | Licence | Version | Use |
| --- | --- | --- | --- |
| Hand-authored (this repository) | Apache-2.0 | 1.8 header today | Unit evidence: small, checkable by hand, one feature each. Remains the primary evidence. |
| CARLA `opendrive-test-files`, `Town01` to `Town07` | MIT, Computer Vision Center (CVC), UAB, 2019 | 1.4, RoadRunner 2019.0.0 | Real-world compatibility corpus for Phase 1. They use only `line` and `arc`. Sizes run from 0.5 MB (Town01) to 2.9 MB (Town05). |
| esmini `resources/xodr` (10 files sampled) | Repository MPL-2.0; no per-file notice | 1.4 to 1.7 | Not adopted. Most carry VIRES user data, and nothing states who authored them or whether the repository licence covers them. |
| ASAM example `Signalized_T_Intersection.xodr` | None declared | 1.8 | Not adopted. It can be checked locally, but not committed. |
| libOpenDRIVE `tests/test.xodr` | MIT (a copy of CARLA Town07, notice kept in a comment) | 1.4 | Same as CARLA Town07. Use the CARLA original. |

SHA-256 of the CARLA files at commit `5929b67cc414`:

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `Town01.xodr` | 498,388 | `15794f0dff5911837e6059ed628618ad666b3526748b698c0be25cdef7b27f70` |
| `Town02.xodr` | 616,712 | `a6f47c763e09cdf34e759540eef8ef9399e2d899d8eaa33054a5203bceb5494b` |
| `Town03.xodr` | 2,271,104 | `a711706c130c12b788538439fa326f95c92b2486cff810c3001e8c5e04098299` |
| `Town04.xodr` | 2,083,394 | `1f334fab0f430819a93ad9f0c4a18cec8ff0c4d208412fa77eabaacdb327360d` |
| `Town05.xodr` | 2,884,414 | `d01a356d8f5bfb2e508347d08cfaa09f4f395062187038ba8b4212141e354e37` |
| `Town06.xodr` | 2,086,305 | `0e1d68c9a37a6052b95d76c8bdee32da879deac7e8a82566e8bd1d57b2fa5abc` |
| `Town07.xodr` | 1,668,844 | `b76247ec1796b072df79c018402cb7f3312a91c3c09af044954a7f4df06b4ef2` |

The maintainer accepted committing `Town01` and `Town02` (1.1 MB together)
on 2026-11-22, on the condition that their licence is stated explicitly. They
are in
[`plugins/roadnetwork-opendrive/tests/fixtures/carla/`](../../plugins/roadnetwork-opendrive/tests/fixtures/carla/),
unmodified, beside the upstream MIT `LICENSE` and a README with the commit and
checksums, and are listed in
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md#carla-opendrive-test-files).
`ost plugin package` 0.23.21 packs only the fixtures named in the manifest's
test lists, so the bundle's own
[THIRD_PARTY_NOTICES.md](../../plugins/roadnetwork-opendrive/THIRD_PARTY_NOTICES.md)
carries the MIT text and is declared under `notices`; every package of the
bundle includes it (observed locally: 32 files, the notice among them). They give the source and package tests two real networks
with 12 and 8 junctions once the reader decodes them. The other towns stay
outside the tree, fetched by pinned commit and checksum for local
compatibility runs. [Design policy §15.1](../design/DESIGN_POLICY.md#151-test-data)
still applies: they extend the evidence, and are never the only evidence for
a claim.

Hand-authored fixtures that Phase 1 needs and no external source provides:

- `<laneOffset>` and `<elevation>` with known closed-form results;
- a `default` junction with road and lane links checkable by hand;
- each rejection: spiral, `<border>`, a `direct` junction, an unsupported
  `revMinor`, a namespaced root;
- prologue variants for `CanRead()`: byte-order mark, comment before the
  root, a long comment that pushes the root past 64 KiB;
- large projected coordinates with an `<offset>`, for ADR 0003.

## 9. Proposed Phase 1 scope

1. **Versions.** `revMajor="1"`, `revMinor` 4 to 9, per §3.3. Update
   [OPENDRIVE_MAPPING.md §1](../reference/OPENDRIVE_MAPPING.md#1-versions-and-detection)
   in the implementing PR.
2. **Parser.** Vendor pugixml 1.16, private to the bundle, with hidden symbols.
3. **Geometry.** `line` and `arc`. Reject the other geometry types with an
   error.
4. **Lanes.** Add `<laneOffset>` to the release-plan scope, next to lane
   sections, `<width>`, type, and links. Reject `<border>`.
5. **Profiles.** `<elevation>` per the Phase 1 profile policy. Superelevation
   and shape have no real-world fixture yet, so they are hand-authored if kept
   in scope.
6. **Junctions.** `default` only.
7. **Georeference.** The verbatim PROJ string and `<offset>` as provenance;
   the ADR 0003 decision governs any transform.
8. **Loss.** One warning each, with counts, for `<roadMark>`, `<userData>`,
   `<signal>`, `<object>`, and other unknown elements.
9. **Fixtures.** Hand-authored unit fixtures per §8, plus the committed CARLA
   `Town01` and `Town02`, declared in the manifest's test lists in the PR that
   decodes them.

## 10. Not examined

- `.xodrz`, named on the ASAM datasheet.
- The 1.9.0 XSD package. 1.9.0 was checked against its HTML specification
  only.
- The ASAM specification text for 1.4 to 1.7, beyond what the schemas and the
  1.8.0 and 1.9.0 revision histories show.
- The behaviour of libOpenDRIVE and esmini on the fixtures. Neither was built.
- The numeric error of `odrSpiral`.
