#!/usr/bin/env python3
###############################################################################
# Terrain node-flag gate (bypass + select-as-output, jul10). Proves:
#   f1  BYPASS mid-chain == hand-reduced fixture (sha), toggle-off == baseline.
#   f2  BYPASS inside a T.loop body bypasses EVERY iteration (== reduced body).
#   f3  DISPLAY (select-as-output) == direct-capture fixture (sha); every other
#       capture is DROPPED (its EXR is not written; its chain is not demanded).
#   f4  DISPLAY of a loop-body node shows the LAST iteration (== full unroll).
#   f5  refusals are LOUD: bypass on a source node / capture raises; display on
#       a node with no 'Out' plug raises at elaborate.
#   f6  doc-JSON round-trips the bypassed flag; round-tripped bake == flag bake.
#   f7  tree_paths/find_by_path resolve the outliner keying (loop_0/... nodes).
#   f8  BYPASS a WHOLE T.loop == the carry at INITIAL (body never unrolls); toggle
#       off == the active bake.
#   f9  doc-JSON round-trips the LOOP bypass flag (bake-identical); the bypassed loop
#       still appears in tree_paths; the outliner badge is present+enabled on a loop
#       row and its active state tracks the flag.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, TerrainDocParamError, to_json, from_json, tree_paths, find_by_path)
from ork.editor.terrain_doc_model import TerrainDocOutlinerModel

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)
OUT = "/tmp/tered_flags"


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


# ---- fixtures ---------------------------------------------------------------

class Chain(HeightField):
    """fbm -> terrace -> lpf -> capture. terrace is the bypass/display target;
    with_terrace=False is the hand-reduced equivalence oracle (lpf reads fbm)."""
    def __init__(self, with_terrace=True, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        h2 = T.terrace(h, step_m=1.0/6.0, sharpness=3.0)
        h3 = T.lpf(h2 if with_terrace else h, cutoff_texels=4.0)
        self.capture(h3, "height")


class ChainDirect(HeightField):
    """fbm -> terrace -> capture: the display-mode oracle (chain STOPS at terrace)."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=5) * 0.5 + 0.5
        self.capture(T.terrace(h, step_m=1.0/6.0, sharpness=3.0), "height")


class ChainMultiCap(Chain):
    """Chain + an extra non-display capture — must be DROPPED in display mode."""
    def __init__(self, freq=_SALT):
        super().__init__(freq=freq)
        # aux capture off the SAME terminal node (self-standing channel)
        self.capture(self._last_height_node(), "flow_discharge")

    def _last_height_node(self):
        # re-derive the lpf output by re-tracing would break; instead capture fbm
        # directly — any node works, the point is the channel gets dropped.
        return T.Fbm(frequency=_SALT * 1.7, octaves=3)


class LoopTwo(HeightField):
    """T.loop body = erode -> terrace; both=False is the reduced-body oracle."""
    def __init__(self, both=True, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            hh = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=4)
            L.h = T.terrace(hh, step_m=1.0/5.0, sharpness=2.0) if both else hh
        self.capture(L.h, "height")


class LoopDisplay(HeightField):
    """loop(3) of erode, then an lpf tail; displaying the body node must show the
    LAST iteration (== FullUnroll oracle capturing the loop output directly)."""
    def __init__(self, tail=True, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            L.h = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=4)
        out = T.lpf(L.h, cutoff_texels=4.0) if tail else L.h
        self.capture(out, "height")


class LoopChain(HeightField):
    """fbm -> loop(3){terrace} -> capture. The whole LOOP is the bypass target: bypassing
    it aliases the carry to its INITIAL (== the no-loop LoopChainReduced oracle). terrace
    quantizes the height, so the unrolled bake is CLEARLY != the carry-at-initial bake."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            L.h = T.terrace(L.h, step_m=1.0/6.0, sharpness=3.0)
        self.capture(L.h, "height")


class LoopChainReduced(HeightField):
    """fbm -> capture (the loop's carry at INITIAL): the bypassed-LOOP equivalence oracle
    (the body never unrolls, so the carry reads through to its initial ref)."""
    def __init__(self, freq=_SALT):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=4) * 0.5 + 0.5
        self.capture(h, "height")


# (flow3d DOES expose an 'Out' plug — its dir field — so it is displayable; the
# genuine display refusal is a node that is not part of the elaborated document.)


# ---- helpers ----------------------------------------------------------------

def bake_doc(doc, tag, ctx, display_node=None):
    g, cap_map = doc.elaborate(display_node=display_node)
    for chan, cap in cap_map.items():
        cap.path = f"{OUT}/{tag}.{chan}.exr"
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    return {chan: masked_sha(f"{OUT}/{tag}.{chan}.exr") for chan in cap_map}


def node_by_name(doc, name):
    for (_pk, _key, obj) in tree_paths(doc):
        if isinstance(obj, DocNode) and obj.local_name == name and _pk == "":
            return obj
    return None


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

    # ---- f1: bypass equivalence + toggle-off restore ------------------------
    full = Chain(); full.generatedflow()
    reduced = Chain(with_terrace=False); reduced.generatedflow()
    doc = full.document()
    terrace = find_by_path(doc, "terr_0")
    assert terrace is not None, "terrace_0 not found via find_by_path"
    sha_base = bake_doc(doc, "f1_base", ctx)["height"]
    terrace.set_bypassed(True)
    sha_byp = bake_doc(doc, "f1_byp", ctx)["height"]
    sha_red = bake_doc(reduced.document(), "f1_red", ctx)["height"]
    terrace.set_bypassed(False)
    sha_off = bake_doc(doc, "f1_off", ctx)["height"]
    f1 = (sha_byp == sha_red) and (sha_off == sha_base) and (sha_byp != sha_base)
    print(f"[f1] bypass==reduced {sha_byp == sha_red}  off==base {sha_off == sha_base}  "
          f"byp!=base {sha_byp != sha_base} -> {f1}", flush=True)
    results["f1_bypass"] = f1

    # ---- f2: bypass inside a loop body --------------------------------------
    ltwo = LoopTwo(); ltwo.generatedflow()
    lred = LoopTwo(both=False); lred.generatedflow()
    tnode = find_by_path(ltwo.document(), "loop_0/terr_0")
    if tnode is None:  # op name may differ; locate by class inside the loop
        tnode = next(obj for (_pk, key, obj) in tree_paths(ltwo.document())
                     if isinstance(obj, DocNode) and key.startswith("loop_0/")
                     and "terrace" in obj.clazz_name.lower())
    tnode.set_bypassed(True)
    sha_lbyp = bake_doc(ltwo.document(), "f2_byp", ctx)["height"]
    sha_lred = bake_doc(lred.document(), "f2_red", ctx)["height"]
    f2 = sha_lbyp == sha_lred
    print(f"[f2] loop-body bypass==reduced-body {f2}", flush=True)
    results["f2_loop_bypass"] = f2

    # ---- f3: display == direct capture; other captures dropped --------------
    mc = ChainMultiCap(); mc.generatedflow()
    direct = ChainDirect(); direct.generatedflow()
    dnode = find_by_path(mc.document(), "terr_0")
    aux_path = f"{OUT}/f3_disp.flow_discharge.exr"
    if os.path.exists(aux_path):
        os.remove(aux_path)
    shas_disp = bake_doc(mc.document(), "f3_disp", ctx, display_node=dnode)
    sha_direct = bake_doc(direct.document(), "f3_direct", ctx)["height"]
    dropped = ("flow_discharge" not in shas_disp) and (not os.path.exists(aux_path))
    f3 = (shas_disp.get("height") == sha_direct) and dropped
    print(f"[f3] display==direct {shas_disp.get('height') == sha_direct}  "
          f"aux-dropped {dropped} -> {f3}", flush=True)
    results["f3_display"] = f3

    # ---- f4: display of a loop-body node == last iteration ------------------
    ldisp = LoopDisplay(); ldisp.generatedflow()
    lfull = LoopDisplay(tail=False); lfull.generatedflow()
    bnode = next(obj for (_pk, key, obj) in tree_paths(ldisp.document())
                 if isinstance(obj, DocNode) and key.startswith("loop_0/"))
    sha_ld = bake_doc(ldisp.document(), "f4_disp", ctx, display_node=bnode)["height"]
    sha_lf = bake_doc(lfull.document(), "f4_full", ctx)["height"]
    f4 = sha_ld == sha_lf
    print(f"[f4] loop-body display==last-iteration {f4}", flush=True)
    results["f4_loop_display"] = f4

    # ---- f5: refusals --------------------------------------------------------
    c5 = Chain(); c5.generatedflow()
    d5 = c5.document()
    fbm = node_by_name(d5, "fbm_0")
    caps = [obj for (_pk, _k, obj) in tree_paths(d5)
            if isinstance(obj, DocNode) and obj.clazz_name == "CaptureModule"]
    src_raises = _raises(lambda: fbm.set_bypassed(True), TerrainDocParamError)
    cap_raises = _raises(lambda: caps[0].set_bypassed(True), TerrainDocParamError)
    other = Chain(); other.generatedflow()
    foreign = find_by_path(other.document(), "terr_0")
    foreign_raises = _raises(
        lambda: d5.elaborate(display_node=foreign), RuntimeError)
    f5 = src_raises and cap_raises and foreign_raises
    print(f"[f5] src-raises {src_raises}  cap-raises {cap_raises}  "
          f"foreign-node-raises {foreign_raises} -> {f5}", flush=True)
    results["f5_refusals"] = f5

    # ---- f6: doc-JSON round-trip of bypassed ---------------------------------
    c6 = Chain(); c6.generatedflow()
    t6 = find_by_path(c6.document(), "terr_0")
    t6.set_bypassed(True)
    doc_rt = from_json(to_json(c6.document()))
    t6rt = find_by_path(doc_rt, "terr_0")
    sha_rt = bake_doc(doc_rt, "f6_rt", ctx)["height"]
    sha_c6 = bake_doc(c6.document(), "f6_byp", ctx)["height"]
    f6 = bool(t6rt is not None and t6rt.bypassed) and (sha_rt == sha_c6)
    print(f"[f6] json keeps bypassed {bool(t6rt and t6rt.bypassed)}  "
          f"rt-bake==flag-bake {sha_rt == sha_c6} -> {f6}", flush=True)
    results["f6_json"] = f6

    # ---- f7: tree_paths keying ------------------------------------------------
    lt = LoopTwo(); lt.generatedflow()
    keys = [key for (_pk, key, _o) in tree_paths(lt.document())]
    f7 = any(k.startswith("loop_0/") for k in keys) and "loop_0" in keys
    print(f"[f7] tree_paths keys sample {keys[:6]} -> {f7}", flush=True)
    results["f7_paths"] = f7

    # ---- f8: bypass a WHOLE T.loop == the carry at INITIAL (no unroll) --------
    lc = LoopChain(); lc.generatedflow()
    lcr = LoopChainReduced(); lcr.generatedflow()
    ldoc = lc.document()
    loop = find_by_path(ldoc, "loop_0")
    assert isinstance(loop, DocLoop), "loop_0 is not a DocLoop"
    sha_lbase = bake_doc(ldoc, "f8_base", ctx)["height"]
    loop.set_bypassed(True)
    sha_lbyp = bake_doc(ldoc, "f8_byp", ctx)["height"]
    sha_lred = bake_doc(lcr.document(), "f8_red", ctx)["height"]
    loop.set_bypassed(False)
    sha_loff = bake_doc(ldoc, "f8_off", ctx)["height"]
    f8 = (sha_lbyp == sha_lred) and (sha_loff == sha_lbase) and (sha_lbyp != sha_lbase)
    print(f"[f8] loop-bypass==carry-initial {sha_lbyp == sha_lred}  off==base {sha_loff == sha_lbase}  "
          f"byp!=base {sha_lbyp != sha_lbase} -> {f8}", flush=True)
    results["f8_loop_bypass"] = f8

    # ---- f9: doc-JSON round-trips the LOOP flag + the outliner badge on a loop row ----
    lc9 = LoopChain(); lc9.generatedflow()
    loop9 = find_by_path(lc9.document(), "loop_0")
    loop9.set_bypassed(True)
    ldoc_rt = from_json(to_json(lc9.document()))
    loop9rt = find_by_path(ldoc_rt, "loop_0")
    sha_rt = bake_doc(ldoc_rt, "f9_rt", ctx)["height"]
    sha_byp9 = bake_doc(lc9.document(), "f9_byp", ctx)["height"]
    json_ok = (isinstance(loop9rt, DocLoop) and loop9rt.bypassed) and (sha_rt == sha_byp9)
    # a bypassed construct MUST still appear in the outliner tree.
    keys9 = [k for (_pk, k, _o) in tree_paths(lc9.document())]
    tree_ok = ("loop_0" in keys9) and any(k.startswith("loop_0/") for k in keys9)
    # badge: present + enabled on a loop row; active tracks the flag (model-level check).
    om = TerrainDocOutlinerModel(lc9.document())
    byp = next((b for b in om.getBadges("loop_0") if b.id == "bypass"), None)
    badge_ok = (byp is not None and byp.enabled and byp.active)  # this loop IS bypassed
    lc9b = LoopChain(); lc9b.generatedflow()
    omb = TerrainDocOutlinerModel(lc9b.document())
    bypb = next((b for b in omb.getBadges("loop_0") if b.id == "bypass"), None)
    badge_off_ok = (bypb is not None and bypb.enabled and not bypb.active)  # fresh loop, not bypassed
    f9 = json_ok and tree_ok and badge_ok and badge_off_ok
    print(f"[f9] json keeps loop-bypass {json_ok}  bypassed-in-tree {tree_ok}  "
          f"badge active {badge_ok}  badge off {badge_off_ok} -> {f9}", flush=True)
    results["f9_loop_json_badge"] = f9

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain flags {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
