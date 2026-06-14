#!/usr/bin/env python3
###############################################################################
# C.4a gate — subdivide GPU topology build vs the retained CPU reference: BYTE-IDENTICAL.
# Both paths number edge midpoints canonically (sorted (min,max) key) and emit the same
# face patterns, so the OBJ dumps must match exactly — topology AND interpolated positions
# (the GPU path's runtime-count interp shaders use identical arithmetic).
# Coverage: pure tris (icosphere), pure quads (box), mixed tri+ngon (cone), mixed
# quad+tri (uvsphere), levels 1+2, and a chained topology op upstream (extrude -> subdivide).
# ORK_HM_SUBD_CPU=1 toggles the reference per-bake (getenv is read per build, same process).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY


def make_asset(kind, level):
  class _A(Hypermesh):
    def __init__(self):
      super().__init__()
      if kind == "ico":
        n = self.icosphere(radius=1.5, subdivisions=1)
      elif kind == "box":
        n = self.box(size=1.0)
      elif kind == "cone":
        n = self.cone(radius=1.2, height=2.0, sides=7)
      elif kind == "uvsphere":
        n = self.uvsphere(radius=1.3, segments=8, rings=6)
      elif kind == "chained":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.extrude_faces(n, distance=0.4, slot=2)
      self.output(self.subdivide(n, level=level))
  return _A


CASES = [("box", 1), ("box", 2), ("ico", 1), ("ico", 2), ("cone", 1), ("uvsphere", 1), ("uvsphere", 2), ("chained", 1)]


def bake_obj(ctx, asset_cls, path):
  live = asset_cls().materialize_live(ctx)
  lev2.hypermesh.dump_obj(live.mesh, ctx, path)
  return open(path).read()


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    tmp = tempfile.mkdtemp(prefix="hm_subdiv_oracle_")
    for kind, level in CASES:
      cls = make_asset(kind, level)
      os.environ.pop("ORK_HM_SUBD_CPU", None)
      gpu = bake_obj(ctx, cls, os.path.join(tmp, "%s_L%d_gpu.obj" % (kind, level)))
      os.environ["ORK_HM_SUBD_CPU"] = "1"
      cpu = bake_obj(ctx, cls, os.path.join(tmp, "%s_L%d_cpu.obj" % (kind, level)))
      os.environ.pop("ORK_HM_SUBD_CPU", None)
      assert len(gpu) > 100, "%s L%d: empty GPU bake" % (kind, level)
      assert gpu == cpu, "%s L%d: GPU-built subdivide diverges from the CPU reference" % (kind, level)
      nv = sum(1 for l in gpu.splitlines() if l.startswith("v "))
      nf = sum(1 for l in gpu.splitlines() if l.startswith("f "))
      print("SUBDIV_ORACLE %-9s L%d PASS (verts=%d faces=%d, byte-identical)" % (kind, level, nv, nf), flush=True)
      # determinism: a second GPU bake must be bit-identical too
      gpu2 = bake_obj(ctx, cls, os.path.join(tmp, "%s_L%d_gpu2.obj" % (kind, level)))
      assert gpu2 == gpu, "%s L%d: GPU bake nondeterministic" % (kind, level)

    # ---- 2.7 STALENESS: upstream topology CHANGE must rebuild the level tables (was: frozen forever).
    # cone(sides) is the dynamic-topology generator; bump sides on the LIVE graph, give the defer-1-frame
    # rebuild its two recomputes, and the subdivided output must match a fresh bake at the new sides.
    class _Dyn(Hypermesh):
      def __init__(self):
        super().__init__()
        self._cone = self.cone(radius=1.2, height=2.0, sides=7)
        self.output(self.subdivide(self._cone, level=1))
    a = _Dyn()
    live = a.materialize_live(ctx)
    p_before = os.path.join(tmp, "dyn_before.obj")
    lev2.hypermesh.dump_obj(live.mesh, ctx, p_before)
    a._cone.inputs.sides = 11                       # topology change on the live graph
    live.recompute(ctx)                             # frame 1: producer re-emits; subdivide defers
    live.recompute(ctx)                             # frame 2: tables rebuild against settled topology
    p_after = os.path.join(tmp, "dyn_after.obj")
    lev2.hypermesh.dump_obj(live.mesh, ctx, p_after)
    after = open(p_after).read()
    assert after != open(p_before).read(), "2.7 STALE: subdivide ignored the upstream topology change"
    class _Ref(Hypermesh):                          # fresh bake at sides=11 = the expected result
      def __init__(self):
        super().__init__()
        self.output(self.subdivide(self.cone(radius=1.2, height=2.0, sides=11), level=1))
    ref = bake_obj(ctx, _Ref, os.path.join(tmp, "dyn_ref.obj"))
    assert after == ref, "2.7: rebuilt tables diverge from a fresh bake at the new topology"
    print("SUBDIV_ORACLE staleness(2.7) PASS (live sides 7->11 == fresh bake)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    os.environ.pop("ORK_HM_SUBD_CPU", None)
    ezapp.mainThreadEnd()
    print("=== hypermesh subdiv oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
