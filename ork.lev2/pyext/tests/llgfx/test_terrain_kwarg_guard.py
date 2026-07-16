#!/usr/bin/env python3
###############################################################################
# Terrain E0 STRUCTURAL-PARAM guard gate (jul13). After E0, editing a CAPTURED
# Terrain Parameter (used only in plug/prop VALUES) is a document mutation that
# preserves topology edits (proven in test_terrain_doc_params). The interim guard
# now protects ONLY the STRUCTURAL path: a param the trace must concretize
# outside a claim site (a loop count, octaves-via-int, raw range(), or a ptex3d
# shader closure) can only change by RE-TRACING the DSL class — which would
# silently discard editor document edits. set_dsl_kwarg refuses that edit unless
# the document is PRISTINE (a fresh re-trace with the unchanged kwargs reproduces
# it byte-for-byte in doc-JSON). MinimalTerrain's `octaves` (Fbm's int-baked
# loop bound) is the structural exemplar. Cases:
#   g1  pristine doc -> structural edit SUCCEEDS (safe re-trace) and the doc reflects it.
#   g2  after add_op_node -> structural edit RAISES and the document (incl. the added
#       node) is UNCHANGED (never replaced).
#   g3  after a node set_param edit -> structural edit RAISES; the param edit is PRESERVED.
#   g4  after undoing back to pristine -> structural edit SUCCEEDS AGAIN (the stateless
#       doc-JSON compare gives this for free — no mutation flag).
# Pure document logic (no bake) — the guard runs on the calling thread.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain.doc import (
    DocNode, TerrainDocParamError, tree_paths, find_by_path, add_op_node, to_json)
from ork.editor.terrain_runtime import TerrainRuntime


# ---- helpers ----------------------------------------------------------------

def _first_op(doc):
    """The first non-capture DocNode (MinimalTerrain: the fbm generator)."""
    for (_pk, _k, obj) in tree_paths(doc):
        if isinstance(obj, DocNode) and obj.clazz_name != "CaptureModule":
            return obj
    return None


def _key_of(doc, node):
    for (_pk, k, obj) in tree_paths(doc):
        if obj is node:
            return k
    return None


def _effective(node, kind, name):
    """Last-write-wins effective value of a recorded param (matches editable_params)."""
    for (k, n, v) in reversed(node.param_actions):
        if k == kind and n == name:
            return v
    return None


def _raises(fn, exc):
    try:
        fn()
        return False
    except exc:
        return True


# ---- main -------------------------------------------------------------------

def run(ez, ctx):
    results = {}

    # ---- g1: pristine doc -> STRUCTURAL edit succeeds (re-trace), doc reflects it
    rt = TerrainRuntime()
    rt.load("new")
    assert rt.document.params.is_structural("octaves"), "octaves must be structural (int-baked)"
    kw0 = int(rt.editable_dsl_kwargs()["octaves"])
    before = to_json(rt.document)
    newv = kw0 + 2
    ret = rt.set_dsl_kwarg("octaves", newv)
    after = to_json(rt.document)
    kw_ok = rt.editable_dsl_kwargs()["octaves"] == newv
    doc_reflects = (after != before)              # octaves propagated into the fbm baked prop
    g1 = kw_ok and doc_reflects and ret == newv
    print(f"[g1] kwarg-updated {kw_ok}  doc-reflects-edit {doc_reflects}  "
          f"returns-coerced {ret == newv} -> {g1}", flush=True)
    results["g1_pristine_edit_succeeds"] = g1

    # ---- g2: after add_op_node -> RAISES, document (incl. added node) unchanged
    rt2 = TerrainRuntime()
    rt2.load("new")
    added = add_op_node(rt2.document, "terrace", _first_op(rt2.document))
    added_key = _key_of(rt2.document, added)
    post_add = to_json(rt2.document)
    raised2 = _raises(
        lambda: rt2.set_dsl_kwarg(
            "octaves", int(rt2.editable_dsl_kwargs()["octaves"]) + 1),
        TerrainDocParamError)
    doc_intact = (to_json(rt2.document) == post_add
                  and find_by_path(rt2.document, added_key) is not None)
    g2 = raised2 and doc_intact
    print(f"[g2] add-then-structural raised {raised2}  doc-intact(added-node-kept) "
          f"{doc_intact} -> {g2}", flush=True)
    results["g2_add_then_kwarg_raises"] = g2

    # ---- g3: after a node set_param edit -> RAISES, param edit preserved ------
    rt3 = TerrainRuntime()
    rt3.load("new")
    fbm = _first_op(rt3.document)
    (pk, pn, pv) = next((k, n, v) for (k, n, v) in fbm.editable_params()
                        if isinstance(v, float) and not isinstance(v, bool))
    editval = float(pv) + 5.0
    fbm.set_param(pk, pn, editval)
    post_edit = to_json(rt3.document)
    raised3 = _raises(
        lambda: rt3.set_dsl_kwarg(
            "octaves", int(rt3.editable_dsl_kwargs()["octaves"]) + 1),
        TerrainDocParamError)
    param_preserved = (to_json(rt3.document) == post_edit
                       and _effective(_first_op(rt3.document), pk, pn) == editval)
    g3 = raised3 and param_preserved
    print(f"[g3] param-edit-then-structural raised {raised3}  param-edit-preserved "
          f"{param_preserved} -> {g3}", flush=True)
    results["g3_param_edit_then_kwarg_raises"] = g3

    # ---- g4: undo back to pristine -> STRUCTURAL edit SUCCEEDS again ----------
    rt4 = TerrainRuntime()
    rt4.load("new")
    s0 = rt4.capture_undo_state()                 # pristine checkpoint
    add_op_node(rt4.document, "terrace", _first_op(rt4.document))
    raised_dirty = _raises(
        lambda: rt4.set_dsl_kwarg(
            "octaves", int(rt4.editable_dsl_kwargs()["octaves"]) + 1),
        TerrainDocParamError)
    rt4.restore_undo_state(s0)                     # L2 — document restored from doc-JSON
    oct0 = int(rt4.editable_dsl_kwargs()["octaves"])
    succeeded_after_undo = True
    try:
        rt4.set_dsl_kwarg("octaves", oct0 + 3)
    except TerrainDocParamError as ex:
        print(f"[g4] post-undo edit unexpectedly refused: {ex}", flush=True)
        succeeded_after_undo = False
    val_ok = rt4.editable_dsl_kwargs()["octaves"] == oct0 + 3
    g4 = raised_dirty and succeeded_after_undo and val_ok
    print(f"[g4] dirty-raised {raised_dirty}  succeeds-after-undo "
          f"{succeeded_after_undo}  value-applied {val_ok} -> {g4}", flush=True)
    results["g4_undo_restores_pristine"] = g4

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain kwarg-guard {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
