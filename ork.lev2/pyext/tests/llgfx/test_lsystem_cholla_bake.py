#!/usr/bin/env python3
###############################################################################
# GR1.c gate C (GPU, linux/3090 node) — the CaneCholla vertical slice END TO END on the GPU:
# H.lsystem(grammar=CaneCholla) -> the C++ evaluator derives the reflected LRuleSet into an
# XfNodeGraph -> LSweepModule skins it to a swept-tube GpuMesh -> materialize() bakes it. We read
# the baked mesh back (dump_obj) so the actual generated geometry (not a promise) is inspectable:
# a non-degenerate, branchy, upright cactus. (that node's offscreen mp4 muxer is broken for ALL scenes,
# so the movie path is unusable here — this bakes the real mesh and dumps it for a silhouette render.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.dflow.lsystem.examples import CaneCholla

OBJ = os.environ.get("CHOLLA_OBJ", "/tmp/cholla.obj")


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  m = Hypermesh()
  n = m.lsystem(
      grammar     = CaneCholla,
      seg_len     = 0.16,
      base_radius = 0.020,
      sides       = 7,
      jitter      = 0.12,
      jit_azimuth = 0.6,
      jit_pitch   = 0.35,
      tropism     = 0.02)
  m.output(n)
  mesh = m.materialize(ctx)
  nv, nf = mesh.num_verts, mesh.num_faces
  print(f"cholla baked: num_verts={nv} num_faces={nf}", flush=True)
  wrote = lev2.hypermesh.dump_obj(mesh, ctx, OBJ)
  print(f"dump_obj wrote {wrote} verts to {OBJ}", flush=True)

  ezapp.mainThreadEnd()
  ecs.headless_exit()
  # a 253-node, 7-sided swept tube is ~thousands of verts/faces; assert non-degenerate.
  ok = (nv > 500 and nf > 500)
  print(f"CHOLLA_BAKE_RESULT={'PASS' if ok else 'FAIL'} (nv={nv} nf={nf})", flush=True)
  sys.exit(0 if ok else 1)


main()
