# Release records

Each tagged version receives an immutable record here: objective, shipped
capabilities, compatibility, artifact names and digests, and known
limitations. Release records are history and are not rewritten after
publication.

| Version | Date | Record |
| --- | --- | --- |
| — | — | No releases yet. v0.1.0 is planned at the end of Phase 1. |

## Release gate

A release record is created only after:

1. the version, tag, and finalized changelog heading agree;
2. every declared hosted CI cell passes;
3. source and package-origin gates pass at their documented levels, and the
   plugins are **discovered and open a fixture from the installed package**,
   not only from the source tree;
4. package digests are reproducible for an unchanged build;
5. third-party notices, SBOM and provenance policy, and target metadata are
   verified;
6. the release is assembled as a draft for human review.

### What enforces each item

[`release.yml`](../../.github/workflows/release.yml) runs on a tag push. It
takes its cells from [`openstrata.ci.yaml`](../../openstrata.ci.yaml) through
`ost ci matrix --lane main`, so it uses the same runners and runtime pins as
CI. One job runs per bundle cell, and it also builds and tests the workspace.

| Item | Automation | Left to the reviewer |
| ---: | --- | --- |
| 1 | `preflight` runs [`tools/make_release_notes.py`](../../tools/make_release_notes.py). The tag must be `vX.Y.Z` or `vX.Y.Z-suffix`. `X.Y.Z` must be the version in `openstrata.toml`, in every bundle and library descriptor, and in every CMake `project()`. The changelog needs a dated `[X.Y.Z]` section for a release, or a nonempty `[Unreleased]` section for a pre-release. | That the changelog says what changed. |
| 2 | Every build job repeats what the workspace and bundle cells check: the dependency graph, `ost build`, `ost test`, and the bundle pyramid up to the cell's `up_to`. | That the `ci` run of the tagged commit passed. |
| 3 | Each job runs `ost plugin test --from-package` up to the cell's level. The bundle cells of CI do the same on every pull request and push to `main`, through `source_checks` in the CI contract. | The L6 `usdview` level, which needs the `lookdev` runtime ([REL-01 report](../reports/rel-01-inspection-2026-11-23.md#2-usdview-launch-l6)). |
| 4 | [`tools/check_package.py`](../../tools/check_package.py) requires the same archive digest from two packagings of one build, and the release job also from a clean rebuild of the bundle at the same path. CI checks the two packagings. | — |
| 5 | `tools/check_package.py` checks the archive against `SHA256SUMS` and the manifest. It checks that `LICENSE` (a copy of the root one) and `THIRD_PARTY_NOTICES.md` ship byte-identical. It checks that the SPDX SBOM names the bundle, its license, and the archive digest, and lists every packaged file with its digest. It checks that the provenance records an `ost`-managed build against the validated runtime of the target. It checks that the target, ABI, and OpenUSD range match the bundle manifest. | The SBOM limitation below. |
| 6 | The packages are pushed to GHCR, an anonymous pull of each is checked, and a **draft** release is created with the packages, their manifests and SBOMs, the source archive, `package-pins.json`, and `SHA256SUMS`. | Publishing the draft. |

The SBOM of `ost` 0.23.21 lists the bundle and its files only. pugixml is
compiled into the plugin library but is not an SBOM package, because
`ost plugin package` has no field to declare a vendored component. Its
license ships in `THIRD_PARTY_NOTICES.md`, which item 5 checks.

### Packages on GHCR

Each release pushes one package per target to the public repository
`ghcr.io/animu-sphere/usd-roadnetwork-plugins`, tagged
`roadnetwork-opendrive-<tag without v>-<target>`. A pre-release therefore never
moves a release's tag, but its files keep the bundle version `X.Y.Z`, so
`SHA256SUMS` tells two pre-releases apart. The notes carry the pin table: the
archive digest to name as the `artifact`, and the OCI digest to pull it from.

The first push from this public repository's workflow creates the package,
public and linked to the repository, as it did for the sibling
`usd-gltf-plugins`. If a push ever leaves it private, the anonymous pull check
fails before a draft exists. Make the package public in its settings on
GitHub, then re-run the failed jobs. Pushing the same digest again is
harmless.

The packages are public as soon as they are pushed, before the draft is
reviewed. Discarding a draft does not remove them. They stay addressable by
digest, and the next tag's push moves nothing of theirs.

### Release process

1. Run the dry run with the **release** workflow's *Run workflow* button, or
   `gh workflow run release.yml --ref main`. It runs every build job, uploads
   the files and the rendered notes as workflow artifacts, and pushes and
   creates nothing.
2. In one release pull request, finalize the `[X.Y.Z]` changelog heading
   with the date, and update the capability matrix and the
   [supported configurations](../reference/SUPPORTED_CONFIGURATIONS.md).
3. After it merges, tag the merge commit and push the tag:
   `git tag -a vX.Y.Z -m vX.Y.Z && git push origin vX.Y.Z`.
4. Review the draft against the release gate: the notes, the files, and
   `SHA256SUMS`. A re-run of the workflow replaces a draft, never a
   published release.
5. Add the release record to the table above, with the artifact names and
   digests from the draft's `SHA256SUMS` and `package-pins.json`, in a pull
   request. After it merges, publish the draft on GitHub.
