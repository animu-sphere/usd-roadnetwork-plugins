# SPDX-License-Identifier: Apache-2.0
"""Cross-check the diagnostic catalogs against the codes used in source.

ADR 0006 and docs/reference/DIAGNOSTICS.md: codes are <PREFIX>-<E|W><NNN>,
ODR- codes belong to this bundle and RN- codes to roadnetwork-core and
roadnetwork-usd. Every code used in source must be in its owner's catalog,
every catalog entry must be well-formed, and no code is listed twice.
"""

import argparse
import json
import pathlib
import re
import sys

SCHEMA = "roadnetwork.diagnostics/v1"
CODE = re.compile(r"^(?P<prefix>[A-Z0-9]+)-(?P<sev>[EW])(?P<num>[0-9]{3})$")
# A code as it appears in a message: "[ODR-E001]". Bracketed so prose that
# merely mentions a prefix is not mistaken for a use.
USE = re.compile(r"\[((?:ODR|RN)-[EW][0-9]{3})\]")
SEVERITY = {"E": "error", "W": "warning"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def load_catalog(path, prefix):
    data = json.loads(path.read_text(encoding="utf-8"))
    check(data.get("schema") == SCHEMA, f"{path}: schema is not {SCHEMA}")
    check(data.get("prefix") == prefix, f"{path}: prefix is not {prefix}")
    entries = data.get("diagnostics")
    check(isinstance(entries, list), f"{path}: 'diagnostics' is not a list")
    codes = []
    for entry in entries or []:
        code = entry.get("code", "")
        match = CODE.match(code)
        check(match is not None, f"{path}: malformed code {code!r}")
        if match:
            check(match["prefix"] == prefix, f"{path}: {code} has the wrong prefix")
            check(entry.get("severity") == SEVERITY[match["sev"]],
                  f"{path}: {code} severity does not match its letter")
        for field in ("summary", "action"):
            check(bool(entry.get(field)), f"{path}: {code} has no {field}")
        codes.append(code)
    check(len(codes) == len(set(codes)), f"{path}: duplicate codes")
    check(codes == sorted(codes), f"{path}: codes are not in ascending order")
    return set(codes)


def used_codes(roots):
    found = {}
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.suffix in SOURCE_SUFFIXES and path.is_file():
                for code in USE.findall(path.read_text(encoding="utf-8")):
                    found.setdefault(code, path)
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[3])
    root = parser.parse_args().repo_root

    bundle = root / "plugins" / "roadnetwork-opendrive"
    odr = load_catalog(
        bundle / "plugin" / "resources" / "roadnetwork-opendrive" / "diagnostics.json", "ODR")
    rn = load_catalog(root / "libs" / "roadnetwork-core" / "resources" / "diagnostics.json", "RN")

    # ODR- codes may only be raised by the bundle; RN- codes only by the
    # shared libraries.
    bundle_uses = used_codes([bundle / "src"])
    library_uses = used_codes([root / "libs" / "roadnetwork-core",
                               root / "libs" / "roadnetwork-usd"])
    for code, path in sorted(bundle_uses.items()):
        check(code.startswith("ODR-"), f"{path}: the bundle raises {code}, owned by another prefix")
        check(not code.startswith("ODR-") or code in odr, f"{path}: {code} is not in the ODR catalog")
    for code, path in sorted(library_uses.items()):
        check(code.startswith("RN-"), f"{path}: a shared library raises {code}, which is not RN-")
        check(not code.startswith("RN-") or code in rn, f"{path}: {code} is not in the RN catalog")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print(f"diagnostic catalogs: ODR {len(odr)} code(s), RN {len(rn)} code(s); "
          f"{len(bundle_uses) + len(library_uses)} code(s) used in source, all catalogued")
    return 0


if __name__ == "__main__":
    sys.exit(main())
