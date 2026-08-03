#!/usr/bin/env python3
###############################################################################
# celestial.py — unit tests for the observer-frame ephemeris
# (obt.project/scripts/ork/hypergraph/ecs/scene/_celestial.py).
#
# Runs STANDALONE with plain CPython — no engine, no staging:
#     python3 obt.project/unittests/celestial.py
# It loads _celestial.py BY PATH rather than importing
# ork.hypergraph.ecs.scene._celestial, because the dotted form would execute the
# scene package __init__ chain (orkengine.core / lev2). The model itself imports
# only `math`, so the file loads alone. Same trick the sim-side component uses.
#
# What is asserted is OBSERVABLE SKY BEHAVIOR, not the transcription of any one
# formula: day length, noon altitude, where the sun rises, that nothing ever
# plunges through the zenith, that scrubbing is continuous, and that the moon's
# synodic cycle and the star dome's rate come out at the textbook values.
###############################################################################

import importlib.util
import math
import os
import random
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_MODEL_PATH = os.path.join(_HERE, "..", "scripts", "ork", "hypergraph", "ecs",
                           "scene", "_celestial.py")

_spec = importlib.util.spec_from_file_location("_celestial", _MODEL_PATH)
_celestial = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_celestial)

CelestialModel = _celestial.CelestialModel

MAX_DECLINATION = 23.44   # obliquity of the ecliptic

###############################################################################
# helpers
###############################################################################


def hourly_model(latitude, day_of_year, **kwargs):
  """A model whose at(x) advances the clock by x HOURS from 0h UT. The day is
  FLOORED: a fractional day_of_year would slide the start off midnight and a
  24h track would then straddle two dawns."""
  return CelestialModel(latitude_deg=latitude, day_of_year=math.floor(day_of_year),
                        time_of_day=0.0, time_scale=3600.0, **kwargs)


def sun_declination(day_of_year):
  """Solar declination at noon UT on a day — latitude-independent."""
  m = CelestialModel(latitude_deg=0.0, day_of_year=day_of_year,
                     time_of_day=12.0, time_scale=1.0)
  return m.at(0.0).sun_declination_deg


def march_equinox_day():
  """The day_of_year (fractional) where the sun's declination crosses zero
  northbound — found, not assumed, so the tests do not depend on which year the
  model's epoch lands in."""
  lo, hi = 60.0, 100.0
  assert sun_declination(lo) < 0.0 < sun_declination(hi)
  for _ in range(40):
    mid = 0.5 * (lo + hi)
    if sun_declination(mid) < 0.0:
      lo = mid
    else:
      hi = mid
  return 0.5 * (lo + hi)


def june_solstice_day():
  """The day_of_year of maximum solar declination (golden-section-free: the
  declination is smooth and unimodal over this window, so a coarse scan plus a
  refine is plenty)."""
  best, best_dec = 150.0, -90.0
  d = 150.0
  while d <= 200.0:
    dec = sun_declination(d)
    if dec > best_dec:
      best, best_dec = d, dec
    d += 0.25
  return best


def elevation_track(model, samples=1441):
  """[(hours, elevation, azimuth)] over one 24h clock day."""
  out = []
  for i in range(samples):
    h = 24.0 * i / (samples - 1)
    s = model.at(h)
    out.append((h, s.sun_elevation_deg, s.sun_azimuth_deg))
  return out


def horizon_crossings(track):
  """(rise, set) as (hours, azimuth) pairs, linearly interpolated across the
  elevation zero-crossings of a 24h track. Returns (None, None) when the sun
  never crosses (polar day / polar night)."""
  rise = None
  fall = None
  for (h0, e0, a0), (h1, e1, a1) in zip(track, track[1:]):
    if e0 <= 0.0 < e1 or e0 < 0.0 <= e1:
      t = -e0 / (e1 - e0)
      rise = (h0 + t * (h1 - h0), a0 + t * ((a1 - a0 + 540.0) % 360.0 - 180.0))
    elif e0 >= 0.0 > e1 or e0 > 0.0 >= e1:
      t = e0 / (e0 - e1)
      fall = (h0 + t * (h1 - h0), a0 + t * ((a1 - a0 + 540.0) % 360.0 - 180.0))
  return rise, fall


def angle_between(u, v):
  """Angle in degrees between two unit 3-tuples."""
  dot = max(-1.0, min(1.0, sum(a * b for a, b in zip(u, v))))
  return math.degrees(math.acos(dot))


###############################################################################


class TestCelestialSun(unittest.TestCase):

  ########################################
  def test_equator_equinox_day_length(self):
    # On the equator the hour angle at sunrise is -90 deg for ANY declination,
    # so the geometric day is half a sidereal-corrected rotation: 11h58m.
    model = hourly_model(0.0, march_equinox_day())
    rise, fall = horizon_crossings(elevation_track(model))
    self.assertIsNotNone(rise)
    self.assertIsNotNone(fall)
    day_length = fall[0] - rise[0]
    self.assertAlmostEqual(day_length, 12.0, delta=0.25)   # +/- 15 minutes

  ########################################
  def test_equator_equinox_rises_east_sets_west(self):
    model = hourly_model(0.0, march_equinox_day())
    rise, fall = horizon_crossings(elevation_track(model))
    self.assertAlmostEqual(rise[1] % 360.0, 90.0, delta=3.0)    # due east
    self.assertAlmostEqual(fall[1] % 360.0, 270.0, delta=3.0)   # due west

  ########################################
  def test_lat45_equinox_noon(self):
    # Noon altitude = 90 - |latitude - declination|; at the equinox that is 45.
    track = elevation_track(hourly_model(45.0, march_equinox_day()))
    peak = max(track, key=lambda s: s[1])
    self.assertAlmostEqual(peak[1], 45.0, delta=2.0)
    self.assertAlmostEqual(peak[2], 180.0, delta=1.0)           # due south

  ########################################
  def test_lat60_summer_solstice(self):
    model = hourly_model(60.0, june_solstice_day())
    track = elevation_track(model)
    peak = max(track, key=lambda s: s[1])
    self.assertAlmostEqual(peak[1], 90.0 - 60.0 + MAX_DECLINATION, delta=2.0)
    rise, fall = horizon_crossings(track)
    self.assertIsNotNone(rise)
    self.assertIsNotNone(fall)
    self.assertGreater(fall[0] - rise[0], 17.0)

  ########################################
  def test_no_zenith_plunge_over_a_year(self):
    # The sun must never climb higher than the observer's best possible noon.
    # A model that ignores latitude (or mixes up declination and altitude) puts
    # it overhead somewhere in the year; this catches that.
    ceiling = 90.0 - abs(45.0 - MAX_DECLINATION)
    worst = -90.0
    for day in range(1, 366):
      model = hourly_model(45.0, float(day))
      for hour in range(24):
        worst = max(worst, model.at(float(hour)).sun_elevation_deg)
    self.assertLessEqual(worst, ceiling + 1.0)
    self.assertGreater(worst, ceiling - 1.0)   # and it does reach it

  ########################################
  def test_latitude_dependence_of_noon_altitude(self):
    equinox = march_equinox_day()
    for latitude in (-60.0, -30.0, 0.0, 30.0, 60.0):
      peak = max(elevation_track(hourly_model(latitude, equinox)),
                 key=lambda s: s[1])
      self.assertAlmostEqual(peak[1], 90.0 - abs(latitude), delta=2.0)


class TestCelestialContinuity(unittest.TestCase):

  ########################################
  def test_scrub_continuity_one_second_steps(self):
    # time_scale=1 -> one wall second is one clock second; the sky turns
    # 0.0042 deg in that time and must never jump.
    rng = random.Random(0xC0FFEE)
    model = CelestialModel(latitude_deg=45.0, day_of_year=100.0,
                           time_of_day=0.0, time_scale=1.0)
    for _ in range(200):
      t = rng.uniform(0.0, 1.0e6)
      a = model.at(t).sun_vector()
      b = model.at(t + 1.0).sun_vector()
      self.assertLess(angle_between(a, b), 0.02)

  ########################################
  def test_no_discontinuity_across_midnight(self):
    # The clock rolls into the next day (and past new year) as a continuous
    # quantity — there is no wrap seam to fall through.
    for day, hour in ((100.0, 23.999), (365.0, 23.999)):
      model = CelestialModel(latitude_deg=45.0, day_of_year=day,
                             time_of_day=hour, time_scale=1.0)
      previous = model.at(0.0).sun_vector()
      for i in range(1, 20):
        current = model.at(float(i) * 0.5).sun_vector()
        self.assertLess(angle_between(previous, current), 0.02)
        previous = current

  ########################################
  def test_azimuth_convention_round_trip(self):
    # The reported azimuth is ASTRONOMICAL (0=N, 90=E); the light angles are the
    # DSL's (about +Y, 0 = travelling toward +Z). They are exact negatives, and
    # the vector must agree with both.
    snap = CelestialModel(latitude_deg=45.0, day_of_year=100.0,
                          time_of_day=15.0, time_scale=1.0).at(0.0)
    elevation, azimuth = snap.sun_light_angles()
    self.assertAlmostEqual(elevation, snap.sun_elevation_deg, places=9)
    self.assertAlmostEqual(azimuth, -snap.sun_azimuth_deg, places=9)
    x, y, z = snap.sun_vector()
    self.assertAlmostEqual(math.sqrt(x * x + y * y + z * z), 1.0, places=9)
    self.assertAlmostEqual(math.degrees(math.asin(y)), snap.sun_elevation_deg,
                           places=6)
    # +X east, -Z north: bearing from north, eastward.
    self.assertAlmostEqual(math.degrees(math.atan2(x, -z)) % 360.0,
                           snap.sun_azimuth_deg % 360.0, places=5)


class TestCelestialMoon(unittest.TestCase):

  ########################################
  def test_moon_phase_advances_monotonically(self):
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    previous = model.at(0.0).moon_phase
    for day in range(1, 400):
      phase = model.at(float(day)).moon_phase
      step = (phase - previous) % 1.0
      self.assertGreater(step, 0.0)
      self.assertLess(step, 0.05)       # ~1/29.53 per day, never a jump back
      previous = phase

  ########################################
  def test_synodic_period(self):
    # Count new moons (phase wraps 1 -> 0) over a year and a bit; the mean gap
    # is the synodic month.
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    step = 0.05
    crossings = []
    t = 0.0
    previous = model.at(t).moon_phase
    while t < 400.0:
      t += step
      phase = model.at(t).moon_phase
      if phase < previous:                       # wrapped through new moon
        frac = (1.0 - previous) / ((1.0 - previous) + phase)
        crossings.append(t - step + frac * step)
      previous = phase
    self.assertGreater(len(crossings), 12)
    period = (crossings[-1] - crossings[0]) / (len(crossings) - 1)
    self.assertAlmostEqual(period, 29.53, delta=0.5)

  ########################################
  def test_full_moon_is_opposite_the_sun(self):
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    checked = 0
    t = 0.0
    while t < 200.0:
      snap = model.at(t)
      if abs(snap.moon_phase - 0.5) < 0.002:
        # elongation falls short of 180 by at most the moon's ecliptic latitude
        self.assertAlmostEqual(snap.sun_moon_elongation_deg, 180.0, delta=6.0)
        self.assertGreater(snap.moon_illumination, 0.99)
        self.assertLess(snap.moon_phase_angle_deg, 6.0)
        checked += 1
        t += 20.0
      t += 0.05
    self.assertGreater(checked, 4)

  ########################################
  def test_new_moon_is_dark(self):
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    checked = 0
    t = 0.0
    while t < 200.0:
      snap = model.at(t)
      if snap.moon_phase < 0.002 or snap.moon_phase > 0.998:
        self.assertLess(snap.sun_moon_elongation_deg, 6.0)
        self.assertLess(snap.moon_illumination, 0.01)
        checked += 1
        t += 20.0
      t += 0.05
    self.assertGreater(checked, 4)

  ########################################
  def test_moon_stays_near_the_ecliptic(self):
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    for day in range(400):
      self.assertLess(abs(model.at(float(day)).moon_ecliptic_lat_deg), 6.0)
      self.assertLess(abs(model.at(float(day)).moon_declination_deg),
                      MAX_DECLINATION + 6.0)


class TestCelestialStarDome(unittest.TestCase):

  ########################################
  def test_sidereal_rate(self):
    # One sidereal day is one solar day plus the ~1 deg the earth moved around
    # the sun: 360.98565 deg/day, and the angle ACCUMULATES (no wrap).
    model = CelestialModel(latitude_deg=45.0, day_of_year=1.0,
                           time_of_day=0.0, time_scale=86400.0)
    for t0 in (0.0, 37.5, 200.0):
      a = model.at(t0).sidereal_angle_deg
      b = model.at(t0 + 1.0).sidereal_angle_deg
      self.assertAlmostEqual(b - a, 360.9856, delta=0.01)

  ########################################
  def test_pole_tracks_latitude(self):
    for latitude in (-45.0, 0.0, 23.5, 51.5, 90.0):
      snap = CelestialModel(latitude_deg=latitude, day_of_year=1.0,
                            time_of_day=0.0, time_scale=1.0).at(0.0)
      self.assertAlmostEqual(snap.pole_elevation_deg, latitude, places=9)
      self.assertAlmostEqual(snap.pole_azimuth_deg, 0.0, places=9)


class TestCelestialConfig(unittest.TestCase):

  ########################################
  def test_defaults_and_round_trip(self):
    cfg = _celestial.normalize_config({"latitude_deg": 12.5})
    self.assertEqual(set(cfg), set(_celestial.CONFIG_DEFAULTS))
    self.assertEqual(cfg["latitude_deg"], 12.5)
    model = CelestialModel.from_config(cfg)
    self.assertEqual(model.config()["latitude_deg"], 12.5)

  ########################################
  def test_unknown_key_is_loud(self):
    with self.assertRaises(KeyError):
      _celestial.normalize_config({"lattitude_deg": 45.0})
    with self.assertRaises(TypeError):
      _celestial.normalize_config(45.0)

  ########################################
  def test_body_key_is_validated(self):
    self.assertEqual(_celestial.normalize_config({})["body"], "sun")
    self.assertEqual(_celestial.normalize_config({"body": "moon"})["body"], "moon")
    with self.assertRaises(ValueError):
      _celestial.normalize_config({"body": "mars"})

  ########################################
  def test_per_light_keys_do_not_reach_the_model(self):
    # body / base_intensity are the per-frame component's business; the ephemeris
    # constructor must not see them (and config() must not report them).
    cfg = _celestial.normalize_config({"body": "moon", "base_intensity": 0.01,
                                       "latitude_deg": 51.5})
    model = CelestialModel.from_config(cfg)
    self.assertEqual(set(model.config()), set(_celestial.MODEL_CONFIG_KEYS))
    self.assertEqual(model.config()["latitude_deg"], 51.5)

  ########################################
  def test_time_scale_scales_the_clock(self):
    slow = CelestialModel(latitude_deg=45.0, day_of_year=100.0,
                          time_of_day=6.0, time_scale=1.0)
    fast = CelestialModel(latitude_deg=45.0, day_of_year=100.0,
                          time_of_day=6.0, time_scale=3600.0)
    self.assertAlmostEqual(slow.at(3600.0).sun_elevation_deg,
                           fast.at(1.0).sun_elevation_deg, places=9)


###############################################################################
# The bridge to the engine's quat convention. Skipped (not failed) when the test
# runs under a bare python that cannot import orkengine — the point of the rest
# of the file is that it needs no engine.
###############################################################################

try:
  import numpy as _np
  from orkengine.core import quat as _engine_quat
  _HAVE_ENGINE = True
except Exception:
  _HAVE_ENGINE = False


def _rotation_matrix(q):
  """orkengine quat → 3x3 rotation matrix. np.array(quat) yields (w,x,y,z) —
  see ork.core/pyext/unittests/math_quat.py, where the identity quat reads
  [1,0,0,0]."""
  w, x, y, z = _np.array(q, copy=False)
  return _np.array([
    [1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w)],
    [2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
    [2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)]])


@unittest.skipUnless(_HAVE_ENGINE, "orkengine not importable (pure-math run)")
class TestCelestialEngineQuats(unittest.TestCase):

  ########################################
  def test_sun_quat_travels_away_from_the_sun(self):
    # A DirectionalLight's direction is its entity's world +Z, so the quat the
    # snapshot hands out must rotate +Z onto the NEGATED to-the-sun vector.
    model = CelestialModel(latitude_deg=45.0, day_of_year=220.0,
                           time_of_day=4.0, time_scale=1440.0)
    for t in (0.0, 7.5, 20.0, 45.0, 59.0):
      snap = model.at(t)
      travel = _rotation_matrix(snap.sun_quat()) @ _np.array([0.0, 0.0, 1.0])
      toward = _np.array(snap.sun_vector())
      self.assertLess(float(_np.abs(travel + toward).max()), 1.0e-5)

  ########################################
  def test_moon_quat_travels_away_from_the_moon(self):
    snap = CelestialModel(latitude_deg=45.0, day_of_year=220.0,
                          time_of_day=22.0, time_scale=1.0).at(0.0)
    travel = _rotation_matrix(snap.moon_quat()) @ _np.array([0.0, 0.0, 1.0])
    self.assertLess(float(_np.abs(travel + _np.array(snap.moon_vector())).max()),
                    1.0e-5)

  ########################################
  def test_star_dome_pole_points_at_the_pole(self):
    # The dome is authored with its local +Y on the north celestial pole, which
    # sits due north (world -Z) at elevation = latitude.
    for latitude in (0.0, 45.0, 60.0):
      snap = CelestialModel(latitude_deg=latitude, day_of_year=1.0,
                            time_of_day=0.0, time_scale=1.0).at(0.0)
      axis = _rotation_matrix(snap.star_dome_quat()) @ _np.array([0.0, 1.0, 0.0])
      expected = _np.array([0.0,
                            math.sin(math.radians(latitude)),
                            -math.cos(math.radians(latitude))])
      self.assertLess(float(_np.abs(axis - expected).max()), 1.0e-5)


###############################################################################

if __name__ == '__main__':
  unittest.main()
