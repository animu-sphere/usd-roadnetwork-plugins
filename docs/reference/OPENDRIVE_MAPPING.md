# OpenDRIVE mapping contract

- **Status:** Draft skeleton (2026-11-22). No element is mapped yet. Each
  section is completed in the PR that implements it, with the fixtures that
  prove it, and the matching [capability matrix](CAPABILITY_MATRIX.md) row is
  updated at the same time.

This will be the normative mapping for the `roadnetwork-opendrive` bundle: how
each ASAM OpenDRIVE element becomes `RoadNetworkData` and which USD values the
shared writer authors from it. Element names below follow the OpenDRIVE XML
schema; the FMT-01 spike confirms them per specification version.

## 1. Versions and detection

| Item | Decision |
| --- | --- |
| Specification versions in scope | open — FMT-01 |
| Version source | `<header revMajor revMinor>` |
| Unsupported version | error, code assigned under ADR 0006 |
| `CanRead()` signature | `.xodr` extension and an `<OpenDRIVE>` root element; no body parsing ([design policy §10.2](../design/DESIGN_POLICY.md#102-canread-is-a-cheap-routing-decision)) |

A per-version table records, for each element below, whether it is supported,
ignored with a warning, or rejected.

## 2. Coordinate frames

OpenDRIVE defines a right-handed inertial frame with `z` up, a reference-line
frame `(s, t, h)` along each road, and local frames for objects and signals.
Headings are measured counterclockwise from the inertial `x` axis. How these
map to USD `upAxis`, `metersPerUnit`, and any origin shift is settled by
[ADR 0003](../adr/0003-crs-axes-units.md).

| Source | Target | Status |
| --- | --- | --- |
| `<header>/<geoReference>` | CRS provenance on `/Asset` | open — ADR 0003 |
| `<header>/<offset>` | origin transform | open — ADR 0003 |

## 3. Roads and reference line

| Source | IR | USD | Status |
| --- | --- | --- | --- |
| `<road id length junction>` | road with source ID, length, junction membership | prim under `/Asset/Network/Roads` (path rule: ADR 0005) | open |
| `<road>/<link>/<predecessor>`, `<successor>` | road links with contact point | relationship (direction: ADR 0005) | open |
| `<road>/<type>` | road type and speed records along `s` | open — ADR 0002 | open |
| `<planView>/<geometry s x y hdg length>` | ordered reference-line segments | open — ADR 0004 | open |
| `<line>` | analytic line | — | Phase 1 |
| `<arc curvature>` | analytic arc | — | Phase 1 |
| `<spiral curvStart curvEnd>` | clothoid | — | Phase 2 |
| `<poly3 a b c d>` | cubic in local `(u, v)` | — | Phase 2 |
| `<paramPoly3 ... pRange>` | parametric cubic | — | Phase 2 |

## 4. Profiles

| Source | IR | Status |
| --- | --- | --- |
| `<elevationProfile>/<elevation s a b c d>` | cubic elevation along `s` | open — must not be silently dropped ([design policy §7.1](../design/DESIGN_POLICY.md#71-minimum-useful-implementation-mvp)) |
| `<lateralProfile>/<superelevation>` | cubic roll along `s` | open |
| `<lateralProfile>/<shape>` | lateral shape | open |

## 5. Lanes

| Source | IR | Status |
| --- | --- | --- |
| `<lanes>/<laneOffset>` | lateral offset of the center lane | open |
| `<laneSection s singleSide>` | lane section with start `s` | open |
| `<lane id type level>` | lane; `id` is positive on the left, negative on the right, `0` for the center lane | open |
| `<lane>/<link>` | lane predecessor and successor | open |
| `<lane>/<width>` / `<border>` | width or border polynomial | open |
| `<lane>/<roadMark>` | road marks | post-MVP |
| `<lane>/<speed>` | lane speed limits | open |

## 6. Junctions

| Source | IR | Status |
| --- | --- | --- |
| `<junction id>` | junction | open |
| `<connection incomingRoad connectingRoad contactPoint>` | connection | open |
| `<laneLink from to>` | lane-level link | open |

## 7. Signals and objects

Post-MVP ([design policy §7.2](../design/DESIGN_POLICY.md#72-after-the-mvp)).
Until mapped, their presence produces a warning with an aggregated count
rather than being dropped silently.

## 8. Loss and diagnostics

Every element that is read but not represented produces a diagnostic listed
in [DIAGNOSTICS.md](DIAGNOSTICS.md). Unknown elements and attributes are
counted and reported, not ignored silently. User data (`<userData>`) and
vendor extensions: open — ADR 0002.
