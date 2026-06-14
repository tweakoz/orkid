#!/usr/bin/env python3
###############################################################################
# C.4c gate — inset GPU build (count->6 scans->emit + the DP collar kernel) vs the retained
# CPU reference (cpu=True forces the CPU topology build + CPU positions). TOPOLOGY must be
# byte-exact (same DP, same walk order); floats within 1e-5 (cross-processor ulp).
# Coverage: 1:1 inset (quad collar), resampled N-gon (the Fuchs/Kedem DP path, m=4 vs N=8),
# no-fill (open hole), nonzero rotate at build, ngon source (cone base), chained
# inset->extrude. Plus the LIVE rotate spin: collar re-tessellation across sector crossings
# (the GPU adaptive path with the 4-byte changed-flag) must keep matching a fresh bake.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY


def make_asset(kind, cpu, rotate=0.0):
  class _A(Hypermesh):
    def __init__(self):
      super().__init__()
      if kind == "plain":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.inset(n, amount=0.25, slot=2, cpu=cpu)
      elif kind == "resampled":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.inset(n, amount=0.2, sides=8, slot=2, cpu=cpu)
      elif kind == "nofill":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        # postcheck=False: a hole INTENTIONALLY creates boundary edges; the CPU reference's
        # edge-health postcheck would (correctly, per its contract) assert otherwise.
        n = self.inset(n, amount=0.3, sides=6, fill=False, slot=2, cpu=cpu, postcheck=False)
      elif kind == "rotated":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.inset(n, amount=0.2, sides=8, rotate=rotate, slot=2, cpu=cpu)
      elif kind == "ngon_src":
        n = self.cone(radius=1.2, height=2.0, sides=7)
        n = self.select(n, S.N.dot(vec3(0, -1, 0)) > 0.5, domain=POLY, op=isolate(group(1)))
        n = self.inset(n, amount=0.25, sides=5, slot=1, cpu=cpu)
      elif kind == "chained":
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        n = self.inset(n, amount=0.2, sides=8, slot=2, mask_inner=isolate(group(5)), cpu=cpu)
        n = self.extrude_faces(n, distance=0.3, slot=5)
      self.output(n)
  return _A


CASES = [("plain", 0.0), ("resampled", 0.0), ("nofill", 0.0), ("rotated", 25.0), ("ngon_src", 0.0), ("chained", 0.0)]


def bake_obj(ctx, asset_cls, path):
  live = asset_cls().materialize_live(ctx)
  lev2.hypermesh.dump_obj(live.mesh, ctx, path)
  return open(path).read()


def compare(kind, gpu, cpu, tol=1e-5):
  gl, cl = gpu.splitlines(), cpu.splitlines()
  assert len(gl) == len(cl), "%s: line-count mismatch (%d vs %d)" % (kind, len(gl), len(cl))
  maxdev = 0.0
  for i, (a, b) in enumerate(zip(gl, cl)):
    if a == b:
      continue
    ta, tb = a.split(), b.split()
    is_float = ta and tb and ta[0] == tb[0] and (ta[0] in ("v", "vn", "vt") or
                (ta[0] == "#" and len(ta) > 1 and ta[1] == tb[1] == "b"))   # `# b` = binormal dump line
    assert is_float, "%s: non-float line differs at %d: %r vs %r" % (kind, i, a, b)
    skip = 2 if ta[0] == "#" else 1
    assert len(ta) == len(tb), "%s: arity mismatch at line %d" % (kind, i)
    for x, y in zip(ta[skip:], tb[skip:]):
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
    tmp = tempfile.mkdtemp(prefix="hm_inset_oracle_")
    for kind, rot in CASES:
      gpu = bake_obj(ctx, make_asset(kind, False, rot), os.path.join(tmp, "%s_gpu.obj" % kind))
      cpu = bake_obj(ctx, make_asset(kind, True, rot), os.path.join(tmp, "%s_cpu.obj" % kind))
      assert len(gpu) > 100, "%s: empty GPU bake" % kind
      maxdev = compare(kind, gpu, cpu)
      nv = sum(1 for l in gpu.splitlines() if l.startswith("v "))
      nf = sum(1 for l in gpu.splitlines() if l.startswith("f "))
      print("INSET_ORACLE %-10s PASS (verts=%d faces=%d, topo exact, float maxdev=%.2g)"
            % (kind, nv, nf, maxdev), flush=True)
      gpu2 = bake_obj(ctx, make_asset(kind, False, rot), os.path.join(tmp, "%s_gpu2.obj" % kind))
      assert gpu2 == gpu, "%s: GPU bake nondeterministic" % kind

    # ---- LIVE rotate spin: the GPU adaptive collar (DP re-run + 4-byte changed flag) across
    # sector crossings must equal a FRESH GPU bake built directly at that rotate.
    class _Spin(Hypermesh):
      def __init__(self):
        super().__init__()
        n = self.box(size=1.0)
        n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
        self._ins = self.inset(n, amount=0.2, sides=8, slot=2, cpu=False)
        self.output(self._ins)
    a = _Spin()
    live = a.materialize_live(ctx)
    for rot in (0.0, 13.0, 31.0, 47.0):                  # sweeps across 45-deg sectors of the 8-ring
      a._ins.inputs.rotate = rot
      live.recompute(ctx)
      p_live = os.path.join(tmp, "spin_%d_live.obj" % int(rot))
      lev2.hypermesh.dump_obj(live.mesh, ctx, p_live)
      ref = bake_obj(ctx, make_asset("rotated", False, rot), os.path.join(tmp, "spin_%d_ref.obj" % int(rot)))
      assert open(p_live).read() == ref, "live rotate=%g diverges from a fresh bake (adaptive collar)" % rot
    print("INSET_ORACLE spin       PASS (live rotate == fresh bakes across sector crossings)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh inset oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
