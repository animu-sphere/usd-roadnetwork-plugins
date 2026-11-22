# SPDX-License-Identifier: Apache-2.0
"""SdfFileFormat contract of the roadnetwork-opendrive bundle (ADR 0006).

Registration and read-only routing, open, sublayer and reference
composition, metadata-only reads, determinism, and reload, including reload
under an outer SdfChangeBlock. The reader (ODR-01) adds the CanRead() sniff
on the prologue fixtures, the rejection of each negative fixture with exactly
one coded error, the accepted version range, and a failed reload that keeps
the previous content. The decoder (ODR-02) adds the decoder and validation
fixtures, which fail a full read but pass a metadata-only one, and the
warnings: each code posted once per successful read, and none by a failed
one. The shared writer still authors an empty layer, so the checks that
depend on authored content (/Asset, provenance) arrive with the stage
contract (USD-01).
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

from pxr import Sdf, Tf, Usd

FIXTURES = pathlib.Path(__file__).resolve().parent / "fixtures"
BASIC = FIXTURES / "basic.xodr"
READER = FIXTURES / "reader"
DECODER = FIXTURES / "decoder"
NETWORK = DECODER / "network.xodr"

# The fixtures each code is proven by (DIAGNOSTICS.md), and whether CanRead()
# accepts them: it looks only at the prologue, so a malformed body or an
# unsupported version still passes the sniff.
REJECTED = [
    (FIXTURES / "invalid.xodr", "ODR-E002", True),
    (READER / "not-utf8.xodr", "ODR-E003", False),
    (READER / "wrong-root.xodr", "ODR-E004", False),
    (READER / "namespaced-root.xodr", "ODR-E004", False),
    (READER / "missing-header.xodr", "ODR-E005", True),
    (READER / "version-malformed.xodr", "ODR-E006", True),
    (READER / "version-unsupported.xodr", "ODR-E007", True),
]
# Content the decoder or validation rejects: a full read fails with the code,
# and a metadata-only read, which decodes the header only, succeeds
# (ADR 0006 section 2).
CONTENT_REJECTED = [(DECODER / f"{name}.xodr", code) for name, code in [
    ("missing-attribute", "ODR-E008"),
    ("invalid-number", "ODR-E009"),
    ("unknown-lane-type", "ODR-E009"),
    ("duplicate-plan-view", "ODR-E010"),
    ("lane-group-mismatch", "ODR-E101"),
    ("lane-link-without-road-link", "ODR-E102"),
    ("lane-link-into-junction", "ODR-E103"),
    ("connection-unresolved", "ODR-E104"),
    ("spiral", "ODR-E201"),
    ("superelevation", "ODR-E202"),
    ("lane-border", "ODR-E203"),
    ("direct-junction", "ODR-E204"),
    ("temporary-lane-layer", "ODR-E205"),
    ("unknown-lane-element", "ODR-E206"),
    ("duplicate-road", "RN-E102"),
    ("unresolved-link", "RN-E111"),
    ("unresolved-lane-link", "RN-E115"),
    ("non-finite", "RN-E301"),
    ("plan-view-gap", "RN-E305"),
    ("inner-border", "RN-E308"),
]]
ACCEPTED = [BASIC, NETWORK, READER / "prologue-bom.xodr", READER / "prologue-doctype.xodr",
            FIXTURES / "carla" / "Town01.xodr", FIXTURES / "carla" / "Town02.xodr"]
SNIFF_BYTES = 64 * 1024

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def expect_tf_error(fn):
    """Run fn and return True if it raised a Tf error or returned falsy."""
    try:
        return not fn()
    except Tf.ErrorException:
        return True


def read_errors(fn):
    """Run fn and return (its result, the messages of the Tf errors it posted)."""
    try:
        return fn(), []
    except Tf.ErrorException as error:
        return None, [str(getattr(e, "commentary", e)) for e in error.args]


def check_opens(path, what, **kwargs):
    layer, errors = read_errors(lambda: Sdf.Layer.OpenAsAnonymous(str(path), **kwargs))
    check(layer is not None and not errors, f"{what} did not open: {errors}")


def check_rejected(path, code, what, **kwargs):
    """ADR 0006 section 3: a failed read posts exactly one error, starting with
    the bracketed code and naming the asset, and opens no layer or stage."""
    layer, errors = read_errors(lambda: Sdf.Layer.OpenAsAnonymous(str(path), **kwargs))
    check(layer is None, f"{what}: the layer opened")
    check(len(errors) == 1, f"{what}: expected one error, got {errors}")
    if errors:
        check(errors[0].startswith(f"[{code}] "), f"{what}: expected [{code}], got {errors[0]!r}")
        check(path.name in errors[0], f"{what}: the error does not name the asset: {errors[0]!r}")
    if not kwargs:
        stage, _ = read_errors(lambda: Usd.Stage.Open(str(path)))
        check(not stage, f"{what}: a stage opened")


def write_variant(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")
    return path


def check_reader_fixtures(tmp):
    fmt = Sdf.FileFormat.FindByExtension("xodr")
    for path in ACCEPTED:
        check(fmt.CanRead(str(path)), f"CanRead rejects {path.name}")
        check_opens(path, path.name)
    for path, code, can_read in REJECTED:
        check(fmt.CanRead(str(path)) == can_read, f"CanRead({path.name}) is not {can_read}")
        check_rejected(path, code, path.name)
        # A metadata-only read checks the container, root, and version too.
        check_rejected(path, code, f"metadata-only {path.name}", metadataOnly=True)
    for path, code in CONTENT_REJECTED:
        check(fmt.CanRead(str(path)), f"CanRead rejects {path.name}")
        check_rejected(path, code, path.name)
        check_opens(path, f"metadata-only {path.name}", metadataOnly=True)

    # CanRead() fails quietly on a missing asset.
    _, errors = read_errors(lambda: fmt.CanRead(str(tmp / "missing.xodr")))
    check(not errors, f"CanRead posted errors for a missing asset: {errors}")

    text = BASIC.read_text(encoding="utf-8")

    # A comment that pushes the root past the 64 KiB sniff window: CanRead()
    # says no, and Read() still accepts the document.
    long_comment = write_variant(tmp / "long-comment.xodr", text.replace(
        "<OpenDRIVE>", "<!--" + "x" * SNIFF_BYTES + "-->\n<OpenDRIVE>", 1))
    check(not fmt.CanRead(str(long_comment)), "CanRead finds a root beyond 64 KiB")
    check_opens(long_comment, "a long leading comment")

    # The same document in UTF-16, with a byte-order mark.
    utf16 = tmp / "utf16.xodr"
    utf16.write_bytes(text.replace('encoding="UTF-8"', 'encoding="UTF-16"').encode("utf-16"))
    check(not fmt.CanRead(str(utf16)), "CanRead accepts UTF-16")
    check_rejected(utf16, "ODR-E003", "UTF-16")

    # A directory resolves, but cannot be read as an asset.
    directory = tmp / "directory.xodr"
    directory.mkdir()
    check(not fmt.CanRead(str(directory)), "CanRead accepts a directory")
    check_rejected(directory, "ODR-E001", "a directory")

    # The version range: revMajor 1 with revMinor 4 to 9 (FMT-01 section 3.3).
    for minor in range(0, 12):
        path = write_variant(tmp / f"version-1-{minor}.xodr",
                             text.replace('revMinor="8"', f'revMinor="{minor}"', 1))
        if 4 <= minor <= 9:
            check_opens(path, f"OpenDRIVE 1.{minor}")
        else:
            check_rejected(path, "ODR-E007", f"OpenDRIVE 1.{minor}")
    path = write_variant(tmp / "version-2-0.xodr",
                         text.replace('revMajor="1" revMinor="8"', 'revMajor="2" revMinor="0"', 1))
    check_rejected(path, "ODR-E007", "OpenDRIVE 2.0")


def check_failed_reload_keeps_content(tmp):
    """ADR 0006 section 3: a failed Read() leaves the target layer unchanged,
    so the previous content survives a failed reload."""
    path = tmp / "failed-reload.xodr"
    shutil.copyfile(BASIC, path)
    layer = Sdf.Layer.FindOrOpen(str(path))
    check(layer is not None, "the failed-reload fixture did not open")
    if layer is None:
        return
    # The read authored an empty layer. Give it content that only a
    # successful read replaces.
    Sdf.CreatePrimInLayer(layer, "/Marker")
    check(bool(layer.GetPrimAtPath("/Marker")), "the marker prim was not authored")

    def rewrite(source, bump):
        shutil.copyfile(source, path)
        later = time.time() + bump
        os.utime(path, (later, later))

    rewrite(FIXTURES / "invalid.xodr", 10)
    reloaded, errors = read_errors(lambda: layer.Reload(force=True))
    check(not reloaded, "reloading a malformed file succeeded")
    check(len(errors) == 1 and errors[0].startswith("[ODR-E002] "),
          f"a failed reload did not post one ODR-E002: {errors}")
    check(bool(layer.GetPrimAtPath("/Marker")), "a failed reload replaced the previous content")

    # Repaired, the file reloads and replaces the content.
    rewrite(BASIC, 20)
    reloaded, errors = read_errors(lambda: layer.Reload(force=True))
    check(bool(reloaded) and not errors, f"reloading the repaired file failed: {errors}")
    check(not layer.GetPrimAtPath("/Marker"), "a successful reload kept the previous content")


WARNING_CODE = re.compile(r"\[((?:ODR|RN)-W[0-9]{3})\]")
OPEN_LAYER = """
import sys
from pxr import Sdf, Tf
try:
    Sdf.Layer.OpenAsAnonymous(sys.argv[1], metadataOnly=sys.argv[2] == "1")
except Tf.ErrorException:
    pass
"""


def posted_warnings(path, metadata_only=False):
    """The warning codes one read posts, in order. TF_WARN goes to stderr,
    which a child process captures."""
    run = subprocess.run([sys.executable, "-c", OPEN_LAYER, str(path), "1" if metadata_only else "0"],
                         capture_output=True, text=True, encoding="utf-8", errors="replace")
    return WARNING_CODE.findall(run.stderr)


def check_warnings(tmp):
    """ADR 0006 section 4: each warning code is posted at most once per read,
    and only by a read that succeeds."""
    cases = [
        (NETWORK, False, ["ODR-W201", "ODR-W202", "ODR-W203", "ODR-W204", "ODR-W205",
                          "ODR-W206", "ODR-W207"]),
        (NETWORK, True, ["ODR-W204"]),
        (BASIC, False, []),
        (FIXTURES / "carla" / "Town01.xodr", False, ["ODR-W201", "ODR-W204", "ODR-W207", "RN-W301"]),
        (FIXTURES / "carla" / "Town02.xodr", False, ["ODR-W201", "ODR-W204", "ODR-W207", "RN-W301"]),
    ]
    for path, metadata_only, expected in cases:
        posted = posted_warnings(path, metadata_only)
        check(posted == expected,
              f"{'metadata-only ' if metadata_only else ''}{path.name} posted {posted}, expected {expected}")
    # The same network with a spiral fails, and posts its error only.
    failing = write_variant(tmp / "network-spiral.xodr", NETWORK.read_text(encoding="utf-8").replace(
        '<arc curvature="0.02"/>', '<spiral curvStart="0.0" curvEnd="0.02"/>', 1))
    check_rejected(failing, "ODR-E201", "the network with a spiral")
    check(posted_warnings(failing) == [], "a failed read posted warnings")


def check_registration():
    fmt = Sdf.FileFormat.FindByExtension("xodr")
    check(fmt is not None, "no file format is registered for .xodr")
    if fmt is None:
        return
    check(fmt.formatId == "xodr", f"formatId is {fmt.formatId!r}")
    check(fmt.target == "usd", f"target is {fmt.target!r}")
    check(fmt.primaryFileExtension == "xodr", "primary extension is not xodr")
    check(fmt.CanRead(str(BASIC)), "CanRead rejects basic.xodr")
    check(fmt.SupportsReading(), "the format does not support reading")
    # Design policy §10.1: read-only. plugInfo.json declares supportsWriting.
    check(not fmt.SupportsWriting(), "the format claims to support writing")


def check_open_and_read_only(tmp):
    stage = Usd.Stage.Open(str(BASIC))
    check(bool(stage), "Usd.Stage.Open failed on basic.xodr")
    if not stage:
        return
    layer = stage.GetRootLayer()
    check(layer.GetFileFormat().formatId == "xodr", "root layer is not read by the xodr format")
    # WriteToString exposes the layer as USDA for inspection; that is not export.
    text = layer.ExportToString()
    check(text.startswith("#usda 1.0"), "ExportToString does not produce USDA")
    check(not layer.rootPrims, "the layer is not empty before the stage contract")
    # Exporting through the xodr format is refused explicitly.
    target = tmp / "export.xodr"
    check(expect_tf_error(lambda: layer.Export(str(target))),
          "exporting to .xodr did not fail")
    check(not target.exists(), "a failed export left a file behind")


def write_usda(path, body):
    path.write_text("#usda 1.0\n" + body, encoding="utf-8")


def check_composition(tmp):
    shutil.copyfile(BASIC, tmp / "net.xodr")

    # Sublayer: the xodr layer is opened by Sdf relative to the anchor.
    write_usda(tmp / "sublayer.usda", "(\n    subLayers = [@./net.xodr@]\n)\n")
    stage = Usd.Stage.Open(str(tmp / "sublayer.usda"))
    check(bool(stage), "a stage sublayering net.xodr did not open")
    if stage:
        formats = {layer.GetFileFormat().formatId for layer in stage.GetUsedLayers()}
        check("xodr" in formats, "the sublayered xodr layer is not used by the stage")

    # Reference: Pcp resolves and opens the xodr asset. Until the stage
    # contract lands the layer has no defaultPrim, so the arc targets nothing yet; the check is that the
    # asset is reached through the plugin. Prim contents are checked once the
    # writer authors /Asset (WORKSPACE.md §5).
    write_usda(tmp / "reference.usda",
               'def "Road" (\n    prepend references = @./net.xodr@\n)\n{\n}\n')
    stage = Usd.Stage.Open(str(tmp / "reference.usda"))
    check(bool(stage), "a stage referencing net.xodr did not open")
    if stage:
        formats = {layer.GetFileFormat().formatId for layer in stage.GetUsedLayers()}
        check("xodr" in formats, "the referenced xodr layer is not used by the stage")
        check(bool(stage.GetPrimAtPath("/Road")), "the referencing prim is missing")


def check_metadata_only():
    full = Sdf.Layer.OpenAsAnonymous(str(BASIC), metadataOnly=False)
    meta = Sdf.Layer.OpenAsAnonymous(str(BASIC), metadataOnly=True)
    check(full is not None, "a full anonymous read failed")
    check(meta is not None, "a metadata-only anonymous read failed")
    if full is None or meta is None:
        return
    # ADR 0006: a metadata-only read authors the header-derived root and no
    # per-element content. Until the decoder and the stage contract land,
    # both reads are empty and must agree.
    check(not meta.rootPrims, "the metadata-only layer is not empty")
    check(meta.ExportToString() == full.ExportToString(),
          "metadata-only and full reads differ")


def check_determinism():
    first = Sdf.Layer.OpenAsAnonymous(str(BASIC)).ExportToString()
    second = Sdf.Layer.OpenAsAnonymous(str(BASIC)).ExportToString()
    check(first == second, "two reads of basic.xodr differ")


def check_reload(tmp):
    path = tmp / "reload.xodr"
    shutil.copyfile(BASIC, path)
    stage = Usd.Stage.Open(str(path))
    check(bool(stage), "the reload fixture did not open")
    if not stage:
        return
    layer = stage.GetRootLayer()
    before = layer.ExportToString()

    # Sdf sends LayerDidReloadContent once per successful reload, after the
    # outermost change block closes. Pcp resyncs only specs that differ, so
    # identical (here empty) content produces no Usd.Notice.ObjectsChanged.
    reloads = []
    listener = Tf.Notice.Register(
        Sdf.Notice.LayerDidReloadContent, lambda notice, sender: reloads.append(sender), layer)

    # Forced reload re-runs Read() inside SdfLayer::_Reload's SdfChangeBlock.
    check(layer.Reload(force=True), "forced reload failed")
    # Nested under a caller's block as well, as Usd.Stage.Reload and
    # SdfLayer::ReloadLayers do.
    with Sdf.ChangeBlock():
        check(layer.Reload(force=True), "forced reload under an outer ChangeBlock failed")
    check(Sdf.Layer.ReloadLayers({layer}, force=True), "ReloadLayers failed")
    check(len(reloads) == 3, f"expected 3 reload notices after forced reloads, got {len(reloads)}")
    # Unforced and unchanged on disk: Sdf skips the read.
    stage.Reload()
    check(len(reloads) == 3, "an unforced reload of an unchanged file re-read it")

    # An on-disk change is picked up by an unforced reload. Bump the
    # timestamp explicitly: a same-second rewrite may not change it.
    text = path.read_text(encoding="utf-8")
    path.write_text(text.replace("<OpenDRIVE>", "<!-- edited -->\n<OpenDRIVE>", 1),
                    encoding="utf-8", newline="\n")
    later = time.time() + 10
    os.utime(path, (later, later))
    check(layer.Reload(), "reload after an on-disk change failed")

    check(len(reloads) == 4, "the on-disk change was not re-read")
    check(layer.ExportToString() == before, "reloaded content differs from the first read")
    check(not layer.dirty, "the layer is dirty after reload")
    check(bool(stage.GetPseudoRoot()), "the stage is unusable after reload")
    del listener


def main():
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="rn-odr-contract-"))
    try:
        for test in (check_registration,
                     lambda: check_open_and_read_only(tmp),
                     lambda: check_composition(tmp),
                     check_metadata_only,
                     check_determinism,
                     lambda: check_reload(tmp),
                     lambda: check_reader_fixtures(tmp),
                     lambda: check_warnings(tmp),
                     lambda: check_failed_reload_keeps_content(tmp)):
            try:
                test()
            except Tf.ErrorException as error:
                failures.append(f"unexpected Tf error: {error}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("roadnetwork-opendrive file-format contract: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
