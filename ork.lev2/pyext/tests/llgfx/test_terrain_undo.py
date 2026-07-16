#!/usr/bin/env python3
###############################################################################
# Terrain undo/redo + delete-node gate (S2, jul10). Proves:
#   u1  DELETE mid-chain reconnects consumers to the pass-through input
#       (bake == hand-reduced fixture sha); UNDO restores byte-identically
#       (doc-JSON checkpoint); REDO deletes again.
#   u2  property edits are undoable; consecutive same-key edits COALESCE into
#       one step (drag ticks -> one undo).
#   u3  bypass and display (select-as-output) toggles are undoable commands.
#   u4  refusals are LOUD: captures cannot be deleted; a consumed source cannot
#       be deleted; an UNREFERENCED source deletes fine.
#   u5  delete inside a T.loop body == reduced-body fixture (every iteration).
#   u6  DSL-kwarg edits (document re-trace) are undoable: kwargs + doc restore.
#   u7  stack mechanics: depth limit, empty no-ops, redo cleared by a new edit.
# State plumbing under test = TerrainRuntime.capture/restore_undo_state (the
# same implementation the editor's UndoStack drives).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, TerrainDocParamError, delete_node, find_by_path, tree_paths)
from ork.editor.terrain_runtime import TerrainRuntime
from ork.editor.undo_stack import UndoStack

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)
OUT = "/tmp/tered_undo"


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


# ---- fixtures ---------------------------------------------------------------

class Chain(HeightField):
    """fbm -> terrace -> lpf -> capture; with_terrace=False = delete oracle."""
    def __init__(self, with_terrace=True, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        h2 = T.terrace(h, step_m=1.0/6.0, sharpness=3.0)
        h3 = T.lpf(h2 if with_terrace else h, cutoff_texels=4.0)
        self.capture(h3, "height")


class LoopTwo(HeightField):
    """T.loop body = erode -> terrace; both=False = reduced-body delete oracle."""
    def __init__(self, both=True, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            hh = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=4)
            L.h = T.terrace(hh, step_m=1.0/5.0, sharpness=2.0) if both else hh
        self.capture(L.h, "height")


class Stray(HeightField):
    """fbm -> capture, plus an UNREFERENCED source node (deletable)."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        _stray = T.Fbm(frequency=freq * 1.3, octaves=2)   # no consumers
        self.capture(h, "height")


class ThreeCaps(HeightField):
    """height + normal (PROTECTED while last-of-kind: the display path needs them)
    + an OPTIONAL channel (deletable)."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        self.capture(h, "height")
        self.capture(h, "normal")
        self.capture(h, "flow_discharge")


# ---- helpers ----------------------------------------------------------------

def bake_doc(doc, tag, ctx):
    g, cap_map = doc.elaborate()
    for chan, cap in cap_map.items():
        cap.path = f"{OUT}/{tag}.{chan}.exr"
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    return masked_sha(f"{OUT}/{tag}.height.exr")


class Host:
    """The editor's record/restore pattern, minus the widgets."""
    def __init__(self, rt, coalesce_window=60.0):
        self.rt = rt
        self.stack = UndoStack(limit=100, coalesce_window=coalesce_window)
        self.state = rt.capture_undo_state()

    def record(self, label, key=None):
        post = self.rt.capture_undo_state()
        self.stack.record(pre=self.state, post=post, restore=self.restore,
                          label=label, coalesce_key=key)
        self.state = post

    def restore(self, st):
        self.rt.restore_undo_state(st)
        self.state = st


def rt_for(inst):
    inst.generatedflow()
    rt = TerrainRuntime()
    rt.document = inst.document()
    return rt


def _raises(fn, exc):
    try:
        fn()
        return False
    except exc:
        return True


# ---- main -------------------------------------------------------------------

def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    # ---- u1: delete + undo (byte-identity) + redo ----------------------------
    rt = rt_for(Chain())
    host = Host(rt)
    sha_base = bake_doc(rt.document, "u1_base", ctx)
    sha_red = bake_doc(rt_for(Chain(with_terrace=False)).document, "u1_red", ctx)
    delete_node(rt.document, find_by_path(rt.document, "terr_0"))
    host.record("delete terr_0")
    sha_del = bake_doc(rt.document, "u1_del", ctx)
    host.stack.undo()
    sha_undo = bake_doc(rt.document, "u1_undo", ctx)
    host.stack.redo()
    sha_redo = bake_doc(rt.document, "u1_redo", ctx)
    u1 = (sha_del == sha_red and sha_undo == sha_base and sha_redo == sha_red
          and find_by_path(rt.document, "terr_0") is None)
    print(f"[u1] delete==reduced {sha_del == sha_red}  undo==base {sha_undo == sha_base}  "
          f"redo==reduced {sha_redo == sha_red} -> {u1}", flush=True)
    results["u1_delete_undo_redo"] = u1

    # ---- u2: param edit undo + coalescing -------------------------------------
    rt2 = rt_for(Chain())
    host2 = Host(rt2)
    terr = find_by_path(rt2.document, "terr_0")
    orig = [v for (k, n, v) in terr.editable_params() if n == "step_m"][0]
    for i in range(5):                                     # a drag: 5 ticks, one key
        terr.set_param("inputs", "step_m", float(orig + i + 1))
        host2.record("edit step_m", key="prop:step_m")
    one_step = host2.stack.depth() == 1
    host2.stack.undo()
    terr_rb = find_by_path(rt2.document, "terr_0")         # document was REPLACED
    val_back = [v for (k, n, v) in terr_rb.editable_params() if n == "step_m"][0]
    host2.stack.redo()
    terr_rd = find_by_path(rt2.document, "terr_0")
    val_fwd = [v for (k, n, v) in terr_rd.editable_params() if n == "step_m"][0]
    u2 = one_step and val_back == orig and val_fwd == orig + 5
    print(f"[u2] coalesced-depth==1 {one_step}  undo->orig {val_back == orig}  "
          f"redo->latest {val_fwd == orig + 5} -> {u2}", flush=True)
    results["u2_param_coalesce"] = u2

    # ---- u3: bypass + display toggles undoable --------------------------------
    rt3 = rt_for(Chain())
    host3 = Host(rt3)
    find_by_path(rt3.document, "terr_0").set_bypassed(True)
    host3.record("bypass terr_0")
    rt3.set_display_key("lpf_0")
    host3.record("display lpf_0")
    disp_set = rt3.display_key == "lpf_0"
    host3.stack.undo()                                     # undo display
    disp_undone = rt3.display_key is None
    byp_still = find_by_path(rt3.document, "terr_0").bypassed
    host3.stack.undo()                                     # undo bypass
    byp_undone = not find_by_path(rt3.document, "terr_0").bypassed
    u3 = disp_set and disp_undone and byp_still and byp_undone
    print(f"[u3] display-set {disp_set}  display-undone {disp_undone}  "
          f"bypass-survives-1st-undo {byp_still}  bypass-undone {byp_undone} -> {u3}",
          flush=True)
    results["u3_toggles_undoable"] = u3

    # ---- u4: refusals + capture deletion rules ---------------------------------
    rt4 = rt_for(Stray())
    caps = [obj for (_pk, _k, obj) in tree_paths(rt4.document)
            if isinstance(obj, DocNode) and obj.clazz_name == "CaptureModule"]
    last_h_ref = _raises(lambda: delete_node(rt4.document, caps[0]), TerrainDocParamError)
    src_ref = _raises(lambda: delete_node(rt4.document, find_by_path(rt4.document, "fbm_0")),
                      TerrainDocParamError)
    try:
        delete_node(rt4.document, find_by_path(rt4.document, "fbm_1"))
        stray_ok = find_by_path(rt4.document, "fbm_1") is None
    except TerrainDocParamError as ex:
        print(f"[u4] stray delete raised: {ex}", flush=True)
        stray_ok = False
    # OPTIONAL-channel capture: deletable, registry cleaned, doc-JSON stays valid,
    # UNDO brings it back; last-normal is PROTECTED like last-height.
    from ork.hypergraph.dflow.terrain.doc import to_json, from_json
    rt4b = rt_for(ThreeCaps())
    host4 = Host(rt4b)
    ncap = next(c.node for c in rt4b.document._captures if "normal" in c.channels)
    last_n_ref = _raises(lambda: delete_node(rt4b.document, ncap), TerrainDocParamError)
    acap = next(c.node for c in rt4b.document._captures if "flow_discharge" in c.channels)
    delete_node(rt4b.document, acap)
    host4.record("delete aux capture")
    reg_clean = all("flow_discharge" not in c.channels for c in rt4b.document._captures)
    json_ok = from_json(to_json(rt4b.document)) is not None
    host4.stack.undo()
    aux_back = any("flow_discharge" in c.channels for c in rt4b.document._captures)
    aux_ok = last_n_ref and reg_clean and json_ok and aux_back
    u4 = last_h_ref and src_ref and stray_ok and aux_ok
    print(f"[u4] last-height-refused {last_h_ref}  consumed-source-refused {src_ref}  "
          f"stray-source-deleted {stray_ok}  last-normal-refused+aux-delete+undelete "
          f"{aux_ok} -> {u4}", flush=True)
    results["u4_refusals"] = u4

    # ---- u5: delete inside a loop body -----------------------------------------
    rt5 = rt_for(LoopTwo())
    delete_node(rt5.document, find_by_path(rt5.document, "loop_0/terr_0"))
    sha_l = bake_doc(rt5.document, "u5_del", ctx)
    sha_lr = bake_doc(rt_for(LoopTwo(both=False)).document, "u5_red", ctx)
    u5 = sha_l == sha_lr
    print(f"[u5] loop-body delete==reduced-body {u5}", flush=True)
    results["u5_loop_delete"] = u5

    # ---- u6: DSL-kwarg edit undo ------------------------------------------------
    rtw = TerrainRuntime()
    rtw.load("warp")
    hostw = Host(rtw)
    kw0 = dict(rtw.editable_dsl_kwargs())
    doc0 = rtw.capture_undo_state()["doc"]
    rtw.set_dsl_kwarg("frequency", float(kw0["frequency"]) + 1.0)
    hostw.record("kwarg frequency")
    changed = rtw.editable_dsl_kwargs()["frequency"] == float(kw0["frequency"]) + 1.0
    hostw.stack.undo()
    kw_back = dict(rtw.editable_dsl_kwargs()) == kw0
    doc_back = rtw.capture_undo_state()["doc"] == doc0
    u6 = changed and kw_back and doc_back
    print(f"[u6] kwarg-changed {changed}  kwargs-restored {kw_back}  "
          f"doc-restored {doc_back} -> {u6}", flush=True)
    results["u6_kwarg_undo"] = u6

    # ---- u7: stack mechanics ------------------------------------------------------
    stack = UndoStack(limit=3, coalesce_window=60.0)
    seen = []
    for i in range(5):
        stack.record(pre=i, post=i + 1, restore=seen.append, label=f"s{i}")
    limit_ok = stack.depth() == 3
    empty_ok = (UndoStack().undo() is None) and (UndoStack().redo() is None)
    stack.undo()
    had_redo = stack.can_redo()
    stack.record(pre=99, post=100, restore=seen.append, label="new")
    redo_cleared = not stack.can_redo()
    u7 = limit_ok and empty_ok and had_redo and redo_cleared
    print(f"[u7] limit {limit_ok}  empty-noop {empty_ok}  redo-then-cleared "
          f"{had_redo and redo_cleared} -> {u7}", flush=True)
    results["u7_stack_mechanics"] = u7

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain undo {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
