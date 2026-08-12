#!/usr/bin/env python3
###############################################################################
# GRASS TASK+MESH DRAW GATE — the carpet actually drawn, by the stage it claims.
#
# The codegen gate (test_grass_taskmesh_codegen.py) proves the generated material carries a
# task stage. It cannot prove a device compiled it, a drawable bound it, or that one blade
# reached the screen. This one renders a whole scene and reads the engine's own evidence.
#
# THE RIG: a TINY synthetic terrain authored in this file (256 m square, 256^2 bake) carrying
# the four channels the carpet reads — height, normal, grass_density, grass_dryness — plus a
# GrassFieldSource/GrassSurface material and a GrassDrawableData over it. Nothing here depends
# on the forest scene or its 8192^2 atlas: the bake is seconds, and the gate runs anywhere.
#
# THREE LEGS, three separate ork.ecs.player.exe runs of that scene (the shipped playback path:
# loader -> AssetSystem wire -> the drawable's own bootstrap):
#
#   WARM      whatever the shader cache holds.
#   COLD      ORKID_DISABLE_SHADER_CACHE=1 — the generated task/mesh/fragment stages JIT from
#             scratch. A first-ever cold compile is where mesh-stage defects have shown up and
#             then failed to reproduce warm, so the cold path is exercised on demand rather
#             than by luck of cache state.
#   REFUSAL   the same scene, same assets, with the drawable pointed at a TASKLESS material
#             instead (an ordinary surface with no mesh/task stage). The drawable must SAY SO —
#             GRASS-MATERIAL-INCOMPLETE — and draw nothing. A refusal that is only written down
#             is not a refusal; this leg is the one that observes it firing.
#
# WHAT COUNTS AS THE CARPET BEING THERE. Two independent readings, because either alone lies:
#   * the engine's task counter — the drawable reports ctx->taskShaderDrawCount() (the pyext's
#     ctx.task_shader_draws) in its "task stage ENGAGED — N task+mesh draws" line, which is
#     printed only after real frames. Zero prints GRASS-TASK-NOT-ENGAGED instead.
#   * the PICTURE — the refusal leg renders the identical terrain, sky and sun with no carpet,
#     so it doubles as the grass-free control: the warm frame must differ from it over a real
#     fraction of the frame. A non-degenerate frame on its own would be satisfied by the
#     terrain alone, which is exactly the failure this gate exists to catch.
#
# CAPABILITY: a device with no task (amplification) stage cannot run any of this. The gate
# SKIPS BY NAME (rc 0, no PASS verdict emitted — it is not a pass and not a failure), read
# from ctx.supports_task_shader in the same headless session that authors the scenes.
#
# Self-configuring: every environment variable the children need is set here, so the default
# invocation takes none.
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORKID_PERFHUD"] = "off"     # the HUD would sit in the frames this gate measures

import re
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

OUTDIR = "/tmp/grass_taskmesh_draw"

TERRAIN   = "grassgate_terra"
EXTENT_M  = 256.0
DIM       = 256          # bake == render resolution (1 texel / m at this extent)
CHUNK     = 128

# CARPET SIZING — a knee-high field over the ground the fixed camera looks at. grid*tile
# (96 m) covers 2*cull with room for the fade, exactly as the drawable's header requires.
FIELD_DIM = 256          # <= the bake dim (the drawable refuses otherwise)
TILE      = 2.0
GRID      = 48
LOD0      = 15.0
LOD1      = 30.0
CULL      = 42.0
CLUSTERS  = 24           # mesh workgroups per full tile -> 24*8 = 192 blades / 4 m^2
BLADE     = dict(height=0.55, height_var=0.35, width=0.045, taper=0.7)

# CAMERA — the player's orbit camera (eye at (d,h,d) looking at (0,2,0)). The terrain this
# DSL bakes sits around y=4..5 m, so this eye stands ~3 m above it and looks down the slope:
# the carpet fills the lower two thirds of the frame out to its cull ring.
CAM_DIST   = 14.0
CAM_HEIGHT = 8.0
SNAP_FRAME = 150         # frames after first-lit; well past the drawable's 60-frame watchdog

# the terrain-side skybox + sun. Deliberately shadow_caster=False: the carpet's shadowing is
# Phase 5's subject, and a cascade pass per frame is time this gate does not need to spend.
SKYBOX = "<ork_envmaps2>/cold4k.xir"

MIN_MEAN     = 1.0       # 8-bit mean over the whole frame (a black frame is not a picture)
MIN_SPREAD   = 0.03      # luminance spread (rejects a flat fill) — _ork.hypermesh.validate.py:50
MIN_CARPET   = 0.02      # >= 2% of pixels must differ from the SAME scene without the carpet

DSL_PATH = None          # filled by main(); the scene body below reads it

# The terrain DSL is written out at run time: dsl_file= takes a module PATH and a gate is ONE
# committed file (flat-layout law). Small, smooth relief — the blades conform to the ground
# normal, so a violent heightfield would be testing the terrain, not the carpet.
DSL_TEXT = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain.solid import Solid


class GrassGateHF(HeightField):
  EXTENT_M = %(extent)r
  MATERIAL_CLASS = Solid
  MATERIAL_PARAMS = {"roughness": 0.95}

  def __init__(self, amplitude_m=6.0):
    super().__init__()
    h = (T.Fbm(frequency=2.0, octaves=4) * 0.5 + 0.5) * float(amplitude_m)
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)   # the bake derives the normal map from the surface
    elev = T.normalize(h)
    # DENSITY is deliberately high and only gently modulated: this gate measures whether the
    # carpet is drawn at all, so the field must not be able to explain an empty frame.
    self.capture(T.clamp(0.75 + 0.25 * (1.0 - elev), 0.0, 1.0), "grass_density", cache=True)
    self.capture(T.clamp(0.15 + 0.70 * elev, 0.0, 1.0), "grass_dryness", cache=True)
''' % dict(extent=EXTENT_M)


###############################################################################
# the scene — identical in every leg but for WHICH material the drawable is handed
###############################################################################

def _scene_class(material_asset):
  class GrassGateScene(Scene):
    def __init__(self):
      super().__init__()
      self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                      SkyboxIntensity=0.7, DiffuseIntensity=1.0,
                      SpecularIntensity=0.5, AmbientLight=core.vec3(0.08))
      self.sun(elevation=40.0, azimuth=210.0, intensity=3.0,
               cascades=1, shadow_caster=False)
      self.terrain(TERRAIN, dsl_file=DSL_PATH, render_dimension=DIM,
                   bake_dimension=DIM, chunk=CHUNK, walkable=False)
      # BOTH materials are declared in EVERY leg, so the legs bake and compile exactly the
      # same things and differ in ONE thing only: which of them the drawable was handed.
      self.asset.Ptex3d(
          "grass_mat", dsl_class=GrassSurface,
          vertex_source=GrassFieldSource(
              tile=TILE, grid=GRID, cull_r=CULL, lod0=LOD0, lod1=LOD1, clusters=CLUSTERS,
              **BLADE),
          fade=(LOD1, CULL, 0.85, 0.0), surf=(0.82, 0.45, 4.0, 0.35))
      # the TASKLESS twin the refusal leg points at: an ordinary surface with no vertex
      # source at all, so it carries neither mesh technique and cannot amplify anything.
      # NOT the terrain's own material — that one's mesh techniques come and go with
      # ORKID_TERRAIN_MESHSHADER, which would make the refusal's NAME depend on ambient env.
      self.asset.Ptex3d("taskless_mat", dsl_class=Solid,
                        albedo=core.vec3(0.30, 0.45, 0.18), roughness=0.9)
      self.entity("grass_carpet", components=[self.SG.component(nodes={
          "grass": {"drawable": GrassDrawableData(
              hf_asset       = TERRAIN,
              material_asset = material_asset,
              field_dim      = FIELD_DIM,
              tile_size      = TILE,
              grid_dim       = GRID,
              lod0_radius    = LOD0,
              lod1_radius    = LOD1,
              cull_radius    = CULL,
              lod_ceiling    = float(CLUSTERS),
              density_scale  = 1.0,
              blade_height     = BLADE["height"],
              blade_height_var = BLADE["height_var"],
              blade_width      = BLADE["width"],
              blade_taper      = BLADE["taper"])},
      })])
  return GrassGateScene


###############################################################################
# the legs
###############################################################################

def _run_leg(name, ecs_path, png_path, cold):
  env = dict(os.environ)
  if cold:
    env["ORKID_DISABLE_SHADER_CACHE"] = "1"   # full JIT of the task/mesh/fragment stages
  else:
    env.pop("ORKID_DISABLE_SHADER_CACHE", None)
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  # --frames well above the 1200 default: the FIRST run on a machine whose caches are empty
  # spends most of that budget waiting on the terrain bake and the IBL cold start (measured
  # 1196 of 1200 before the composite went lit), and a gate must not fail for being early.
  cmd = [player, ecs_path, "--snapshot", png_path,
         "--snapshot-frame", str(SNAP_FRAME), "--frames", "6000",
         "--camdist", str(CAM_DIST), "--camheight", str(CAM_HEIGHT)]
  print("[grassdraw] %s: %s" % (name, " ".join(cmd)), flush=True)
  p = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=900)
  log = p.stdout + p.stderr
  # the whole child log is kept: a leg that fails on a machine nobody is sitting at is
  # unreadable from the summary alone, and these are the only record of what it said.
  with open(os.path.join(OUTDIR, name + ".log"), "w") as fh:
    fh.write(log)
  for line in log.splitlines():
    if "GRASS" in line:
      print("  [%s] %s" % (name, line.strip()), flush=True)
  return p.returncode, log


def _engaged_count(log):
  """N from the drawable's 'task stage ENGAGED — N task+mesh draws' line — the engine's own
  ctx.task_shader_draws at the moment its watchdog reported."""
  m = re.search(r"task stage ENGAGED . (\d+) task\+mesh draws in (\d+) frames", log)
  return (int(m.group(1)), int(m.group(2))) if m else (None, None)


def _frame(path):
  return np.asarray(Image.open(path).convert("RGB")).astype(np.float32)


def _stats(img):
  lum = img.mean(axis=2)
  return dict(mean=float(img.mean()),
              spread=float((lum.max() - lum.min()) / 255.0))


###############################################################################

def main():
  global DSL_PATH
  os.makedirs(OUTDIR, exist_ok=True)
  workdir = tempfile.mkdtemp(prefix="grassdraw_")
  DSL_PATH = os.path.join(workdir, "grassgate_hf.py")
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
    for leg, mtl in (("grass", "grass_mat"), ("refusal", "taskless_mat")):
      sd = ecs.SceneData()
      _scene_class(mtl)().build(sd)
      paths[leg] = os.path.join(workdir, leg + ".ecs")
      with open(paths[leg], "w") as fh:
        fh.write(sd.serializeJson())
  ez.mainThreadEnd()

  if not capable:
    # named SKIP: not a pass, not a failure. No TESTVERDICT line is emitted on purpose.
    print("GRASS-DRAW-SKIP: ctx.supports_task_shader is False — this device has no task "
          "(amplification) stage, so the grass carpet cannot be drawn here at all.", flush=True)
    print("=== grass task+mesh draw gate SKIPPED (no task shader stage) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0)

  # ---- run the three legs -------------------------------------------------
  legs = {}
  for name, scene, cold in (("warm", "grass", False),
                            ("cold", "grass", True),
                            ("refusal", "refusal", False)):
    png = os.path.join(OUTDIR, name + ".png")
    if os.path.exists(png):
      os.remove(png)
    rc, log = _run_leg(name, paths[scene], png, cold)
    legs[name] = dict(rc=rc, log=log, png=png)

  # ---- the checks ---------------------------------------------------------
  fails = []
  notes = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      fails.append(label)

  for name in ("warm", "cold", "refusal"):
    L = legs[name]
    check("%s_player_ok" % name, L["rc"] == 0 and os.path.exists(L["png"]),
          "rc=%d png=%s" % (L["rc"], os.path.exists(L["png"])))

  # 1. the carpet engaged the amplification stage, warm and cold
  for name in ("warm", "cold"):
    n, frames = _engaged_count(legs[name]["log"])
    check("%s_task_shader_draws" % name, (n is not None) and n > 0,
          ("task_shader_draws=%s over %s frames" % (n, frames)) if n is not None else
          "no 'task stage ENGAGED' line — the pass carried no task stage")
    refused = re.search(r"GRASS-[A-Z-]+:[^\n]*", legs[name]["log"])
    check("%s_no_refusal" % name, refused is None,
          refused.group(0)[:150] if refused else "no named refusal on the positive leg")

  # REPORTED, not asserted: with the cache bypassed every stage is re-parsed and re-written,
  # so the cold leg's datablock writes stand well above the warm leg's on a primed tree. It
  # stays a note because a machine whose cache was empty before the gate ran would see the
  # warm leg pay those same first-time compiles — a real difference that says nothing.
  notes.append("shader datablock writes warm=%d cold=%d"
               % (legs["warm"]["log"].count("writing to cache"),
                  legs["cold"]["log"].count("writing to cache")))

  # 2. the refusal fired BY NAME, and nothing pretended to be the carpet
  rlog = legs["refusal"]["log"]
  check("refusal_named", "GRASS-MATERIAL-INCOMPLETE" in rlog,
        (re.search(r"GRASS-MATERIAL-INCOMPLETE[^\n]*", rlog).group(0)[:150]
         if "GRASS-MATERIAL-INCOMPLETE" in rlog else "no named refusal on stdout"))
  check("refusal_drew_nothing", "task stage ENGAGED" not in rlog,
        "no ENGAGED line (the refusal exits the bootstrap before any draw is configured)")

  # 3. the picture: non-degenerate, and the carpet is IN it (differential against the
  #    refusal leg's frame — the same scene, same camera, no carpet)
  try:
    warm = _frame(legs["warm"]["png"])
    cold = _frame(legs["cold"]["png"])
    bare = _frame(legs["refusal"]["png"])
    ws = _stats(warm)
    check("warm_frame_nondegenerate", ws["mean"] >= MIN_MEAN and ws["spread"] >= MIN_SPREAD,
          "mean=%.2f spread=%.3f" % (ws["mean"], ws["spread"]))
    if warm.shape == bare.shape:
      d = np.abs(warm - bare).max(axis=2)
      carpet = float((d > 8.0).mean())
      check("carpet_is_in_the_picture", carpet >= MIN_CARPET,
            "%.4f of the frame differs from the same scene with no carpet (floor %.2f)"
            % (carpet, MIN_CARPET))
    else:
      check("carpet_is_in_the_picture", False, "frame size mismatch")
    if warm.shape == cold.shape:
      # REPORTED, not asserted: the warm/cold pair is a smoke, not the parity oracle.
      dc = np.abs(warm - cold)
      notes.append("warm-vs-cold frame delta mean=%.3f max=%d"
                   % (float(dc.mean()), int(dc.max())))
  except Exception as e:
    check("frames_readable", False, repr(e))

  ok = (len(fails) == 0)
  detail = "grass task+mesh draw gate | " + " | ".join(notes + ["renders=%s" % OUTDIR])
  if fails:
    detail += " | failed=" + ",".join(fails)
  rc = verdict(ok, detail)
  print("=== grass task+mesh draw gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
  ecs.headless_exit()
  sys.exit(rc)


if __name__ == "__main__":
  main()
