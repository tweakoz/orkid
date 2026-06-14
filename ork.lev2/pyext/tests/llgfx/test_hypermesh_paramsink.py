#!/usr/bin/env python3
###############################################################################
# E.6/2.12 gate — MaterialParamSink (graph node driving a material UBO param
# by name).
#
#   1. mesh PASSTHROUGH: a chain with material_param() computes identically
#      (counts + face tags) to the same chain without it.
#   2. the pokeable "value" DATA plug round-trips through the new generic
#      InPlugData.value getter (poke -> read parity).
#   3. serdes: the graph round-trips byte-identical (param_name + plug value
#      are reflected state), and the clone recomputes to the same mesh.
#
# Pipeline-level application of the value is covered by the fxpipeline rebind
# selftest (test_fxpipeline_rebind.py) + the param_pulse viewer demo.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh

PNAME = "glow_gain"


class Plain(Hypermesh):
  def __init__(self):
    super().__init__()
    b = self.box(size=1.0)
    g = self.assign_gid(b, gid=3)            # creates __tags (tag-parity oracle)
    self.output(g)


class Sunk(Hypermesh):
  def __init__(self):
    super().__init__()
    b = self.box(size=1.0)
    g = self.assign_gid(b, gid=3)
    self._sink = self.material_param(g, name=PNAME, value=0.25)
    self.output(self._sink)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  hm = lev2.hypermesh
  try:
    _body(ezapp, ctx, hm)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    # ALWAYS tear down — a skipped headless_exit spins the process forever
    # (the coreappexit teardown trap)
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


def _body(ezapp, ctx, hm):

  ##############################################################
  # 1) passthrough parity vs the sink-free chain
  ##############################################################
  ref   = Plain().generatedflow()
  liveR = hm.materialize_live(ref, ctx)
  sunk_asset = Sunk()
  snk   = sunk_asset.generatedflow()
  liveS = hm.materialize_live(snk, ctx)
  mR, mS = liveR.mesh, liveS.mesh
  assert (mS.num_verts, mS.num_corners, mS.num_faces) == (mR.num_verts, mR.num_corners, mR.num_faces), \
      "sink passthrough changed mesh counts"
  tR = list(hm.read_face_tags(mR, ctx))
  tS = list(hm.read_face_tags(mS, ctx))
  assert tS == tR, "sink passthrough changed face tags: %s vs %s" % (tS, tR)
  print("paramsink passthrough PASS (%d faces, tag parity)" % mS.num_faces, flush=True)

  ##############################################################
  # 2) pokeable plug + the generic InPlugData.value getter
  ##############################################################
  sink = sunk_asset._sink
  assert sink.param_name == PNAME
  assert abs(sink.inputs.value.value - 0.25) < 1e-6, \
      "authored plug value misread: %r" % sink.inputs.value.value
  sink.inputs.value = 0.7
  assert abs(sink.inputs.value.value - 0.7) < 1e-6, \
      "poked plug value misread: %r" % sink.inputs.value.value   # fp32 storage
  print("paramsink poke/read PASS", flush=True)

  ##############################################################
  # 3) serdes: byte-identical + clone recompute parity
  ##############################################################
  js    = snk.serializeJson()
  clone = core.Object.deserializeJson(js)
  assert clone.serializeJson() == js, "paramsink graph re-serialization diverged"
  liveC = hm.materialize_live(clone, ctx)
  tC    = list(hm.read_face_tags(liveC.mesh, ctx))
  assert tC == tS, "clone recompute tags diverged"
  print("paramsink serdes PASS (byte-identical + recompute parity)", flush=True)

  ezapp.mainThreadEnd()
  print("=== hypermesh paramsink gate PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


if __name__ == "__main__":
  main()
