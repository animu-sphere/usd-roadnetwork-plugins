# SPDX-License-Identifier: Apache-2.0
"""SdfFileFormat contract of the roadnetwork-opendrive bundle (ADR 0006).

Phase 0 evidence on the empty bundle: registration and read-only routing,
open, sublayer and reference composition, metadata-only reads, determinism,
and reload, including reload under an outer SdfChangeBlock. The bundle does
not decode OpenDRIVE yet, so every read authors an empty layer; the checks
that depend on content (/Asset, provenance, rejected input) arrive with the
Phase 1 reader.
"""

import os
import pathlib
import shutil
import sys
import tempfile
import time

from pxr import Sdf, Tf, Usd

FIXTURES = pathlib.Path(__file__).resolve().parent / "fixtures"
BASIC = FIXTURES / "basic.xodr"

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
    check(not layer.rootPrims, "the Phase 0 layer is not empty")
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

    # Reference: Pcp resolves and opens the xodr asset. The Phase 0 layer has
    # no defaultPrim, so the arc targets nothing yet; the check is that the
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
    # per-element content. In Phase 0 both reads are empty and must agree.
    check(not meta.rootPrims, "the Phase 0 metadata-only layer is not empty")
    check(meta.ExportToString() == full.ExportToString(),
          "metadata-only and full reads differ on the empty bundle")


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
                     lambda: check_reload(tmp)):
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
