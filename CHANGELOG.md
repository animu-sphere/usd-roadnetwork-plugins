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
- Source CI on GitHub-hosted Windows, Ubuntu, and macOS runners: the
  `openstrata.ci.yaml` support matrix and the workflow generated from it
  build, test (L0–L5), and package the workspace against OpenUSD 26.08.

### Changed

- `roadnetwork-opendrive` declares `supportsWriting: false`: saving or
  exporting a layer through the `.xodr` format now fails with an explicit
  Sdf error instead of returning false silently.
