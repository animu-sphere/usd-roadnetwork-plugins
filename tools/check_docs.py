# SPDX-License-Identifier: Apache-2.0
"""Check the documentation for drift against its sources of truth.

Each rule documented in docs/README.md ("Keeping documents in sync") has one
check here. The script needs only the Python standard library and git, and
runs from any directory inside the repository:

    python tools/check_docs.py

It exits non-zero and lists every violation when a check fails.
"""

import json
import pathlib
import re
import subprocess
import sys
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO_URL = "https://github.com/animu-sphere/usd-roadnetwork-plugins/blob/main/"

ADR_INDEX = ROOT / "docs/adr/README.md"
RELEASE_PLAN = ROOT / "docs/roadmap/release-plan.md"
CAPABILITY_MATRIX = ROOT / "docs/reference/CAPABILITY_MATRIX.md"
DIAGNOSTICS = ROOT / "docs/reference/DIAGNOSTICS.md"
CATALOG_GLOBS = (
    "libs/*/resources/diagnostics.json",
    "plugins/*/plugin/resources/*/diagnostics.json",
)

LINK = re.compile(r"(?<!!)\[(?:[^\]\\]|\\.)*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
FENCE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE = re.compile(r"`[^`]*`")
FEATURE_ID = re.compile(r"\bF-\d{2}\b")
FEATURE_ROW = re.compile(r"^\|\s*(F-\d{2})\s*\|")
CODE_ROW = re.compile(r"^\|\s*`((?:[A-Z0-9]+)-[EW]\d{3})`\s*\|")
ADR_FILE = re.compile(r"^(\d{4})-[a-z0-9-]+\.md$")
ADR_STATUS = re.compile(r"^- \*\*Status:\*\*\s*([A-Z][a-z]+)")
ADR_TITLE = re.compile(r"^# ADR (\d{4}) — (.+)$")
CAPABILITY_STATUSES = ("supported", "policy default", "unsupported", "unverified")

failures = []


def fail(path, line, message):
    where = path.relative_to(ROOT).as_posix()
    failures.append(f"{where}:{line}: {message}" if line else f"{where}: {message}")


def markdown_files():
    out = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "*.md"],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout
    return sorted(ROOT / p for p in out.split("\0") if p and (ROOT / p).is_file())


def prose_lines(path):
    """Yield (line number, text) for lines outside fenced code blocks."""
    fenced = False
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if FENCE.match(line):
            fenced = not fenced
            continue
        if not fenced:
            yield number, line


def slug(text):
    """GitHub's heading anchor: lowercase, punctuation dropped, spaces to '-'."""
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)  # keep link text only
    text = text.replace("`", "").replace("*", "").strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


_anchors = {}


def anchors(path):
    if path not in _anchors:
        seen, result = {}, set()
        for _, line in prose_lines(path):
            match = HEADING.match(line)
            if match:
                base = slug(match.group(2))
                count = seen.get(base, 0)
                seen[base] = count + 1
                result.add(base if count == 0 else f"{base}-{count}")
        _anchors[path] = result
    return _anchors[path]


def check_links(files):
    """Every relative link (and link to this repository on GitHub) resolves."""
    for path in files:
        for number, line in prose_lines(path):
            for target in LINK.findall(INLINE_CODE.sub("", line)):
                if target.startswith(REPO_URL):
                    target = "/" + target[len(REPO_URL):]
                elif re.match(r"^[a-z][a-z0-9+.-]*:", target):
                    continue  # external URL or mailto
                file_part, _, anchor = target.partition("#")
                file_part = urllib.parse.unquote(file_part)
                if not file_part:
                    dest = path
                elif file_part.startswith("/"):
                    dest = ROOT / file_part.lstrip("/")
                else:
                    dest = (path.parent / file_part).resolve()
                if not dest.exists():
                    fail(path, number, f"broken link {target!r}")
                    continue
                if anchor and dest.is_file() and dest.suffix == ".md":
                    if anchor not in anchors(dest):
                        fail(path, number, f"no heading for anchor {target!r}")


def table_rows(path, first_cell):
    """Yield (line number, cells) for table rows whose first cell matches."""
    for number, line in prose_lines(path):
        if line.startswith("|") and re.match(first_cell, line):
            yield number, [c.strip() for c in line.strip().strip("|").split("|")]


def check_adr_index():
    """The ADR index lists every ADR with the title and status of its file."""
    adrs = {}
    for path in sorted((ROOT / "docs/adr").glob("*.md")):
        match = ADR_FILE.match(path.name)
        if not match:
            continue
        title = status = None
        for _, line in prose_lines(path):
            title = title or (ADR_TITLE.match(line) and ADR_TITLE.match(line).group(2))
            status = status or (ADR_STATUS.match(line) and ADR_STATUS.match(line).group(1))
        if not title or not status:
            fail(path, 0, "needs a '# ADR NNNN — title' heading and a '- **Status:**' line")
        adrs[match.group(1)] = (path, title, status)

    listed = set()
    for number, cells in table_rows(ADR_INDEX, r"^\|\s*\[\d{4}\]"):
        adr = re.match(r"\[(\d{4})\]\(([^)]+)\)", cells[0])
        if not adr or adr.group(1) not in adrs:
            fail(ADR_INDEX, number, f"index row for an unknown ADR: {cells[0]}")
            continue
        listed.add(adr.group(1))
        path, title, status = adrs[adr.group(1)]
        if cells[1] != title:
            fail(ADR_INDEX, number, f"title {cells[1]!r} differs from {path.name}: {title!r}")
        if not cells[2].startswith(status):
            fail(ADR_INDEX, number, f"status {cells[2]!r} differs from {path.name}: {status}")
    for missing in sorted(set(adrs) - listed):
        fail(ADR_INDEX, 0, f"ADR {missing} is not in the index")


def check_feature_ids(files):
    """Feature IDs are defined once, in the release plan, and cited only if defined."""
    defined = {}
    for number, cells in table_rows(RELEASE_PLAN, r"^\|\s*F-\d{2}\s*\|"):
        if cells[0] in defined:
            fail(RELEASE_PLAN, number, f"{cells[0]} is defined twice")
        defined[cells[0]] = number
    for path in files:
        for number, line in prose_lines(path):
            if path == RELEASE_PLAN and FEATURE_ROW.match(line):
                continue
            for feature in FEATURE_ID.findall(line):
                if feature not in defined:
                    fail(path, number, f"{feature} is not defined in the release plan")


def check_capability_matrix():
    """Rows use the status vocabulary, and a supported row cites evidence."""
    in_vocabulary = False
    for number, line in prose_lines(CAPABILITY_MATRIX):
        if line.startswith("## "):
            in_vocabulary = line.strip() == "## Status vocabulary"
            continue
        if in_vocabulary or not line.startswith("|") or re.match(r"^\|\s*-", line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cells[0] in ("Format", "Capability"):
            continue  # header row
        status = next((c for c in cells[1:] if c.startswith(CAPABILITY_STATUSES)), None)
        if status is None:
            fail(CAPABILITY_MATRIX, number, f"row {cells[0]!r} has no status from the vocabulary")
        elif status.startswith("supported") and cells[-1] in ("", "—", "-"):
            fail(CAPABILITY_MATRIX, number, f"supported row {cells[0]!r} cites no evidence")


def check_diagnostics():
    """The codes listed in DIAGNOSTICS.md are exactly the catalogued codes."""
    catalogued = {}
    for pattern in CATALOG_GLOBS:
        for catalog in sorted(ROOT.glob(pattern)):
            for entry in json.loads(catalog.read_text(encoding="utf-8"))["diagnostics"]:
                catalogued[entry["code"]] = catalog
    documented = {}
    for number, cells in table_rows(DIAGNOSTICS, r"^\|\s*`[A-Z0-9]+-[EW]\d{3}`"):
        code = cells[0].strip("`")
        if code in documented:
            fail(DIAGNOSTICS, number, f"{code} is listed twice")
        documented[code] = number
    for code in sorted(set(catalogued) - set(documented)):
        fail(DIAGNOSTICS, 0, f"{code} is in {catalogued[code].relative_to(ROOT).as_posix()} "
             "but not in the assigned-codes tables")
    for code in sorted(set(documented) - set(catalogued)):
        fail(DIAGNOSTICS, documented[code], f"{code} is listed but in no catalog")


def main():
    files = markdown_files()
    check_links(files)
    check_adr_index()
    check_feature_ids(files)
    check_capability_matrix()
    check_diagnostics()
    if failures:
        print(f"check_docs: {len(failures)} problem(s)", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"check_docs: {len(files)} Markdown files OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
