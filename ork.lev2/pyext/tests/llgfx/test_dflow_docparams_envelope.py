#!/usr/bin/env ork.python
###############################################################################
# E2 close — the E0 doc-params ENVELOPE gate (fast, headless).
#
# Proves the GraphData-family document wire form now carries document params
# (JUL13_DFLOW E0/E1-tail) WITHOUT breaking doc-less assets:
#
#   (a) a GraphDataDocument with NO params serializes the PLAIN reflection JSON
#       byte-identically (a doc-less .orj), and a plain .orj (no envelope) loads
#       with an EMPTY params table — shape-detection, not a legacy shim.
#   (b) a GraphDataDocument WITH params serializes the {"params":..,"graphdata":..}
#       envelope; to_json/from_json round-trips values + unit tags + structural flags
#       + a vec (tuple) param, and the wrapped graph (nodes/edges/positions) survives.
#   (c) HypermeshDocument (the hypermesh family specialization): family tag; save/load
#       ride the SAME plain reflection-JSON artifact (Hypermesh.save/load_graphdata);
#       save REFUSES loudly if document params are present (they cannot ride the interop
#       artifact — the envelope snapshot does); the inherited envelope to_json still works.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

dflow = core.dataflow


def _check(results, name, cond):
  results[name] = bool(cond)
  print(f"  [envelope] {name}: {'ok' if cond else 'FAIL'}", flush=True)


def _mk_graph():
  # a small live graph: Pool.pool --> NozzleEmitter.pool (content is irrelevant to the
  # document wire form under test — any GraphData exercises the envelope + save/load).
  gd   = dflow.GraphData.createShared()
  gd.cacheable = False
  pool = gd.create("pool0", lev2.particles.Pool)
  noz  = gd.create("noz0",  lev2.particles.NozzleEmitter)
  gd.connect(noz.inputs.pool, pool.outputs.pool)
  return gd


def _run(results):
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.hypergraph.dflow.hypermesh_document import HypermeshDocument
  from ork.hypergraph.dflow.document import GraphDocumentError

  # ---- (a) plain (no params) round-trip is byte-identical + empty on load ----
  doc = GraphDataDocument(_mk_graph())
  doc.set_node_pos("pool0", 11.0, 22.0)
  plain = doc.to_json()
  _check(results, "plain_is_string", isinstance(plain, str))
  _check(results, "plain_byte_identical", plain == doc._graph.serializeJson())
  doc_p = GraphDataDocument.from_json(plain)
  _check(results, "plain_loads_empty_params", len(doc_p.params) == 0)
  _check(results, "plain_nodes_intact",
         {k for (_pk, k, _o) in doc_p.tree_paths()} == {"pool0", "noz0"})
  _check(results, "plain_pos_intact", doc_p.node_pos("pool0") == (11.0, 22.0))

  # a bare reflection JSON (never envelope-wrapped) still loads (shape detection)
  raw = _mk_graph().serializeJson()
  doc_raw = GraphDataDocument.from_json(raw)
  _check(results, "raw_orj_loads", len(doc_raw.params) == 0
         and {k for (_p, k, _o) in doc_raw.tree_paths()} == {"pool0", "noz0"})

  # ---- (b) params survive the envelope round-trip ----------------------------
  doc2 = GraphDataDocument(_mk_graph())
  doc2.set_node_pos("noz0", 5.0, 6.0)
  doc2.params.declare("amplitude", 60.0, tag="meters")   # tagged (E0 part 2)
  doc2.params.declare("seed", 7)                          # plain scalar
  doc2.params.declare("center", (1.0, 2.0, 3.0))          # vec/tuple param
  doc2.params.mark_structural("seed")
  env = doc2.to_json()
  parsed = json.loads(env)
  _check(results, "envelope_shape",
         isinstance(parsed, dict) and "params" in parsed and "graphdata" in parsed)

  doc3 = GraphDataDocument.from_json(env)
  _check(results, "env_param_order", doc3.params.names() == ["amplitude", "seed", "center"])
  _check(results, "env_scalar_value",
         abs(doc3.params.get("amplitude") - 60.0) < 1e-6 and doc3.params.get("seed") == 7)
  _check(results, "env_tag_survives", doc3.params.tag_of("amplitude") == "meters"
         and doc3.params.tag_of("seed") is None)
  _check(results, "env_tuple_survives",
         isinstance(doc3.params.get("center"), tuple) and doc3.params.get("center") == (1.0, 2.0, 3.0))
  _check(results, "env_structural_survives",
         doc3.params.is_structural("seed") and not doc3.params.is_structural("amplitude"))
  _check(results, "env_graph_intact",
         {k for (_pk, k, _o) in doc3.tree_paths()} == {"pool0", "noz0"}
         and doc3.node_pos("noz0") == (5.0, 6.0)
         and len(doc3.edges()) == 1)
  # checkpoint contract composes with the envelope (capture -> restore keeps params)
  doc4 = GraphDataDocument.restore_checkpoint(doc2.capture_checkpoint())
  _check(results, "env_checkpoint_restore",
         doc4.params.get("seed") == 7 and doc4.params.tag_of("amplitude") == "meters")

  # ---- (c) HypermeshDocument specialization ----------------------------------
  hdoc = HypermeshDocument(_mk_graph())
  _check(results, "hm_family_tag", hdoc.family == "hypermesh" and HypermeshDocument.FAMILY == "hypermesh")
  _check(results, "hm_is_graphdata_doc", isinstance(hdoc, GraphDataDocument))
  art = os.path.join(tempfile.mkdtemp(prefix="hmdoc_"), "asset.hmgraph.json")
  hdoc.save(art)
  # the artifact is the plain reflection JSON (interop with load_graphdata / the C++ host)
  with open(art) as f:
    disk = f.read()
  _check(results, "hm_save_is_plain", disk == hdoc._graph.serializeJson())
  hdoc2 = HypermeshDocument.load(art)
  _check(results, "hm_load_roundtrip",
         isinstance(hdoc2, HypermeshDocument)
         and {k for (_pk, k, _o) in hdoc2.tree_paths()} == {"pool0", "noz0"})
  # save REFUSES loudly with document params (they cannot ride the interop artifact)
  hdoc.params.declare("wobble", 0.5)
  refused = False
  try:
    hdoc.save(art)
  except GraphDocumentError:
    refused = True
  _check(results, "hm_save_refuses_with_params", refused)
  # ...but the inherited envelope snapshot DOES carry them
  hd_env = json.loads(hdoc.to_json())
  _check(results, "hm_envelope_carries_params",
         "params" in hd_env and hd_env["params"].get("wobble", {}).get("value") == 0.5)


def run(ez, ctx):
  results = {}
  print("=" * 60, flush=True)
  print("E2 doc-params envelope gate", flush=True)
  print("=" * 60, flush=True)
  _run(results)
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
    print("=== dflow doc-params envelope gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    for k, v in (results.items() if 'results' in dir() else []):
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
