#!/usr/bin/env python3
###############################################################################
# star_catalog.py — unit test for the BRIGHT STAR CATALOGUE bake
# (obt.project/scripts/ork/hypergraph/assets/mesh/_bsc5.py + star_catalog.py):
# that the committed catalog parses to the record counts its ReadMe states,
# that the baked DOME-SPACE direction lands each star exactly where
# _celestial._alt_az puts it under the shipped tilt(latitude-90) * spin(-sidereal)
# orientation, that magnitude -> luminance is Pogson's ratio to float precision,
# and that the B-V -> temperature -> blackbody tint chain is physically sane.
#
# Runs STANDALONE with plain CPython — no engine, no staging, NO GPU:
#     python3 obt.project/unittests/star_catalog.py
#
# HOW (the star_dome.py pattern): _bsc5.py and _celestial.py are both stdlib-only
# by construction, so they are loaded BY PATH — a dotted import would drag in the
# assets/scene package __init__ chains, which pull orkengine.
#
# The ENGINE classes are SKIPPED (not failed) when orkengine is unimportable:
# one re-runs the position check through the real
# CelestialSnapshot.star_dome_quat() instead of the pure-python quaternion, the
# other exercises the splat-quad emitter (which needs lev2.Geometry's module).
# Neither needs a GPU.
#
# WHAT IT DOES NOT COVER: the .ogeo bake, the drawable, and anything a shader
# does with the baked attributes — the material slice and a rendered-frame gate
# own those.
###############################################################################

import importlib.util
import math
import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.join(_HERE, "..", "scripts")
_MESH_DIR = os.path.join(_SCRIPTS, "ork", "hypergraph", "assets", "mesh")
_SCENE_DIR = os.path.join(_SCRIPTS, "ork", "hypergraph", "ecs", "scene")


def _load(name, path):
  spec = importlib.util.spec_from_file_location(name, path)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


_bsc5 = _load("_probe_bsc5", os.path.join(_MESH_DIR, "_bsc5.py"))
_celestial = _load("_probe_celestial", os.path.join(_SCENE_DIR, "_celestial.py"))

_STARS, _STATS = _bsc5.load_catalog(verbose=False)
_BY_HR = {s.hr: s for s in _STARS}

# Published J2000 positions / photometry for the probe stars, to catch a byte
# column slipped by one: a mis-sliced RA still round-trips through the
# convention check, but it is not where the star is.
#   HR   name        RA deg     Dec deg    V      B-V
PROBES = (
  (2491, "Sirius",   101.2871, -16.7161, -1.46,  0.00),
  (7001, "Vega",     279.2346,  38.7836,  0.03,  0.00),
  ( 424, "Polaris",   37.9529,  89.2642,  2.02,  0.60),
  (2061, "Betelgeuse", 88.7929,  7.4069,  0.50,  1.85),
)

# (latitude, sidereal angle) samples for the convention check — both signs of
# latitude, the equator, and spins spread around the circle.
SITES = (-33.9, 0.0, 23.5, 45.0, 60.0, 78.0)
SPINS = (0.0, 47.3, 123.4, 210.0, 359.1)

# The UBV zero point: an unreddened A0V star. Ballesteros puts B-V=0.65 (the
# sun's) at 5778K, which is the band the color test asserts.
SUNLIKE_BV = 0.65


###############################################################################
# pure-python quaternion — the same rotation star_dome_quat() builds, with no
# engine types, so the convention check runs under a bare python3.
###############################################################################


def _axis_angle(axis, radians):
  s = math.sin(0.5 * radians)
  return (math.cos(0.5 * radians), axis[0] * s, axis[1] * s, axis[2] * s)


def _mul(a, b):
  w1, x1, y1, z1 = a
  w2, x2, y2, z2 = b
  return (w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
          w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
          w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
          w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)


def _apply(q, v):
  w, x, y, z = q
  m = ((1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w)),
       (2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
       (2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)))
  return tuple(sum(m[r][c] * v[c] for c in range(3)) for r in range(3))


def _dome_quat(pole_elevation_deg, sidereal_angle_deg):
  """The port of CelestialSnapshot.star_dome_quat() onto the plain tuples
  above: tilt(+X, latitude-90) * spin(+Y, -sidereal)."""
  return _mul(_axis_angle((1.0, 0.0, 0.0),
                          math.radians(pole_elevation_deg - 90.0)),
              _axis_angle((0.0, 1.0, 0.0),
                          math.radians(-sidereal_angle_deg)))


def _angle_between(a, b):
  """Degrees between two directions, via atan2(|a x b|, a.b) on NORMALIZED
  inputs. Not acos(dot): acos loses half the mantissa near zero angle, and a
  float32 quaternion's ~1e-8 length deficit alone reads as 0.007 degrees
  through it — which would put a tolerance floor above the errors this test
  exists to catch."""
  a = _unit(a)
  b = _unit(b)
  cross = (a[1] * b[2] - a[2] * b[1],
           a[2] * b[0] - a[0] * b[2],
           a[0] * b[1] - a[1] * b[0])
  return math.degrees(math.atan2(math.sqrt(sum(c * c for c in cross)),
                                 sum(x * y for x, y in zip(a, b))))


def _unit(v):
  length = math.sqrt(sum(float(c) * float(c) for c in v))
  return tuple(float(c) / length for c in v)


###############################################################################


class TestCatalogCounts(unittest.TestCase):

  ########################################
  def test_record_and_star_totals(self):
    # The ReadMe's File Summary (9110 records) and Description (9096 stars).
    self.assertEqual(_STATS["records"], 9110)
    self.assertEqual(_STATS["stars"], 9096)
    self.assertEqual(len(_STARS), 9096)

  ########################################
  def test_filtered_set_is_the_known_non_stellar_records(self):
    # Not just the COUNT: the identity of the 14 novae / extragalactic objects
    # retained to preserve the HR numbering. A different 14 means a different
    # (or mangled) catalog file.
    self.assertEqual(len(_STATS["filtered"]), 14)
    self.assertEqual(_STATS["filtered"], _bsc5.NON_STELLAR_HR)

  ########################################
  def test_every_star_has_finite_position_magnitude_and_tint(self):
    for s in _STARS:
      self.assertTrue(0.0 <= s.ra_deg < 360.0, msg=repr(s))
      self.assertTrue(-90.0 <= s.dec_deg <= 90.0, msg=repr(s))
      self.assertTrue(math.isfinite(s.vmag), msg=repr(s))
      self.assertTrue(math.isfinite(s.luminance) and s.luminance > 0.0,
                      msg=repr(s))
      self.assertAlmostEqual(math.sqrt(sum(c * c for c in s.direction)), 1.0,
                             places=12, msg=repr(s))
      for c in s.tint:
        self.assertTrue(math.isfinite(c) and 0.0 <= c <= 1.0, msg=repr(s))

  ########################################
  def test_probe_stars_parse_to_their_published_values(self):
    for hr, name, ra, dec, vmag, bv in PROBES:
      s = _BY_HR[hr]
      self.assertAlmostEqual(s.ra_deg, ra, places=3, msg=name)
      self.assertAlmostEqual(s.dec_deg, dec, places=3, msg=name)
      self.assertAlmostEqual(s.vmag, vmag, places=2, msg=name)
      self.assertAlmostEqual(s.bv, bv, places=2, msg=name)


###############################################################################


class TestDomeConvention(unittest.TestCase):
  """The baked direction, turned by the dome's own orientation, must land where
  the ephemeris says the star is. This is the whole reason the bake exists."""

  ########################################
  def test_baked_direction_matches_alt_az(self):
    for hr, name, _ra, _dec, _v, _bv in PROBES:
      s = _BY_HR[hr]
      for latitude in SITES:
        for spin in SPINS:
          world = _apply(_dome_quat(latitude, spin), s.direction)
          elevation, azimuth = _celestial._alt_az(s.ra_deg, s.dec_deg,
                                                  spin, latitude)
          want = _celestial._sky_vector(elevation, azimuth)
          self.assertLess(_angle_between(world, want), 1.0e-9,
                          msg="%s at latitude %g spin %g" % (name, latitude, spin))

  ########################################
  def test_convention_holds_for_the_whole_catalog(self):
    # One site/spin, but every star: a mapping that is right for four probes and
    # wrong in a quadrant would survive the test above.
    latitude, spin = 45.0, 123.4
    q = _dome_quat(latitude, spin)
    worst = 0.0
    for s in _STARS:
      elevation, azimuth = _celestial._alt_az(s.ra_deg, s.dec_deg, spin, latitude)
      worst = max(worst, _angle_between(_apply(q, s.direction),
                                        _celestial._sky_vector(elevation, azimuth)))
    self.assertLess(worst, 1.0e-9)

  ########################################
  def test_polaris_sits_on_the_pole_at_every_spin(self):
    # Polaris is 0.74 deg off the pole, so: within ~1 deg of dome +Y in object
    # space, and — however the sky is spun — within ~1 deg of the pole's own
    # place in the sky (due north at elevation = latitude).
    polaris = _BY_HR[424]
    self.assertLess(_angle_between(polaris.direction, (0.0, 1.0, 0.0)), 1.0)
    for latitude in SITES:
      pole = _celestial._sky_vector(latitude, 0.0)
      for spin in SPINS:
        world = _apply(_dome_quat(latitude, spin), polaris.direction)
        self.assertLess(_angle_between(world, pole), 1.0,
                        msg="latitude %g spin %g" % (latitude, spin))


###############################################################################


class TestPogson(unittest.TestCase):

  ########################################
  def test_zero_magnitude_is_unity(self):
    self.assertEqual(_bsc5.pogson_luminance(0.0), 1.0)

  ########################################
  def test_five_magnitudes_is_exactly_a_hundred(self):
    # The defining ratio. Checked across the catalog's whole magnitude span, so
    # any clamp, offset or shaping term anywhere in the chain shows up here.
    for v in (-2.0, -1.46, 0.0, 0.03, 2.5, 6.0, 7.96):
      ratio = _bsc5.pogson_luminance(v) / _bsc5.pogson_luminance(v + 5.0)
      self.assertAlmostEqual(ratio / 100.0, 1.0, places=12, msg="V=%g" % v)

  ########################################
  def test_one_magnitude_is_the_pogson_step(self):
    step = 10.0 ** 0.4
    for v in (-1.0, 0.0, 3.3, 7.0):
      ratio = _bsc5.pogson_luminance(v) / _bsc5.pogson_luminance(v + 1.0)
      self.assertAlmostEqual(ratio / step, 1.0, places=12, msg="V=%g" % v)

  ########################################
  def test_catalog_luminances_follow_the_magnitudes(self):
    for s in (_BY_HR[hr] for hr, *_ in PROBES):
      self.assertAlmostEqual(s.luminance / (10.0 ** (-0.4 * s.vmag)), 1.0,
                             places=12, msg=repr(s))
    brightest = min(_STARS, key=lambda s: s.vmag)
    self.assertEqual(brightest.hr, 2491)                      # Sirius
    self.assertAlmostEqual(brightest.luminance, 3.8371, places=4)

  ########################################
  def test_radiance_carries_the_luminance_recoverably(self):
    # The vertex buffer ships tint * luminance in one float3; because the tint
    # is peak-normalized to 1, max(radiance) IS the luminance, exactly. The
    # material slice depends on that.
    for s in (_BY_HR[hr] for hr, *_ in PROBES):
      self.assertEqual(max(s.radiance), s.luminance, msg=repr(s))


###############################################################################


class TestColor(unittest.TestCase):

  ########################################
  def test_sunlike_color_index_gives_a_sunlike_temperature(self):
    t = _bsc5.bv_to_temperature(SUNLIKE_BV)
    self.assertGreater(t, 5700.0)
    self.assertLess(t, 5900.0)

  ########################################
  def test_temperature_decreases_with_color_index(self):
    temps = [_bsc5.bv_to_temperature(bv)
             for bv in (-0.28, 0.0, 0.3, 0.65, 1.0, 1.85, 2.5)]
    for hot, cool in zip(temps, temps[1:]):
      self.assertGreater(hot, cool)

  ########################################
  def test_blue_stars_tint_bluer_than_red_stars(self):
    blue = _bsc5.bv_to_linear_srgb(-0.2)
    red  = _bsc5.bv_to_linear_srgb(1.2)
    self.assertGreater(blue[2] / blue[0], red[2] / red[0])
    self.assertEqual(max(blue), blue[2])                       # blue-dominant
    self.assertEqual(max(red), red[0])                         # red-dominant

  ########################################
  def test_tints_are_peak_normalized_chroma(self):
    for bv in (-0.28, 0.0, 0.65, 1.5, 3.0, 5.74):
      tint = _bsc5.bv_to_linear_srgb(bv)
      self.assertAlmostEqual(max(tint), 1.0, places=12, msg="B-V=%g" % bv)
      self.assertTrue(all(0.0 <= c <= 1.0 for c in tint), msg="B-V=%g" % bv)

  ########################################
  def test_catalog_extremes_stay_inside_the_formula(self):
    # Every B-V in the file must produce a positive temperature — the
    # Ballesteros pole sits at B-V ~ -0.67, well below the catalog's -0.28.
    for s in _STARS:
      self.assertGreater(_bsc5.bv_to_temperature(s.bv), 0.0, msg=repr(s))


###############################################################################
# ENGINE (still CPU-only) — the shipped quat helper and the splat emitter.
###############################################################################

try:
  from orkengine.core import vec3, quat        # noqa: F401
  _HAVE_ENGINE = True
except Exception:
  _HAVE_ENGINE = False

try:
  # by NAME, not by path: the module's own absolute imports (asset_core,
  # _common) need the package on sys.path anyway.
  sys.path.insert(0, os.path.abspath(_SCRIPTS))
  import ork.hypergraph.assets.mesh.star_catalog as _star_catalog
  _HAVE_MESH = True
except Exception as error:
  _star_catalog = None
  _HAVE_MESH = False
  _MESH_SKIP = "assets.mesh.star_catalog not importable (%s)" % error
else:
  _MESH_SKIP = ""


@unittest.skipUnless(_HAVE_ENGINE, "orkengine not importable (pure-math run)")
class TestDomeConventionThroughShippedQuat(unittest.TestCase):

  ########################################
  def test_star_dome_quat_places_the_baked_directions(self):
    # Same assertion as TestDomeConvention, but through the ACTUAL rotation the
    # DSL hands the entity — so a change to star_dome_quat's axes or multiply
    # order breaks the bake here rather than on screen. TOLERANCE: the helper
    # builds float32 quats, worth ~1e-5 degrees of aim.
    import numpy as np
    for hr, name, _ra, _dec, _v, _bv in PROBES:
      s = _BY_HR[hr]
      for latitude in SITES:
        for spin in SPINS:
          snap = _celestial.CelestialSnapshot(pole_elevation_deg=latitude,
                                              sidereal_angle_deg=spin)
          q = tuple(float(c) for c in np.array(snap.star_dome_quat(), copy=False))
          elevation, azimuth = _celestial._alt_az(s.ra_deg, s.dec_deg,
                                                  spin, latitude)
          self.assertLess(_angle_between(_apply(q, s.direction),
                                         _celestial._sky_vector(elevation, azimuth)),
                          1.0e-4,
                          msg="%s at latitude %g spin %g" % (name, latitude, spin))


@unittest.skipUnless(_HAVE_MESH, _MESH_SKIP)
class TestSplatEmit(unittest.TestCase):
  """The quad layout — attribute placement, size and winding. No GPU: this only
  calls the array builder, not the .ogeo bake."""

  ########################################
  def setUp(self):
    self.stars = [_BY_HR[hr] for hr, *_ in PROBES]
    self.half = _star_catalog.QUAD_HALF_ANGLE_DEG
    self.radius = _star_catalog.LOCAL_RADIUS
    (self.verts, self.tris, self.norms,
     self.radiance, self.uvs) = _star_catalog._splat_geometry(
       self.stars, self.radius, self.half)

  ########################################
  def test_four_verts_and_two_tris_per_star(self):
    n = len(self.stars)
    self.assertEqual(self.verts.shape, (n * 4, 3))
    self.assertEqual(self.tris.shape, (n * 2, 3))
    self.assertEqual(self.norms.shape, (n * 4, 3))
    self.assertEqual(self.radiance.shape, (n * 4, 3))
    self.assertEqual(self.uvs.shape, (n * 4, 2))

  ########################################
  def test_per_star_attributes_are_replicated_across_the_quad(self):
    for i, s in enumerate(self.stars):
      for k in range(4):
        self.assertLess(_angle_between(tuple(map(float, self.norms[i * 4 + k])),
                                       s.direction), 1.0e-4, msg=repr(s))
        self.assertAlmostEqual(float(max(self.radiance[i * 4 + k])),
                               s.luminance, places=5, msg=repr(s))
      corners = {tuple(map(float, self.uvs[i * 4 + k])) for k in range(4)}
      self.assertEqual(corners, {(-1.0, -1.0), (1.0, -1.0),
                                 (1.0, 1.0), (-1.0, 1.0)})

  ########################################
  def test_quad_subtends_the_declared_angle(self):
    for i, s in enumerate(self.stars):
      for k in range(4):
        v = self.verts[i * 4 + k]
        offaxis = _angle_between(tuple(float(c) / float(
                                   sum(x * x for x in map(float, v)) ** 0.5)
                                   for c in v), s.direction)
        # corner sits on the quad DIAGONAL: atan(sqrt(2) tan(half))
        want = math.degrees(math.atan(math.sqrt(2.0)
                                      * math.tan(math.radians(self.half))))
        self.assertAlmostEqual(offaxis, want, places=4, msg=repr(s))

  ########################################
  def test_tris_wind_outward_before_the_flip(self):
    # _splat_geometry emits OUTWARD-facing winding; build_geometry(flip_winding)
    # reverses it for the inside view, exactly as StarDomeMesh does.
    for t in self.tris:
      a, b, c = (self.verts[i].astype(float) for i in t)
      n = self.norms[t[0]].astype(float)
      cross = ((b - a)[1] * (c - a)[2] - (b - a)[2] * (c - a)[1],
               (b - a)[2] * (c - a)[0] - (b - a)[0] * (c - a)[2],
               (b - a)[0] * (c - a)[1] - (b - a)[1] * (c - a)[0])
      self.assertGreater(sum(x * y for x, y in zip(cross, n)), 0.0)


###############################################################################

if __name__ == '__main__':
  _result = unittest.main(exit=False, verbosity=2).result
  _ok = _result.wasSuccessful()
  print("VERDICT: star_catalog %s (%d tests)"
        % ("PASS" if _ok else "FAIL", _result.testsRun))
  sys.exit(0 if _ok else 1)
