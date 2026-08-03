#!/usr/bin/env python3
###############################################################################
# W7-S3 — TERRAIN SUN-SHADOW CASTER GATE (two runtime-convicted holes, one rig).
#
# An isolated raised block ("mesa") is placed UP-SUN of the origin at an azimuth
# the test varies, so its shadow always lands on the ground the fixed orbit
# camera is looking at. Rotating the azimuth therefore moves the CASTER without
# moving either the receiver or the camera — which is what separates the two
# defects this gate stands over:
#
#   H3 (caster set rode the eye frustum): at az=135 the mesa itself is OUTSIDE
#       the camera frustum while the ground it shadows is still on screen.
#       Terrain consumed the EYE survivor set in the sun-cascade depth passes,
#       so the shadow vanished with the caster (measured: ground 31.9 -> 61.0).
#   H1 (toward-light extrusion was elevation-blind): at the SHIPPED
#       ShadowMaxDistance the ~600m-out crest is ~570m away ALONG the light at
#       15 degrees elevation, and the band's window reached 260m — so it cast
#       nothing on the near ground (measured: 31.9 -> 61.1; only a 2000m
#       distance recovered it).
#
# Both are asserted DIFFERENTIALLY against a same-rig control whose sun simply
# does not cast: the control calibrates what "unshadowed" reads as on this
# machine, so the gate never depends on an absolute pixel value.
#
# The captures go through ork.ecs.player.exe rather than ork.testing's
# capture_app because the observable IS the .ecs playback path: the terrain
# drawable, the frame prologue's union sun cull and the cascade depth passes
# only exist there. The authoring half uses the harness.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene import Scene
from ork.testing import verdict

OUTDIR = "/tmp/terrain_shadow_caster_gate"

# ROI: the lit/shadowed ground the mesa's shadow falls across, in the fixed
# orbit camera's 1280x720 frame. Deliberately wide and flat — the discriminating
# signal is a whole shadowed apron, not an edge.
ROI = (slice(420, 700), slice(480, 1200))

SUN_ELEV_DEG = 15.0     # low enough that the elevation-blind extrusion falls short
MESA_REACH_M = 550.0    # up-sun distance to the crest (its shadow lands near origin)
MESA_AMP_M   = 150.0
EXTENT_M     = 2000.0
DIM          = 384
CHUNK        = 128

# The terrain DSL is written out at run time: dsl_file= takes a module PATH, and
# a gate is ONE committed file (flat-layout law). Authored in world meters via
# the gradient trick documented inline.
DSL_PATH = None  # filled in by main(), read by the Scene body below
DSL_TEXT = '''
import math
from orkengine.core import vec3 as _vec3
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain.solid import Solid


class GateSolid(Solid):
  def __init__(self, ctx, *, albedo=_vec3(0.55, 0.5, 0.4), roughness=0.9,
               metallic=0.0, instance_variation=0.0):
    self.surface(albedo=albedo, metallic=metallic, roughness=roughness, cull="front")


def _world_gradient(dir_x, dir_z, extent_m):
  """T.gradient computes dot(uv,dir)*scale+bias with uv in [0,1) mapping to
  world via uv = world/EXTENT + 0.5; this scale/bias pair makes the field equal
  dot(world_xz, dir) directly, IN METERS, so the masks below are world-authored."""
  return T.gradient(dir_x=dir_x, dir_y=dir_z, scale=extent_m,
                    bias=-0.5 * extent_m * (dir_x + dir_z))


class GateMesaHF(HeightField):
  EXTENT_M = %(extent)r
  MATERIAL_CLASS = GateSolid

  def __init__(self, az_deg=90.0, reach_m=550.0, half_len_m=150.0,
               half_wid_m=150.0, amplitude_m=150.0, soft_m=20.0):
    super().__init__()
    az = math.radians(float(az_deg))
    tx, tz = math.sin(az), math.cos(az)   # sun TRAVEL direction (Scene.sun convention)
    ax, azd = -tx, -tz                    # up-sun axis: the mesa sits here
    px, pz = tz, -tx                      # perpendicular axis
    fa = _world_gradient(ax, azd, self.EXTENT_M)
    fp = _world_gradient(px, pz, self.EXTENT_M)
    reach, hl, hw, soft = float(reach_m), float(half_len_m), float(half_wid_m), float(soft_m)
    # a world-meter box from two opposing smoothstep edges (T.band is an
    # elevation-percentile helper, not a box mask over arbitrary bounds)
    mask_a = T.smoothstep(fa, reach - hl - soft, reach - hl + soft) * \\
             T.smoothstep(fa, reach + hl + soft, reach + hl - soft)
    mask_p = T.smoothstep(fp, -hw - soft, -hw + soft) * \\
             T.smoothstep(fp, hw + soft, hw - soft)
    self.capture(mask_a * mask_p * float(amplitude_m), "height")
''' % dict(extent=EXTENT_M)


def _scene_class(az_deg, casts):
  class MesaScene(Scene):
    def __init__(self):
      super().__init__()
      self.scenegraph(preset="ForwardPBR",
                      skybox_path="<ork_envmaps2>/cold4k.xir",
                      SkyboxIntensity=0.6, DiffuseIntensity=1.0,
                      SpecularIntensity=0.5, AmbientLight=core.vec3(0.05))
      # every shadow tunable SHIPPED: the gate's whole point is that the
      # defaults deliver the caster, so nothing here may tune them.
      self.sun(elevation=SUN_ELEV_DEG, azimuth=az_deg, cascades=2,
               shadow_map_size=1024, shadow_caster=casts, intensity=3.0)
      self.terrain("mesa", dsl_file=DSL_PATH,
                   render_dimension=DIM, bake_dimension=DIM, chunk=CHUNK,
                   dsl_kwargs=dict(az_deg=az_deg, reach_m=MESA_REACH_M,
                                   half_len_m=150.0, half_wid_m=150.0,
                                   amplitude_m=MESA_AMP_M, soft_m=20.0),
                   walkable=False)
  return MesaScene


def _roi_mean(path):
  a = np.asarray(Image.open(path).convert("RGB")).astype(float)
  return float(a[ROI[0], ROI[1]].mean())


def main():
  global DSL_PATH
  os.makedirs(OUTDIR, exist_ok=True)
  workdir = tempfile.mkdtemp(prefix="terrshadow_")
  DSL_PATH = os.path.join(workdir, "gate_mesa_hf.py")
  with open(DSL_PATH, "w") as fh:
    fh.write(DSL_TEXT)

  cases = [("in_frustum", 45.0, True),    # caster visible to the camera
           ("out_frustum", 135.0, True),  # SAME rig, caster rotated off screen (H3)
           ("control", 135.0, False)]     # SAME rig, sun casts nothing (calibration)

  # ---- author: one .ecs per case (harness lifecycle) ----------------------
  ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  paths = {}
  for name, az, casts in cases:
    sd = ecs.SceneData()
    _scene_class(az, casts)().build(sd)
    paths[name] = os.path.join(workdir, name + ".ecs")
    with open(paths[name], "w") as fh:
      fh.write(sd.serializeJson())
  ez.mainThreadEnd()

  # ---- play each scene offscreen and capture the settled frame ------------
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  means = {}
  fails = []
  for name, _, _ in cases:
    png = os.path.join(OUTDIR, name + ".png")
    pr = subprocess.run([player, paths[name], "--snapshot", png],
                        timeout=180, capture_output=True, text=True)
    if pr.returncode != 0 or not os.path.exists(png):
      fails.append("%s: player rc=%d" % (name, pr.returncode))
      means[name] = float("nan")
      continue
    means[name] = _roi_mean(png)

  ok = not fails
  detail = "terrain sun-shadow caster gate"
  if ok:
    lit = means["control"]
    # DIFFERENTIAL: both casting poses must darken the SAME ground well below
    # the non-casting control, and must agree with each other — agreement is
    # the view-independence claim (H3), the darkening at shipped knobs is the
    # elevation-correct reach (H1).
    shadowed_in  = means["in_frustum"] < 0.7 * lit
    shadowed_out = means["out_frustum"] < 0.7 * lit
    agree        = abs(means["in_frustum"] - means["out_frustum"]) <= 0.1 * lit
    ok = shadowed_in and shadowed_out and agree
    detail += (" | control(no caster)=%.2f in_frustum=%.2f out_frustum=%.2f"
               " | shadowed in:%s out:%s | view-independent:%s | %s"
               % (lit, means["in_frustum"], means["out_frustum"],
                  shadowed_in, shadowed_out, agree, OUTDIR))
  else:
    detail += " | " + "; ".join(fails)

  rc = verdict(ok, detail)
  ecs.headless_exit()
  sys.exit(rc)


if __name__ == "__main__":
  main()
