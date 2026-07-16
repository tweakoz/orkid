#!/usr/bin/env python3
###############################################################################
# Terrain add-node / add-loop / new-terrain gate (jul10). Proves:
#   a1  add_op_node inserts mid-chain with rewire == the AUTHORED equivalent
#       fixture (sha), and lands at chain position (creation order resolves).
#   a2  add_loop inserts an EMPTY loop = IDENTITY (bake byte-identical); the loop
#       is a real DocLoop (count editable).
#   a3  add_into_loop appends to the body with per-iteration semantics == the
#       AUTHORED `with T.loop(...)` fixture (sha).
#   a4  a SOURCE add (fbm) starts an unconsumed branch: bake unchanged, node
#       present.
#   a5  names: custom name applied; sibling collision uniquified.
#   a6  add is UNDOABLE (doc-JSON checkpoint round-trip).
#   a7  MinimalTerrain (`new`): loads via runtime.load('new'), has height+normal
#       captures + editable frequency/octaves kwargs, and BAKES.
#   a8  model.createItem drives the same mutations (the outliner add flow) and
#       returns the new row's key.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, TerrainDocParamError, tree_paths, find_by_path,
    add_op_node, add_loop, add_into_loop, to_json, from_json)
from ork.editor.terrain_runtime import TerrainRuntime
from ork.editor.terrain_doc_model import TerrainDocOutlinerModel
from ork.editor.undo_stack import UndoStack

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)
OUT = "/tmp/tered_add"


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


# ---- fixtures ---------------------------------------------------------------

class Base(HeightField):
    """fbm -> lpf -> capture (the add target)."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        self.capture(T.lpf(h, cutoff_texels=4.0), "height")


class WithTerrace(HeightField):
    """the a1 oracle: terrace at DEFAULTS between the remap and the lpf."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        self.capture(T.lpf(T.terrace(h), cutoff_texels=4.0), "height")


class WithLoop(HeightField):
    """the a3 oracle: a 4x erode_thermal loop at DEFAULTS between remap and lpf."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        with T.loop(4, h=h) as L:
            L.h = T.erode_thermal(L.h)
        self.capture(T.lpf(L.h, cutoff_texels=4.0), "height")


class WithFlowErode(HeightField):
    """the a9 oracle: the flow3d+flow_erode composite at DEFAULTS mid-chain."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        f = T.flow3d(h)
        h = T.flow_erode(h, f.discharge)
        self.capture(T.lpf(h, cutoff_texels=4.0), "height")


class WithFcb(HeightField):
    """the a10 oracle: fill_closed_basins at DEFAULTS chained through .filled."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        h = T.fill_closed_basins(h).filled
        self.capture(T.lpf(h, cutoff_texels=4.0), "height")


def doc_of(inst):
    inst.generatedflow()
    return inst.document()


def bake_doc(doc, tag, ctx):
    g, cap_map = doc.elaborate()
    for chan, cap in cap_map.items():
        cap.path = f"{OUT}/{tag}.{chan}.exr"
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    return masked_sha(f"{OUT}/{tag}.height.exr")


def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    sha_oracle_terrace = bake_doc(doc_of(WithTerrace()), "a1_oracle", ctx)
    sha_oracle_loop = bake_doc(doc_of(WithLoop()), "a3_oracle", ctx)

    # ---- a1: mid-chain op insert == authored fixture ---------------------------
    d1 = doc_of(Base())
    sha_base = bake_doc(d1, "a1_base", ctx)
    anchor = find_by_path(d1, "remap_1")
    node = add_op_node(d1, "terrace", anchor)
    pos_ok = d1._root.index(node) == d1._root.index(anchor) + 1
    sha_added = bake_doc(d1, "a1_added", ctx)
    a1 = (sha_added == sha_oracle_terrace) and (sha_added != sha_base) and pos_ok
    print(f"[a1] add==authored {sha_added == sha_oracle_terrace}  changed {sha_added != sha_base}  "
          f"chain-position {pos_ok} -> {a1}", flush=True)
    results["a1_add_op"] = a1

    # ---- a2: empty loop is identity --------------------------------------------
    d2 = doc_of(Base())
    lp = add_loop(d2, find_by_path(d2, "remap_1"), count=4)
    sha_ident = bake_doc(d2, "a2_ident", ctx)
    a2 = (sha_ident == sha_base) and isinstance(lp, DocLoop) and lp.count == 4
    print(f"[a2] empty-loop identity {sha_ident == sha_base}  is-DocLoop {isinstance(lp, DocLoop)} -> {a2}",
          flush=True)
    results["a2_empty_loop_identity"] = a2

    # ---- a3: add into loop == authored loop fixture ----------------------------
    add_into_loop(d2, lp, "erode_thermal")
    sha_body = bake_doc(d2, "a3_body", ctx)
    a3 = sha_body == sha_oracle_loop
    print(f"[a3] into-loop==authored {a3}", flush=True)
    results["a3_add_into_loop"] = a3

    # ---- a4: source add = unconsumed branch, bake unchanged --------------------
    d4 = doc_of(Base())
    src = add_op_node(d4, "fbm", find_by_path(d4, "remap_1"))
    sha_src = bake_doc(d4, "a4_src", ctx)
    a4 = (sha_src == sha_base) and (find_by_path(d4, src.local_name) is src)
    print(f"[a4] bake-unchanged {sha_src == sha_base}  node-present "
          f"{find_by_path(d4, src.local_name) is src} -> {a4}", flush=True)
    results["a4_source_branch"] = a4

    # ---- a5: naming ---------------------------------------------------------------
    d5 = doc_of(Base())
    n1 = add_op_node(d5, "terrace", find_by_path(d5, "remap_1"), name="myterrace")
    n2 = add_op_node(d5, "terrace", find_by_path(d5, "remap_1"), name="myterrace")
    a5 = (n1.local_name == "myterrace" and n2.local_name != n1.local_name
          and find_by_path(d5, n2.local_name) is n2)
    print(f"[a5] custom={n1.local_name!r} collision->{n2.local_name!r} -> {a5}", flush=True)
    results["a5_names"] = a5

    # ---- a6: add is undoable --------------------------------------------------------
    rt6 = TerrainRuntime()
    rt6.document = doc_of(Base())
    stack = UndoStack()
    pre = rt6.capture_undo_state()
    add_op_node(rt6.document, "terrace", find_by_path(rt6.document, "remap_1"))
    stack.record(pre=pre, post=rt6.capture_undo_state(),
                 restore=rt6.restore_undo_state, label="add terrace")
    stack.undo()
    a6 = to_json(rt6.document) == pre["doc"]
    print(f"[a6] undo restores pre-add doc {a6}", flush=True)
    results["a6_add_undo"] = a6

    # ---- a7: `new` minimal viable ----------------------------------------------------
    rt7 = TerrainRuntime()
    rt7.load("new")
    chans = sorted(c for cap in rt7.document._captures for c in cap.channels)
    kw = rt7.editable_dsl_kwargs()
    sha_new = bake_doc(rt7.document, "a7_new", ctx)
    a7 = (chans == ["height", "normal"] and "frequency" in kw and "octaves" in kw
          and len(sha_new) == 64)
    print(f"[a7] new: channels={chans} kwargs={sorted(kw)} baked={len(sha_new) == 64} -> {a7}",
          flush=True)
    results["a7_new_minimal"] = a7

    # ---- a7b: dim is a Terrain Parameters session row (clamped, chunk-rounded,
    # in the undo checkpoint) ---------------------------------------------------
    from ork.editor.terrain_doc_model import TerrainParamsPropertyModel
    ppm = TerrainParamsPropertyModel(rt7)
    has_dim_row = "dim" in ppm.getChildren("")
    pre_dim = rt7.capture_undo_state()
    ppm.setValue("dim", 1000)                     # -> chunk-rounded (128 -> 1024)
    rounded = rt7.preview_dim == 1024
    ppm.setValue("dim", 1)                        # -> clamped up to chunk
    clamped = rt7.preview_dim == rt7.chunk
    rt7.restore_undo_state(pre_dim)
    undone = rt7.preview_dim == pre_dim["dim"]
    # slider range annotations must survive the C++ trampoline (VarMap, int-typed):
    # the Int editor defaults 0..100 without them.
    ann = lev2.ui.PropertySheetModel.getAnnotations(ppm, "dim")
    ann_ok = (ann is not None and int(ann.min) == 128 and int(ann.max) == 16384)
    a7b = has_dim_row and rounded and clamped and undone and ann_ok
    print(f"[a7b] dim-row {has_dim_row}  1000->1024 {rounded}  1->chunk {clamped}  "
          f"undo-restores {undone}  range-annotations {ann_ok} -> {a7b}", flush=True)
    results["a7b_dim_param"] = a7b

    # ---- a8: model.createItem (the outliner add flow) ---------------------------------
    d8 = doc_of(Base())
    om = TerrainDocOutlinerModel(d8)
    fac_node = [f["id"] for f in om.getFactories("remap_1")]
    fac_loop_row_missing = om.getFactories("capture_0") == []
    k1 = om.createItem("remap_1", "terrace", "terrace")     # default name -> auto
    k2 = om.createItem("remap_1", "loop", "loop")
    lp8 = om.object_for_key(k2)
    k3 = om.createItem(k2, "erode_thermal", "erode_thermal")
    a8 = ("loop" in fac_node and "terrace" in fac_node and fac_loop_row_missing
          and om.object_for_key(k1) is not None
          and isinstance(lp8, DocLoop)
          and k3.startswith(k2 + "/"))
    print(f"[a8] factories={len(fac_node)} node-add={k1!r} loop-add={k2!r} "
          f"into-loop={k3!r} -> {a8}", flush=True)
    results["a8_model_createitem"] = a8

    # ---- a9: flow_erode composite add == authored flow3d+flow_erode fixture -----
    d9 = doc_of(Base())
    add_op_node(d9, "flow_erode", find_by_path(d9, "remap_1"))
    sha9 = bake_doc(d9, "a9_flow", ctx)
    sha9o = bake_doc(doc_of(WithFlowErode()), "a9_oracle", ctx)
    a9 = sha9 == sha9o
    print(f"[a9] flow_erode composite==authored {a9}", flush=True)
    results["a9_flow_erode_composite"] = a9

    # ---- a10: fill_closed_basins add (struct .filled chain) == authored ----------
    d10 = doc_of(Base())
    add_op_node(d10, "fill_closed_basins", find_by_path(d10, "remap_1"))
    sha10 = bake_doc(d10, "a10_fcb", ctx)
    sha10o = bake_doc(doc_of(WithFcb()), "a10_oracle", ctx)
    a10 = sha10 == sha10o
    print(f"[a10] fcb .filled chain==authored {a10}", flush=True)
    results["a10_fcb_filled"] = a10

    # ---- a11: terrace/basin_fill/flow_erode record an editable `blend` param ------
    # (the lpf idiom: scalar blend = module runtime param, default 1.0 — must land in
    # param_actions so the propsheet exposes it on nodes created via the add menu)
    a11 = True
    for op in ("terrace", "basin_fill", "flow_erode"):
        d11 = doc_of(Base())
        n11 = add_op_node(d11, op, find_by_path(d11, "remap_1"))
        blends = [v for (k, nm, v) in n11.param_actions if nm == "blend"]
        good = blends == [1.0]
        if not good:
            print(f"[a11] {op}: blend param_actions={blends} (want [1.0])", flush=True)
        a11 = a11 and good
    print(f"[a11] blend param recorded on all three ops {a11}", flush=True)
    results["a11_blend_param"] = a11

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain add {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
