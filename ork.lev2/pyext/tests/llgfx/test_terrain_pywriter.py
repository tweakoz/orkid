#!/usr/bin/env python3
###############################################################################
# Terrain .py DSL WRITER gate (JUL09 S3 codegen half). Proves the editor's Save
# regenerates a runnable HeightField .py asset whose bake is BYTE-IDENTICAL to the
# document it came from — the round-trip contract (doc -> .py -> reload -> bake).
#
#   p1  the editor-reachable surface: runtime.load('new') + add_op_node terrace +
#       add_loop + add_into_loop erode_thermal -> emit -> reload -> bake; sha match.
#   p2  corpus round-trip: 'erox' (simple chain + loop) and 'erodeflow' (two loops
#       with L.i params, multi-output flow3d / fill_closed_basins, relax_uv) emit,
#       reload, bake -> per-channel sha identical.
#   p3  authored ExprModule ('xxx2' hfdisplacement): the writer re-renders REAL DSL from
#       the captured ExprIR tree (T.expr(...), never a compiled-blob hatch) + bakes identical.
#   p7  a bypassed whole T.loop emits `bypassed=True` and round-trips bake-identical.
#   p8  switch + generator-group bypass are refused with the typed error.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocSwitch, DocGroupCall, TerrainDocParamError, find_by_path, tree_paths,
    add_op_node, add_loop, add_into_loop)
from ork.hypergraph.dflow.terrain.pywriter import to_python, class_name_from_stem
from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
from ork.editor.terrain_runtime import TerrainRuntime


@T.group
def _gen_group(gain=1.0):
    # a GENERATOR group: NO terrain-typed input arg -> NOT bypassable (set_bypassed refuses).
    return T.Fbm(frequency=4.0, octaves=3) * gain


class SwitchGenGroupDoc(HeightField):
    """A doc holding a T.switch and a generator @T.group — the two bypass-refusal cases."""
    def __init__(self):
        super().__init__()
        base = T.Fbm(frequency=4.0, octaves=5) * 0.5 + 0.5
        g = _gen_group(gain=0.3)                       # generator group (no terrain input)
        smooth = T.lpf(base, cutoff=6.0)
        ridged = T.terrace(base, step_m=1.0/8.0, sharpness=6.0)
        sw = T.switch("smooth", smooth=smooth, ridged=ridged)
        self.capture(sw + g, "height")                 # consume BOTH so elaborate is valid

OUT = "/tmp/tered_pyw"
SRCDIR = "/tmp/tered_pyw_src"
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


def masked_sha(path):
    """sha256 of an EXR with the embedded capDate zeroed (identical bakes differ only
    in that timestamp) — the sibling battery's byte-identity convention."""
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


def bake_channels(doc, extent, dim, tag, ctx, channels):
    """Bake the document at `dim` (routing each capture to a per-channel EXR) and
    return {channel: masked_sha} for the requested channels."""
    g, _cap = doc.elaborate()
    paths = {}
    for cap in lev2.terrain.capture_modules(g):
        ch_list = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()] or ["height"]
        if len(ch_list) == 1:
            cap.path = f"{OUT}/{tag}.{ch_list[0]}.exr"
        else:
            cap.path = f"{OUT}/{tag}.{{channel}}.exr"
        for c in ch_list:
            paths[c] = f"{OUT}/{tag}.{c}.exr"
    lev2.terrain.bake_heightfield(g, ctx, dim, extent_m=extent)
    return {c: masked_sha(paths[c]) for c in channels}


def reload_doc(src, tag):
    """Write emitted source to disk, import it, instantiate the HeightField, and
    return (document, EXTENT_M)."""
    os.makedirs(SRCDIR, exist_ok=True)
    p = os.path.join(SRCDIR, f"{tag}.py")
    with open(p, "w") as f:
        f.write(src)
    cls = load_dsl_class(p)
    inst = cls()
    inst.generatedflow()
    return inst.document(), float(cls.EXTENT_M)


def channels_of(doc):
    return sorted(c for cap in doc._captures for c in cap.channels)


def _roundtrip(doc, extent, dim, tag, ctx, channels, label):
    """doc -> emit -> reload -> bake; assert every requested channel's sha matches the
    source doc's bake. Returns (ok, detail)."""
    cname = class_name_from_stem(tag)
    src = to_python(doc, class_name=cname, extent_m=extent, source_note=label)
    doc2, extent2 = reload_doc(src, tag)
    chans_ok = channels_of(doc2) == channels_of(doc)
    extent_ok = extent2 == extent
    sha_a = bake_channels(doc, extent, dim, f"{tag}_a", ctx, channels)
    sha_b = bake_channels(doc2, extent2, dim, f"{tag}_b", ctx, channels)
    per = {c: (sha_a[c] == sha_b[c]) for c in channels}
    ok = chans_ok and extent_ok and all(per.values())
    print(f"[{label}] channels_match={chans_ok} extent_match={extent_ok} "
          f"sha_identical={per}", flush=True)
    if not ok:
        # keep the emitted source around for inspection on failure.
        print(f"[{label}] emitted source at {SRCDIR}/{tag}.py", flush=True)
    return ok


def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    # ---- p1: the editor-reachable add surface round-trips ----------------------
    rt = TerrainRuntime()
    rt.load("new")
    doc = rt.document
    fbm = find_by_path(doc, "fbm_0")
    terr = add_op_node(doc, "terrace", fbm)
    loop = add_loop(doc, terr)
    add_into_loop(doc, loop, "erode_thermal")
    p1 = _roundtrip(doc, rt.extent_m, 256, "p1_editoradd", ctx, ["height", "normal"], "p1")
    results["p1_editor_add_roundtrip"] = p1

    # ---- p2a: erox (simple chain + single loop) --------------------------------
    epath = resolve_dsl_file("erox")
    ecls = load_dsl_class(epath)
    einst = ecls(); einst.generatedflow()
    p2a = _roundtrip(einst.document(), float(ecls.EXTENT_M), 128, "p2_erox", ctx,
                     ["height", "normal"], "p2a-erox")
    results["p2a_erox_roundtrip"] = p2a

    # ---- p2b: erodeflow (two loops w/ L.i params, multi-output flow3d/fcb, relax_uv)
    fpath = resolve_dsl_file("erodeflow")
    fcls = load_dsl_class(fpath)
    finst = fcls(); finst.generatedflow()
    p2b = _roundtrip(finst.document(), float(fcls.EXTENT_M), 128, "p2_erodeflow", ctx,
                     ["height", "filled", "flow_metrics"], "p2b-erodeflow")
    results["p2b_erodeflow_roundtrip"] = p2b

    # ---- p3: authored ExprModule (xxx2 hfdisplacement) round-trips via REAL DSL re-rendered
    # from the captured ExprIR tree (E2.5 — the compiled-blob escape hatch is GONE): the
    # emitted .py must contain a T.expr(...) call for the expr node (NO raw shadertext) AND
    # bake byte-identical (the tree re-renders the identical SurfNode -> identical shadertext).
    xpath = resolve_dsl_file("xxx2")
    xcls = load_dsl_class(xpath)
    xinst = xcls(); xinst.generatedflow()
    xdoc = xinst.document()
    expr_names = [ch.local_name for (_pk, _k, ch) in _all_nodes(xdoc)
                  if isinstance(ch, DocNode) and ch.clazz_name == "ExprModule"]
    src = to_python(xdoc, class_name="Xxx2Round", extent_m=float(xcls.EXTENT_M))
    emits_expr = "T.expr(" in src
    p3rt = _roundtrip(xdoc, float(xcls.EXTENT_M), 128, "p3_xxx2", ctx,
                      ["height"], "p3-xxx2-tree")
    p3 = emits_expr and p3rt and len(expr_names) > 0
    print(f"[p3] expr_nodes={expr_names} T.expr_emitted={emits_expr} roundtrip={p3rt} -> {p3}",
          flush=True)
    results["p3_expr_tree_roundtrip"] = p3

    # ---- p4: editor-authored T.expr node round-trips (needs B1 ExprModule.expr_source) ---
    # add_op_node 'expr' inserts an identity T.expr("ctx.input(0)") after fbm; the writer emits
    # T.expr(<source>, inputs=[...]) (NOT the compiled blob), and reload -> bake is identical.
    if hasattr(lev2.terrain.ExprModule.createShared(), "expr_source"):
        rt4 = TerrainRuntime(); rt4.load("new")
        doc4 = rt4.document
        fbm4 = find_by_path(doc4, "fbm_0")
        add_op_node(doc4, "expr", fbm4)
        src4 = to_python(doc4, class_name=class_name_from_stem("p4_expr"), extent_m=rt4.extent_m)
        has_expr = "T.expr(" in src4
        p4rt = _roundtrip(doc4, rt4.extent_m, 256, "p4_expr", ctx, ["height", "normal"], "p4")
        p4 = has_expr and p4rt
        print(f"[p4] T.expr emitted={has_expr} roundtrip={p4rt} -> {p4}", flush=True)
        results["p4_expr_roundtrip"] = p4
    else:
        print("[p4] SKIP: ExprModule.expr_source absent (needs the B1 C++ rebuild)", flush=True)

    # ---- p5: a bypassed node round-trips as T.bypass + bakes == the flattened chain ------
    # fbm -> terrace(bypassed) -> caps: the writer emits the terrace + a following T.bypass, and
    # the C++ splice makes the bake equal the flattened form (fbm straight to caps == 'new').
    rt5 = TerrainRuntime(); rt5.load("new")
    doc5 = rt5.document
    fbm5 = find_by_path(doc5, "fbm_0")
    terr5 = add_op_node(doc5, "terrace", fbm5)
    terr5.set_bypassed(True)
    src5 = to_python(doc5, class_name=class_name_from_stem("p5_bypass"), extent_m=rt5.extent_m)
    has_bypass = "T.bypass(" in src5
    p5rt = _roundtrip(doc5, rt5.extent_m, 256, "p5_bypass", ctx, ["height", "normal"], "p5")
    rt5f = TerrainRuntime(); rt5f.load("new")
    sha_byp = bake_channels(doc5, rt5.extent_m, 256, "p5_byp", ctx, ["height"])
    sha_flat = bake_channels(rt5f.document, rt5f.extent_m, 256, "p5_flat", ctx, ["height"])
    bake_flat = sha_byp["height"] == sha_flat["height"]
    p5 = has_bypass and p5rt and bake_flat
    print(f"[p5] T.bypass emitted={has_bypass} roundtrip={p5rt} bake==flattened={bake_flat} "
          f"-> {p5}", flush=True)
    results["p5_bypass_roundtrip"] = p5

    # ---- p6: doc-level select_output survives save/reload + the reloaded bake honors it --
    # fbm -> terrace -> lpf -> caps, marking terrace as output: the reloaded bake re-points the
    # captures to terrace (drops the lpf tail) — equal to the original doc's bake, and DIFFERENT
    # from an otherwise-identical chain WITHOUT the marker.
    rt6 = TerrainRuntime(); rt6.load("new")
    doc6 = rt6.document
    fbm6 = find_by_path(doc6, "fbm_0")
    terr6 = add_op_node(doc6, "terrace", fbm6)
    add_op_node(doc6, "lpf", terr6)
    doc6.set_select_output(terr6)
    src6 = to_python(doc6, class_name=class_name_from_stem("p6_selout"), extent_m=rt6.extent_m)
    has_sel = "self.select_output(" in src6
    doc6r, ext6 = reload_doc(src6, "p6_selout")
    sel_retained = doc6r._select_output is not None
    sha_reload = bake_channels(doc6r, ext6, 256, "p6_reload", ctx, ["height"])
    sha_orig = bake_channels(doc6, rt6.extent_m, 256, "p6_orig", ctx, ["height"])
    rt6b = TerrainRuntime(); rt6b.load("new")
    fbm6b = find_by_path(rt6b.document, "fbm_0")
    terr6b = add_op_node(rt6b.document, "terrace", fbm6b)
    add_op_node(rt6b.document, "lpf", terr6b)
    sha_nosel = bake_channels(rt6b.document, rt6b.extent_m, 256, "p6_nosel", ctx, ["height"])
    reload_eq = sha_reload["height"] == sha_orig["height"]
    honored = sha_reload["height"] != sha_nosel["height"]
    p6 = has_sel and sel_retained and reload_eq and honored
    print(f"[p6] select_output emitted={has_sel} retained={sel_retained} reload==orig={reload_eq} "
          f"honored(!=no-marker)={honored} -> {p6}", flush=True)
    results["p6_select_output"] = p6

    # ---- p7: a bypassed whole T.loop emits `bypassed=True` + round-trips bake-identical --
    # fbm -> terrace -> loop(erode){bypassed}: the writer emits T.loop(..., bypassed=True); the
    # reloaded loop skips its unroll (carry aliases to its initial) -> bake == the source doc.
    rt7 = TerrainRuntime(); rt7.load("new")
    doc7 = rt7.document
    fbm7 = find_by_path(doc7, "fbm_0")
    terr7 = add_op_node(doc7, "terrace", fbm7)
    loop7 = add_loop(doc7, terr7)
    add_into_loop(doc7, loop7, "erode_thermal")
    loop7.set_bypassed(True)
    src7 = to_python(doc7, class_name=class_name_from_stem("p7_loopbypass"), extent_m=rt7.extent_m)
    has_bypass_kw = "bypassed=True" in src7
    p7rt = _roundtrip(doc7, rt7.extent_m, 256, "p7_loopbypass", ctx, ["height", "normal"], "p7")
    p7 = has_bypass_kw and p7rt
    print(f"[p7] loop bypassed=True emitted={has_bypass_kw} roundtrip={p7rt} -> {p7}", flush=True)
    results["p7_loop_bypass_roundtrip"] = p7

    # ---- p8: switch + generator-group bypass are REFUSED with the typed error ------------
    sgd = SwitchGenGroupDoc(); sgd.generatedflow()
    sdoc = sgd.document()
    sw = next(o for (_pk, _k, o) in tree_paths(sdoc) if isinstance(o, DocSwitch))
    gen = next(o for (_pk, _k, o) in tree_paths(sdoc) if isinstance(o, DocGroupCall))
    switch_refuses = _raises(lambda: sw.set_bypassed(True), TerrainDocParamError)
    gengroup_refuses = _raises(lambda: gen.set_bypassed(True), TerrainDocParamError)
    p8 = switch_refuses and gengroup_refuses
    print(f"[p8] switch-refuses={switch_refuses} generator-group-refuses={gengroup_refuses} -> {p8}",
          flush=True)
    results["p8_bypass_refusals"] = p8

    return results


def _raises(fn, exc):
    try:
        fn()
        return False
    except exc:
        return True


def _all_nodes(doc):
    from ork.hypergraph.dflow.terrain.doc import tree_paths
    return tree_paths(doc)


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain pywriter {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
