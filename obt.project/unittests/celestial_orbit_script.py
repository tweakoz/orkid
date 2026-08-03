#!/usr/bin/env python3
###############################################################################
# celestial_orbit_script.py — unit test for the SIM-SIDE per-frame script
# (obt.project/scripts/ork/hypergraph/ecs/scene/_celestial_orbit.py): that one
# shared script aims a sun by the sun's angles and a moon by the moon's, and
# publishes the night policy through the ENTITY-VARMAP LIGHT BRIDGE keys
# SceneGraphSystem::_updateLightBridge actually reads.
#
# Runs STANDALONE with plain CPython — no engine, no staging:
#     python3 obt.project/unittests/celestial_orbit_script.py
#
# HOW: the script's only engine dependency is `from orkengine.ecssim import vec3,
# quat` (two constructors and a quat multiply), so the probe STUBS that module and
# drives onUpdate with a fake component/entity over scn_procsky.py's real clock.
# The stubbing happens in a SUBPROCESS — this file is also its own probe, re-exec'd
# with --emit-track — because poisoning sys.modules["orkengine"] would wreck every
# other test sharing the collector's interpreter (ork.test.python.unittests.all.py).
#
# WHAT IT DOES NOT COVER: that the bridge then lands on the GPU. That is the
# scene-run gate's job. What it does cover is everything that silently rots — a
# renamed bridge key, a body switch that aims the wrong angles, the by-path module
# load the subinterpreter needs, and the never-both-cast law along a real sky track.
###############################################################################

import importlib.util
import json
import math
import os
import subprocess
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.join(_HERE, "..", "scripts")
_SCENE_DIR = os.path.join(_SCRIPTS, "ork", "hypergraph", "ecs", "scene")

# the policy's own constants, so a retune moves the expectations with it (import
# by path — the module has no dependencies at all)
_spec = importlib.util.spec_from_file_location(
  "_night_policy", os.path.join(_SCENE_DIR, "_night_policy.py"))
_policy = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_policy)

# scn_procsky.py's sky: a mid-latitude late-summer day in 60 wall seconds, opened
# just before dawn — so a 0..60s track is one whole day AND one whole night.
PROBE_CONFIG = {"latitude_deg": 45.0, "day_of_year": 220.0,
                "time_of_day": 4.0, "time_scale": 1440.0}
SUN_INTENSITY  = 4.0
MOON_INTENSITY = 0.4
TRACK_SECONDS  = 60.0
TRACK_SAMPLES  = 241


###############################################################################
# the probe — runs in its own interpreter (see header)
###############################################################################


def _emit_track():
  import types

  sys.path.insert(0, os.path.abspath(_SCRIPTS))

  ecssim = types.ModuleType("orkengine.ecssim")

  class _vec3:
    def __init__(self, x, y, z):
      self.value = (x, y, z)

  class _quat:
    def __init__(self, axis, angle):
      self.axis, self.angle = axis.value, angle

    def __mul__(self, other):
      # the script builds azimuth(+Y) * elevation(+X); keep both so the test can
      # tell a sun-aimed light from a moon-aimed one.
      return {"az_axis": self.axis, "az_angle": self.angle,
              "el_axis": other.axis, "el_angle": other.angle}

  ecssim.vec3, ecssim.quat = _vec3, _quat
  package = types.ModuleType("orkengine")
  package.ecssim = ecssim
  sys.modules["orkengine"] = package
  sys.modules["orkengine.ecssim"] = ecssim

  def _load(name, filename):
    spec = importlib.util.spec_from_file_location(
      name, os.path.join(_SCENE_DIR, filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

  celestial = _load("_probe_celestial", "_celestial.py")
  table = {
    "sun":  celestial.normalize_config(dict(PROBE_CONFIG, body="sun",
                                            base_intensity=SUN_INTENSITY)),
    "moon": celestial.normalize_config(dict(PROBE_CONFIG, body="moon",
                                            base_intensity=MOON_INTENSITY)),
  }
  os.environ[celestial.CONFIG_ENV_KEY] = json.dumps(table)

  orbit = _load("_probe_orbit", "_celestial_orbit.py")

  class _Vars:
    pass

  class _Entity:
    def __init__(self, eid, name):
      self.id, self.name = eid, name
      self.vars = _Vars()
      self.orientation = None

  class _Component:
    def __init__(self, entity):
      self.entity = entity

  class _UpdInfo:
    def __init__(self, abstime):
      self.abstime = abstime

  sun = _Component(_Entity(1, "sun"))
  moon = _Component(_Entity(2, "moon"))

  track = []
  for i in range(TRACK_SAMPLES):
    t = TRACK_SECONDS * i / (TRACK_SAMPLES - 1)
    orbit.onUpdate(sun, _UpdInfo(t))
    orbit.onUpdate(moon, _UpdInfo(t))
    sv, mv = sun.entity.vars, moon.entity.vars
    track.append({
      "t": t,
      "sun_elevation": sv.sun_elevation,
      "moon_elevation": sv.moon_elevation,
      "moon_illumination": sv.moon_illumination,
      "sun_intensity": sv.light_intensity,
      "sun_casts": sv.light_casts_shadows,
      "moon_intensity": mv.light_intensity,
      "moon_casts": mv.light_casts_shadows,
      "sun_azimuth_angle": sun.entity.orientation["az_angle"],
      "moon_azimuth_angle": moon.entity.orientation["az_angle"],
      "sun_elevation_angle": sun.entity.orientation["el_angle"],
      "moon_elevation_angle": moon.entity.orientation["el_angle"],
      "policy_sun_scale": sv.night_sun_scale,
      "policy_moon_scale": mv.night_moon_scale,
    })

  # a light whose name is not in the published table is a mis-wired scene
  missing = None
  try:
    orbit.onUpdate(_Component(_Entity(3, "nosuchlight")), _UpdInfo(0.0))
  except Exception as error:
    missing = type(error).__name__

  json.dump({"track": track, "missing_config_error": missing}, sys.stdout)


###############################################################################

_PROBE = None


def probe():
  global _PROBE
  if _PROBE is None:
    out = subprocess.run([sys.executable, os.path.abspath(__file__),
                          "--emit-track"],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         env=dict(os.environ), check=True)
    _PROBE = json.loads(out.stdout.decode("utf-8"))
  return _PROBE


class TestCelestialOrbitScript(unittest.TestCase):

  ########################################
  def test_missing_config_is_loud(self):
    self.assertEqual(probe()["missing_config_error"], "KeyError")

  ########################################
  def test_moon_is_aimed_at_the_moon_not_the_sun(self):
    # The bodies share the script; the aiming must not.
    track = probe()["track"]
    for row in track:
      self.assertAlmostEqual(row["sun_elevation_angle"],
                             math.radians(row["sun_elevation"]), places=9)
      self.assertAlmostEqual(row["moon_elevation_angle"],
                             math.radians(row["moon_elevation"]), places=9)
    apart = max(abs(r["sun_azimuth_angle"] - r["moon_azimuth_angle"])
                for r in track)
    self.assertGreater(apart, 0.5)

  ########################################
  def test_never_two_shadow_casters_over_a_whole_day(self):
    for row in probe()["track"]:
      self.assertFalse(row["sun_casts"] > 0.5 and row["moon_casts"] > 0.5,
                       "both cast at t=%.2f" % row["t"])

  ########################################
  def test_bridge_intensities_are_the_declared_value_scaled(self):
    for row in probe()["track"]:
      self.assertAlmostEqual(row["sun_intensity"],
                             SUN_INTENSITY * row["policy_sun_scale"], places=9)
      self.assertAlmostEqual(row["moon_intensity"],
                             MOON_INTENSITY * row["policy_moon_scale"], places=9)
      self.assertLessEqual(row["sun_intensity"], SUN_INTENSITY + 1.0e-9)
      self.assertLessEqual(row["moon_intensity"], MOON_INTENSITY + 1.0e-9)

  ########################################
  def test_the_day_has_a_sun_cascade_and_the_night_a_moon_cascade(self):
    # This scene's clock must actually EXERCISE the handoff, or the gauge scene
    # gauges nothing.
    track = probe()["track"]
    sun_frames = [r for r in track if r["sun_casts"] > 0.5]
    moon_frames = [r for r in track if r["moon_casts"] > 0.5]
    self.assertTrue(sun_frames)
    self.assertTrue(moon_frames)
    # the sun keeps the cascade into the top of twilight (its light is ~1% there)
    self.assertTrue(all(r["sun_elevation"] > _policy.SUN_SHADOW_OFF_ELEVATION_DEG
                        for r in sun_frames))
    self.assertTrue(all(r["moon_elevation"] > _policy.MOON_DARK_ELEVATION_DEG
                        and r["sun_elevation"] <= _policy.SUN_SHADOW_OFF_ELEVATION_DEG
                        for r in moon_frames))
    # and there is a shadowless gap between them (nothing pops across the swap)
    dark = [r for r in track if r["sun_casts"] < 0.5 and r["moon_casts"] < 0.5]
    self.assertTrue(dark)

  ########################################
  def test_moon_light_is_off_in_daylight(self):
    for row in probe()["track"]:
      if row["sun_elevation"] > 1.0:
        self.assertEqual(row["moon_intensity"], 0.0)


###############################################################################

if __name__ == '__main__':
  if "--emit-track" in sys.argv:
    _emit_track()
  else:
    _result = unittest.main(exit=False, verbosity=2).result
    _ok = _result.wasSuccessful()
    print("VERDICT: celestial_orbit_script %s (%d tests)"
          % ("PASS" if _ok else "FAIL", _result.testsRun))
    sys.exit(0 if _ok else 1)
