#!/usr/bin/env python3
###############################################################################
# star_dome.py — unit test for the SIM-SIDE star-dome aiming script
# (obt.project/scripts/ork/hypergraph/ecs/scene/_star_dome.py): that the dome's
# local +Y really lands on the north celestial pole for the declared latitude,
# that the shell turns at the SIDEREAL rate (360.9856 deg/day) and in the
# retrograde sense, and that the quat it builds on the sim's ecssim types is the
# same rotation CelestialSnapshot.star_dome_quat() hands the DSL.
#
# Runs STANDALONE with plain CPython — no engine, no staging:
#     python3 obt.project/unittests/star_dome.py
#
# HOW (the celestial_orbit_script.py pattern): the script's only engine
# dependency is `from orkengine.ecssim import vec3, quat` — two constructors and
# a multiply — so the probe STUBS that module with a real pure-python quaternion
# and drives onUpdate over scn_procsky.py's clock at a grid of latitudes. The
# stubbing happens in a SUBPROCESS (this file is also its own probe, re-exec'd
# with --emit-quats) because poisoning sys.modules["orkengine"] would wreck every
# other test sharing the collector's interpreter.
#
# The ENGINE-PARITY class is skipped (not failed) when orkengine is unimportable;
# everything else is pure math and always runs.
#
# WHAT IT DOES NOT COVER: the material's twilight fade and the star field itself
# (a rendered-frame gate's job) — only the aim.
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

# scn_procsky.py's clock: a mid-latitude late-summer day in 60 wall seconds.
PROBE_CLOCK = {"day_of_year": 220.0, "time_of_day": 4.0, "time_scale": 1440.0}
LATITUDES = (-45.0, -10.0, 0.0, 23.5, 45.0, 60.0, 75.0)
TIMES = (0.0, 3.7, 17.5, 45.0, 59.0)
RATE_DT = 0.25                    # wall seconds between the rate samples

# The sidereal day: the sky turns 360 + 0.9856 degrees per solar day.
SIDEREAL_RATE_DEG_PER_DAY = 360.9856


def _entity_name(latitude):
  return "stars_%g" % latitude


###############################################################################
# the probe — runs in its own interpreter (see header)
###############################################################################


def _emit_quats():
  import types

  sys.path.insert(0, os.path.abspath(_SCRIPTS))

  ecssim = types.ModuleType("orkengine.ecssim")

  class _vec3:
    def __init__(self, x, y, z):
      self.value = (float(x), float(y), float(z))

  class _quat:
    """axis/angle -> (w,x,y,z), Hamilton product. The engine's convention is
    what the parity test compares against; nothing here assumes it."""

    def __init__(self, axis=None, angle=0.0, wxyz=None):
      if wxyz is not None:
        self.wxyz = tuple(float(c) for c in wxyz)
        return
      ax, ay, az = axis.value
      norm = math.sqrt(ax * ax + ay * ay + az * az) or 1.0
      s = math.sin(0.5 * angle) / norm
      self.wxyz = (math.cos(0.5 * angle), ax * s, ay * s, az * s)

    def __mul__(self, other):
      w1, x1, y1, z1 = self.wxyz
      w2, x2, y2, z2 = other.wxyz
      return _quat(wxyz=(w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
                         w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                         w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                         w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2))

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
  table = {}
  for latitude in LATITUDES:
    table[_entity_name(latitude)] = celestial.normalize_config(
      dict(PROBE_CLOCK, latitude_deg=latitude))
  os.environ[celestial.CONFIG_ENV_KEY] = json.dumps(table)

  dome = _load("_probe_star_dome", "_star_dome.py")

  class _Entity:
    def __init__(self, eid, name):
      self.id, self.name = eid, name
      self.orientation = None

  class _Component:
    def __init__(self, entity):
      self.entity = entity

  class _UpdInfo:
    def __init__(self, abstime):
      self.abstime = abstime

  samples = []
  for i, latitude in enumerate(LATITUDES):
    comp = _Component(_Entity(i + 1, _entity_name(latitude)))
    for t in TIMES:
      for at in (t, t + RATE_DT):
        dome.onUpdate(comp, _UpdInfo(at))
        samples.append({"latitude": latitude, "t": at,
                        "quat": list(comp.entity.orientation.wxyz)})

  # a dome whose name is not in the published table is a mis-wired scene
  missing = None
  try:
    dome.onUpdate(_Component(_Entity(99, "nosuchdome")), _UpdInfo(0.0))
  except Exception as error:
    missing = type(error).__name__

  json.dump({"samples": samples, "missing_config_error": missing}, sys.stdout)


###############################################################################
# pure-python quaternion readout (no numpy — this half must run anywhere)
###############################################################################


def _matrix(q):
  w, x, y, z = q
  return ((1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w)),
          (2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
          (2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)))


def _apply(q, v):
  m = _matrix(q)
  return tuple(sum(m[r][c] * v[c] for c in range(3)) for r in range(3))


def _conj(q):
  w, x, y, z = q
  return (w, -x, -y, -z)


def _mul(a, b):
  w1, x1, y1, z1 = a
  w2, x2, y2, z2 = b
  return (w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
          w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
          w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
          w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)


def _axis_angle(q):
  """(unit axis, angle in degrees, 0..360) of a quaternion rotation."""
  w, x, y, z = q
  s = math.sqrt(x * x + y * y + z * z)
  angle = 2.0 * math.atan2(s, w)
  axis = (x / s, y / s, z / s) if s > 1e-12 else (0.0, 1.0, 0.0)
  return axis, math.degrees(angle)


def _dot(a, b):
  return sum(p * q for p, q in zip(a, b))


###############################################################################

_PROBE = None


def probe():
  global _PROBE
  if _PROBE is None:
    out = subprocess.run([sys.executable, os.path.abspath(__file__),
                          "--emit-quats"],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         env=dict(os.environ), check=True)
    _PROBE = json.loads(out.stdout.decode("utf-8"))
  return _PROBE


def samples_for(latitude):
  return [s for s in probe()["samples"] if s["latitude"] == latitude]


def pole_vector(latitude):
  """World direction of the north celestial pole: due north (world -Z) at
  elevation = latitude (+X east, +Y up, +Z south)."""
  return (0.0,
          math.sin(math.radians(latitude)),
          -math.cos(math.radians(latitude)))


class TestStarDomeScript(unittest.TestCase):

  ########################################
  def test_missing_config_is_loud(self):
    self.assertEqual(probe()["missing_config_error"], "KeyError")

  ########################################
  def test_local_up_lands_on_the_celestial_pole(self):
    # The dome is authored with its local +Y on the pole, so the quat must carry
    # +Y onto (north, elevation = latitude) for every site.
    for latitude in LATITUDES:
      expect = pole_vector(latitude)
      for s in samples_for(latitude):
        got = _apply(s["quat"], (0.0, 1.0, 0.0))
        self.assertLess(max(abs(a - b) for a, b in zip(got, expect)), 1.0e-6,
                        msg="latitude %g at t=%g: pole %s != %s"
                            % (latitude, s["t"], got, expect))

  ########################################
  def test_the_sky_turns_at_the_sidereal_rate_about_the_pole(self):
    # The relative rotation between two instants must be about the POLE axis,
    # RETROGRADE (the sky's apparent turn reverses the earth's), and at
    # 360.9856 deg per solar day.
    day_fraction = RATE_DT * PROBE_CLOCK["time_scale"] / 86400.0
    expected_deg = SIDEREAL_RATE_DEG_PER_DAY * day_fraction
    for latitude in LATITUDES:
      pole = pole_vector(latitude)
      rows = samples_for(latitude)
      for i in range(0, len(rows), 2):
        q0, q1 = rows[i]["quat"], rows[i + 1]["quat"]
        axis, angle = _axis_angle(_mul(q1, _conj(q0)))
        # about the pole, and NEGATIVE about it (retrograde)
        self.assertLess(abs(_dot(axis, pole) + 1.0), 1.0e-6,
                        msg="latitude %g: turn axis %s is not the retrograde "
                            "pole %s" % (latitude, axis, pole))
        self.assertAlmostEqual(angle, expected_deg, delta=0.01)

  ########################################
  def test_a_whole_probe_day_wheels_the_sky_once(self):
    # 60 wall seconds at time_scale 1440 is one solar day: the accumulated
    # sidereal turn is one full sky rotation plus the ~1 degree solar lead, so
    # the dome comes back to (almost) the same orientation.
    rows = {s["t"]: s["quat"] for s in samples_for(45.0)}
    q0, q1 = rows[0.0], rows[59.0]
    _, angle = _axis_angle(_mul(q1, _conj(q0)))
    leftover = SIDEREAL_RATE_DEG_PER_DAY * (59.0 / 60.0)
    leftover -= 360.0 * math.floor(leftover / 360.0)
    self.assertAlmostEqual(min(angle, 360.0 - angle), min(leftover, 360.0 - leftover),
                           delta=0.02)


###############################################################################
# The bridge to the engine's own quat convention + the DSL-side helper. Skipped
# (not failed) under a bare python that cannot import orkengine — the point of
# the rest of the file is that it needs no engine.
###############################################################################

try:
  from orkengine.core import quat as _engine_quat   # noqa: F401
  _spec = importlib.util.spec_from_file_location(
    "_celestial_for_stars", os.path.join(_SCENE_DIR, "_celestial.py"))
  _celestial = importlib.util.module_from_spec(_spec)
  _spec.loader.exec_module(_celestial)
  _HAVE_ENGINE = True
except Exception:
  _HAVE_ENGINE = False


@unittest.skipUnless(_HAVE_ENGINE, "orkengine not importable (pure-math run)")
class TestStarDomeEngineParity(unittest.TestCase):

  ########################################
  def test_sim_quat_matches_the_snapshot_helper(self):
    # The sim script rebuilds star_dome_quat() on ecssim types because
    # orkengine.core is absent in the subinterpreter. The two must be the SAME
    # rotation (compared as matrices — q and -q are one rotation).
    #
    # TOLERANCE: the DSL-side helper feeds the UNWRAPPED sidereal angle (~1500
    # radians at this epoch) into a FLOAT32 quat, which costs it ~2e-4 of the
    # matrix; the sim script wraps first and keeps full precision, so this
    # residual is the helper's rounding, not a disagreement. Any convention
    # mismatch (axis order, multiply order, angle sign) is O(1).
    import numpy as np
    for latitude in LATITUDES:
      model = _celestial.CelestialModel(latitude_deg=latitude, **PROBE_CLOCK)
      for s in samples_for(latitude):
        want = np.array(model.at(s["t"]).star_dome_quat(), copy=False)  # (w,x,y,z)
        a = np.array(_matrix(list(want)))
        b = np.array(_matrix(s["quat"]))
        self.assertLess(float(np.abs(a - b).max()), 5.0e-4,
                        msg="latitude %g at t=%g" % (latitude, s["t"]))


###############################################################################

if __name__ == '__main__':
  if "--emit-quats" in sys.argv:
    _emit_quats()
  else:
    _result = unittest.main(exit=False, verbosity=2).result
    _ok = _result.wasSuccessful()
    print("VERDICT: star_dome %s (%d tests)"
          % ("PASS" if _ok else "FAIL", _result.testsRun))
    sys.exit(0 if _ok else 1)
