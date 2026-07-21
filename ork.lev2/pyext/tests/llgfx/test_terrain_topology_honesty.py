#!/usr/bin/env python3
###############################################################################
# Terrain node-editor TOPOLOGY-HONESTY oracle (task #24). The canvas adapter must
# render the TRUE document topology regardless of how the document was built (fresh
# trace, editor mutation, or a doc-JSON reload) — at least with all filters disabled.
# This gate pins the four honesty violations the display/bypass audit found:
#
#   V1 scatter sinks   — a mask-driven placement set (base.scatter) is a REAL bake sink;
#                        it must appear as a node with an input edge from each weight
#                        producer (before: only nameless weight captures, no sink).
#   V2 expression rows — a plug driven by a document parameter (_ParamExpr) or L.i must
#                        get a (read-only) propsheet row, never be silently dropped.
#   V3 group pill edges — an edge that arrives via a @T.group's input pill must render
#                        (before: dropped — trace-built and reloaded interiors diverged).
#   V4 declared outputs — EVERY declared output plug of a node must be listed regardless
#                        of usage (before: only the currently-consumed outputs).
#
# The core oracle: for each asset the adapter's rendered edge set (unioned over every
# navigable level, all filters off) is EXACTLY the document's edge set derived from
# doc-side slot iteration (DocNode.connections, loop-carry initial/body_out, group arg +
# output pills, switch branches). Plus: every declared output present; a trace-built vs
# doc-JSON-rebuilt asset renders the IDENTICAL edge set.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json, importlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.base import HeightField as _HFBase
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, DocGroupCall, DocSwitch, _DocOutPlug,
    tree_paths, to_json, from_json, loop_external_refs)
from ork.editor.terrain_node_model import TerrainNodeGraphModel, _Glue
from ork.editor.terrain_doc_model import TerrainNodePropertyModel
from ork.editor.terrain_runtime import TerrainRuntime

dflow = core.dataflow

# the three named real assets (loop + flow multi-output graphs) + inline fixtures that
# exercise the group/scatter/param paths those three do not.
LOOP_ASSETS = ("xxx", "xxx3", "erox")


# ---- inline fixtures (group / scatter / param — not covered by the loop assets) -----

@T.group
def _ridge(h, gain=1.0):
  return h + T.Fbm(frequency=8.0, octaves=4) * gain


class GroupHF(HeightField):
  """Two independent @T.group expansions — exercises the group input-pill edge (V3)."""
  def __init__(self):
    super().__init__()
    h = T.Fbm(frequency=3.0, octaves=5) * 0.5 + 0.5
    h = _ridge(h, gain=0.2)
    h = _ridge(h, gain=0.4)
    self.capture(h, "height")


class ScatterHF(HeightField):
  """A 2-type scatter sink ('buildings') — exercises the invisible-sink violation (V1)."""
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=3.0, octaves=4) * 0.5 + 0.5) * 40.0
    self.capture(h, "height")
    alt = T.normalize(h)
    self.scatter("buildings", density=0.02, seed=11, align="normal",
                 cutoff=0.05, jitter=0.9, lift=0.25,
                 types={"low": 1.0 - alt, "high": alt})


class ParamHF(HeightField):
  """A plug driven by a document parameter (amp_m) + an arithmetic one (sim_time*0.5) —
  exercises the dropped expression-row violation (V2) via the E0 runtime trace path."""
  EXTENT_M = 4096.0
  def __init__(self, sim_time=250.0, amp_m=2000.0):
    super().__init__()
    h = (T.Fbm(frequency=8.0, octaves=3) * 0.5 + 0.5) * amp_m
    h = T.terrace(h, step_m=sim_time * 0.5)
    self.capture(h, "height", cache=True)


# ---- host/runtime shims (the connect-gate pattern) --------------------------

class _Host:
  def _recordEdit(self, *a, **k): pass
  def _requestRebake(self, *a, **k): pass


class _RT:
  def __init__(self, doc):
    self.document = doc
    self.display_key = None
  _is_displayable = staticmethod(TerrainRuntime._is_displayable)


def _mk_model(doc):
  host = _Host(); rt = _RT(doc)
  return TerrainNodeGraphModel(host, rt, glue=_Glue(host, rt))


def _load_asset(name):
  """Instantiate an asset HeightField via the standard trace path (its __init__ records
  the document); close the trace (no elaborate — the canvas renders the document only)."""
  mod = importlib.import_module("ork.hypergraph.assets.terrain.%s" % name)
  cls = next(v for v in vars(mod).values()
             if isinstance(v, type) and issubclass(v, _HFBase) and v is not _HFBase)
  hf = cls()
  hf.close_trace()
  return hf.document()


# ---- canonical edge extractors ----------------------------------------------

def _iter_level_models(root):
  """Every navigable level model: the root + every group/loop interior (recursive)."""
  seen, stack = set(), [root]
  while stack:
    m = stack.pop()
    if id(m) in seen:
      continue
    seen.add(id(m))
    yield m
    for nid in m.nodes():
      if m.is_group(nid):
        cm = m.child_model(nid)
        if cm is not None:
          stack.append(cm)


def doc_edges(doc):
  """The document's edge set from doc-side slot iteration, as canonical
  (producer_obj_id, producer_plug, consumer_obj_id, consumer_port). Consumer identity
  mirrors how the adapter names each pill's owner so the two sets compare directly."""
  out = set()

  def walk(children):
    for ch in children:
      if isinstance(ch, DocNode):
        for (inp, ref) in ch.connections:
          if ref is not None:
            out.add((id(ref.node), ref.plug_name, id(ch), inp))
      elif isinstance(ch, DocLoop):
        for name, c in ch.carries.items():
          if c.initial_ref is not None:
            out.add((id(c.initial_ref.node), c.initial_ref.plug_name, id(ch), name))
          if c.body_out_ref is not None:
            out.add((id(c.body_out_ref.node), c.body_out_ref.plug_name, id(c), "in"))
        # loop-invariant external feeds are non-carry loop inputs (the shared doc helper) —
        # rendered as a parent-level edge into the loop node, so count them here too.
        for (port, ref) in loop_external_refs(doc, ch):
          out.add((id(ref.node), ref.plug_name, id(ch), port))
        walk(ch.children)
      elif isinstance(ch, DocGroupCall):
        for an, v in ch.args.items():
          if isinstance(v, _DocOutPlug):
            out.add((id(v.node), v.plug_name, id(ch), an))
        if ch.output_ref is not None:
          out.add((id(ch.output_ref.node), ch.output_ref.plug_name, id(ch.output_ref), "in"))
        walk(ch.children)
      elif isinstance(ch, DocSwitch):
        for bn, r in ch.branches.items():
          if r is not None:
            out.add((id(r.node), r.plug_name, id(ch), bn))
  walk(doc._root)
  return out


def adapter_edges(root):
  """The adapter's rendered edge set (unioned over every level), same canonical form.
  Synthetic scatter-sink edges are additive display (not doc-modeled) — skipped here so
  the equality oracle stays over the doc-native topology; V1 is checked separately."""
  out = set()
  for m in _iter_level_models(root):
    lvl = m._lvl()
    for (snid, sport, dnid, dport) in lvl["edges"]:
      if lvl["kind"].get(dnid) == "scatter":
        continue
      pobj, pplug = lvl["producer"][snid][sport]
      cobj = lvl["obj"][dnid]
      out.add((id(pobj), pplug, id(cobj), dport))
  return out


def adapter_raw_edges(root):
  """Union of the adapter's stable-key edges (nids are tree-path/pill/scatter keys) over
  every level — comparable ACROSS documents (trace-built vs doc-JSON-rebuilt)."""
  out = set()
  for m in _iter_level_models(root):
    for e in m.edges():
      out.add(tuple(e))
  return out


def weakly_connected_components(nids, edges):
  """Count of connected components treating the rendered edges as UNDIRECTED (the
  owner-visible 'the canvas shows N disconnected islands' symptom)."""
  parent = {n: n for n in nids}

  def find(a):
    while parent[a] != a:
      parent[a] = parent[parent[a]]
      a = parent[a]
    return a

  def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb:
      parent[ra] = rb
  for (s, _sp, d, _dp) in edges:
    if s in parent and d in parent:
      union(s, d)
  return len({find(n) for n in nids})


def missing_declared_outputs(root):
  """Any declared output plug (reflection) of any node NOT listed by the adapter."""
  missing = []
  for m in _iter_level_models(root):
    lvl = m._lvl()
    for nid in m.nodes():
      if lvl["kind"].get(nid) != "node":
        continue
      node = lvl["obj"][nid]
      spec = dflow.plugSpec("terrain::%sData" % node.clazz_name)
      declared = [p["name"] for p in spec["outputs"]] if spec else []
      have = {p for (p, _t) in m.outputs(nid)}
      for d in declared:
        if d not in have:
          missing.append((nid, node.clazz_name, d, sorted(have)))
  return missing


# ---- checks -----------------------------------------------------------------

def check_edge_parity(label, doc):
  d = doc_edges(doc)
  a = adapter_edges(_mk_model(doc))
  ok = (a == d)
  if not ok:
    print(f"  [{label}] EDGE MISMATCH  doc-only={sorted(d - a)[:4]}  "
          f"adapter-only={sorted(a - d)[:4]}", flush=True)
  print(f"  [{label}] edge parity: doc={len(d)} adapter={len(a)} -> {ok}", flush=True)
  return ok


def check_declared_outputs(label, doc):
  miss = missing_declared_outputs(_mk_model(doc))
  ok = not miss
  if not ok:
    print(f"  [{label}] MISSING declared outputs: {miss[:4]}", flush=True)
  print(f"  [{label}] declared-outputs present -> {ok}", flush=True)
  return ok


def run(ez, ctx):
  results = {}

  # ---- the three named loop/flow assets + a group fixture: edge parity + outputs ----
  edge_ok = True
  out_ok = True
  for name in LOOP_ASSETS:
    doc = _load_asset(name)
    edge_ok &= check_edge_parity(name, doc)
    out_ok &= check_declared_outputs(name, doc)
  gdoc = GroupHF(); gdoc.close_trace(); gdoc = gdoc.document()
  edge_ok &= check_edge_parity("GroupHF", gdoc)      # V3: group pill_in edge must render
  out_ok &= check_declared_outputs("GroupHF", gdoc)
  results["edge_parity"] = edge_ok
  results["declared_outputs"] = out_ok

  # ---- V4 sharpness: xxx's in-loop flow3d exposes Out+Discharge+Metrics --------------
  xdoc = _load_asset("xxx")
  xm = _mk_model(xdoc)
  flow_outs = None
  for m in _iter_level_models(xm):
    lvl = m._lvl()
    for nid in m.nodes():
      if lvl["kind"].get(nid) == "node" and lvl["obj"][nid].clazz_name == "Flow3DModule":
        flow_outs = {p for (p, _t) in m.outputs(nid)}
  v4 = flow_outs is not None and {"Out", "Discharge", "Metrics"} <= flow_outs
  print(f"  [V4] xxx flow3d outputs={sorted(flow_outs) if flow_outs else None} -> {v4}", flush=True)
  results["v4_all_outputs_listed"] = v4

  # ---- OWNER live repro: opening xxx must render ONE connected graph at root ----------
  # (before: two islands — {const_0, loop_0, basin/lpf, captures} and the {fbm->remap} chain,
  # because the loop reads the remap chain as a loop-invariant external whose edge into loop_0
  # was dropped at the parent level). Assert (a) an edge from the remap chain into loop_0 and
  # (b) a single weakly-connected component (the directly-visible symptom).
  root = _mk_model(xdoc)
  root_nids = list(root.nodes())
  root_edges = list(root.edges())
  loop_key = next((n for n in root_nids if root._lvl()["kind"].get(n) == "loop"), None)
  into_loop = [(s, sp, d, dp) for (s, sp, d, dp) in root_edges if d == loop_key]
  remap_into_loop = [e for e in into_loop
                     if root._lvl()["kind"].get(e[0]) == "node"
                     and root._lvl()["obj"][e[0]].clazz_name == "RemapModule"]
  ncomp = weakly_connected_components(root_nids, root_edges)
  v_conn = (loop_key is not None) and bool(remap_into_loop) and (ncomp == 1)
  print(f"  [xxx-root] into-loop edges={[(s, d) for (s, _p, d, _q) in into_loop]}", flush=True)
  print(f"  [xxx-root] remap->loop present={bool(remap_into_loop)} components={ncomp} "
        f"(want 1) -> {v_conn}", flush=True)
  results["xxx_single_component"] = v_conn

  # ---- V3 sharpness: the group input pill actually drives an interior node -----------
  gm = _mk_model(gdoc)
  pill_edge = False
  for m in _iter_level_models(gm):
    for (snid, _sp, _dn, _dp) in m.edges():
      if m._lvl()["kind"].get(snid) == "pill_in":
        pill_edge = True
  print(f"  [V3] a group pill_in drives an interior edge -> {pill_edge}", flush=True)
  results["v3_group_pill_edge"] = pill_edge

  # ---- V1: the scatter sink is a real node with an input edge from each weight -------
  sdoc = ScatterHF(); sdoc.close_trace(); sdoc = sdoc.document()
  sm = _mk_model(sdoc)
  scatter_nids = [n for n in sm.nodes() if sm._lvl()["kind"].get(n) == "scatter"]
  scatter_in_edges = [e for e in sm.edges()
                      if sm._lvl()["kind"].get(e[2]) == "scatter"]
  has_sink = (len(scatter_nids) == 1
              and sm.name(scatter_nids[0]) == "scatter: buildings"
              and len(sm.inputs(scatter_nids[0])) == 2      # two types
              and len(scatter_in_edges) == 2)               # an input edge per weight
  # survives a doc-JSON reload (weight captures are doc-backed): the sink reappears.
  sdoc2 = from_json(json.loads(json.dumps(to_json(sdoc))))
  sm2 = _mk_model(sdoc2)
  reload_sink = sum(1 for n in sm2.nodes() if sm2._lvl()["kind"].get(n) == "scatter") == 1
  v1 = has_sink and reload_sink
  print(f"  [V1] scatter sink node+edges={has_sink} survives-reload={reload_sink} -> {v1}", flush=True)
  results["v1_scatter_sink_visible"] = v1

  # ---- V2: an expression-driven plug gets a (read-only) propsheet row ----------------
  rt = TerrainRuntime()
  rt.source_label = "ParamHF"
  rt.extent_m = float(ParamHF.EXTENT_M)
  rt._capture_dsl_kwargs(ParamHF, {})
  rt.document = rt._trace_param_document(rt._dsl_kwargs)
  expr_nodes = [o for (_pk, _k, o) in tree_paths(rt.document)
                if isinstance(o, DocNode) and o.param_exprs]
  v2 = bool(expr_nodes)
  for node in expr_nodes:
    labels = {p.label for p in TerrainNodePropertyModel(node)._props}
    for (_kind, name) in node.param_exprs:
      shown = any(name in s for s in labels)
      v2 &= shown
      if not shown:
        print(f"  [V2] MISSING row for expr param {node.clazz_name}.{name}", flush=True)
  print(f"  [V2] every expression-driven plug has a propsheet row -> {v2}", flush=True)
  results["v2_expression_rows"] = v2

  # ---- trace-built vs doc-JSON-rebuilt render the IDENTICAL edge set ------------------
  # erox (loop carry) + xxx (loop carry AND a loop-invariant external pill) — the pill keys
  # are STABLE (tree-path derived), so a reload must reproduce the exact same rendered edges.
  equiv = True
  for name in ("erox", "xxx"):
    t_doc = _load_asset(name)
    r_doc = from_json(json.loads(json.dumps(to_json(t_doc))))
    t_edges = adapter_raw_edges(_mk_model(t_doc))
    r_edges = adapter_raw_edges(_mk_model(r_doc))
    this = (t_edges == r_edges) and len(t_edges) > 0
    equiv &= this
    if not this:
      print(f"  [equiv] {name} trace-only={sorted(t_edges - r_edges)[:3]} "
            f"reload-only={sorted(r_edges - t_edges)[:3]}", flush=True)
    print(f"  [equiv] {name} trace==reload edge set ({len(t_edges)} edges) -> {this}", flush=True)
  results["trace_vs_rebuilt_equiv"] = equiv

  return results


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    results = run(ez, ctx)
    ok = bool(results) and all(results.values())
  except Exception:
    import traceback; traceback.print_exc()
    results = {}
  finally:
    ez.mainThreadEnd()
    print(f"\n=== terrain topology-honesty {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
