#!/usr/bin/env python3
###############################################################################
# night_policy.py — unit tests for the day/night shadow handoff policy
# (obt.project/scripts/ork/hypergraph/ecs/scene/_night_policy.py).
#
# Runs STANDALONE with plain CPython — no engine, no staging:
#     python3 obt.project/unittests/night_policy.py
# Loaded BY PATH (same reason celestial.py does it: the dotted import would drag
# the scene package's orkengine imports in). The policy module imports nothing.
#
# WHAT IS ASSERTED is the OWNER RULING, on a dense sweep of the whole sky rather
# than at hand-picked angles:
#   * at most ONE shadow caster, ever;
#   * a caster only ever flips where THAT body's intensity scale is under
#     HANDOFF_SCALE_LIMIT — i.e. the handoff cannot pop;
#   * both scales are continuous (bounded step) along every axis;
#   * the moon is black in daylight, and sun-down + no-moon is shadowless.
# If a tunable in _night_policy.py moves, this is the file that catches the pop.
###############################################################################

import importlib.util
import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_POLICY_PATH = os.path.join(_HERE, "..", "scripts", "ork", "hypergraph", "ecs",
                            "scene", "_night_policy.py")

_spec = importlib.util.spec_from_file_location("_night_policy", _POLICY_PATH)
_policy = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_policy)

night_policy = _policy.night_policy
LIMIT = _policy.HANDOFF_SCALE_LIMIT

# Sweep resolution. Fine enough that a flip's neighbours bracket it tightly (the
# no-pop assertion is only as strong as the step), coarse enough to stay a
# sub-second pure-python test.
SUN_STEP_DEG  = 0.25
MOON_STEP_DEG = 0.25
ILLUM_STEP    = 0.02


def _frange(lo, hi, step):
  out = []
  i = 0
  while True:
    x = lo + i * step
    if x > hi + 1.0e-9:
      break
    out.append(x)
    i += 1
  return out


SUN_ELEVATIONS  = _frange(-25.0, 25.0, SUN_STEP_DEG)
MOON_ELEVATIONS = _frange(-15.0, 40.0, MOON_STEP_DEG)
ILLUMINATIONS   = _frange(0.0, 1.0, ILLUM_STEP)

# Coarse "other axes" set, so the per-axis fine sweeps stay cheap.
SUN_COARSE   = _frange(-24.0, 24.0, 3.0)
MOON_COARSE  = _frange(-12.0, 36.0, 3.0)
ILLUM_COARSE = [0.0, 0.05, 0.14, 0.16, 0.3, 0.5, 0.75, 1.0]


###############################################################################


class TestNeverBothCast(unittest.TestCase):

  ########################################
  def test_at_most_one_caster_on_the_whole_sky(self):
    # The single-cascade law. A second shadow-casting directional would put the
    # renderer in its "first wins" fallback (fwdnode_impl_sub.cpp).
    for sun_el in SUN_COARSE:
      for moon_el in MOON_COARSE:
        for illum in ILLUMINATIONS:
          p = night_policy(sun_el, moon_el, illum)
          self.assertFalse(p["sun_casts"] and p["moon_casts"],
                           "both cast at sun_el=%.2f moon_el=%.2f illum=%.2f"
                           % (sun_el, moon_el, illum))

  ########################################
  def test_scales_stay_in_unit_range(self):
    for sun_el in SUN_COARSE:
      for moon_el in MOON_COARSE:
        for illum in ILLUMINATIONS:
          p = night_policy(sun_el, moon_el, illum)
          for k in ("sun_intensity_scale", "moon_intensity_scale"):
            self.assertGreaterEqual(p[k], 0.0)
            self.assertLessEqual(p[k], 1.0)

  ########################################
  def test_illumination_outside_unit_range_is_clamped_not_fatal(self):
    # The ephemeris can hand back 1.0000000002 off a cosine.
    for illum in (-0.5, -1.0e-12, 1.0 + 1.0e-12, 1.5):
      p = night_policy(-12.0, 30.0, illum)
      self.assertGreaterEqual(p["moon_intensity_scale"], 0.0)
      self.assertLessEqual(p["moon_intensity_scale"], 1.0)


class TestNoPopAtTheHandoff(unittest.TestCase):
  """A caster may only flip where that body's own intensity scale is already
  under HANDOFF_SCALE_LIMIT — checked at BOTH samples bracketing every flip, so
  the assertion covers the whole flip neighbourhood and not just one side."""

  def _check_axis(self, samples):
    """samples: [(label, policy_dict)] along one axis, in order."""
    for (label_a, a), (label_b, b) in zip(samples, samples[1:]):
      for body in ("sun", "moon"):
        if a[body + "_casts"] == b[body + "_casts"]:
          continue
        key = body + "_intensity_scale"
        for label, p in ((label_a, a), (label_b, b)):
          self.assertLess(p[key], LIMIT,
                          "%s caster flips at %s with %s=%.4f (limit %.3f)"
                          % (body, label, key, p[key], LIMIT))

  ########################################
  def test_flips_along_sun_elevation(self):
    for moon_el in MOON_COARSE:
      for illum in ILLUM_COARSE:
        self._check_axis([
          ("sun_el=%.2f moon_el=%.2f illum=%.2f" % (s, moon_el, illum),
           night_policy(s, moon_el, illum)) for s in SUN_ELEVATIONS])

  ########################################
  def test_flips_along_moon_elevation(self):
    for sun_el in SUN_COARSE:
      for illum in ILLUM_COARSE:
        self._check_axis([
          ("sun_el=%.2f moon_el=%.2f illum=%.2f" % (sun_el, m, illum),
           night_policy(sun_el, m, illum)) for m in MOON_ELEVATIONS])

  ########################################
  def test_flips_along_illumination(self):
    for sun_el in SUN_COARSE:
      for moon_el in MOON_COARSE:
        self._check_axis([
          ("sun_el=%.2f moon_el=%.2f illum=%.3f" % (sun_el, moon_el, i),
           night_policy(sun_el, moon_el, i)) for i in ILLUMINATIONS])


class TestContinuity(unittest.TestCase):
  """Both scales are C1 products of smoothsteps, so a fine step must move them
  only a little. A hard threshold sneaking into a scale shows up here."""

  # Slopes are bounded by the ramp widths; these are those bounds with slack.
  SUN_ELEVATION_BOUND  = 0.10   # per SUN_STEP_DEG
  MOON_ELEVATION_BOUND = 0.10   # per MOON_STEP_DEG
  ILLUM_BOUND          = 0.15   # per ILLUM_STEP

  def _check(self, samples, bound):
    for a, b in zip(samples, samples[1:]):
      for k in ("sun_intensity_scale", "moon_intensity_scale"):
        self.assertLessEqual(abs(b[k] - a[k]), bound,
                             "%s jumped %.4f (bound %.3f)"
                             % (k, abs(b[k] - a[k]), bound))

  ########################################
  def test_continuous_in_sun_elevation(self):
    for moon_el in MOON_COARSE:
      for illum in ILLUM_COARSE:
        self._check([night_policy(s, moon_el, illum) for s in SUN_ELEVATIONS],
                    self.SUN_ELEVATION_BOUND)

  ########################################
  def test_continuous_in_moon_elevation(self):
    for sun_el in SUN_COARSE:
      for illum in ILLUM_COARSE:
        self._check([night_policy(sun_el, m, illum) for m in MOON_ELEVATIONS],
                    self.MOON_ELEVATION_BOUND)

  ########################################
  def test_continuous_in_illumination(self):
    for sun_el in SUN_COARSE:
      for moon_el in MOON_COARSE:
        self._check([night_policy(sun_el, moon_el, i) for i in ILLUMINATIONS],
                    self.ILLUM_BOUND)


class TestPolicyIntent(unittest.TestCase):
  """The ruling in plain terms, at the poses a viewer actually sees."""

  ########################################
  def test_daylight_sun_full_and_casting(self):
    for sun_el in (5.0, 20.0, 45.0, 70.0):
      p = night_policy(sun_el, 30.0, 1.0)
      self.assertAlmostEqual(p["sun_intensity_scale"], 1.0, places=9)
      self.assertTrue(p["sun_casts"])
      self.assertFalse(p["moon_casts"])

  ########################################
  def test_moon_is_black_in_daylight(self):
    # A full moon high overhead at noon contributes nothing.
    for sun_el in (0.5, 5.0, 30.0, 70.0):
      p = night_policy(sun_el, 60.0, 1.0)
      self.assertEqual(p["moon_intensity_scale"], 0.0)
      self.assertFalse(p["moon_casts"])

  ########################################
  def test_deep_night_full_moon_takes_the_cascade(self):
    p = night_policy(-30.0, 45.0, 1.0)
    self.assertEqual(p["sun_intensity_scale"], 0.0)
    self.assertFalse(p["sun_casts"])
    self.assertTrue(p["moon_casts"])
    self.assertGreater(p["moon_intensity_scale"], 0.9)

  ########################################
  def test_sun_down_no_moon_is_shadowless(self):
    # moon below the horizon, and moon new-dark with the moon up: both cleanly
    # off, nothing casting, nothing lit.
    for moon_el, illum in ((-10.0, 1.0), (-0.5, 1.0), (45.0, 0.0), (45.0, 0.05)):
      p = night_policy(-30.0, moon_el, illum)
      self.assertFalse(p["sun_casts"])
      self.assertFalse(p["moon_casts"])
      self.assertEqual(p["sun_intensity_scale"], 0.0)
      self.assertEqual(p["moon_intensity_scale"], 0.0)

  ########################################
  def test_crescent_moon_does_not_cast(self):
    for illum in (0.0, 0.05, 0.10, 0.149):
      self.assertFalse(night_policy(-30.0, 45.0, illum)["moon_casts"])

  ########################################
  def test_moon_brightness_grows_with_illumination_and_altitude(self):
    previous = -1.0
    for illum in ILLUMINATIONS:
      scale = night_policy(-30.0, 45.0, illum)["moon_intensity_scale"]
      self.assertGreaterEqual(scale, previous)
      previous = scale
    previous = -1.0
    for moon_el in MOON_ELEVATIONS:
      scale = night_policy(-30.0, moon_el, 1.0)["moon_intensity_scale"]
      self.assertGreaterEqual(scale, previous)
      previous = scale

  ########################################
  def test_sun_brightness_grows_with_elevation(self):
    previous = -1.0
    for sun_el in SUN_ELEVATIONS:
      scale = night_policy(sun_el, 45.0, 1.0)["sun_intensity_scale"]
      self.assertGreaterEqual(scale, previous)
      previous = scale

  ########################################
  def test_twilight_is_a_crossfade_not_a_switch(self):
    # Somewhere between the sun casting and the moon casting, BOTH are dim and
    # NEITHER casts — that gap is what makes the handoff invisible.
    gap = [s for s in SUN_ELEVATIONS
           if not night_policy(s, 45.0, 1.0)["sun_casts"]
           and not night_policy(s, 45.0, 1.0)["moon_casts"]]
    self.assertTrue(gap)
    for sun_el in gap:
      p = night_policy(sun_el, 45.0, 1.0)
      self.assertLess(p["sun_intensity_scale"], LIMIT)
      self.assertLess(p["moon_intensity_scale"], LIMIT)


###############################################################################

if __name__ == '__main__':
  _result = unittest.main(exit=False, verbosity=2).result
  _ok = _result.wasSuccessful()
  print("VERDICT: night_policy %s (%d tests)"
        % ("PASS" if _ok else "FAIL", _result.testsRun))
  sys.exit(0 if _ok else 1)
