#!/usr/bin/env python3
###############################################################################
# Terrain E0 DOCUMENT-PARAMETERS gate (jul13). Proves DSL ctor kwargs are now
# first-class DOCUMENT DATA captured via _ParamExpr, so editing a "Terrain
# Parameter" mutates the document + re-elaborates (no re-trace) — the P0
# topology-loss defect is fixed at its real level, not merely guarded.
#
#   a  pristine CAPTURED edit -> NO re-trace (SAME doc object mutated) + plug re-evaluates.
#   b  THE P0 PROOF: add_op_node + a node set_param edit, THEN a captured kwarg edit ->
#      SUCCEEDS and every edit is preserved (the added node + the param edit survive).
#   c  arithmetic capture (sim_time * 0.5) re-evaluates correctly on the param change.
#   d  STRUCTURAL params: raw range(passes) + a warp ptex3d-closure asset flag structural;
#      editing them RAISES on an edited document and RE-TRACES on a pristine one.
#   e  doc-JSON round-trips the params table + the per-node param exprs.
#   f  undo across a captured param edit restores the prior value + document.
#   g  pywriter round-trip: an edited captured param becomes the new ctor default, an
#      editor-added node survives, and re-import (editor path) rebakes BYTE-IDENTICAL.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, TerrainDocParamError, tree_paths, find_by_path, add_op_node, to_json,
    from_json, param_expr_string, _eval_param_node)
from ork.hypergraph.dflow.terrain.pywriter import to_python, class_name_from_stem
from ork.hypergraph.dflow.terrain.resolve import load_dsl_class
from ork.editor.terrain_runtime import TerrainRuntime


# ---- fixtures ---------------------------------------------------------------

class ArithParam(HeightField):
    """A captured ARITHMETIC kwarg: step_m = sim_time * 0.5 (Ex.3)."""
    EXTENT_M = 4096.0

    def __init__(self, sim_time=250.0, amp_m=2000.0):
        super().__init__()
        h = (T.Fbm(frequency=8.0, octaves=3) * 0.5 + 0.5) * amp_m
        h = T.terrace(h, step_m=sim_time * 0.5)
        self.capture(h, "height", cache=True)
        self.capture(h, "normal", cache=True)


class RawRangeParam(HeightField):
    """A kwarg forced through raw Python control flow (range) -> STRUCTURAL (Ex.4 Case B)."""
    EXTENT_M = 4096.0

    def __init__(self, passes=3, amp_m=2000.0):
        super().__init__()
        h = (T.Fbm(frequency=6.0, octaves=4) * 0.5 + 0.5) * amp_m
        for _ in range(passes):                     # range(passes) forces __index__ -> structural
            h = T.lpf(h, cutoff=8.0, units='meters')
        self.capture(h, "height", cache=True)
        self.capture(h, "normal", cache=True)


# ---- helpers ----------------------------------------------------------------

OUT = "/tmp/tered_docparams"
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


def bake_height_sha(doc, extent, dim, tag, ctx):
    g, _cap = doc.elaborate()
    for cap in lev2.terrain.capture_modules(g):
        chans = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()]
        # path EVERY capture (not just height): an unpathed capture flushes its
        # ".cookhash" currency sidecar into the CWD (repo-root dropping, JUL13 filed).
        cap.path = f"{OUT}/{tag}.{chans[0]}.exr"
    lev2.terrain.bake_heightfield(g, ctx, dim, extent_m=extent)
    return masked_sha(f"{OUT}/{tag}.height.exr")


def first_op(doc):
    for (_pk, _k, obj) in tree_paths(doc):
        if isinstance(obj, DocNode) and obj.clazz_name != "CaptureModule":
            return obj
    return None


def key_of(doc, node):
    for (_pk, k, obj) in tree_paths(doc):
        if obj is node:
            return k
    return None


def effective(node, kind, name):
    v = None
    for (k, n, val) in node.param_actions:
        if k == kind and n == name:
            v = val
    return v


def load_fixture(cls, kwargs=None):
    """Editor-path load of a fixture HeightField (mirrors TerrainRuntime DSL load)."""
    rt = TerrainRuntime()
    rt.source_label = cls.__name__
    rt.extent_m = float(cls.EXTENT_M)
    rt._capture_dsl_kwargs(cls, kwargs or {})
    rt.document = rt._trace_param_document(rt._dsl_kwargs)
    return rt


def raises(fn, exc):
    try:
        fn()
        return False
    except exc:
        return True


# ---- main -------------------------------------------------------------------

def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    # ---- a: pristine captured edit mutates the SAME document (no re-trace) -----
    rt = TerrainRuntime()
    rt.load("voronoi")
    doc_obj = rt.document
    before = effective(first_op(rt.document), "inputs", "amplitude")
    rt.set_dsl_kwarg("amplitude", 2600.0)              # voronoi's meters(2000) typed literal
    same = rt.document is doc_obj                       # mutated, NOT replaced
    after = effective(first_op(rt.document), "inputs", "amplitude")
    plug_updated = (after == 2600.0 and after != before)
    kw_ok = rt.editable_dsl_kwargs()["amplitude"] == 2600.0
    a = same and plug_updated and kw_ok
    print(f"[a] same-doc-object={same} plug-reevaluated={plug_updated} kwarg={kw_ok} -> {a}",
          flush=True)
    results["a_pristine_no_retrace"] = a

    # ---- b: THE P0 PROOF — add node + param edit, THEN captured kwarg edit ------
    rt2 = TerrainRuntime()
    rt2.load("voronoi")
    added = add_op_node(rt2.document, "terrace", first_op(rt2.document))
    akey = key_of(rt2.document, added)
    added.set_param("inputs", "step_m", 137.0)         # an editor node-param edit
    doc_obj2 = rt2.document
    rt2.set_dsl_kwarg("frequency", 12.0)               # captured kwarg edit
    node_kept = find_by_path(rt2.document, akey) is not None
    param_kept = effective(find_by_path(rt2.document, akey), "inputs", "step_m") == 137.0
    same2 = rt2.document is doc_obj2
    freq_applied = effective(first_op(rt2.document), "inputs", "frequency") == 12.0
    b = node_kept and param_kept and same2 and freq_applied
    print(f"[b] added-node-kept={node_kept} node-param-kept={param_kept} "
          f"same-doc={same2} kwarg-applied={freq_applied} -> {b}", flush=True)
    results["b_p0_topology_preserved"] = b

    # ---- c: arithmetic capture re-evaluates ------------------------------------
    rtc = load_fixture(ArithParam)
    terr = next(o for (_pk, _k, o) in tree_paths(rtc.document)
                if isinstance(o, DocNode) and o.clazz_name == "TerraceModule")
    expr = terr.param_exprs[("inputs", "step_m")]
    src_ok = param_expr_string(expr) == "(sim_time * 0.5)"
    v0 = float(_eval_param_node(expr, rtc.document.params))
    rtc.set_dsl_kwarg("sim_time", 100.0)
    v1 = float(_eval_param_node(terr.param_exprs[("inputs", "step_m")], rtc.document.params))
    c = src_ok and v0 == 125.0 and v1 == 50.0 and effective(terr, "inputs", "step_m") == 50.0
    print(f"[c] expr='{param_expr_string(expr)}' v0={v0} v1={v1} -> {c}", flush=True)
    results["c_arithmetic_capture"] = c

    # ---- d: structural params (raw range + warp closure) guard correctly -------
    rtd = load_fixture(RawRangeParam)
    passes_struct = rtd.document.params.is_structural("passes")
    amp_captured = not rtd.document.params.is_structural("amp_m")
    # pristine structural edit re-traces (succeeds); dirty structural edit refuses.
    retraced = True
    try:
        rtd.set_dsl_kwarg("passes", 5)
    except TerrainDocParamError:
        retraced = False
    rtd2 = load_fixture(RawRangeParam)
    add_op_node(rtd2.document, "terrace", first_op(rtd2.document))
    dirty_refused = raises(lambda: rtd2.set_dsl_kwarg("passes", 5), TerrainDocParamError)
    # warp: every numeric kwarg captured inside a ptex3d hfbake closure is STRUCTURAL.
    rtw = TerrainRuntime()
    rtw.load("warp")
    warp_struct = all(rtw.document.params.is_structural(n)
                      for n in ("frequency", "octaves", "ring_amp_m", "ring_period_m"))
    d = (passes_struct and amp_captured and retraced and dirty_refused and warp_struct)
    print(f"[d] range-passes-structural={passes_struct} amp-captured={amp_captured} "
          f"pristine-retrace={retraced} dirty-refused={dirty_refused} "
          f"warp-closures-structural={warp_struct} -> {d}", flush=True)
    results["d_structural_guarded"] = d

    # ---- e: doc-JSON round-trips params + param exprs ---------------------------
    rte = TerrainRuntime()
    rte.load("voronoi")
    rte.set_dsl_kwarg("amplitude", 3100.0)
    j = to_json(rte.document)
    doc_r = from_json(j)
    json_eq = to_json(doc_r) == j
    params_eq = (doc_r.params.names() == rte.document.params.names()
                 and doc_r.params.get("amplitude") == 3100.0
                 and doc_r.params.is_structural("octaves"))
    # the reloaded doc's captured expr re-evaluates against the restored table
    fbm_r = first_op(doc_r)
    expr_r = fbm_r.param_exprs.get(("inputs", "amplitude"))
    expr_eq = expr_r is not None and float(_eval_param_node(expr_r, doc_r.params)) == 3100.0
    e = json_eq and params_eq and expr_eq
    print(f"[e] json-eq={json_eq} params-eq={params_eq} expr-reeval={expr_eq} -> {e}", flush=True)
    results["e_docjson_roundtrip"] = e

    # ---- f: undo across a captured param edit -----------------------------------
    rtf = TerrainRuntime()
    rtf.load("voronoi")
    s0 = rtf.capture_undo_state()
    v_before = rtf.editable_dsl_kwargs()["amplitude"]
    rtf.set_dsl_kwarg("amplitude", 2750.0)
    changed = rtf.editable_dsl_kwargs()["amplitude"] == 2750.0
    rtf.restore_undo_state(s0)
    restored = (rtf.editable_dsl_kwargs()["amplitude"] == v_before
                and rtf.document.params.get("amplitude") == v_before
                and to_json(rtf.document) == s0["doc"])
    f = changed and restored
    print(f"[f] changed={changed} restored={restored} -> {f}", flush=True)
    results["f_undo_param_edit"] = f

    # ---- g: pywriter round-trip -> byte-identical rebake ------------------------
    rtg = TerrainRuntime()
    rtg.load("new")
    add_op_node(rtg.document, "terrace", find_by_path(rtg.document, "fbm_0"))
    rtg.set_dsl_kwarg("frequency", 9.0)                 # captured edit -> becomes ctor default
    sha_src = bake_height_sha(rtg.document, rtg.extent_m, 256, "g_src", ctx)
    src = to_python(rtg.document, class_name=class_name_from_stem("g_docparams"),
                    extent_m=rtg.extent_m)
    default_ok = "frequency=9.0" in src and "frequency=frequency" in src
    srcdir = f"{OUT}/src"
    os.makedirs(srcdir, exist_ok=True)
    p = f"{srcdir}/g_docparams.py"
    with open(p, "w") as fh:
        fh.write(src)
    cls_r = load_dsl_class(p)
    rtr = load_fixture(cls_r)                            # EDITOR-path re-import
    node_survived = any(o.clazz_name == "TerraceModule"
                        for (_pk, _k, o) in tree_paths(rtr.document)
                        if isinstance(o, DocNode))
    params_reimport = rtr.document.params.get("frequency") == 9.0
    sha_reload = bake_height_sha(rtr.document, float(cls_r.EXTENT_M), 256, "g_reload", ctx)
    byte_identical = sha_src == sha_reload
    g = default_ok and node_survived and params_reimport and byte_identical
    print(f"[g] default-emitted={default_ok} node-survived={node_survived} "
          f"params-reimport={params_reimport} byte-identical={byte_identical} -> {g}", flush=True)
    results["g_pywriter_roundtrip"] = g

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain doc-params {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
