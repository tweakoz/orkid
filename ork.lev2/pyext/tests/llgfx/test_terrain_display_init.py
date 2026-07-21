#!/usr/bin/env ork.python
###############################################################################
# Terrain INITIAL-DISPLAY gate (#88 round-2 UX / #17 de-fang). On a terrain doc open
# with NO session display override, the canvas must NOT be flagless: the DISPLAY badge
# rides the EFFECTIVE terminal — the node whose output the default bake / viewport already
# shows, resolved deterministically (TerrainRuntime.effective_display_terminal) as the
# producer feeding the primary 'height' capture. Proves, on the REAL xxx3 asset:
#   g1  the resolver returns the height-capture producer (displayable, NOT a capture sink);
#   g2  EXACTLY one node is_output on open, and it IS that terminal (canvas not flagless);
#   g3  an explicit override moves the badge; clearing restores the implicit terminal badge;
#   g4  the resolution is DETERMINISTIC (repeat call + fresh load agree — the #17 tie-break);
#   g5  the terminal SURVIVES a doc-JSON round-trip (the badge is stable across a reload,
#       where the trace-recorded _out_names are gone).
# Pure doc/model (no GPU): the resolver + is_output are Python. The rendered field the badge
# advertises is covered by `ork.dflow.edit.py xxx3 --selftest` (non-black viewport).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2   # noqa

from ork.editor.terrain_runtime import TerrainRuntime
from ork.editor.terrain_node_model import TerrainNodeGraphModel, _Glue
from ork.hypergraph.dflow.terrain.doc import tree_paths, to_json, from_json


def _runtime_for(source_or_doc):
    rt = TerrainRuntime(preview_dim=256, chunk=128)
    if isinstance(source_or_doc, str):
        rt.load(source_or_doc)
    else:
        rt.document = source_or_doc
    return rt


def _obj_for_key(doc, key):
    for (_pk, k, o) in tree_paths(doc):
        if k == key:
            return o
    return None


def _height_producer(doc):
    cap = next(c for c in doc._captures if "height" in c.channels)
    return cap.node.connections[0][1].node        # the node feeding the 'height' capture


def main():
    results = {}

    # ---- g1 / g2 / g3: resolve + badge + override on the REAL xxx3 asset ----------
    rt = _runtime_for("xxx3")
    doc = rt.document
    producer = _height_producer(doc)
    term = rt.effective_display_terminal()
    term_obj = _obj_for_key(doc, term) if term else None
    g1 = bool(term is not None and term_obj is producer
              and rt._is_displayable(term_obj)
              and getattr(term_obj, "clazz_name", None) != "CaptureModule")
    print(f"[g1] terminal={term!r} clazz={getattr(term_obj,'clazz_name',None)} "
          f"== height-producer {term_obj is producer} displayable {rt._is_displayable(term_obj)} -> {g1}",
          flush=True)
    results["g1_resolve"] = g1

    model = TerrainNodeGraphModel(None, rt, glue=_Glue(None, rt))
    flagged = [nid for nid in model.nodes() if model.is_output(nid)]
    g2 = (flagged == [term]) and rt.display_key is None      # exactly one; default bake unchanged
    print(f"[g2] is_output nodes {flagged}  display_key {rt.display_key} -> {g2}", flush=True)
    results["g2_badge_on_open"] = g2

    other = next(nid for nid in model.nodes() if model.has_display_flag(nid) and nid != term)
    rt._display_key = other                                  # explicit session override
    ov = model.is_output(other) and not model.is_output(term)
    rt._display_key = None                                   # cleared -> implicit terminal badge back
    rt._eff_terminal_cache = None                            # (doc object stable; force a fresh resolve)
    rst = model.is_output(term) and not model.is_output(other)
    g3 = bool(ov and rst)
    print(f"[g3] override moves badge {ov}  clear restores terminal {rst} -> {g3}", flush=True)
    results["g3_override_restore"] = g3

    # ---- g4: DETERMINISTIC resolution (repeat call + fresh independent load agree) ----
    rt._eff_terminal_cache = None
    again = rt.effective_display_terminal()
    rt2 = _runtime_for("xxx3")
    fresh = rt2.effective_display_terminal()
    g4 = (again == term) and (fresh == term)
    print(f"[g4] repeat {again!r} fresh-load {fresh!r} == {term!r} -> {g4}", flush=True)
    results["g4_deterministic"] = g4

    # ---- g5: the terminal SURVIVES a doc-JSON round-trip (badge stable across reload) ----
    rt_rt = _runtime_for(from_json(to_json(doc)))
    term_rt = rt_rt.effective_display_terminal()
    producer_rt = _height_producer(rt_rt.document)
    term_rt_obj = _obj_for_key(rt_rt.document, term_rt) if term_rt else None
    g5 = bool(term_rt is not None and term_rt_obj is producer_rt and term_rt == term)
    print(f"[g5] round-trip terminal {term_rt!r} == {term!r} and == its height-producer "
          f"{term_rt_obj is producer_rt} -> {g5}", flush=True)
    results["g5_json_roundtrip"] = g5

    ok = all(results.values())
    print(f"\n=== terrain initial-display {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    print(f"TERRAIN_INITDISP_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
