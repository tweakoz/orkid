#!/usr/bin/env python3
###############################################################################
# C.4b gate — extrude FACE-mode GPU topology build vs the retained CPU reference.
# TOPOLOGY (all `f` lines + counts) must be BYTE-IDENTICAL; float vertex data must match within
# 1e-5 (the per-dup tables are computed from the same expressions but on different processors —
# CPU vs GPU FMA contraction differs by ~1 ulp; unlike subdivide, where both paths' floats ran on
# the GPU, bit-exactness across processors is not achievable). Coverage: single-segment (box
# cap+walls), keep_base, multi-segment tube with twist/scale expressions (S.t), mixed tri+ngon
# source (cone), expression distance, chained extrude->extrude.
# ORK_HM_EXTRUDE_CPU=1 toggles the reference per-bake (live getenv).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY


def make_asset(kind):
  class _A(Hypermesh):
    def __init__(self):
      super().__init__()
      if kind == "box_single":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.extrude_faces(n, distance=0.5, slot=2)
      elif kind == "box_keepbase":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.extrude_faces(n, distance=0.5, slot=2, keep_base=True)
      elif kind == "cone_mixed":
        n = self.cone(radius=1.2, height=2.0, sides=7)
        n = self.select(n, S.N.dot(vec3(0, -1, 0)) > 0.5, domain=POLY, op=isolate(group(1)))
        n = self.extrude_faces(n, distance=0.3, slot=1)     # the NGON base extrudes
      elif kind == "multiseg":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.extrude_faces(n, distance=S.t * 0.8, segments=3, twist=0.4 * S.t,
                               scale=1.0 - 0.3 * S.t, slot=2)
      elif kind == "expr_dist":
        n = self.icosphere(radius=1.0, subdivisions=1)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.3, domain=POLY, op=isolate(group(0)))
        n = self.extrude_faces(n, distance=0.2 + 0.1 * S.sin(S.P.dot(vec3(3, 0, 0))), slot=0)
      elif kind == "chained":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.extrude_faces(n, distance=0.4, slot=2, mask_cap=isolate(group(3)))
        n = self.extrude_faces(n, distance=0.25, slot=3)
      self.output(n)
  return _A


CASES = ["box_single", "box_keepbase", "cone_mixed", "multiseg", "expr_dist", "chained"]


def bake_obj(ctx, asset_cls, path):
  live = asset_cls().materialize_live(ctx)
  lev2.hypermesh.dump_obj(live.mesh, ctx, path)
  return open(path).read()


def compare(kind, gpu, cpu, tol=1e-5):
  """topology byte-exact; float lines numerically within tol. Returns max float deviation."""
  gl, cl = gpu.splitlines(), cpu.splitlines()
  assert len(gl) == len(cl), "%s: line-count mismatch (%d vs %d)" % (kind, len(gl), len(cl))
  maxdev = 0.0
  for i, (a, b) in enumerate(zip(gl, cl)):
    if a == b:
      continue
    ta, tb = a.split(), b.split()
    assert ta and tb and ta[0] == tb[0] and ta[0] in ("v", "vn", "vt"), \
        "%s: non-float line differs at %d: %r vs %r" % (kind, i, a, b)
    assert len(ta) == len(tb), "%s: arity mismatch at line %d" % (kind, i)
    for x, y in zip(ta[1:], tb[1:]):
      d = abs(float(x) - float(y))
      maxdev = max(maxdev, d)
      assert d <= tol, "%s: float deviates %.3g at line %d (%r vs %r)" % (kind, d, i, a, b)
  return maxdev


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    tmp = tempfile.mkdtemp(prefix="hm_extrude_oracle_")
    for kind in CASES:
      cls = make_asset(kind)
      os.environ.pop("ORK_HM_EXTRUDE_CPU", None)
      gpu = bake_obj(ctx, cls, os.path.join(tmp, "%s_gpu.obj" % kind))
      os.environ["ORK_HM_EXTRUDE_CPU"] = "1"
      cpu = bake_obj(ctx, cls, os.path.join(tmp, "%s_cpu.obj" % kind))
      os.environ.pop("ORK_HM_EXTRUDE_CPU", None)
      assert len(gpu) > 100, "%s: empty GPU bake" % kind
      maxdev = compare(kind, gpu, cpu)
      nv = sum(1 for l in gpu.splitlines() if l.startswith("v "))
      nf = sum(1 for l in gpu.splitlines() if l.startswith("f "))
      print("EXTRUDE_ORACLE %-12s PASS (verts=%d faces=%d, topo exact, float maxdev=%.2g)"
            % (kind, nv, nf, maxdev), flush=True)
      gpu2 = bake_obj(ctx, cls, os.path.join(tmp, "%s_gpu2.obj" % kind))
      assert gpu2 == gpu, "%s: GPU bake nondeterministic" % kind
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    os.environ.pop("ORK_HM_EXTRUDE_CPU", None)
    ezapp.mainThreadEnd()
    print("=== hypermesh extrude oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
