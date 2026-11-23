# SPDX-License-Identifier: Apache-2.0
"""Check what a bundle package carries: digests, notices, SBOM, provenance, and target.

This is release gate item 5 (docs/releases/README.md#release-gate), and item 4
when a second packaging is given. It reads the JSON that
`ost plugin package --json` prints, and the files next to the archive:

    ost plugin package plugins/roadnetwork-opendrive --json > package.json
    python tools/check_package.py package.json [--repackaged second.json]

It needs only the Python standard library. It exits non-zero and lists every
violation when a check fails.
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUNDLE = ROOT / "plugins" / "roadnetwork-opendrive"
# What the bundle's THIRD_PARTY_NOTICES.md must name: one entry per component
# that THIRD_PARTY_NOTICES.md at the root says a package carries.
NOTICE_MARKERS = ("pugixml 1.16", "Copyright (c) 2006-2026 Arseny Kapoulkine", "CARLA OpenDRIVE test files")

failures = []


def fail(message):
    failures.append(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def bundle_manifest():
    """The fields of openstrata.plugin.yaml this check compares, read without a YAML parser."""
    text = (BUNDLE / "openstrata.plugin.yaml").read_text(encoding="utf-8")

    def scalar(block, key):
        match = re.search(rf"^{block}:\s*$.*?^\s+{key}:\s*\"?([^\"\s]+)\"?", text, re.M | re.S)
        return match[1] if match else None

    notices = re.search(r"^notices:\s*\n((?:\s+-\s+\S+\n)+)", text, re.M)
    license_ = re.search(r"^license:\s*(\S+)", text, re.M)
    return {
        "name": scalar("plugin", "name"),
        "version": scalar("plugin", "version"),
        "kind": scalar("plugin", "kind"),
        "license": license_[1] if license_ else None,
        "openusd": scalar("runtime", "openusd"),
        "notices": re.findall(r"-\s+(\S+)", notices[1]) if notices else [],
    }


def check_digests(package, directory, archive):
    """The archive, the SBOM, and SHA256SUMS agree with the files on disk."""
    actual = sha256(archive)
    if package["archive_digest"] != f"sha256:{actual}":
        fail(f"ost reported {package['archive_digest']}, but {archive.name} hashes to sha256:{actual}")
    sums = {}
    for line in (directory / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        digest, _, name = line.partition("  ")
        sums[name] = digest
    for name in (archive.name, "sbom.spdx.json"):
        if name not in sums:
            fail(f"SHA256SUMS does not list {name}")
        elif sums[name] != sha256(directory / name):
            fail(f"SHA256SUMS has {sums[name]} for {name}, which hashes to {sha256(directory / name)}")
    return actual


def check_manifest(manifest, source, archive, digest):
    """Identity, target metadata, and provenance."""
    target = manifest.get("target", "")
    expected_archive = f"{source['name']}-{source['version']}-{target}.tar.zst"
    if archive.name != expected_archive or manifest.get("archive") != expected_archive:
        fail(f"the archive is {archive.name}, and the manifest names {manifest.get('archive')}; "
             f"expected {expected_archive}")
    if manifest.get("archive_digest") != f"sha256:{digest}":
        fail(f"the manifest records {manifest.get('archive_digest')}, not sha256:{digest}")
    if manifest.get("kind") != "openstrata.plugin-bundle":
        fail(f"the manifest kind is {manifest.get('kind')}")
    if manifest.get("created_unix") != 0:
        fail(f"the manifest records creation time {manifest.get('created_unix')}, so its digest is not stable")
    plugin = manifest.get("plugin", {})
    for key in ("name", "version", "kind", "license"):
        if plugin.get(key) != source[key]:
            fail(f"the manifest has plugin {key} {plugin.get(key)!r}, the bundle manifest {source[key]!r}")

    compatibility = manifest.get("component", {}).get("compatibility", {})
    if compatibility.get("openusd") != source["openusd"]:
        fail(f"the package declares OpenUSD {compatibility.get('openusd')!r}, the bundle {source['openusd']!r}")

    provenance = manifest.get("provenance", {})
    outputs = provenance.get("build_outputs", {})
    if outputs.get("origin") != "ost-managed" or outputs.get("status") != "matched":
        fail(f"the package does not come from the last managed build: {outputs.get('detail')}")
    if outputs.get("override_accepted") is not False or outputs.get("outputs", {}).get("differences"):
        fail("the package provenance accepted an override or records differing outputs")
    runtime = provenance.get("runtime", {})
    if runtime.get("id") != f"openstrata-{target}" or runtime.get("validation") != "passed":
        fail(f"the package was built against runtime {runtime.get('id')!r} "
             f"(validation {runtime.get('validation')!r}), not a validated openstrata-{target}")
    if not str(runtime.get("digest", "")).startswith("sha256:"):
        fail("the package provenance records no runtime digest")
    if not provenance.get("validation", {}).get("passed"):
        fail("the package provenance records a failed or missing runtime validation")
    platform, profile = provenance.get("platform"), provenance.get("profile")
    if not (platform and profile and target.startswith(f"{platform}-") and target.endswith(f"-{profile}")):
        fail(f"the target {target!r} does not match platform {platform!r} and profile {profile!r}")
    if compatibility.get("abi") != provenance.get("cxx_abi") or not compatibility.get("abi"):
        fail(f"the package ABI {compatibility.get('abi')!r} differs from the build's {provenance.get('cxx_abi')!r}")
    if compatibility.get("targets") != [provenance.get("variant")]:
        fail(f"the package targets {compatibility.get('targets')} are not the build variant {provenance.get('variant')!r}")


def check_notices(manifest, source):
    """Every declared notice file ships byte-identical to the bundle's, and LICENSE is the root's."""
    files = {entry["path"]: entry["sha256"] for entry in manifest.get("files", [])}
    installed = {entry["destination"] for entry in manifest.get("component", {}).get("install", [])}
    if "LICENSE" not in source["notices"]:
        fail("the bundle manifest does not ship LICENSE under notices")
    for name in source["notices"]:
        path = BUNDLE / name
        if name not in installed or name not in files:
            fail(f"the notice {name} is not in the package")
        elif files[name] != f"sha256:{sha256(path)}":
            fail(f"the packaged {name} differs from {path.relative_to(ROOT).as_posix()}")
    if (BUNDLE / "LICENSE").read_bytes() != (ROOT / "LICENSE").read_bytes():
        fail("plugins/roadnetwork-opendrive/LICENSE is not a copy of LICENSE")
    notices = (BUNDLE / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
    for marker in NOTICE_MARKERS:
        if marker not in notices:
            fail(f"the bundle's THIRD_PARTY_NOTICES.md does not mention {marker!r}")


def check_sbom(sbom, manifest, source, digest):
    """The SBOM describes this archive and lists every packaged file with its digest."""
    if sbom.get("spdxVersion") != "SPDX-2.3" or sbom.get("dataLicense") != "CC0-1.0":
        fail(f"the SBOM is {sbom.get('spdxVersion')} under {sbom.get('dataLicense')}, not SPDX-2.3 under CC0-1.0")
    if sbom.get("creationInfo", {}).get("created") != "1970-01-01T00:00:00Z":
        fail("the SBOM creation time is not fixed, so its digest is not stable")
    packages = {package.get("SPDXID"): package for package in sbom.get("packages", [])}
    package = packages.get("SPDXRef-Package")
    if not package:
        fail("the SBOM has no SPDXRef-Package")
        return
    if (package.get("name"), package.get("versionInfo")) != (source["name"], source["version"]):
        fail(f"the SBOM describes {package.get('name')} {package.get('versionInfo')}")
    if package.get("licenseDeclared") != source["license"]:
        fail(f"the SBOM declares license {package.get('licenseDeclared')}, the bundle {source['license']}")
    checksums = {entry.get("algorithm"): entry.get("checksumValue") for entry in package.get("checksums", [])}
    if checksums.get("SHA256") != digest:
        fail(f"the SBOM package checksum {checksums.get('SHA256')} is not the archive's {digest}")
    listed = {}
    for entry in sbom.get("files", []):
        values = {item.get("algorithm"): item.get("checksumValue") for item in entry.get("checksums", [])}
        listed[entry.get("fileName")] = f"sha256:{values.get('SHA256')}"
    packaged = {entry["path"]: entry["sha256"] for entry in manifest.get("files", [])}
    if listed != packaged:
        missing = sorted(set(packaged) - set(listed))
        extra = sorted(set(listed) - set(packaged))
        changed = sorted(name for name in set(listed) & set(packaged) if listed[name] != packaged[name])
        fail(f"the SBOM files differ from the package: missing {missing}, extra {extra}, changed {changed}")
    contained = {relation.get("relatedSpdxElement") for relation in sbom.get("relationships", [])
                 if relation.get("spdxElementId") == "SPDXRef-Package" and relation.get("relationshipType") == "CONTAINS"}
    if contained != {entry.get("SPDXID") for entry in sbom.get("files", [])}:
        fail("the SBOM does not relate every file to the package with CONTAINS")


def differing_files(first_manifest, manifest):
    """The packaged paths whose digest differs between a saved manifest and the current one."""
    first = json.loads(pathlib.Path(first_manifest).read_text(encoding="utf-8"))
    before = {entry["path"]: entry["sha256"] for entry in first.get("files", [])}
    after = {entry["path"]: entry["sha256"] for entry in manifest.get("files", [])}
    return sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name))


def load(path):
    report = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    if not report.get("ok"):
        sys.exit(f"error: {path} records a failed `ost plugin package`")
    return report["data"]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("package", help="the JSON `ost plugin package --json` printed")
    parser.add_argument("--repackaged", action="append", default=[], metavar="JSON",
                        help="the JSON of a later packaging, which must give the same archive digest")
    parser.add_argument("--first-manifest", metavar="MANIFEST",
                        help="a copy of the first packaging's manifest.json, to name the files a mismatch changed")
    arguments = parser.parse_args()

    package = load(arguments.package)
    archive = pathlib.Path(package["archive"])
    directory = archive.parent
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    sbom = json.loads((directory / "sbom.spdx.json").read_text(encoding="utf-8"))
    source = bundle_manifest()

    digest = check_digests(package, directory, archive)
    check_manifest(manifest, source, archive, digest)
    check_notices(manifest, source)
    check_sbom(sbom, manifest, source, digest)
    for path in arguments.repackaged:
        again = load(path)
        if again["archive_digest"] != package["archive_digest"]:
            fail(f"{path} gives {again['archive_digest']}, the first packaging {package['archive_digest']}")
            if arguments.first_manifest:
                for name in differing_files(arguments.first_manifest, manifest):
                    fail(f"  {name} differs from the first packaging")

    if failures:
        print(f"{archive.name}: {len(failures)} problem(s)", file=sys.stderr)
        for message in failures:
            print(f"  {message}", file=sys.stderr)
        return 1
    print(f"{archive.name}: sha256:{digest}; digests, notices, SBOM, provenance, and target metadata check out"
          + (f"; {len(arguments.repackaged)} repackaging(s) reproduce it" if arguments.repackaged else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
