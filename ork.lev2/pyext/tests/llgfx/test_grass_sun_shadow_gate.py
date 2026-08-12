#!/usr/bin/env python3
###############################################################################
# GRASS SUN-SHADOW RECEIVE gate — a cast shadow lands ON the blades, measured.
#
# WHY THIS EXISTS. The carpet's fragment reads the sun cascades through the same
# _sun_shadow_factorQ(pbd._wpos, ...) every other forward-PBR surface does, and
# the geometry arrives from a TASK+MESH pair whose per-vertex varyings are an
# ARRAY rewrite of the vertex path's (fxv2_template._mesh_varying_writes). That
# rewrite is the one place where a world position could reach the fragment
# wrong, and every consumer of a bad wpos in fwdtools.i2 FAILS OPEN to 1.0 — a
# fully-lit carpet, no error, no black pixel. Nothing in the tree measured it:
# the draw gate (test_grass_taskmesh_draw.py) turns shadow casting OFF, and the
# fast-filter gate (test_sun_shadow_fastpath_gate.py) reads generated TEXT.
# So the carpet could have stopped receiving shadows and every gate stayed green.
#
# THE RIG: a tiny flat synthetic terrain authored in this file, a grass carpet
# over it, and ONE fat opaque sphere floating between a high sun and the field.
# The sphere casts a disc that lands on the blades inside the camera's view.
#
# FIVE LEGS, five ork.ecs.player.exe runs of that scene (the shipped playback
# path), differing only in what is in the scene:
#
#   bare_noocc    terrain only, nothing casting
#   bare_occ      terrain only, sphere in                  -> the RIG control
#   grass_noocc   terrain + carpet, nothing casting
#   grass_occ     terrain + carpet, sphere in              -> the SUBJECT
#   grass_occ_full  same, carpet material with the FULL PCSS filter
#                   (shadow_filter=0) instead of the shipped reduced one
#
# THE SHADOW FOOTPRINT IS NOT ASSERTED, IT IS DERIVED. The masked pixels are the
# ones the TERRAIN pair says went dark — a statement about the shadow map alone,
# made without ever looking at a grass frame, so using it to select where to
# measure the carpet is not circular. A hand-computed disc would have to be
# re-derived every time the camera or the sun moved, and would silently measure
# the wrong pixels when it drifted.
#
# THE VERDICT reads blade pixels only (the ones the carpet ADDED to the frame,
# differenced against the same scene with no carpet), so terrain showing between
# the blades cannot carry the measurement:
#
#   * grass_receives_shadow — mean luminance of blade pixels INSIDE the
#     footprint over blade pixels OUTSIDE it, in the SAME frame. Floor 0.80;
#     observed 0.22 (mac/MoltenVK, 2026-08-06). The gap to the floor is wide on
#     purpose: this gate exists to catch the shadow going AWAY (ratio -> 1.0),
#     not to pin a penumbra's exact darkness, which is a look and moves.
#   * fast_matches_full — the reduced filter (the carpet's shipped default) and
#     the full PCSS evaluator must land within 15% of each other. The reduced
#     filter is allowed to be softer; it is not allowed to stop shadowing.
#   * occluder_changed_nothing_else — outside the footprint the two carpet
#     frames must agree to within 2.5%, or the "shadow" being measured is really
#     an exposure shift. The bound is measured, not chosen: the two frames come
#     from two processes that each scatter their own carpet (see CTRL_TOL).
#
# COOKIE (cloud-shadow) leg: NOT RUN — UNVERIFIED here. Arming the sun cookie
# needs a published cloud deck (LightManager::_sun_cookie), which this rig has
# no sky for; test_cloud_shadow_cookie_gate.py owns that path generically. The
# carpet takes cookie and cascade from the SAME world position in one product
# (fwdtools.i2: _sun_shadow_factorQ(pbd._wpos,..) * _sun_cookie_sample(pbd._wpos)),
# so the leg below exercises the shared input; the cookie's own projection is
# the other gate's subject.
#
# CAPABILITY: no task (amplification) stage means no carpet at all — the gate
# SKIPS BY NAME (rc 0, no verdict emitted), read from ctx.supports_task_shader.
#
# Self-configuring: every environment variable the children need is set here.
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORKID_PERFHUD"] = "off"     # the HUD would sit in the frames this gate measures

import shutil
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

from orkengine import core          # core MUST import before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.grass import GrassFieldSource, GrassSurface
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.lev2 import GrassDrawableData
from ork.testing import verdict

OUTDIR = "/tmp/grass_sun_shadow_gate"

TERRAIN  = "grassshadow_terra"
EXTENT_M = 256.0
DIM      = 256           # bake == render resolution
CHUNK    = 128

# CARPET — knee-high, dense, well inside the camera's view.
FIELD_DIM = 256
TILE      = 2.0
GRID      = 48
LOD0      = 15.0
LOD1      = 30.0
CULL      = 42.0
CLUSTERS  = 24
BLADE     = dict(height=0.55, height_var=0.35, width=0.045, taper=0.7)

# OCCLUDER — a fat sphere over the origin. The sun travels toward +Z at 70 deg
# elevation, so the disc lands a few metres in front of the sphere, on blades.
OCC_R  = 3.0
OCC_Y  = 8.0
SUN_EL = 70.0
SUN_AZ = 0.0

# SUN INTENSITY is deliberately modest: a bright sun clips the whole carpet to
# 255 and every luminance ratio below becomes a comparison of saturated pixels.
SUN_INTENSITY = 1.6

# THE CASCADE RIG is the engine's shipped default band ladder (10 m band 0,
# x4 per band, 4 bands) rather than a rig tuned to make this scene easy — a
# gate that only shadows under its own private cascade setup proves nothing
# about the scenes that ship.
CASCADES        = 4
BAND_RADIUS     = 10.0
BAND_RATIO      = 4.0
SHADOW_MAX_DIST = 250.0

CAM_DIST   = 16.0
CAM_HEIGHT = 10.0
SNAP_FRAME = 150         # frames after first-lit; past the drawable's watchdog

SKYBOX = "<ork_envmaps2>/cold4k.xir"

SHADOW_RATIO_MAX  = 0.80   # blade luminance in-shadow / out-of-shadow
RIG_RATIO_MAX     = 0.80   # same, on the terrain — proves the sphere really casts
FAST_FULL_TOL     = 0.15   # reduced filter vs full PCSS
# OUTSIDE THE FOOTPRINT the two frames must agree — but they are two SEPARATE
# player processes drawing a stochastic carpet (per-blade placement and bend are
# regenerated per run), so the blade pixels they light are not the same pixels to
# the last one. Six runs on mac/MoltenVK 2026-08-07 spread 0.13 / 0.15 / 0.29 /
# 0.45 / 1.04 / 1.66 percent, so this bound is 1.5x the worst of them: tight
# enough that a real exposure shift (the failure it guards, which moves the whole
# frame) still trips it, loose enough that the carpet's own dither does not.
CTRL_TOL          = 0.025
MIN_CARPET_IN_MASK = 0.05  # >= 5% of the footprint must be blade pixels
MIN_MASK_PX        = 2000  # ...and the footprint must be a real region

DSL_PATH = None          # filled by main(); the scene body below reads it

# The terrain DSL is written out at run time: dsl_file= takes a module PATH and a
# gate is ONE committed file (flat-layout law). NEARLY FLAT on purpose — the
# footprint has to be a stable disc, so the ground must not tilt under it, and
# the density/dryness channels are constants so the field cannot explain a thin
# carpet.
DSL_TEXT = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain.solid import Solid


class GrassShadowHF(HeightField):
  EXTENT_M = %(extent)r
  MATERIAL_CLASS = Solid
  MATERIAL_PARAMS = {"roughness": 0.95}

  def __init__(self):
    super().__init__()
    h = (T.Fbm(frequency=2.0, octaves=3) * 0.5 + 0.5) * 0.25
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)
    self.capture(T.clamp(h * 0.0 + 1.0,  0.0, 1.0), "grass_density", cache=True)
    self.capture(T.clamp(h * 0.0 + 0.25, 0.0, 1.0), "grass_dryness", cache=True)
''' % dict(extent=EXTENT_M)


###############################################################################
# the scene — identical in every leg but for what it contains
###############################################################################

def _scene_class(carpet_material, want_occluder):
  class GrassShadowScene(Scene):
    def __init__(self):
      super().__init__()
      # AmbientLight is near zero and the skybox is dimmed: what is being
      # measured is the DIRECT term's removal, and a bright ambient floor is
      # exactly what would let a dead shadow factor still read as "dark enough".
      self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                      SkyboxIntensity=0.25, DiffuseIntensity=1.0,
                      SpecularIntensity=0.5, AmbientLight=core.vec3(0.02))
      self.sun(elevation=SUN_EL, azimuth=SUN_AZ, intensity=SUN_INTENSITY,
               cascades=CASCADES, shadow_caster=True,
               shadow_map_size=2048, shadow_max_distance=SHADOW_MAX_DIST,
               shadow_band_radius=BAND_RADIUS, shadow_band_ratio=BAND_RATIO)
      self.terrain(TERRAIN, dsl_file=DSL_PATH, render_dimension=DIM,
                   bake_dimension=DIM, chunk=CHUNK, walkable=False)
      # BOTH carpet materials are declared in EVERY leg, so every leg bakes and
      # compiles the same things and differs in ONE thing: which one is drawn.
      # shadow_filter is the ONLY difference between them (1 = the shipped
      # reduced sun filter, 0 = the full PCSS evaluator).
      src = dict(tile=TILE, grid=GRID, cull_r=CULL, lod0=LOD0, lod1=LOD1,
                 clusters=CLUSTERS, **BLADE)
      for name, filt in (("grass_mat_fast", 1.0), ("grass_mat_full", 0.0)):
        self.asset.Ptex3d(name, dsl_class=GrassSurface,
                          vertex_source=GrassFieldSource(**src),
                          fade=(LOD1, CULL, 0.85, 0.0),
                          surf=(0.82, 0.45, 4.0, 0.35),
                          shadow_filter=filt)
      occ_mat = self.asset.Ptex3d("occ_mat", dsl_class=Solid,
                                  albedo=core.vec3(0.6, 0.2, 0.2), roughness=0.9)
      if want_occluder:
        ball = self.asset.IcoSphere("occ_ball", radius=OCC_R, subdivisions=3,
                                    material=occ_mat)
        self.entity("occluder",
                    transform={"translation": core.vec3(0, OCC_Y, 0)},
                    components=[self.SG.component(nodes={"n": {"drawable": ball}})])
      if carpet_material:
        self.entity("grass_carpet", components=[self.SG.component(nodes={
            "grass": {"drawable": GrassDrawableData(
                hf_asset         = TERRAIN,
                material_asset   = carpet_material,
                field_dim        = FIELD_DIM,
                tile_size        = TILE,
                grid_dim         = GRID,
                lod0_radius      = LOD0,
                lod1_radius      = LOD1,
                cull_radius      = CULL,
                lod_ceiling      = float(CLUSTERS),
                density_scale    = 1.0,
                blade_height     = BLADE["height"],
                blade_height_var = BLADE["height_var"],
                blade_width      = BLADE["width"],
                blade_taper      = BLADE["taper"])},
        })])
  return GrassShadowScene


###############################################################################
# the legs
###############################################################################

LEGS = (
  # name             carpet material     occluder
  ("bare_noocc",     None,               False),
  ("bare_occ",       None,               True),
  ("grass_noocc",    "grass_mat_fast",   False),
  ("grass_occ",      "grass_mat_fast",   True),
  ("grass_occ_full", "grass_mat_full",   True),
)


def _run_leg(name, ecs_path, png_path):
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  # --frames well above the default: the FIRST run on a machine with cold
  # caches spends most of its budget on the terrain bake and the IBL warm-up,
  # and a gate must not fail for being early.
  cmd = [player, ecs_path, "--snapshot", png_path,
         "--snapshot-frame", str(SNAP_FRAME), "--frames", "6000",
         "--camdist", str(CAM_DIST), "--camheight", str(CAM_HEIGHT)]
  print("[grassshadow] %s" % name, flush=True)
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
  with open(os.path.join(OUTDIR, name + ".log"), "w") as fh:
    fh.write(p.stdout + p.stderr)
  return p.returncode


def _lum(path):
  return np.asarray(Image.open(path).convert("RGB")).astype(np.float32).mean(axis=2)


def _rgb(path):
  return np.asarray(Image.open(path).convert("RGB")).astype(np.float32)


###############################################################################

def main():
  global DSL_PATH
  os.makedirs(OUTDIR, exist_ok=True)
  workdir = tempfile.mkdtemp(prefix="grassshadow_")
  DSL_PATH = os.path.join(workdir, "grassshadow_hf.py")
  with open(DSL_PATH, "w") as fh:
    fh.write(DSL_TEXT)

  # ---- capability + authoring (one headless session) ----------------------
  ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  capable = bool(ctx.supports_task_shader)
  paths = {}
  if capable:
    for name, mtl, occ in LEGS:
      sd = ecs.SceneData()
      _scene_class(mtl, occ)().build(sd)
      paths[name] = os.path.join(workdir, name + ".ecs")
      with open(paths[name], "w") as fh:
        fh.write(sd.serializeJson())
  ez.mainThreadEnd()

  if not capable:
    print("GRASS-SHADOW-SKIP: ctx.supports_task_shader is False — no task "
          "(amplification) stage on this device, so there is no carpet to "
          "shadow.", flush=True)
    print("=== grass sun-shadow gate SKIPPED (no task shader stage) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0)

  # ---- render ------------------------------------------------------------
  pngs = {}
  rcs = {}
  for name, _mtl, _occ in LEGS:
    png = os.path.join(OUTDIR, name + ".png")
    if os.path.exists(png):
      os.remove(png)
    rcs[name] = _run_leg(name, paths[name], png)
    pngs[name] = png

  # ---- the checks --------------------------------------------------------
  fails = []
  notes = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      fails.append(label)

  for name, _m, _o in LEGS:
    check("%s_player_ok" % name,
          rcs[name] == 0 and os.path.exists(pngs[name]),
          "rc=%d png=%s" % (rcs[name], os.path.exists(pngs[name])))

  if fails:
    rc = verdict(False, "grass sun-shadow gate | a leg did not render | failed="
                        + ",".join(fails))
    print("=== grass sun-shadow gate FAILED ===", flush=True)
    ecs.headless_exit()
    sys.exit(rc)

  bare_n = _lum(pngs["bare_noocc"])
  bare_o = _lum(pngs["bare_occ"])
  gr_n   = _lum(pngs["grass_noocc"])
  gr_o   = _lum(pngs["grass_occ"])
  gr_f   = _lum(pngs["grass_occ_full"])

  if not (bare_n.shape == bare_o.shape == gr_n.shape == gr_o.shape == gr_f.shape):
    rc = verdict(False, "grass sun-shadow gate | frame size mismatch across legs")
    print("=== grass sun-shadow gate FAILED ===", flush=True)
    ecs.headless_exit()
    sys.exit(rc)

  # THE FOOTPRINT, derived from the terrain pair alone (see the header).
  drop = bare_n - bare_o
  mask = drop > 6.0
  ctrl = (np.abs(drop) < 1.0) & (bare_n > 8.0)
  check("footprint_is_a_region", int(mask.sum()) >= MIN_MASK_PX,
        "%d masked px (floor %d)" % (int(mask.sum()), MIN_MASK_PX))

  # ...and the RIG control: the sphere really removes the sun from the ground.
  rig = float(bare_o[mask].mean() / max(bare_n[mask].mean(), 1e-6))
  check("rig_casts_a_shadow", rig < RIG_RATIO_MAX,
        "terrain in-footprint/out ratio %.4f (ceiling %.2f)" % (rig, RIG_RATIO_MAX))

  # BLADE PIXELS ONLY — what the carpet added over the same scene without it.
  carpet = (np.abs(_rgb(pngs["grass_noocc"]) - _rgb(pngs["bare_noocc"])).max(axis=2) > 8.0)
  cover = float(carpet[mask].mean()) if mask.sum() else 0.0
  check("blades_are_in_the_footprint", cover >= MIN_CARPET_IN_MASK,
        "%.3f of the footprint is blade pixels (floor %.2f)" % (cover, MIN_CARPET_IN_MASK))

  cm = mask & carpet     # blades inside the shadow
  cc = ctrl & carpet     # blades outside it

  def blade_ratio(frame):
    return float(frame[cm].mean() / max(frame[cc].mean(), 1e-6))

  fast = blade_ratio(gr_o)
  full = blade_ratio(gr_f)
  check("grass_receives_shadow", fast < SHADOW_RATIO_MAX,
        "blade luminance in-shadow/out-of-shadow %.4f (ceiling %.2f, "
        "%d shadowed px / %d lit px)" % (fast, SHADOW_RATIO_MAX, int(cm.sum()), int(cc.sum())))
  check("full_filter_receives_shadow", full < SHADOW_RATIO_MAX,
        "full-PCSS blade ratio %.4f (ceiling %.2f)" % (full, SHADOW_RATIO_MAX))
  rel = abs(fast - full) / max(full, 1e-6)
  check("fast_matches_full", rel <= FAST_FULL_TOL,
        "reduced %.4f vs full %.4f -> %.1f%% apart (tol %.0f%%)"
        % (fast, full, rel * 100.0, FAST_FULL_TOL * 100.0))

  # Outside the footprint nothing may have moved, or the "shadow" is exposure.
  outside = float(gr_o[cc].mean() / max(gr_n[cc].mean(), 1e-6))
  check("occluder_changed_nothing_else", abs(outside - 1.0) <= CTRL_TOL,
        "carpet outside the footprint occ/noocc %.5f (tol %.2f%%)"
        % (outside, CTRL_TOL * 100.0))

  notes.append("blade lum shadowed=%.2f lit=%.2f" % (gr_o[cm].mean(), gr_o[cc].mean()))
  notes.append("COOKIE LEG NOT RUN (no cloud deck in this rig) — "
               "test_cloud_shadow_cookie_gate.py owns that path")

  ok = (len(fails) == 0)
  detail = "grass sun-shadow gate | " + " | ".join(notes + ["renders=%s" % OUTDIR])
  if fails:
    detail += " | failed=" + ",".join(fails)
  rc = verdict(ok, detail)
  print("=== grass sun-shadow gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
  ecs.headless_exit()
  sys.exit(rc)


if __name__ == "__main__":
  main()
