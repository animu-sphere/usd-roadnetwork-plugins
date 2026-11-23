# SPDX-License-Identifier: Apache-2.0
"""Check a release tag against the repository, and render the release notes.

With --tag, the tag must be vX.Y.Z or vX.Y.Z-suffix, and X.Y.Z must be the
version of the workspace, of every member, and of every CMake project. The
changelog must have the section the tag needs: [X.Y.Z] with a date for a
release, or a nonempty [Unreleased] for a pre-release. Without --tag this is a
dry run of the workspace version, which takes [X.Y.Z] once the changelog has
it, and [Unreleased] before.

Package files are named by X.Y.Z, because the suffix belongs to the tag and not
to the bundle. It needs only the Python standard library:

    python tools/make_release_notes.py --tag v0.1.0 --github-output
    python tools/make_release_notes.py --tag v0.1.0 --checksums SHA256SUMS --out notes.md
"""

import argparse
import pathlib
import re
import sys
import tomllib

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPOSITORY = "https://github.com/animu-sphere/usd-roadnetwork-plugins"
TAG = re.compile(r"v(\d+\.\d+\.\d+)(-[0-9A-Za-z][0-9A-Za-z.-]*)?")
BLOCK_VERSION = r"^{block}:\s*$.*?^\s+version:\s*(\S+)"
REQUIRED_VERSION = re.compile(r"^\s+version:\s*\"([^\"]*)\"", re.M)
CMAKE_VERSION = re.compile(r"^project\(\s*\S+\s+VERSION\s+(\S+)", re.M)
# A link relative to the repository root, which the release page would resolve elsewhere.
RELATIVE_LINK = re.compile(r"\]\((?![A-Za-z][A-Za-z0-9+.-]*:|#|/)([^)\s]+)\)")

NOTES = """# usd-roadnetwork-plugins {tag}

Opens OpenDRIVE (`.xodr`) road networks as read-only OpenUSD layers through
`Usd.Stage.Open`.

- **What is supported:** [capability matrix]({blob}/docs/reference/CAPABILITY_MATRIX.md)
- **Runtimes and platforms:** [supported configurations]({blob}/docs/reference/SUPPORTED_CONFIGURATIONS.md)
- **Release record:** [release records]({blob}/docs/releases/README.md)

{changes}

## Files

| File | Contents |
| --- | --- |
| `roadnetwork-opendrive-{version}-<target>.tar.zst` | The plugin bundle for one OpenUSD runtime target |
| `roadnetwork-opendrive-{version}-<target>.manifest.json` | Its OpenStrata manifest, with provenance |
| `roadnetwork-opendrive-{version}-<target>.sbom.spdx.json` | Its SPDX software bill of materials |
| `usd-roadnetwork-plugins-{source}-src.tar.gz` | The source at this tag |
| `package-pins.json` | The digests to pin each package on GHCR |
| `SHA256SUMS` | Checksums of the files above |

Each package carries the project's `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Checksums

```text
{checksums}
```
"""


def fail(message):
    sys.exit(f"error: {message}")


def versions():
    """Return the workspace version, after checking that every member and project agrees."""
    version = tomllib.loads((ROOT / "openstrata.toml").read_text(encoding="utf-8"))["project"]["version"]
    major, minor = version.split(".")[:2]
    # Members release together, so each requires the others at the current minor version.
    allowed = f">={major}.{minor},<{major}.{int(minor) + 1}"
    descriptors = [(path, "plugin") for path in sorted(ROOT.glob("plugins/*/openstrata.plugin.yaml"))]
    descriptors += [(path, "library") for path in sorted(ROOT.glob("libs/*/openstrata.library.yaml"))]
    for path, block in descriptors:
        text = path.read_text(encoding="utf-8")
        name = path.relative_to(ROOT).as_posix()
        match = re.search(BLOCK_VERSION.format(block=block), text, re.M | re.S)
        if not match:
            fail(f"{name} has no {block} version")
        if match[1] != version:
            fail(f"openstrata.toml has {version}, but {name} has {match[1]}")
        requires = text.split("\nrequires:", 1)[1] if "\nrequires:" in text else ""
        for required in REQUIRED_VERSION.findall(requires.split("\nusd:", 1)[0]):
            if required != allowed:
                fail(f"{name} requires a member at {required!r}, but version {version} needs {allowed!r}")
    for path in sorted([*ROOT.glob("libs/*/CMakeLists.txt"), *ROOT.glob("plugins/*/CMakeLists.txt")]):
        match = CMAKE_VERSION.search(path.read_text(encoding="utf-8"))
        name = path.relative_to(ROOT).as_posix()
        if not match:
            fail(f"{name} declares no project VERSION")
        if match[1] != version:
            fail(f"openstrata.toml has {version}, but {name} has project VERSION {match[1]}")
    return version


def changelog_section(name):
    """Return the heading and body of one changelog section, or None."""
    lines = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8").splitlines()
    for start, line in enumerate(lines):
        if line.startswith(f"## [{name}]"):
            end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
            return line, "\n".join(lines[start + 1:end]).strip()
    return None


def changes_for(version, prerelease):
    name = "Unreleased" if prerelease else version
    section = changelog_section(name)
    if not section:
        fail(f"CHANGELOG.md has no [{name}] section")
    heading, body = section
    if not body:
        fail(f"the [{name}] section of CHANGELOG.md is empty")
    if not prerelease and not re.search(r"\b\d{4}-\d{2}-\d{2}\b", heading):
        fail(f"the changelog heading has no release date: {heading}")
    return body


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tag", help="the tag being released; omit for a dry run")
    parser.add_argument("--github-output", action="store_true", help="print the results as GitHub step outputs")
    parser.add_argument("--checksums", type=pathlib.Path, help="a SHA256SUMS file to include")
    parser.add_argument("--out", type=pathlib.Path, help="where to write the notes")
    arguments = parser.parse_args()

    version = versions()
    if arguments.tag:
        match = TAG.fullmatch(arguments.tag)
        if not match:
            fail(f"tag {arguments.tag} is not vX.Y.Z or vX.Y.Z-suffix")
        if match[1] != version:
            fail(f"tag {arguments.tag} does not match the workspace version {version}")
        tag, prerelease = arguments.tag, bool(match[2])
    else:
        tag, prerelease = f"v{version}", changelog_section(version) is None
    changes = changes_for(version, prerelease)

    if arguments.github_output:
        print(f"version={version}\ntag={tag}\nis_release={'true' if arguments.tag else 'false'}")
        print(f"prerelease={'true' if prerelease else 'false'}")
    if arguments.out:
        checksums = arguments.checksums.read_text(encoding="utf-8").strip() if arguments.checksums else ""
        blob = f"{REPOSITORY}/blob/{tag if arguments.tag else 'main'}"
        changes = RELATIVE_LINK.sub(lambda link: f"]({blob}/{link[1]})", changes)
        notes = NOTES.format(tag=tag, version=version, source=tag[1:], changes=changes,
                             checksums=checksums, blob=blob)
        arguments.out.write_text(notes, encoding="utf-8", newline="\n")
    if not arguments.github_output and not arguments.out:
        print(f"{tag}: every member is version {version}, and CHANGELOG.md has the section the tag needs.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
