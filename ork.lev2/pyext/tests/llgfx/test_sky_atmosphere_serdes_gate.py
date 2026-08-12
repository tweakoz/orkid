#!/usr/bin/env ork.python
###############################################################################
# Gate — a scene-authored SkyAtmosphere survives the .ecs JSON round trip.
#
# SceneGraphSystemData._userParams is a varmap, and a varmap value holding a
# shared_ptr to a reflected object used to have no encoder: the tojson step in
# ork.scene.viewer.py wrote "SkyAtmosphere": "null:", so every knob a scene set
# (SkyExposure included) was gone before the player ever read the file. The
# varmap object codec table (ork/reflect/properties/codec.h, registered for
# pbr::skyatmospheredata_ptr_t in lev2_init.cpp) is what routes it through
# standard reflection instead. Asserts:
#   1. ENCODE: the .ecs carries a pbr::SkyAtmosphereData object node whose
#      properties hold the authored knob values (not "null:"),
#   2. DECODE: deserializing that .ecs and re-serializing reproduces the same
#      object node — proving the value came back as the CONCRETE ptr type the
#      scenegraph reads with typedValueForKey<skyatmospheredata_ptr_t>, not as
#      a bare object_ptr_t or an empty var.
#   3. HAZE TIER: the aerial-perspective / ground-haze knobs survive a decode +
#      re-encode at off-default values. These are authored at the JSON tier
#      rather than through python properties because they are reflected but not
#      yet pybound — and that is the point: the .ecs rail is what a scene file
#      actually travels on, so it is the rail that has to carry them.
#
# Non-render gate: no frames, no capture — serialization only.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object

# authored knob values — deliberately off every default
CHAIN_MIN_ANGLE = 2.0
CHAIN_MAX_HZ    = 3.5
SNAPSHOT_WIDTH  = 128
SNAPSHOT_HEIGHT = 64
SKY_EXPOSURE    = 7.5
SUN_DISC_INTENS = 250.0
SNAP_INTERVAL   = 2.5
LEVEL_BATCHES   = 3
SLICES_PER_FRAME = 4
MIPCHAIN_BUDGET = 6144
CROSSFADE_SECS  = 0.25

# the snapshot extent the engine ships at. Asserted, not merely authored: the
# extent is what the whole IBL feed's per-slice cost scales with, so a silent
# default drift is a silent performance change on every procedural-sky scene.
DEFAULT_SNAPSHOT_WH = (512, 256)

# the chained-refilter cadence ceiling the engine ships at, in Hz. Same reason it
# is asserted rather than authored: it is a measured value (the slowest rate with
# neither lag nor pop at 512x256), so drifting it silently changes the pacing of
# every procedural-sky scene. 0 would mean uncapped.
DEFAULT_CHAIN_MAX_HZ = 2.0

# the declared-cadence knob ships DISABLED (0 = keep the sun-motion trigger).
# Asserted because that zero is load-bearing: a nonzero default would retire the
# sun-angle trigger on every procedural-sky scene at once.
DEFAULT_SNAPSHOT_INTERVAL = 0.0

# the three granularity knobs ship UNSET (0), which is what routes them to the
# ORKID_MT_IBL_* env vars / the measured defaults in radiancemaps_processor.cpp.
# Asserted because a nonzero default here would take that fallback away from the
# bake path and every scene at once, silently changing IBL pacing.
DEFAULT_GRANULARITY = 0

# the publish crossfade's WALL-CLOCK bound, in seconds. Asserted because it is
# what makes a fade a duration rather than a frame count: at 0 the window is
# frames-only again and its perceived length goes back to being a function of
# the frame rate on every procedural-sky scene.
DEFAULT_CROSSFADE_MAX_SECS = 0.5

# the ground-haze layer ships at density ZERO — aerial perspective is then the
# pure geophysical medium. Asserted because that zero is what makes the graded
# look opt-IN: a nonzero default would put an artist haze on every procedural-sky
# scene in the repo at once.
DEFAULT_HAZE_DENSITY = 0.0

# haze knob values authored straight into the .ecs, deliberately off every
# default. Property NAMES here are the reflected names from describeX — a rename
# on the C++ side must break this gate, which is the coverage the round trip
# through python properties cannot give while the fields are unbound.
HAZE_AUTHORED = {
  "AerialPerspectiveEnable": False,
  "HazeDensity":             0.25,
  "HazeScaleHeight":         1.5,
  "HazePhaseG":              0.75,
  "HazeScatterTint":         [1.0, 0.9, 0.75],
  "HazeInscatterTint":       [1.1, 0.75, 0.45],
  "HazeMaxDistanceKm":       80.0,
}


def _find_atmo(node):
  """the SkyAtmosphereData property dict, wherever it sits in the .ecs tree"""
  if isinstance(node, dict):
    obj = node.get("object")
    if isinstance(obj, dict) and obj.get("class") == "pbr::SkyAtmosphereData":
      return obj["properties"]
    for v in node.values():
      found = _find_atmo(v)
      if found is not None:
        return found
  elif isinstance(node, list):
    for v in node:
      found = _find_atmo(v)
      if found is not None:
        return found
  return None


def _build_scene_data():
  atmo = lev2.SkyAtmosphereData()
  assert (atmo.ibl_snapshot_width, atmo.ibl_snapshot_height) == DEFAULT_SNAPSHOT_WH, (
      "snapshot extent default moved to %dx%d" % (atmo.ibl_snapshot_width, atmo.ibl_snapshot_height))
  assert abs(atmo.ibl_chain_max_hz - DEFAULT_CHAIN_MAX_HZ) < 1e-6, (
      "chain cadence ceiling default moved to %f Hz" % atmo.ibl_chain_max_hz)
  assert abs(atmo.ibl_snapshot_interval - DEFAULT_SNAPSHOT_INTERVAL) < 1e-6, (
      "declared snapshot interval default moved to %f s" % atmo.ibl_snapshot_interval)
  assert atmo.ibl_level_batches == DEFAULT_GRANULARITY, (
      "ibl_level_batches default moved to %d" % atmo.ibl_level_batches)
  assert atmo.ibl_slices_per_frame == DEFAULT_GRANULARITY, (
      "ibl_slices_per_frame default moved to %d" % atmo.ibl_slices_per_frame)
  assert atmo.ibl_mipchain_budget_px == DEFAULT_GRANULARITY, (
      "ibl_mipchain_budget_px default moved to %d" % atmo.ibl_mipchain_budget_px)
  assert abs(atmo.ibl_crossfade_max_secs - DEFAULT_CROSSFADE_MAX_SECS) < 1e-6, (
      "crossfade wall-clock bound default moved to %f s" % atmo.ibl_crossfade_max_secs)
  atmo.ibl_snapshot_interval   = SNAP_INTERVAL
  atmo.ibl_level_batches       = LEVEL_BATCHES
  atmo.ibl_slices_per_frame    = SLICES_PER_FRAME
  atmo.ibl_mipchain_budget_px  = MIPCHAIN_BUDGET
  atmo.ibl_crossfade_max_secs  = CROSSFADE_SECS
  atmo.ibl_chain_min_angle_deg = CHAIN_MIN_ANGLE
  atmo.ibl_chain_max_hz        = CHAIN_MAX_HZ
  atmo.ibl_snapshot_width      = SNAPSHOT_WIDTH
  atmo.ibl_snapshot_height     = SNAPSHOT_HEIGHT
  atmo.sky_exposure            = SKY_EXPOSURE
  atmo.sun_disc_intensity      = SUN_DISC_INTENS
  sd    = ecs.SceneData()
  sgsys = sd.addSceneGraphSystem()
  sgsys.declareLayer("std_forward")
  sgsys.declareParams({
    "preset":        "ForwardPBR",
    "SkySource":     "procedural",
    "SkyAtmosphere": atmo,
  })
  return sd


def _assert_knobs(props, tag):
  assert abs(props["IblChainMinAngleDeg"] - CHAIN_MIN_ANGLE) < 1e-6, (tag, props)
  assert abs(props["IblChainMaxHz"] - CHAIN_MAX_HZ) < 1e-6, (tag, props)
  assert abs(props["IblSnapshotInterval"] - SNAP_INTERVAL) < 1e-6, (tag, props)
  assert props["IblLevelBatches"] == LEVEL_BATCHES, (tag, props)
  assert props["IblSlicesPerFrame"] == SLICES_PER_FRAME, (tag, props)
  assert props["IblMipChainBudgetPx"] == MIPCHAIN_BUDGET, (tag, props)
  assert abs(props["IblCrossfadeMaxSecs"] - CROSSFADE_SECS) < 1e-6, (tag, props)
  assert props["IblSnapshotWidth"] == SNAPSHOT_WIDTH, (tag, props)
  assert props["IblSnapshotHeight"] == SNAPSHOT_HEIGHT, (tag, props)
  assert abs(props["SkyExposure"] - SKY_EXPOSURE) < 1e-6, (tag, props)
  assert abs(props["SunDiscIntensity"] - SUN_DISC_INTENS) < 1e-6, (tag, props)


def _assert_haze_defaults(props):
  """the shipped haze tier, as it reaches a .ecs with no scene authoring"""
  assert props["AerialPerspectiveEnable"] is True, props
  assert abs(props["HazeDensity"] - DEFAULT_HAZE_DENSITY) < 1e-9, (
      "ground haze default density moved to %f" % props["HazeDensity"])


def _assert_haze(props, tag):
  for key, want in HAZE_AUTHORED.items():
    got = props[key]
    if isinstance(want, list):
      assert len(got) == 3 and all(abs(a - b) < 1e-6 for a, b in zip(got, want)), (tag, key, got)
    elif isinstance(want, bool):
      assert got is want, (tag, key, got)
    else:
      assert abs(got - want) < 1e-6, (tag, key, got)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ok = False
  try:
    js1 = _build_scene_data().serializeJson()
    assert '"null:"' not in js1.replace(" ", ""), "a userparam encoded as null: " + js1
    props1 = _find_atmo(json.loads(js1))
    assert props1 is not None, "no pbr::SkyAtmosphereData node in the .ecs:\n" + js1
    _assert_knobs(props1, "encode")
    _assert_haze_defaults(props1)
    print("sky atmosphere ENCODE PASS (%d reflected properties)" % len(props1), flush=True)

    js2    = Object.deserializeJson(js1).serializeJson()
    props2 = _find_atmo(json.loads(js2))
    assert props2 is not None, "round trip LOST the atmosphere:\n" + js2
    _assert_knobs(props2, "decode")
    assert props2 == props1, "round trip altered the medium"
    print("sky atmosphere DECODE PASS (round trip identical)", flush=True)

    # haze tier — authored into the .ecs text (reflected, not yet pybound), then
    # decoded and re-encoded. _find_atmo hands back the live sub-dict, so the
    # update below edits the tree that gets re-serialized.
    tree3 = json.loads(js1)
    _find_atmo(tree3).update(HAZE_AUTHORED)
    js3    = Object.deserializeJson(json.dumps(tree3)).serializeJson()
    props3 = _find_atmo(json.loads(js3))
    assert props3 is not None, "haze round trip LOST the atmosphere:\n" + js3
    _assert_haze(props3, "haze")
    _assert_knobs(props3, "haze")
    print("sky atmosphere HAZE PASS (%d haze knobs round tripped)" % len(HAZE_AUTHORED), flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== sky atmosphere serdes gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
