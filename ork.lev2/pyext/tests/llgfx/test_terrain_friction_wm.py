#!/usr/bin/env python3
###############################################################################
# W·M SURFACE RESPONSE — physics-leg oracle (the gate for per-contact friction
# modulation on the terrain heightfield collider).
#
# The terrain collider's per-contact friction is  friction(contact) = base +
# W(x,z)·M[:,friction], where W is the SAME baked RGBA class-weight capture the
# material consumes and M[:,friction] is the reflected `friction_rows` vector on
# BulletShapeTerrainData. This oracle drives a minimal headless (offscreen) ECS
# bullet sim and measures the observable proxy the spec names: SLIDE DISPLACEMENT
# of a dynamic capsule on a fixed slope, one capsule on each surface class.
#
# Synthetic corpus (deterministic, analytic):
#   height.exr   — a constant-slope ramp (height = SLOPE*row): a body slides in +/-z.
#   <cap>.exr    — RGBA class weights, LEFT half (x<0) = class-0, RIGHT half = class-1.
#   manifest     — extent_m so the collider resolves world scale.
# friction_rows = [-0.3, +0.4, 0, 0]: class-0 REDUCES friction (talus — slides more),
# class-1 RAISES it (grip — slides less). The natural residual carries delta 0.
#
# Checks (base combined friction is 0.3 here; deltas move class-0 -> mu 0.0, class-1
# -> mu 0.7; both < tan(theta) so both slide, class-0 strictly faster):
#   o1  OFF SYMMETRY   — with surface_weights unset the two mirrored capsules slide
#       an IDENTICAL distance (feature off = plain collider, no class bias). This is the
#       control: the capsules are physically identical apart from their x (class) side.
#   o2  ON ORDERING    — with W·M on, the class-0 (left) capsule slides strictly
#       FARTHER than the class-1 (right) capsule (low friction vs high friction). Since
#       o1 proves they are otherwise identical, this asymmetry is CAUSED by W·M, and its
#       SIGN (class-0 slides more) confirms friction_rows is applied with the right sense.
#   o3  ASYMMETRY SUBSTANTIAL — the class-0/class-1 slide ratio is well above 1 (the
#       friction contrast is real, not float noise). Ratio is scale-free, so it is robust
#       to how many sim steps the frame budget advances.
#   o4  SLIDING OCCURRED — both configs actually slide (guards a no-op pass).
# (o1..o4 are all WITHIN-a-single-run comparisons — robust to sim-step-count variance;
#  cross-run magnitude compares are deliberately avoided.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json, shutil

from orkengine import core          # core before lev2 (import-order law)
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3, CrcStringProxy

_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)
from ork.testing.app import headless_app

_tok = CrcStringProxy()

DIM      = 64
EXTENT   = 64.0
SLOPE    = 2.0            # tan(theta): steep enough both classes slide (theta ~ 63 deg)
ASSET    = "wmfric_oracle"
CAPTURE  = "surfclass"
Z0       = 10.0          # spawn z (upper slope, room to slide toward -z)
STEPS    = 1600          # sim advance frames after spawn
ROWS     = [-0.3, 0.4, 0.0, 0.0]


def gen_assets():
  """Write the synthetic terrain artifacts into the REAL assetcache (the collider
  resolves the asset-wired form <assetcache>/terrain/<asset>/...). MUST run after
  engine init — <assetcache> is unresolved before then."""
  base = core.Path.expandPathString("<assetcache>/terrain/%s" % ASSET)
  assert "<assetcache>" not in base, "assetcache unresolved (call after engine init)"
  os.makedirs(base, exist_ok=True)

  def wexr(path, fmt, fill):
    img = lev2.Image(); img.initWithFormat(DIM, DIM, fmt)
    for y in range(DIM):
      for x in range(DIM):
        fill(img.pixel32f(x, y), x, y)
    img.writeToFile(path)

  # constant-slope ramp along rows (world z); height in TRUE METERS.
  wexr(base + "/height.exr", _tok.R32F, lambda p, x, y: p.__setitem__(0, SLOPE * float(y)))

  # RGBA class weights: left half (image col < DIM/2 -> world x<0) = class 0, right = class 1.
  def wfill(p, x, y):
    if x < DIM // 2:
      p[0], p[1], p[2], p[3] = 1.0, 0.0, 0.0, 0.0
    else:
      p[0], p[1], p[2], p[3] = 0.0, 1.0, 0.0, 0.0
  wexr(base + "/%s.exr" % CAPTURE, _tok.RGBA32F, wfill)

  json.dump({"scale": {"extent_m": EXTENT}}, open(base + "/%s.terrain.json" % ASSET, "w"))
  return base


def build_scene(surface_weights):
  scene = ecs.SceneData()
  sp = scene.declareSystem("BulletSystem")
  sp.simulationRate = 240.0
  sp.linGravity = vec3(0, -9.8, 0)
  sp.test_deactivation = False
  # BulletSystem hard-requires a SceneGraphSystem (its debug drawable rides std_forward).
  sg = scene.declareSystem("SceneGraphSystem")
  sg.declareLayer("std_forward")
  sg.declareParams({"preset": "ForwardPBR"})

  at = scene.declareArchetype("TerrArch")
  ct = at.declareComponent("BulletObjectComponent")
  shp = ecs.BulletShapeTerrainData()
  shp.hf_asset = ASSET
  shp.render_dimension = 0
  if surface_weights:
    shp.surface_weights = CAPTURE
    shp.friction_rows = list(ROWS)
  ct.shape = shp
  ct.mass = 0.0
  ct.friction = 0.3          # base terrain friction (combined with capsule 1.0 -> 0.3)
  ct.restitution = 0.0
  s1 = scene.declareSpawner("terr_spawner")
  s1.archetype = at
  s1.autospawn = True
  s1.transform.translation = vec3(0, 0, 0)

  ac = scene.declareArchetype("CapArch")
  cc = ac.declareComponent("BulletObjectComponent")
  cap = ecs.BulletShapeCapsuleData()
  cap.radius = 0.5
  cap.extent = 1.0
  cc.shape = cap
  cc.mass = 1.0
  cc.friction = 1.0
  cc.restitution = 0.0
  cc.allowSleeping = False
  cc.angularFactor = vec3(0, 0, 0)   # lock rotation -> PURE slide (no rolling)
  s2 = scene.declareSpawner("cap_spawner")
  s2.archetype = ac
  s2.autospawn = False
  return scene


def run(H, surface_weights):
  """Spawn one capsule on each half, step, return (left_slide, right_slide) in meters."""
  scene = build_scene(surface_weights)
  ctrl = ecs.Controller()
  ctrl.bindScene(scene)
  ctrl.gpuInit(H.ctx)
  ctrl.createSimulation()
  ctrl.startSimulation()
  ctrl.installUpdateCallbackOnEzApp(H.ezapp)
  ctrl.installGpuUpdateCallbackOnEzApp(H.ezapp)
  ctrl.installRenderCallbackOnEzApp(H.ezapp)

  row = Z0 + (DIM / 2 - 0.5)
  y0 = SLOPE * row + 2.0     # a couple meters above the ramp surface -> drop + settle + slide

  def spawn(x):
    SAD = ecs.SpawnAnonDynamic("cap_spawner")
    SAD.overridexf.translation = vec3(x, y0, Z0)
    return ctrl.spawnEntity(SAD)

  H.run_frames(5)
  eL = spawn(-16.0)          # x<0 -> class 0 (row -0.3, low friction)
  eR = spawn(+16.0)          # x>0 -> class 1 (row +0.4, high friction)
  H.run_frames(STEPS)

  sim = ctrl.simulation
  def zpos(e):
    ent = sim.findEntityByRef(e.id)
    assert ent is not None, "spawned capsule ref not found in simulation"
    return ent.transform.translation.z
  zL, zR = zpos(eL), zpos(eR)

  ctrl.uninstallRenderCallbackOnEzApp(H.ezapp)
  ctrl.uninstallGpuUpdateCallbackOnEzApp(H.ezapp)
  ctrl.uninstallUpdateCallbackOnEzApp(H.ezapp)
  ctrl.stopSimulation()
  ctrl.terminateSimulation()
  # slide distance = how far each moved downhill from the common start z.
  return (Z0 - zL, Z0 - zR)


def main():
  results = {}
  base_dir = None
  with headless_app(width=128, height=96, offscreen=True, lockstep=True) as H:
    base_dir = gen_assets()
    on_L, on_R   = run(H, True)
    off_L, off_R = run(H, False)

  print("ON  slide: class0(left)=%.3f  class1(right)=%.3f" % (on_L, on_R), flush=True)
  print("OFF slide: left=%.3f  right=%.3f" % (off_L, off_R), flush=True)

  results["o1_off_symmetry"]      = abs(off_L - off_R) < 0.05
  results["o2_on_ordering"]       = (on_L > on_R + 0.4)
  results["o3_asymmetry_ratio"]   = (on_R > 1e-3) and (on_L / on_R > 1.2)
  results["o4_sliding_occurred"]  = (on_R > 0.5) and (off_L > 0.5)

  # best-effort cleanup of the synthetic asset dir (leave on failure for inspection)
  ok = all(results.values())
  if ok and base_dir:
    shutil.rmtree(base_dir, ignore_errors=True)

  print("\n=== W·M surface-response physics oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
  for k, v in results.items():
    print("    %-22s %s" % (k, "ok" if v else "FAIL"), flush=True)
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
