#!/usr/bin/env python3
###############################################################################
# JUL09 camera slice GATE (headless, mac) — editor uicam matches the C++ player
# + orbit target rides the terrain SURFACE.
#
#  * near/far/fov/zoom: TerrainRuntime.setup_camera must set the SAME EzUiCam params
#    the player uses (main.cpp:414-420) — near_min 0.75, far_max 100000, fov 65deg,
#    base_zmoveamt 0.05 (setupUiCameraX otherwise defaults far=1000 -> clips terrain,
#    fov 45 -> the owner's "zoom unusable").
#  * surface orbit target: on load the orbit target (uicam.center) sits at the sampled
#    height at the terrain-center XZ; after a rebake that CHANGES the height, the target
#    keeps its (panned) XZ but re-evaluates Y on the NEW surface.
#
# Zoom FEEL is owner-verify (windowed) — this gate asserts the numeric contract.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, math

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.editor.terrain_runtime import TerrainRuntime
from ork.hypergraph.dflow.terrain.doc import DocNode

DIM = 256
EXTENT_M = 512.0
HEIGHT_M = 60.0
PX, PZ = 80.0, -40.0     # a "panned" orbit target XZ (off-center)
TOL = 0.05               # meters — the sampler feeds center.y directly, so this is tight


def _find_scalar(doc):
  def walk(children):
    for ch in children:
      if isinstance(ch, DocNode):
        for (kind, name, value) in ch.editable_params():
          if isinstance(value, float) and not isinstance(value, bool):
            return (ch, kind, name, value)
      sub = getattr(ch, "children", None)
      if sub is not None:
        r = walk(sub)
        if r is not None:
          return r
    return None
  return walk(doc._root)


def main():
  results = {}
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  rt = TerrainRuntime(preview_dim=DIM, chunk=128)
  rt.load("voronoi", extent_m=EXTENT_M, height_m=HEIGHT_M)
  rt.set_context(ctx)
  rt.setup_camera()
  rt.create_live_scene(ctx, dim=DIM)     # bakes -> loads heights -> centers orbit on surface

  # ---- (1) camera near/far/fov/zoom == the player's EzUiCam constants -------
  uc = rt.uicam
  cam_ok = (abs(uc.near_min - 0.75) < 1e-4
            and abs(uc.far_max - 100000.0) < 1e-1
            and abs(uc.fov - math.radians(65.0)) < 1e-3
            and abs(uc.base_zmoveamt - 0.05) < 1e-4)
  print(f"[cam] near_min={uc.near_min} far_max={uc.far_max} fov={uc.fov:.4f}(rad) "
        f"base_zmoveamt={uc.base_zmoveamt} -> match_player={cam_ok}", flush=True)
  results["cam_matches_player"] = cam_ok

  # ---- (2) orbit target on the SURFACE after load --------------------------
  c0 = uc.center
  surf0 = rt.terrain_height(c0.x, c0.z)
  load_ok = (rt._cpu_hf is not None) and (abs(c0.y - surf0) < TOL)
  print(f"[cam] after load: center=({c0.x:.2f},{c0.y:.3f},{c0.z:.2f}) "
        f"terrain_height(xz)={surf0:.3f} -> on_surface={load_ok}", flush=True)
  results["target_on_surface_after_load"] = load_ok

  # ---- (3) pan + a height-changing rebake -> keep XZ, re-eval Y -------------
  old_at_pan = rt.terrain_height(PX, PZ)
  uc.center = vec3(PX, c0.y, PZ)          # pan the orbit target off-center
  tgt = _find_scalar(rt.document)
  assert tgt is not None, "no editable scalar param on voronoi to change the height"
  node, kind, name, old = tgt
  node.set_param(kind, name, float(old) * 1.7 + 2.0)   # change the field -> new heights
  rt.schedule_rebuild()
  assert rt.prepare_rebuild(ctx, dim=DIM)  # Phase A (GPU): re-elaborate + bake
  assert rt.apply_pending_rebuild()        # Phase B: swap + re-load heights + re-center (keep_xz)

  c1 = uc.center
  new_at_pan = rt.terrain_height(PX, PZ)
  keep_xz = abs(c1.x - PX) < 1e-3 and abs(c1.z - PZ) < 1e-3
  on_surface = abs(c1.y - new_at_pan) < TOL
  height_changed = abs(new_at_pan - old_at_pan) > 1e-3
  print(f"[cam] after rebake: center=({c1.x:.2f},{c1.y:.3f},{c1.z:.2f}) "
        f"terrain_height(xz)={new_at_pan:.3f} (was {old_at_pan:.3f})  "
        f"keep_xz={keep_xz} on_surface={on_surface} height_changed={height_changed}", flush=True)
  results["rebake_keeps_xz"] = keep_xz
  results["target_on_surface_after_rebake"] = on_surface
  results["rebake_changed_height"] = height_changed

  rt._destroy_simulation()
  ez.mainThreadEnd()
  ok = all(results.values())
  print(f"\n=== terrain camera slice {'PASSED' if ok else 'FAILED'} ===", flush=True)
  for k, v in results.items():
    print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
