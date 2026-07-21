#!/usr/bin/env python3
###############################################################################
# Terrain connect/disconnect gate (E2 close). Proves the FORMALIZED edge overrides:
#
#   c1  TerrainDoc.connect(dst_key,in_plug,src_key,out_plug) OWNS the native-edge
#       store write — re-wiring a consumer's input to a different source overwrites
#       the old source (the DocNode.connections slot changes), and re-wiring back
#       overwrites again (splice-on-wire semantics).
#   c2  TerrainDoc.connect refuses LOUDLY (TerrainDocParamError) on an unknown key
#       and an unknown plug (ops self-defend).
#   c3  TerrainDoc.disconnect is a LOUD refusal (terrain inputs are always fed) with
#       the DECIDED reason text — never a silent no-op.
#   c4  the C1 CANVAS ADAPTER connect path routes THROUGH doc.connect (the adapter no
#       longer pokes the store itself): model.connect mutates the doc via the override,
#       and model.disconnect surfaces the loud refusal ("refused", reason).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, TerrainDocParamError, tree_paths)
from ork.editor.terrain_node_model import TerrainNodeGraphModel, _Glue
from ork.editor.terrain_runtime import TerrainRuntime

_SALT = 3.017


class ConnFx(HeightField):
  """Two bare Fbm sources + a height capture fed by the FIRST — the capture input is
  the re-wire target; the second source is the alternate producer (no processing-op
  params, so this fixture is insulated from op-signature churn)."""
  def __init__(self):
    super().__init__()
    a = T.Fbm(frequency=_SALT, octaves=4)
    b = T.Fbm(frequency=_SALT * 1.7, octaves=3)   # alternate source (stays in the doc)
    self._b = b
    self.capture(a, "height")


# ---- minimal canvas-adapter host/runtime shims (the f12 flags pattern) -------
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


def _key_of(doc, obj):
  for (_pk, k, o) in tree_paths(doc):
    if o is obj:
      return k
  return None


def _endpoints(doc):
  cap = next(o for (_pk, _k, o) in tree_paths(doc)
             if isinstance(o, DocNode) and o.clazz_name == "CaptureModule")
  in_name, src_ref = cap.connections[0]
  fbm_a = src_ref.node
  fbm_b = next(o for (_pk, _k, o) in tree_paths(doc)
               if isinstance(o, DocNode) and not o.connections
               and o.clazz_name != "CaptureModule" and o is not fbm_a)
  return cap, in_name, fbm_a, fbm_b


def _raises(fn, exc, needle=None):
  try:
    fn()
    return False
  except exc as ex:
    return (needle is None) or (needle in str(ex))


def run(ez, ctx):
  results = {}

  # ---- c1: doc.connect owns the native-store write (overwrite both ways) -----
  fx = ConnFx(); fx.generatedflow()
  doc = fx.document()
  cap, in_name, fbm_a, fbm_b = _endpoints(doc)
  cap_key, a_key, b_key = _key_of(doc, cap), _key_of(doc, fbm_a), _key_of(doc, fbm_b)

  doc.connect(cap_key, in_name, b_key, "Out")            # re-wire capture <- fbm_b
  rewired = (cap.connections[0][0] == in_name and cap.connections[0][1].node is fbm_b
             and len(cap.connections) == 1)
  doc.connect(cap_key, in_name, a_key, "Out")            # overwrite back <- fbm_a
  restored = (cap.connections[0][1].node is fbm_a and len(cap.connections) == 1)
  c1 = rewired and restored
  print(f"[c1] rewire->b {rewired}  overwrite->a {restored} -> {c1}", flush=True)
  results["c1_connect_overwrites"] = c1

  # ---- c2: connect refuses loudly on bad key / bad plug ----------------------
  bad_key  = _raises(lambda: doc.connect("no_such_node", in_name, a_key, "Out"),
                     TerrainDocParamError, "no document object")
  bad_plug = _raises(lambda: doc.connect(cap_key, "no_such_plug", a_key, "Out"),
                     TerrainDocParamError)
  c2 = bad_key and bad_plug
  print(f"[c2] bad-key-loud {bad_key}  bad-plug-loud {bad_plug} -> {c2}", flush=True)
  results["c2_connect_refusals"] = c2

  # ---- c3: disconnect is a LOUD refusal with the decided reason --------------
  loud = _raises(lambda: doc.disconnect(cap_key, in_name, a_key, "Out"),
                 TerrainDocParamError, "terrain inputs are always fed")
  # base surface: the override REPLACED the base _unsupported raise (no NotImplementedError leak)
  from ork.hypergraph.dflow.document import GraphDocument
  is_override = doc.disconnect.__func__ is not GraphDocument.disconnect
  c3 = loud and is_override
  print(f"[c3] disconnect-loud-refusal {loud}  is-override {is_override} -> {c3}", flush=True)
  results["c3_disconnect_refusal"] = c3

  # ---- c4: the CANVAS ADAPTER routes connect THROUGH doc.connect -------------
  fx2 = ConnFx(); fx2.generatedflow()
  doc2 = fx2.document()
  cap2, in2, a2, b2 = _endpoints(doc2)
  cap2_key, b2_key = _key_of(doc2, cap2), _key_of(doc2, b2)
  model = _mk_model(doc2)

  # spy: prove the adapter delegates to the doc override (not its own store poke)
  calls = []
  _orig = doc2.connect
  doc2.connect = lambda *a, **k: (calls.append((a, k)), _orig(*a, **k))[1]

  cc_ok, cc_reason = model.can_connect(b2_key, "Out", cap2_key, in2)
  model.connect(b2_key, "Out", cap2_key, in2)           # canvas gesture: fbm_b -> capture
  adapter_routes = (len(calls) == 1 and cap2.connections[0][1].node is b2)
  doc2.connect = _orig

  # adapter disconnect surfaces the loud refusal (splice still works via connect-overwrite)
  dret = model.disconnect(b2_key, "Out", cap2_key, in2)
  disc_surfaced = (isinstance(dret, tuple) and dret[0] == "refused"
                   and "terrain inputs are always fed" in dret[1])
  # the refused disconnect did NOT orphan the input (still fed)
  still_fed = (cap2.connections[0][1] is not None)
  c4 = bool(cc_ok and adapter_routes and disc_surfaced and still_fed)
  print(f"[c4] can_connect {cc_ok}  adapter->doc.connect {adapter_routes}  "
        f"disconnect-refused {disc_surfaced}  still-fed {still_fed} -> {c4}", flush=True)
  results["c4_adapter_routes_through_doc"] = c4

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
    print(f"\n=== terrain connect/disconnect {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
