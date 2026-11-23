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
one. The stage contract (USD-01) adds the authored content: /Asset and its
provenance, a metadata-only /Asset that agrees with a full one, two
references of one network that keep their own relationship targets, reload
with unchanged paths and targets, and usdchecker on the generated layers.
The writer's own unit test covers the property groups one by one. The
release gate adds the limits on untrusted input (ODR-E501, depth, and
entities) and reads with MaterialX's own pugixml loaded.
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

from pxr import Gf, Plug, Sdf, Tf, Usd, UsdGeom

BUNDLE = pathlib.Path(__file__).resolve().parents[1]
FIXTURES = BUNDLE / "tests" / "fixtures"
BASIC = FIXTURES / "basic.xodr"
READER = FIXTURES / "reader"
DECODER = FIXTURES / "decoder"
NETWORK = DECODER / "network.xodr"
PROJECTED = FIXTURES / "writer" / "projected.xodr"

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
ACCEPTED = [BASIC, NETWORK, PROJECTED, READER / "prologue-bom.xodr", READER / "prologue-doctype.xodr",
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


MAX_DOCUMENT_BYTES = 512 * 1024 * 1024  # odr::kMaxDocumentBytes
NESTING_DEPTH = 1_000_000


def check_untrusted_input(tmp):
    """The limits of OPENDRIVE_MAPPING.md section 1, "Untrusted input"."""
    fmt = Sdf.FileFormat.FindByExtension("xodr")
    text = BASIC.read_text(encoding="utf-8")

    # Size: one byte over the limit fails with ODR-E501 before anything is
    # read. Extending the file leaves the rest unwritten, so this costs no
    # 512 MiB of writes, and the prologue still passes the sniff.
    large = tmp / "too-large.xodr"
    with large.open("wb") as stream:
        stream.write(text.encode("utf-8"))
        stream.truncate(MAX_DOCUMENT_BYTES + 1)
    try:
        check(fmt.CanRead(str(large)), "CanRead rejects the prologue of an oversized asset")
        started = time.monotonic()
        check_rejected(large, "ODR-E501", "an asset one byte over the limit")
        check_rejected(large, "ODR-E501", "metadata-only, an asset one byte over the limit", metadataOnly=True)
        check(time.monotonic() - started < 30, "rejecting an oversized asset took longer than reading it would")
        _, errors = read_errors(lambda: Sdf.Layer.OpenAsAnonymous(str(large)))
        check(errors and str(MAX_DOCUMENT_BYTES) in errors[0], f"ODR-E501 does not state the limit: {errors}")
    finally:
        large.unlink()

    # Depth: ignored elements are counted, not visited, however deep they nest.
    nested = "<userData>" * NESTING_DEPTH + "</userData>" * NESTING_DEPTH
    deep = write_variant(tmp / "deep-user-data.xodr", text.replace("  <road ", nested + "\n  <road ", 1))
    check_opens(deep, f"{NESTING_DEPTH} nested <userData> elements")
    check(posted_warnings(deep) == ["ODR-W204"], f"deep <userData> posted {posted_warnings(deep)}")
    deep.unlink()
    # An unknown element in the lane structure fails at its first level.
    nested = "<deep>" * NESTING_DEPTH + "</deep>" * NESTING_DEPTH
    deep = write_variant(tmp / "deep-lanes.xodr", text.replace("<lanes>", "<lanes>" + nested, 1))
    check_rejected(deep, "ODR-E206", f"{NESTING_DEPTH} nested unknown elements in <lanes>")
    deep.unlink()

    # External references: a DOCTYPE's entities, internal or external, are
    # never declared or expanded, and their references stay literal text.
    doctype = ('<!DOCTYPE OpenDRIVE [\n'
               '  <!ENTITY external SYSTEM "file:///etc/passwd">\n'
               '  <!ENTITY a "aaaaaaaaaa">\n'
               '  <!ENTITY b "&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;">\n'
               ']>\n')
    check('name="basic"' in text, "basic.xodr no longer has the header name this check rewrites")
    entities = write_variant(tmp / "entities.xodr", text.replace("<OpenDRIVE>", doctype + "<OpenDRIVE>", 1)
                             .replace('name="basic"', 'name="&b;&external;&amp;"', 1))
    layer, errors = read_errors(lambda: Sdf.Layer.OpenAsAnonymous(str(entities)))
    check(layer is not None and not errors, f"a DOCTYPE with entities did not open: {errors}")
    if layer is not None:
        name = asset_properties(layer, "roadnetwork:source:name").get("roadnetwork:source:name")
        check(name == "&b;&external;&", f"entity references were expanded or lost: {name!r}")


def check_materialx_coexistence(tmp):
    """MaterialX ships its own pugixml, in the same `pugi` namespace, and
    OpenUSD can load it into this process (FMT-01 section 6). The bundle's copy
    has hidden symbols; interleaved reads through both prove neither binds to
    the other's."""
    if Plug.Registry().GetPluginWithName("usdMtlx") is None:
        print("skipped the MaterialX check: this runtime has no usdMtlx plugin")
        return
    for round_ in range(2):
        document = write_variant(tmp / f"material-{round_}.mtlx", (
            '<?xml version="1.0"?>\n<materialx version="1.39">\n'
            f'  <standard_surface name="SR_{round_}" type="surfaceshader"/>\n'
            f'  <surfacematerial name="M_{round_}" type="material">\n'
            f'    <input name="surfaceshader" type="surfaceshader" nodename="SR_{round_}"/>\n'
            '  </surfacematerial>\n</materialx>\n'))
        stage, errors = read_errors(lambda: Usd.Stage.Open(str(document)))
        check(stage and stage.GetPrimAtPath(f"/MaterialX/Materials/M_{round_}"),
              f"MaterialX document {round_} did not open through usdMtlx: {errors}")
        for path in (NETWORK, FIXTURES / "carla" / "Town01.xodr"):
            layer, errors = read_errors(lambda: Sdf.Layer.OpenAsAnonymous(str(path)))
            check(layer is not None and layer.GetPrimAtPath("/Asset/Network"),
                  f"{path.name} did not open with MaterialX loaded (round {round_}): {errors}")


def check_failed_reload_keeps_content(tmp):
    """ADR 0006 section 3: a failed Read() leaves the target layer unchanged,
    so the previous content survives a failed reload."""
    path = tmp / "failed-reload.xodr"
    shutil.copyfile(BASIC, path)
    layer = Sdf.Layer.FindOrOpen(str(path))
    check(layer is not None, "the failed-reload fixture did not open")
    if layer is None:
        return
    # Give the layer content that only a successful read replaces.
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
    check([prim.name for prim in layer.rootPrims] == ["Asset"], "the only root prim is not /Asset")
    check(layer.defaultPrim == "Asset", f"defaultPrim is {layer.defaultPrim!r}")
    # No environment-dependent value is authored (WORKSPACE.md section 5).
    check(str(BASIC.parent) not in text and BASIC.name not in text,
          "the layer records the source path")
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
        check(bool(stage.GetPrimAtPath("/Asset/Network/Roads/road_1/section_0/right_1")),
              "the sublayered lane is missing")

    # Reference: the arc targets the defaultPrim, /Asset.
    write_usda(tmp / "reference.usda",
               'def "Road" (\n    prepend references = @./net.xodr@\n)\n{\n}\n')
    stage = Usd.Stage.Open(str(tmp / "reference.usda"))
    check(bool(stage), "a stage referencing net.xodr did not open")
    if stage:
        formats = {layer.GetFileFormat().formatId for layer in stage.GetUsedLayers()}
        check("xodr" in formats, "the referenced xodr layer is not used by the stage")
        road = stage.GetPrimAtPath("/Road")
        check(road.GetTypeName() == "Xform", "the referencing prim did not take /Asset's type")
        check(bool(stage.GetPrimAtPath("/Road/Network/Roads/road_1")), "the referenced road is missing")

    # Payload: unloaded, the arc contributes nothing; loaded, it composes like
    # a reference.
    write_usda(tmp / "payload.usda",
               'def "Lazy" (\n    prepend payload = @./net.xodr@\n)\n{\n}\n')
    stage = Usd.Stage.Open(str(tmp / "payload.usda"), Usd.Stage.LoadNone)
    check(bool(stage), "a stage with a net.xodr payload did not open")
    if stage:
        lane = "/Lazy/Network/Roads/road_1/section_0/right_1"
        check(not stage.GetPrimAtPath(lane), "an unloaded payload composed its content")
        stage.Load("/Lazy")
        check(bool(stage.GetPrimAtPath(lane)), "a loaded payload did not compose the lane")
        check(stage.GetPrimAtPath("/Lazy").GetAttribute("roadnetwork:source:format").Get() == "opendrive",
              "the payload's /Asset provenance did not reach the payload prim")
        stage.Unload("/Lazy")
        check(not stage.GetPrimAtPath(lane), "an unloaded payload kept its content")


def relationship_targets(stage):
    """Every authored relationship target of the composed stage, by owning
    property."""
    targets = {}
    for prim in stage.Traverse():
        for relationship in prim.GetAuthoredRelationships():
            targets[str(relationship.GetPath())] = [str(t) for t in relationship.GetTargets()]
    return targets


def check_two_references(tmp):
    """ADR 0005 section 6: one network referenced twice keeps two sets of
    relationship targets, each inside its own reference, and flattening keeps
    them. /Asset attributes reach both referencing prims (ADR 0003)."""
    shutil.copyfile(NETWORK, tmp / "city.xodr")
    write_usda(tmp / "two.usda", (
        '(\n    defaultPrim = "World"\n    metersPerUnit = 1\n    upAxis = "Z"\n)\n'
        'def Xform "World"\n{\n'
        '    def "CityA" (\n        prepend references = @./city.xodr@\n    )\n    {\n    }\n'
        '    def "CityB" (\n        prepend references = @./city.xodr@\n    )\n    {\n    }\n'
        '}\n'))
    stage = Usd.Stage.Open(str(tmp / "two.usda"))
    check(bool(stage), "the stage with two references did not open")
    if not stage:
        return
    targets = relationship_targets(stage)
    check(len(targets) > 0, "the referenced networks have no relationships")
    for owner, paths in targets.items():
        city = owner.split("/")[2]
        check(bool(paths) and all(path.startswith(f"/World/{city}/") for path in paths),
              f"{owner} targets outside its reference: {paths}")
    lane = "Network/Roads/road_1/section_0/right_1"
    for city in ("CityA", "CityB"):
        end_links = stage.GetPrimAtPath(f"/World/{city}/{lane}").GetRelationship("roadnetwork:endLinks")
        check([str(t) for t in end_links.GetTargets()] ==
              [f"/World/{city}/Network/Roads/road_1/section_1/right_1"],
              f"{city}: the lane's end links are not remapped into their reference")
        geo = stage.GetPrimAtPath(f"/World/{city}").GetAttribute("roadnetwork:source:geoReference").Get()
        check(bool(geo), f"{city}: the georeference does not reach the referencing prim")

    # Flattening keeps the remapped targets.
    flat = Usd.Stage.Open(stage.Flatten())
    check(relationship_targets(flat) == targets, "flattening changed relationship targets")


def check_usdchecker():
    """The Phase 1 exit: generated layers pass usdchecker."""
    tool = shutil.which("usdchecker")
    for path in (BASIC, NETWORK, PROJECTED, FIXTURES / "carla" / "Town01.xodr",
                 FIXTURES / "carla" / "Town02.xodr"):
        if tool:
            run = subprocess.run([tool, str(path)], capture_output=True, text=True,
                                 encoding="utf-8", errors="replace")
            check(run.returncode == 0 and "Success!" in run.stdout,
                  f"usdchecker fails {path.name}: {run.stdout[-2000:]}{run.stderr[-2000:]}")
        else:
            from pxr import UsdUtils
            checker = UsdUtils.ComplianceChecker()
            checker.CheckCompliance(str(path))
            problems = checker.GetErrors() + checker.GetFailedChecks()
            check(not problems, f"the compliance checker fails {path.name}: {problems}")


def check_import_record():
    """roadnetwork:import:importerVersion is the bundle version (ADR 0002)."""
    manifest = (BUNDLE / "openstrata.plugin.yaml").read_text(encoding="utf-8")
    match = re.search(r"^plugin:\n(?:[ \t]+.*\n)*?[ \t]+version:[ \t]*([0-9][^\s#]*)", manifest, re.M)
    check(match is not None, "the manifest has no plugin version")
    layer = Sdf.Layer.OpenAsAnonymous(str(BASIC))
    if match is None or layer is None:
        return
    asset = "/Asset.roadnetwork:import:"
    check(layer.GetAttributeAtPath(asset + "importer").default == "roadnetwork-opendrive",
          "the importer is not roadnetwork-opendrive")
    check(layer.GetAttributeAtPath(asset + "importerVersion").default == match.group(1),
          "the importer version differs from the manifest")


def check_projected(tmp):
    """ADR 0003 sections 3-5 on writer/projected.xodr: the georeference and
    offset reach /Asset unchanged, and through a reference every
    visualization point lies within 0.5 mm of its closed-form source curve,
    although the points are single precision at 5,400 km."""
    x0, y0, z = 500737.123456, 5427713.654321, 12.5
    line_end, radius = x0 + 60.0, 100.0
    # Lateral offsets in curve order (ADR 0005 section 5): the centerlines of
    # left_1 and right_1; the center border, then the outer borders.
    offsets = {"Centerlines": [1.5, -1.75], "Boundaries": [0.0, 3.0, -3.5]}

    shutil.copyfile(PROJECTED, tmp / "projected.xodr")
    write_usda(tmp / "projected.usda",
               'def "Site" (\n    prepend references = @./projected.xodr@\n)\n{\n}\n')
    stage = Usd.Stage.Open(str(tmp / "projected.usda"))
    check(bool(stage), "the stage referencing projected.xodr did not open")
    if not stage:
        return
    site = stage.GetPrimAtPath("/Site")
    check(site.GetAttribute("roadnetwork:source:geoReference").Get() ==
          "+proj=utm +zone=32 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
          "the georeference is not verbatim")
    check(site.GetAttribute("roadnetwork:source:offset").Get() == Gf.Vec3d(-500000.0, -5400000.0, 0.0),
          "the offset is not verbatim")
    check(site.GetAttribute("roadnetwork:source:offsetHeading").Get() == 0.0, "the offset heading")
    # The semantic layer keeps the source coordinates in double.
    road = stage.GetPrimAtPath("/Site/Network/Roads/road_1")
    check(list(road.GetAttribute("roadnetwork:referenceLine:x").Get()) == [x0, line_end],
          "the reference line x values are not exact")

    origin = stage.GetPrimAtPath("/Site/Visualization").GetAttribute("xformOp:translate").Get()
    check(origin is not None and all(value % 1024.0 == 0.0 for value in origin),
          f"the origin {origin} is not a multiple of 1,024 m")
    worst = 0.0
    for container, laterals in offsets.items():
        prim = stage.GetPrimAtPath(f"/Site/Visualization/{container}/road_1")
        matrix = UsdGeom.Xformable(prim).ComputeLocalToWorldTransform(Usd.TimeCode.Default())
        counts = prim.GetAttribute("curveVertexCounts").Get()
        points = prim.GetAttribute("points").Get()
        check(list(counts) and len(counts) == len(laterals), f"{container}: {len(counts)} curves")
        start = 0
        for count, t in zip(counts, laterals):
            for point in points[start:start + count]:
                world = matrix.Transform(Gf.Vec3d(point))
                if world[0] <= line_end:
                    error = abs(world[1] - (y0 + t))
                else:
                    center = Gf.Vec2d(line_end, y0 + radius)
                    error = abs((Gf.Vec2d(world[0], world[1]) - center).GetLength() - (radius - t))
                worst = max(worst, error, abs(world[2] - z))
            start += count
    check(worst <= 0.0005, f"a visualization point is {worst * 1000:.3f} mm from its source curve")


def asset_properties(layer, prefix):
    asset = layer.GetPrimAtPath("/Asset")
    return {name: asset.properties[name].default for name in asset.properties.keys()
            if name.startswith(prefix)}


def check_metadata_only():
    """ADR 0006 section 2: a metadata-only read authors the layer metadata,
    /Asset with the provenance and counts of a full read, and an empty
    /Asset/Network; its warnings are those of the header scan."""
    for path in (BASIC, NETWORK, FIXTURES / "carla" / "Town01.xodr"):
        full = Sdf.Layer.OpenAsAnonymous(str(path), metadataOnly=False)
        meta = Sdf.Layer.OpenAsAnonymous(str(path), metadataOnly=True)
        check(full is not None, f"a full read of {path.name} failed")
        check(meta is not None, f"a metadata-only read of {path.name} failed")
        if full is None or meta is None:
            continue
        what = f"metadata-only {path.name}"
        check(meta.defaultPrim == "Asset", f"{what}: defaultPrim is {meta.defaultPrim!r}")
        for key in ("upAxis", "metersPerUnit"):
            check(meta.pseudoRoot.GetInfo(key) == full.pseudoRoot.GetInfo(key), f"{what}: {key} differs")
        check([prim.name for prim in meta.rootPrims] == ["Asset"], f"{what}: root prims")
        check([prim.name for prim in meta.GetPrimAtPath("/Asset").nameChildren] == ["Network"],
              f"{what}: /Asset holds more than an empty Network")
        check(not meta.GetPrimAtPath("/Asset/Network").nameChildren, f"{what}: /Asset/Network is not empty")
        check(asset_properties(meta, "roadnetwork:source:") == asset_properties(full, "roadnetwork:source:"),
              f"{what}: the source provenance differs from a full read")
        meta_import = asset_properties(meta, "roadnetwork:import:")
        full_import = asset_properties(full, "roadnetwork:import:")
        for key in ("importer", "importerVersion", "roadCount", "junctionCount", "laneCount"):
            name = "roadnetwork:import:" + key
            check(meta_import.get(name) == full_import.get(name), f"{what}: {name} differs from a full read")
        codes = list(meta_import.get("roadnetwork:import:warningCodes", []))
        check(codes == posted_warnings(path, metadata_only=True),
              f"{what}: the recorded warnings {codes} are not the posted ones")
    full = Sdf.Layer.OpenAsAnonymous(str(NETWORK))
    counts = asset_properties(full, "roadnetwork:import:")
    check((counts["roadnetwork:import:roadCount"], counts["roadnetwork:import:junctionCount"],
           counts["roadnetwork:import:laneCount"]) == (3, 1, 13),
          f"network.xodr counts: {counts}")
    codes = list(counts["roadnetwork:import:warningCodes"])
    check(codes == posted_warnings(NETWORK), f"network.xodr records {codes}, not the posted warnings")


def check_determinism():
    first = Sdf.Layer.OpenAsAnonymous(str(BASIC)).ExportToString()
    second = Sdf.Layer.OpenAsAnonymous(str(BASIC)).ExportToString()
    check(first == second, "two reads of basic.xodr differ")


def check_reload(tmp):
    path = tmp / "reload.xodr"
    shutil.copyfile(NETWORK, path)
    stage = Usd.Stage.Open(str(path))
    check(bool(stage), "the reload fixture did not open")
    if not stage:
        return
    layer = stage.GetRootLayer()
    before = layer.ExportToString()
    paths_before = [str(prim.GetPath()) for prim in stage.Traverse()]
    targets_before = relationship_targets(stage)
    check(len(paths_before) > 1 and bool(targets_before), "the reload fixture has no content")

    # Sdf sends LayerDidReloadContent once per successful reload, after the
    # outermost change block closes.
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
    # ADR 0005 section 7: an unchanged network keeps its paths and targets.
    check([str(prim.GetPath()) for prim in stage.Traverse()] == paths_before,
          "prim paths changed across reload")
    check(relationship_targets(stage) == targets_before, "relationship targets changed across reload")
    check(not layer.dirty, "the layer is dirty after reload")
    check(bool(stage.GetPseudoRoot()), "the stage is unusable after reload")
    del listener


def main():
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="rn-odr-contract-"))
    try:
        for test in (check_registration,
                     lambda: check_open_and_read_only(tmp),
                     lambda: check_composition(tmp),
                     lambda: check_two_references(tmp),
                     check_metadata_only,
                     check_import_record,
                     lambda: check_projected(tmp),
                     check_usdchecker,
                     check_determinism,
                     lambda: check_reload(tmp),
                     lambda: check_reader_fixtures(tmp),
                     lambda: check_untrusted_input(tmp),
                     lambda: check_materialx_coexistence(tmp),
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
