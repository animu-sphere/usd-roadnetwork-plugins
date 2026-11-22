# Changelog

All notable user-visible changes are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
semantic versioning for tagged releases.

## [Unreleased]

### Added

- Initial documentation: design policy, workspace contract, API boundary,
  reference skeletons (capability matrix, OpenDRIVE mapping, diagnostics,
  supported configurations), proposed ADRs 0001–0008, and the phase roadmap.
- Apache-2.0 license.
- OpenStrata workspace root and the `roadnetwork-opendrive` bundle scaffold.
  The bundle registers `.xodr` and authors an empty layer; it does not parse
  OpenDRIVE yet.
- `.gitattributes` fixing LF line endings for sources, docs, and XML
  fixtures.
- Skeleton `roadnetwork-core` and `roadnetwork-usd` static libraries with
  OpenStrata library descriptors, declared as workspace members. The bundle
  now authors its (still empty) layer through the shared `roadnetwork-usd`
  writer.
- Minimal hand-authored OpenDRIVE XML fixtures replace the scaffold
  placeholders, with an L5 golden for `basic.xodr`.
- ADR 0006 accepted: the file-format contract and diagnostic numbering.
  The bundle ships empty `ODR-` and `RN-` diagnostic catalogs
  (`diagnostics.json`, `rn-diagnostics.json`) in its plugin resources.
- File-format contract, writer change-block, and catalog cross-check tests,
  run by the composed root's CTest suite.
- OpenDRIVE reader (ODR-01): `CanRead()` sniffs the first 64 KiB for an
  `<OpenDRIVE>` root, and `Read()` rejects an unreadable asset, a non-UTF-8
  document, malformed XML, a wrong or namespaced root, a missing header, and
  any version other than 1.4 to 1.9 with `ODR-E001` to `ODR-E007`. A failed
  reload keeps the previous layer content. Accepted files still open as an
  empty layer, because roads are not decoded yet.
- OpenDRIVE decoder (ODR-02): a full read decodes roads, line and arc
  reference lines, elevation, lane offset, lane sections and widths, road
  and lane links, and default junctions into `RoadNetworkData`, builds the
  canonical lane graph from one-sided or two-sided links, and validates the
  result. Unsupported representations (spiral, poly3, paramPoly3, non-zero
  superelevation, lane borders, non-default junctions, temporary lane
  layers) and ambiguous lane links fail with `ODR-E008` to `ODR-E206`.
  Validation failures are relayed with their `RN-` code. Data that is read
  but not represented (road marks, signals, objects, user data, and others)
  is reported once per kind in `ODR-W201` to `ODR-W208`. CARLA `Town01` and
  `Town02` decode and validate. Files still open as an empty layer until the
  stage contract (USD-01).
- pugixml 1.16 (MIT), vendored in `third_party/pugixml` and compiled into the
  `roadnetwork-opendrive` bundle with hidden symbols.
- Source CI on GitHub-hosted Windows, Ubuntu, and macOS runners: the
  `openstrata.ci.yaml` support matrix and the workflow generated from it
  build and run the CTest suite of the workspace, and build, test (L0–L5),
  and package the OpenDRIVE bundle against OpenUSD 26.08. Changes to
  documentation alone do not build.
- `roadnetwork-core` defines the `RoadNetworkData` model of ADR 0001: roads
  with analytic reference lines, profiles, and lane sections; junctions and
  connections; the canonical lane graph; and source provenance.
  `Validate()` checks the model's identity, reference, lane-structure, and
  numeric invariants and reports the first failure with a stable `RN-E1xx`
  or `RN-E3xx` code. No decoder uses it yet, so imports are unchanged.
- `roadnetwork-core` evaluates line and arc reference lines, elevation, lane
  offset, and lane widths (ADR 0004), and computes lane borders and
  centerlines. `TessellateRoad()` samples each lane section into polylines
  within a 1 cm error bound, with shared vertices between adjacent lanes.
  `Validate()` now also rejects a lane border that reaches the center of
  curvature of an arc, with `RN-E308`. The writer does not use them yet.
- `Validate()` can return warnings. `RN-W301` reports an outermost lane
  border that folds back past an arc's center of curvature. CARLA has three,
  all sidewalks.

### Changed

- `roadnetwork-opendrive` declares `supportsWriting: false`: saving or
  exporting a layer through the `.xodr` format now fails with an explicit
  Sdf error instead of returning false silently.
- `RN-E308` applies only when the folding border is the inner border of a
  lane: the center border, or the outer border of a lane with another lane
  outside it. When only the outermost lane's outer border folds, the result
  is `RN-W301` (ADR 0004 §2).
