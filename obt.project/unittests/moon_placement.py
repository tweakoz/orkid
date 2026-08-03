#!/usr/bin/env python3
###############################################################################
# moon_placement.py — unit tests for the AUTHORED MOON: the three declarations
# (moon_phase / moon_initial_elevation / moon_orbit_rate) that re-epoch the
# lunar elements in _celestial.py's MOON PLACEMENT block, plus the year that
# now reaches the same surface.
#
# Runs STANDALONE with plain CPython — no engine, no staging:
#     python3 obt.project/unittests/moon_placement.py
# Loads _celestial.py and _night_policy.py BY PATH, same reason as
# celestial.py: the dotted import would drag in orkengine.core/lev2.
#
# WHAT IS ASSERTED, and why each one is here:
#
#   DECLARED MEANS DECLARED — a declared phase/elevation is what t=0 shows, to
#     the last digit worth quoting, for the four named phases and for arbitrary
#     fractions. A placement that "mostly" lands is a placement the scene has to
#     eyeball, which is the thing being removed.
#   THE RATE IS SYNODIC — moon_orbit_rate=2 halves the measured lunation, over
#     simulated time (successive new moons), not over a formula. And it COMPOSES:
#     the declared phase still holds exactly at t=0 at any rate, because the rate
#     term is zero there by construction.
#   NOTHING ELSE MOVES — declaring the moon leaves the sun, the sidereal angle
#     and the date exactly where they were, and an UNDECLARED sky is the old
#     ephemeris BIT FOR BIT (the REFERENCE table was taken from the code before
#     the placement work landed; == is deliberate, not almostEqual).
#   OVER-CONSTRAINED IS LOUD — a phase pins the moon's ecliptic longitude, so an
#     elevation declared with it only has the orbit's 5.15 deg of latitude to
#     move in. Outside that band the pair must RAISE, naming both declarations
#     and the band that would work, and an elevation taken from that message
#     must then hold. Silently bending either one is the failure this forbids.
#   ONE BRIGHTNESS PATH — phase reaches the sky through the moon LIGHT's
#     intensity: illumination -> _night_policy.moon_intensity_scale -> the
#     SkyBody==2 light. phase='new' must drive that to zero, and phase='full'
#     must not, so the sky's moon disc and its moonlit scatter follow the
#     declaration with no channel of their own.
###############################################################################

import importlib.util
import json
import math
import os
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCENE_DIR = os.path.join(_HERE, "..", "scripts", "ork", "hypergraph", "ecs",
                          "scene")


def _load(name):
  spec = importlib.util.spec_from_file_location(
    name, os.path.join(_SCENE_DIR, "%s.py" % name))
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  return mod


_celestial = _load("_celestial")
_night_policy = _load("_night_policy")

CelestialModel = _celestial.CelestialModel

# The gauge site: the swest scene's own (pueblo latitude, Aug 11, noon UT).
SITE = dict(latitude_deg=36.0, day_of_year=223.0, time_of_day=12.0)

# t=0 tolerances. The solves converge to ~1e-13 of a cycle / the last bit of the
# bisection bracket; these are the "worth quoting" figures, three orders looser.
PHASE_TOL = 1.0e-9        # of a synodic cycle
ELEVATION_TOL = 1.0e-6    # degrees

# Snapshots taken from _celestial.py BEFORE the moon-placement work, for three
# unrelated sites/clocks and three times each:
#   (config, [(wall_seconds, sun_el, sun_az, moon_el, moon_az, phase, illum,
#              sidereal)])
REFERENCE = [
  ({'latitude_deg': 45.0, 'day_of_year': 220.0, 'time_of_day': 4.0,
    'time_scale': 480.0},
   [(0.0, -8.911497451555627, 55.96762068388023, -48.63555119351328,
     313.4839276675775, 0.25397182911939525, 0.5124315482691186,
     80295.99409199867),
    (7.5, 0.3925205675772404, 66.99084558033239, -54.872671224180934,
     334.2404476919658, 0.25530596721951904, 0.516606388125179,
     80311.03516063833),
    (123.25, -11.167028377469293, 306.75662151791437, 24.6766205404048,
     211.6428525715307, 0.27570697510651987, 0.5801584038510719,
     80543.16898664385)]),
  ({'latitude_deg': 36.0, 'day_of_year': 223.0, 'time_of_day': 12.0,
    'time_scale': 480.0},
   [(0.0, 69.35194122681885, 176.3825768223157, -41.79797968762612,
     86.69659382748307, 0.35700981781400265, 0.8111258965512158,
     81499.279583172),
    (7.5, 66.03151985848876, 214.1469758113307, -30.093065321891796,
     95.1739937130252, 0.35826685277480314, 0.8142122706160977,
     81514.32065181166),
    (123.25, -9.991225426336465, 62.82524280424031, -29.31512119034598,
     263.5249306549067, 0.37762948003143343, 0.8591682106246671,
     81746.45447781718)]),
  ({'latitude_deg': -33.0, 'day_of_year': 15.0, 'time_of_day': 21.5,
    'time_scale': 3600.0, 'year': 2011},
   [(0.0, -23.89285131340133, 220.860305316106, 31.624931072391274,
     343.18204077376015, 0.35831404097163533, 0.8143186641436391,
     1456997.4861463688),
    (7.5, -2.225218011126841, 116.92907318079494, -40.42131013665202,
     274.57162966186957, 0.36875804693457453, 0.8391813593030555,
     1457110.2941611663),
    (123.25, -36.36766114513319, 170.0964793590817, 44.054654178197154,
     8.453876016397151, 0.5431573004306999, 0.9807819275214087,
     1458851.2978562077)]),
]

_REFERENCE_FIELDS = ("sun_elevation_deg", "sun_azimuth_deg",
                     "moon_elevation_deg", "moon_azimuth_deg", "moon_phase",
                     "moon_illumination", "sidereal_angle_deg")


###############################################################################
# helpers
###############################################################################


def phase_error(current, target):
  """Shortest signed distance on the synodic circle."""
  return ((current - target + 0.5) % 1.0) - 0.5


def lunations(model, count=3, step_days=0.02, max_days=400.0):
  """[days between successive NEW moons] measured by scrubbing the model's own
  clock — the phase wraps 1->0 at new, so a decrease is a lunation boundary,
  refined by linear interpolation across the wrap."""
  d0 = model._epoch_days
  crossings = []
  prev_d, prev_p = d0, model.evaluate_days(d0).moon_phase
  d = d0
  while d < d0 + max_days and len(crossings) < count + 1:
    d += step_days
    p = model.evaluate_days(d).moon_phase
    if p < prev_p:                                  # wrapped through new
      t = (1.0 - prev_p) / ((1.0 - prev_p) + p)
      crossings.append(prev_d + t * step_days)
    prev_d, prev_p = d, p
  return [b - a for a, b in zip(crossings, crossings[1:])]


###############################################################################


class TestDeclaredPlacement(unittest.TestCase):

  ########################################
  def test_named_phases_land_at_t0(self):
    for name, target in sorted(_celestial.PHASE_NAMES.items()):
      model = CelestialModel(moon_phase=name, **SITE)
      got = model.at(0.0).moon_phase
      self.assertLess(abs(phase_error(got, target)), PHASE_TOL,
                      "%s: declared %.4f, got %.12f" % (name, target, got))

  ########################################
  def test_fraction_phases_land_at_t0(self):
    for target in (0.1, 0.42, 0.87):
      model = CelestialModel(moon_phase=target, **SITE)
      got = model.at(0.0).moon_phase
      self.assertLess(abs(phase_error(got, target)), PHASE_TOL,
                      "%.4f: got %.12f" % (target, got))

  ########################################
  def test_named_phases_carry_their_illumination(self):
    # The names are not labels: full is a lit disc, new is a dark one, and the
    # quarters are half-lit. This is the value the light's intensity rides.
    expected = {"new": 0.0, "first_quarter": 0.5, "full": 1.0,
                "last_quarter": 0.5}
    for name, illum in sorted(expected.items()):
      got = CelestialModel(moon_phase=name, **SITE).at(0.0).moon_illumination
      self.assertAlmostEqual(got, illum, delta=0.005,
                             msg="%s illumination %.4f" % (name, got))

  ########################################
  def test_initial_elevation_lands_at_t0(self):
    for target in (-40.0, 0.0, 15.5, 60.0):
      model = CelestialModel(moon_initial_elevation=target, **SITE)
      got = model.at(0.0).moon_elevation_deg
      self.assertAlmostEqual(got, target, delta=ELEVATION_TOL,
                             msg="declared %.3f, got %.9f" % (target, got))

  ########################################
  def test_site_and_clock_are_echoed(self):
    cfg = _celestial.normalize_config(dict(SITE, year=2033, moon_phase="full",
                                           moon_initial_elevation=None,
                                           moon_orbit_rate=3.0))
    model = CelestialModel.from_config(cfg)
    self.assertEqual(model.year, 2033)
    self.assertEqual(model.day_of_year, 223.0)
    self.assertEqual(model.time_of_day, 12.0)
    self.assertEqual(model.latitude_deg, 36.0)
    self.assertEqual(model.moon_phase, 0.5)
    self.assertEqual(model.moon_orbit_rate, 3.0)
    # config() is what a caller reads back out; it must round-trip whole.
    self.assertEqual(model.config(),
                     {k: cfg[k] for k in _celestial.MODEL_CONFIG_KEYS})

  ########################################
  def test_the_declaration_is_year_proof(self):
    # The point of declaring: the SAME phase on the same date of ANY year. The
    # undeclared moon of those years is all over the cycle, which is the defect.
    years = (1999, 2000, 2007, 2033, 2100)
    declared = [CelestialModel(moon_phase="full", year=y, **SITE).at(0.0)
                for y in years]
    for y, snap in zip(years, declared):
      self.assertLess(abs(phase_error(snap.moon_phase, 0.5)), PHASE_TOL,
                      "year %d: phase %.12f" % (y, snap.moon_phase))
    spread = [CelestialModel(year=y, **SITE).at(0.0).moon_phase for y in years]
    self.assertGreater(max(spread) - min(spread), 0.2,
                       "the undeclared moon should NOT be reproducible across "
                       "years — if it is, this test proves nothing")

  ########################################
  def test_declaring_the_moon_leaves_the_sun_and_the_date_alone(self):
    plain = CelestialModel(**SITE)
    for kwargs in ({"moon_phase": "new"},
                   {"moon_initial_elevation": 20.0},
                   {"moon_orbit_rate": 7.0},
                   {"moon_phase": 0.25, "moon_orbit_rate": 0.5}):
      declared = CelestialModel(**dict(SITE, **kwargs))
      for t in (0.0, 30.0, 400.0):
        a, b = plain.at(t), declared.at(t)
        for field in ("sun_elevation_deg", "sun_azimuth_deg",
                      "sun_declination_deg", "sidereal_angle_deg",
                      "epoch_days", "julian_day"):
          self.assertEqual(getattr(a, field), getattr(b, field),
                           "%s moved %s at t=%.1f" % (kwargs, field, t))


###############################################################################


class TestOrbitRate(unittest.TestCase):

  ########################################
  def test_earth_rate_is_the_earth_lunation(self):
    measured = lunations(CelestialModel(**SITE))
    for period in measured:
      self.assertAlmostEqual(period, 29.53, delta=0.4,
                             msg="lunation %.3f days" % period)

  ########################################
  def test_rate_scales_the_synodic_period(self):
    base = sum(lunations(CelestialModel(**SITE))) / 3.0
    for rate in (0.5, 2.0, 4.0):
      measured = lunations(CelestialModel(moon_orbit_rate=rate, **SITE))
      mean = sum(measured) / len(measured)
      self.assertAlmostEqual(mean, base / rate, delta=0.02 * base / rate,
                             msg="rate %.2f: %.3f days, expected %.3f"
                                 % (rate, mean, base / rate))

  ########################################
  def test_rate_composes_with_a_declared_phase(self):
    for rate in (0.25, 1.0, 2.0, 13.0):
      for name in ("new", "full", "first_quarter"):
        got = CelestialModel(moon_phase=name, moon_orbit_rate=rate,
                             **SITE).at(0.0).moon_phase
        self.assertLess(abs(phase_error(got, _celestial.PHASE_NAMES[name])),
                        PHASE_TOL,
                        "rate %.2f %s: %.12f" % (rate, name, got))

  ########################################
  def test_rate_composes_with_a_declared_elevation(self):
    for rate in (0.5, 3.0):
      got = CelestialModel(moon_initial_elevation=12.0, moon_orbit_rate=rate,
                           **SITE).at(0.0).moon_elevation_deg
      self.assertAlmostEqual(got, 12.0, delta=ELEVATION_TOL)


###############################################################################


class TestUndeclaredIsUnchanged(unittest.TestCase):

  ########################################
  def test_default_model_matches_the_pre_placement_ephemeris(self):
    for cfg, rows in REFERENCE:
      model = CelestialModel(**cfg)
      for row in rows:
        snap = model.at(row[0])
        got = tuple(getattr(snap, f) for f in _REFERENCE_FIELDS)
        self.assertEqual(got, tuple(row[1:]),
                         "%s at t=%.2f drifted from the reference ephemeris"
                         % (cfg, row[0]))

  ########################################
  def test_rate_one_is_inert(self):
    # 1.0 is the DEFAULT, so it must not even round the element clock.
    plain = CelestialModel(**SITE)
    rated = CelestialModel(moon_orbit_rate=1.0, **SITE)
    for t in (0.0, 11.0, 250.0):
      self.assertEqual(plain.at(t).as_dict(), rated.at(t).as_dict())

  ########################################
  def test_default_config_declares_no_moon(self):
    cfg = _celestial.normalize_config({})
    self.assertIsNone(cfg["moon_phase"])
    self.assertIsNone(cfg["moon_initial_elevation"])
    self.assertEqual(cfg["moon_orbit_rate"], 1.0)


###############################################################################


class TestOverConstrained(unittest.TestCase):
  """A phase and an elevation together pin the same body from two directions."""

  ########################################
  def test_phase_plus_impossible_elevation_names_the_conflict(self):
    # Full moon at NOON is under the observer's feet; asking for it 30 deg up is
    # asking for a moon opposite the sun on the same side of the sky as the sun.
    with self.assertRaises(ValueError) as caught:
      CelestialModel(moon_phase="full", moon_initial_elevation=30.0, **SITE)
    message = str(caught.exception)
    for expected in ("moon_phase", "moon_initial_elevation", "cannot both hold",
                     "5.15"):
      self.assertIn(expected, message)
    # the message has to carry the band that WOULD work, in numbers
    numbers = [float(tok.strip(",")) for tok in message.replace("[", " ")
               .replace("]", " ").split()
               if _looks_numeric(tok.strip(","))]
    self.assertTrue(any(n < -60.0 for n in numbers),
                    "no reachable-band figure in: %s" % message)

  ########################################
  def test_the_band_the_message_names_actually_works(self):
    lo, hi = _reported_band(SITE, "full")
    self.assertLess(lo, hi)
    for target in (lo + 0.1 * (hi - lo), 0.5 * (lo + hi), hi - 0.1 * (hi - lo)):
      model = CelestialModel(moon_phase="full", moon_initial_elevation=target,
                             **SITE)
      snap = model.at(0.0)
      self.assertAlmostEqual(snap.moon_elevation_deg, target,
                             delta=ELEVATION_TOL)
      self.assertLess(abs(phase_error(snap.moon_phase, 0.5)), PHASE_TOL,
                      "the phase gave way to the elevation")

  ########################################
  def test_elevation_alone_is_only_bounded_by_the_sky(self):
    # No phase: the moon may be re-epoched anywhere on its orbit, so a whole
    # sweep of altitudes is reachable — but not 89 deg at latitude 36 in August.
    with self.assertRaises(ValueError) as caught:
      CelestialModel(moon_initial_elevation=89.0, **SITE)
    self.assertIn("unreachable", str(caught.exception))
    self.assertIn("moon_initial_elevation", str(caught.exception))

  ########################################
  def test_the_declared_elevation_pins_the_moons_declination(self):
    # WHY the pair is worth declaring at all: inside the band, the elevation is
    # what decides how high that phase rides — the whole "lit all night" ask.
    lo, hi = _reported_band(SITE, "full")
    low_moon = _culmination(CelestialModel(moon_phase="full", **SITE,
                                           moon_initial_elevation=lo + 0.2))
    high_moon = _culmination(CelestialModel(moon_phase="full", **SITE,
                                            moon_initial_elevation=hi - 0.2))
    self.assertGreater(high_moon - low_moon, 3.0,
                       "the elevation declaration should move the night's moon "
                       "(%.2f vs %.2f deg at culmination)"
                       % (low_moon, high_moon))


###############################################################################


class TestLoudValidation(unittest.TestCase):

  ########################################
  def test_unknown_config_key_raises(self):
    with self.assertRaises(KeyError) as caught:
      _celestial.normalize_config({"moon_phaze": "full"})
    self.assertIn("moon_phaze", str(caught.exception))

  ########################################
  def test_unknown_phase_name_raises(self):
    for bad in ("gibbous", "FULL", "half"):
      with self.assertRaises(ValueError) as caught:
        _celestial.phase_fraction(bad)
      self.assertIn("first_quarter", str(caught.exception))

  ########################################
  def test_out_of_range_phase_raises(self):
    for bad in (-0.1, 1.5, 180.0):
      with self.assertRaises(ValueError):
        _celestial.phase_fraction(bad)

  ########################################
  def test_bad_orbit_rate_raises(self):
    for bad in (0.0, -1.0):
      with self.assertRaises(ValueError):
        _celestial.normalize_config({"moon_orbit_rate": bad})

  ########################################
  def test_bad_initial_elevation_raises(self):
    with self.assertRaises(ValueError):
      _celestial.normalize_config({"moon_initial_elevation": 120.0})


###############################################################################


class TestConfigTransport(unittest.TestCase):
  """The declaration has to survive the channel it travels on: the config is
  JSON on PythonComponentData.scriptData, which is what rides the .ecs across
  the two-process author->player recipe."""

  ########################################
  def test_json_round_trip_reproduces_the_moon(self):
    declared = dict(SITE, year=2033, moon_phase="full",
                    moon_initial_elevation=-70.0, moon_orbit_rate=2.5)
    cfg = _celestial.normalize_config(declared)
    carried = json.loads(json.dumps(cfg))
    self.assertEqual(carried, cfg)          # nothing lost to JSON's types
    here = CelestialModel.from_config(cfg)
    there = CelestialModel.from_config(carried)
    for t in (0.0, 42.0, 500.0):
      self.assertEqual(here.at(t).as_dict(), there.at(t).as_dict())

  ########################################
  def test_the_moon_keys_travel_with_the_site_and_clock(self):
    # Scene.moon()/Scene.stars() copy exactly MODEL_CONFIG_KEYS off the sun's
    # published config — the placement has to be in that tuple or the moon
    # LIGHT would be aimed by a different moon than the one declared.
    for key in ("moon_phase", "moon_initial_elevation", "moon_orbit_rate"):
      self.assertIn(key, _celestial.MODEL_CONFIG_KEYS)
    sun_cfg = _celestial.normalize_config(dict(SITE, moon_phase="full"))
    moon_cfg = {k: sun_cfg[k] for k in _celestial.MODEL_CONFIG_KEYS}
    moon_cfg["body"] = "moon"
    moon_cfg["base_intensity"] = 0.4
    moon_cfg = _celestial.normalize_config(moon_cfg)
    self.assertLess(abs(phase_error(
      CelestialModel.from_config(moon_cfg).at(0.0).moon_phase, 0.5)), PHASE_TOL)


###############################################################################


class TestPhaseDrivesTheOneBrightnessPath(unittest.TestCase):
  """phase reaches the rendered sky ONLY through the moon light's intensity:
  illumination -> _night_policy.moon_intensity_scale -> base_intensity ->
  the SkyBody==2 light (which is what the sky's moon disc and its moonlit
  scatter read). No second phase channel exists, and none may be added."""

  ########################################
  def test_new_moon_is_dark_and_full_moon_is_not(self):
    # The GEOMETRY is held fixed (moon 40 deg up, sun 20 deg down: the middle of
    # a clear night) and only the declared phase varies, so what this measures
    # is the phase -> illumination -> intensity path and nothing else.
    scales = {}
    for name in ("new", "full"):
      illumination = CelestialModel(moon_phase=name, **SITE).at(0.0) \
                                                            .moon_illumination
      scales[name] = _night_policy.moon_intensity_scale(-20.0, 40.0,
                                                        illumination)
    self.assertLess(scales["new"], 1.0e-3,
                    "phase='new' still lights the sky (moon scale %.6f)"
                    % scales["new"])
    self.assertGreater(scales["full"], 0.5,
                       "phase='full' does not light (moon scale %.6f)"
                       % scales["full"])

  ########################################
  def test_a_new_moon_is_never_up_in_a_dark_sky_either(self):
    # The other half of the same statement, and the reason the test above has to
    # fix the geometry: a new moon RIDES WITH THE SUN, so it is not merely dim
    # at night, it is not there. The declaration moves the body, not a dimmer.
    model = CelestialModel(moon_phase="new", **SITE)
    for i in range(0, 1801):
      snap = model.at(i * 0.1)
      if snap.sun_elevation_deg < -18.0:
        self.assertLess(snap.moon_elevation_deg, 12.0,
                        "a new moon %0.1f deg up in an %0.1f-deg-dark sky"
                        % (snap.moon_elevation_deg, snap.sun_elevation_deg))


###############################################################################


def _looks_numeric(token):
  try:
    float(token)
    return True
  except ValueError:
    return False


def _reported_band(site, phase):
  """The reachable elevation band a phase leaves, read OUT OF the error message
  the impossible pair raises — so the test uses the same numbers an author
  would."""
  try:
    CelestialModel(moon_phase=phase, moon_initial_elevation=90.0, **site)
  except ValueError as error:
    tokens = [tok.strip(",") for tok in str(error).split()]
    numbers = [float(t) for t in tokens if _looks_numeric(t)]
    band = [n for n in numbers if -90.0 <= n <= 90.0]
    return min(band[-2:]), max(band[-2:])
  raise AssertionError("90 degrees should not be reachable at a declared phase")


def _culmination(model, hours=24.0, samples=1441):
  """Highest the moon gets over one clock day."""
  seconds = hours * 3600.0 / model.time_scale
  return max(model.at(seconds * i / (samples - 1)).moon_elevation_deg
             for i in range(samples))


###############################################################################

if __name__ == "__main__":
  unittest.main()
