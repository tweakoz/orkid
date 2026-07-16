#!/usr/bin/env ork.python
###############################################################################
# E2 GraphDocument gate (fast, headless).
#
# Proves the base graph-document surface + its two specializations (JUL13_DFLOW E2):
#
#   (a) GraphDataDocument over a small LIVE PARTICLE graph (the doc-less-family
#       default): enumerate nodes + typed edges, set a plug param and read it back,
#       set node positions and round-trip them through serializeJson/deserialize,
#       add + connect a module with typed-connect VALIDATION, and take a LOUD,
#       CATCHABLE failure on an invalid (type-mismatched) connect.
#
#   (b) base-class contract: a real TerrainDoc IS a GraphDocument and the shared
#       editor-surface methods dispatch on it (tree_paths / to_json / find_by_path /
#       editable_params / node positions / undo checkpoint / elaborate), plus the
#       hoisted ParamTable + error hierarchy re-export intact.
#
#   (c) FIRST cross-family editor-MODEL proof (E2 slice 2): the GENERIC editor models
#       (GraphDocumentOutlinerModel + GraphDocumentPropertyModel) drive a GraphDataDocument
#       over the same particle graph HEADLESSLY — enumerate outliner rows, read bypass/output
#       badges (active flag tracks the document bypass state), select a node, list its editable
#       params, set one THROUGH the property model, and verify the plug value changed. The
#       canvas will bind these exact models over any family.
#
# The doc-JSON byte-compat oracle and the elaborated-graph byte-identity oracle run
# out of band (scratchpad) — they gate the terrain refactor's structural identity.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

dflow = core.dataflow


def _check(results, name, cond):
  results[name] = bool(cond)
  print(f"  [graphdoc] {name}: {'ok' if cond else 'FAIL'}", flush=True)


###############################################################################
# (a) GraphDataDocument over a small live particle graph
###############################################################################
def _run_graphdata(results):
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.hypergraph.dflow.document import GraphDocument, GraphDocumentError

  # build: Pool.pool --> NozzleEmitter.pool (the canonical particle-buffer edge)
  gd   = dflow.GraphData.createShared()
  gd.cacheable = False                                   # particles run live per-frame
  pool = gd.create("pool0", lev2.particles.Pool)
  noz  = gd.create("noz0",  lev2.particles.NozzleEmitter)
  gd.connect(noz.inputs.pool, pool.outputs.pool)

  doc = GraphDataDocument(gd)
  _check(results, "gd_isinstance", isinstance(doc, GraphDocument))

  # node enumeration (flat; obj == module name)
  tp    = doc.tree_paths()
  keys  = {k for (_pk, k, _o) in tp}
  _check(results, "gd_nodes_enumerated", keys == {"pool0", "noz0"})
  ncls  = dict(doc.nodes())
  _check(results, "gd_node_classes",
         ncls.get("noz0") == "psys::NozzleEmitterData"
         and ncls.get("pool0") == "psys::ParticlePoolData")

  # typed edge enumeration
  edges = doc.edges()
  one   = len(edges) == 1 and edges[0]["out_module"] == "pool0" \
          and edges[0]["in_module"] == "noz0"
  _check(results, "gd_edges_enumerated", one)

  # set a plug param and read it back (FloatXf plug -> read via reflection JSON)
  doc.set_param("noz0", "inputs", "EmissionRate", 250.0)
  rb = doc.get_param("noz0", "inputs", "EmissionRate")
  _check(results, "gd_param_roundtrip", rb is not None and abs(rb - 250.0) < 1e-3)
  ep = dict((n, v) for (_k, n, v) in doc.editable_params("noz0"))
  _check(results, "gd_editable_params",
         "EmissionRate" in ep and abs((ep["EmissionRate"] or 0) - 250.0) < 1e-3)

  # positions round-trip through serializeJson / deserialize (the graph-level layout)
  doc.set_node_pos("pool0", 11.0, 22.0)
  doc.set_node_pos("noz0",  33.0, 44.0)
  js   = doc.to_json()
  doc2 = GraphDataDocument.from_json(js)
  p0   = doc2.node_pos("pool0")
  p1   = doc2.node_pos("noz0")
  pos_ok = (p0 is not None and abs(p0[0] - 11.0) < 1e-3 and abs(p0[1] - 22.0) < 1e-3
            and p1 is not None and abs(p1[0] - 33.0) < 1e-3 and abs(p1[1] - 44.0) < 1e-3)
  _check(results, "gd_positions_roundtrip", pos_ok)
  # the plug param survived the graph round-trip too
  rb2 = doc2.get_param("noz0", "inputs", "EmissionRate")
  _check(results, "gd_param_survives_roundtrip", rb2 is not None and abs(rb2 - 250.0) < 1e-3)
  # edges re-enumerate after deserialize
  _check(results, "gd_edges_roundtrip", len(doc2.edges()) == 1)

  # add + connect a module with typed-connect validation (matching plug types)
  gname = doc.add_node(lev2.particles.Gravity, "grav0")
  _check(results, "gd_add_node", gname == "grav0" and "grav0" in {k for (_p, k, _o) in doc.tree_paths()})
  doc.connect("grav0", "pool", "noz0", "pool")           # ParticleBuffer -> ParticleBuffer (ok)
  _check(results, "gd_add_connect", len(doc.edges()) == 2)

  # LOUD failure on an invalid connect: Float output -> ParticleBuffer input (type mismatch)
  loud = False
  try:
    doc.connect("grav0", "pool", "pool0", "UnitAge")     # float -> particle-buffer (mismatch)
  except GraphDocumentError:
    loud = True
  _check(results, "gd_invalid_connect_loud", loud)
  # the failed connect must NOT have mutated the graph
  _check(results, "gd_invalid_connect_noop", len(doc.edges()) == 2)

  # add_node self-defends against a non-module class (moduleClasses-validated)
  bogus = False
  try:
    doc.add_node(dflow.GraphData, "nope")
  except (GraphDocumentError, Exception):
    bogus = True
  _check(results, "gd_add_node_validates", bogus)

  # delete_node is genuinely unavailable in this python-only slice -> LOUD, not silent
  du = False
  try:
    doc.delete_node("grav0")
  except NotImplementedError:
    du = True
  _check(results, "gd_delete_loud_unavailable", du)

  # elaborate() is the IDENTITY (the graph IS the document)
  _check(results, "gd_elaborate_identity", doc.elaborate() is gd)


###############################################################################
# (b) base-class contract for TerrainDoc
###############################################################################
def _run_terrain(results):
  from ork.hypergraph.dflow.document import GraphDocument, GraphDocumentError, ParamTable
  from ork.hypergraph.dflow.terrain.doc import (
      TerrainDoc, _ParamTable, TerrainDocParamError, DocNode)
  from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class

  # error + params-table hoist: re-export intact, hierarchy preserved
  _check(results, "tr_error_hierarchy", issubclass(TerrainDocParamError, GraphDocumentError))
  _check(results, "tr_paramtable_hoist",
         issubclass(_ParamTable, ParamTable) and _ParamTable._error_cls is TerrainDocParamError)

  # a real terrain document (erodeflow = loops + L.i; a rich structural case)
  cls  = load_dsl_class(resolve_dsl_file("erodeflow"), None)
  inst = cls()
  doc  = inst._doc
  _check(results, "tr_isinstance", isinstance(doc, GraphDocument))

  # shared editor surface dispatches on the terrain document
  tp = doc.tree_paths()
  _check(results, "tr_tree_paths", bool(tp) and all(len(t) == 3 for t in tp))
  first_key = tp[0][1]
  _check(results, "tr_find_by_path", doc.find_by_path(first_key) is tp[0][2])
  dj = doc.to_json()
  _check(results, "tr_to_json", isinstance(dj, dict) and dj.get("version") is not None)

  # a node's editable params dispatch through the base surface
  a_node = next((o for (_p, _k, o) in tp if isinstance(o, DocNode)), None)
  eparams = doc.editable_params(a_node) if a_node is not None else None
  _check(results, "tr_editable_params", isinstance(eparams, list))

  # node positions (base in-memory backing store) + ADDITIVE doc-JSON round-trip
  doc.set_node_pos(first_key, 5.0, 6.0)
  npos = doc.node_pos(first_key)
  _check(results, "tr_node_pos", npos == (5.0, 6.0) and first_key in doc.node_layout)
  dj2 = doc.to_json()
  _check(results, "tr_pos_in_json", "positions" in dj2)
  from ork.hypergraph.dflow.terrain.doc import from_json as _tr_from_json
  doc_rt = _tr_from_json(dj2)
  _check(results, "tr_pos_roundtrip", doc_rt.node_pos(first_key) == (5.0, 6.0))

  # a pristine document (no positions) still serializes WITHOUT a positions key
  # (the byte-compat invariant the oracle enforces)
  inst2 = cls()
  _check(results, "tr_no_pos_no_key", "positions" not in inst2._doc.to_json())

  # undo checkpoint contract composes with capture/restore
  snap  = doc.capture_checkpoint()
  doc3  = TerrainDoc.restore_checkpoint(snap)
  _check(results, "tr_checkpoint_restore",
         isinstance(doc3, GraphDocument) and doc3.to_json() == snap)

  # elaborate returns a real GraphData (terrain's is (graph, capture_map) — the family
  # tuple whose first element is the canonical GraphData product)
  g = doc.elaborate()
  graph = g[0] if isinstance(g, tuple) else g
  _check(results, "tr_elaborate", graph is not None and hasattr(graph, "serializeJson"))


###############################################################################
# (c) generic editor models over a GraphDataDocument (cross-family model proof)
###############################################################################
def _run_generic_models(results):
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.editor.graphdoc_models import (
      GraphDocumentOutlinerModel, GraphDocumentPropertyModel)

  _uip = lev2.ui.PropertyType

  # same small particle graph: Pool.pool --> NozzleEmitter.pool
  gd   = dflow.GraphData.createShared()
  gd.cacheable = False
  pool = gd.create("pool0", lev2.particles.Pool)
  noz  = gd.create("noz0",  lev2.particles.NozzleEmitter)
  gd.connect(noz.inputs.pool, pool.outputs.pool)
  doc  = GraphDataDocument(gd)

  # --- outliner model: enumerate rows + display names ---
  om    = GraphDocumentOutlinerModel(doc)
  roots = om.getChildren("")
  _check(results, "gm_outliner_rows", set(roots) == {"pool0", "noz0"})
  dn = om.getDisplayName("noz0")
  _check(results, "gm_display_name", "noz0" in dn and "Nozzle" in dn)
  _check(results, "gm_object_for_key", om.object_for_key("noz0") == "noz0")

  # --- badges read from the base document surface ---
  nbadges = {b.id: b for b in om.getBadges("noz0")}
  _check(results, "gm_badges_present", "bypass" in nbadges and "display" in nbadges)
  # noz0 has the 'pool' input plug -> bypass badge ENABLED; not yet bypassed -> inactive
  _check(results, "gm_bypass_enabled",
         nbadges["bypass"].enabled and not nbadges["bypass"].active)
  # the badge ACTIVE flag tracks the document bypass state (L2)
  doc.set_bypassed("noz0", True)
  _check(results, "gm_bypass_active_tracks",
         {b.id: b for b in om.getBadges("noz0")}["bypass"].active is True)
  doc.set_bypassed("noz0", False)

  # --- node property model: select, list params, set one, verify the plug changed ---
  doc.set_param("noz0", "inputs", "EmissionRate", 250.0)   # guarantee a float-typed row
  pm = GraphDocumentPropertyModel(doc)
  pm.set_object("noz0")
  keys   = pm.getChildren("")
  er_key = next((k for k in keys if k.startswith("EmissionRate")), None)
  _check(results, "gm_editable_params_listed", er_key is not None)
  _check(results, "gm_param_type_float",
         er_key is not None and pm.getPropertyType(er_key) == _uip.Float)

  before      = pm.getValue(er_key) if er_key is not None else None
  if er_key is not None:
    pm.setValue(er_key, 500.0)
  after_model = pm.getValue(er_key) if er_key is not None else None
  after_plug  = doc.get_param("noz0", "inputs", "EmissionRate")
  _check(results, "gm_set_through_model",
         before is not None and after_model is not None
         and abs(after_model - 500.0) < 1e-3 and abs(after_plug - 500.0) < 1e-3)


###############################################################################
def run(ez, ctx):
  results = {}
  print("=" * 60, flush=True)
  print("E2 GraphDocument gate", flush=True)
  print("=" * 60, flush=True)
  _run_graphdata(results)
  _run_terrain(results)
  _run_generic_models(results)
  return results


def main():
  ez  = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    results = run(ez, ctx)
    ok = bool(results) and all(results.values())
  except Exception:
    import traceback; traceback.print_exc()
  finally:
    ez.mainThreadEnd()
    print("=" * 60, flush=True)
    print("=== dflow graphdocument gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
