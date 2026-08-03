#!/usr/bin/env python3
###############################################################################
# star_splat.py — unit test for the STAR SPLAT sizing + energy law
# (obt.project/scripts/ork/hypergraph/assets/mesh/_splat_sizing.py, the module
# the catalog BAKE and the star material both size themselves from) and for the
# material the DSL actually emits from it.
#
# The two claims under test:
#
#   THE ENVELOPE FITS. The baked quad is a fixed ANGULAR size; the runtime sigma
#   floor is a PIXEL size, so the coarsest supported configuration demands the
#   largest angular footprint. If the envelope is smaller than that demand the
#   gaussian is cut by the quad rectangle — a square-cornered blob that silently
#   loses energy. So: the envelope is >= the worst-case demand, the mesh bakes
#   exactly that envelope, and the shader's refusal test is not vacuous (it does
#   fire just past the declared worst case).
#
#   ENERGY IS INVARIANT. amplitude * 2*pi*sigma_x*sigma_y is the closed-form
#   footprint sum of a 2D gaussian; the material sets amplitude = flux /
#   (2*pi*sigma_px^2), so that sum stays equal to the star's flux across a
#   resolution change AND across the floor's widening clamp.
#
# Runs STANDALONE with plain CPython — no engine, no staging, NO GPU:
#     python3 obt.project/unittests/star_splat.py
#
# The sizing module is stdlib-only by construction, so it is loaded BY PATH (a
# dotted import would drag in the assets package __init__ chains, which pull
# orkengine). The MATERIAL class needs orkengine.core (the DSL's vector types),
# so those tests SKIP — not fail — when it is unimportable. None of them need a
# GPU: they generate shader text, they do not compile or run it.
#
# WHAT IT DOES NOT COVER: whether the GPU agrees. The closed form here is the
# specification; ork.lev2/pyext/tests/llgfx/test_star_splat_energy_gate.py holds
# the shader to it on rendered pixels at two resolutions.
###############################################################################

import importlib.util
import math
import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.join(_HERE, "..", "scripts")
_MESH_DIR = os.path.join(_SCRIPTS, "ork", "hypergraph", "assets", "mesh")


def _load(name, path):
  spec = importlib.util.spec_from_file_location(name, path)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


_sz = _load("_probe_splat_sizing", os.path.join(_MESH_DIR, "_splat_sizing.py"))

# The two configurations the render gate uses, as PIXEL ANGULAR SIZES: a 65 deg
# vertical fov across 480 and across 1080 rows.
FOV_DEG = 65.0
COARSE_PIXEL_DEG = FOV_DEG / 480.0
FINE_PIXEL_DEG = FOV_DEG / 1080.0

# THE FIELD FAILURE, as a named configuration. A 640x360 window at the walker's
# default fov subtends 0.181 deg/px — coarser than the ORIGINAL 0.16 worst case,
# so every star in the sky drew the magenta refusal. The envelope now covers it;
# these are the assertions that make a constant edit reintroducing it LOUD.
FIELD_HEIGHT_PX = 360
FIELD_PIXEL_DEG = FOV_DEG / FIELD_HEIGHT_PX

# Configurations the engine actually renders, as (label, fov_deg, height_px).
# Every one of them must shade stars, not refusals.
DEPLOYED_CONFIGS = (
  ("640x360 small window",      FOV_DEG, 360),
  ("640x480",                   FOV_DEG, 480),
  ("1280x720 reference",        FOV_DEG, 720),
  ("1920x1080",                 FOV_DEG, 1080),
  ("VR 110deg / 1600px eye",    110.0,   1600),
  ("VR 110deg at 0.5 scale",    110.0,   800),
  ("VR device default 90deg",   90.0,    1200),
)


def _px_q(pixel_deg, half_angle_deg=None):
  """A pixel's angular size expressed in QUAD units (the material's `px_q`,
  which the shader reads as max(|dFdx(uv)|,|dFdy(uv)|))."""
  half = _sz.QUAD_HALF_ANGLE_DEG if half_angle_deg is None else half_angle_deg
  return math.radians(pixel_deg) / math.tan(math.radians(half))


###############################################################################
class TestEnvelopeFitsTheWorstCase(unittest.TestCase):
  """The baked angular envelope vs the runtime pixel floor — the one place the
  two sizing regimes can silently disagree."""

  ########################################
  def test_baked_envelope_covers_the_declared_worst_case(self):
    self.assertGreaterEqual(_sz.QUAD_HALF_ANGLE_DEG,
                            _sz.required_half_angle_deg())

  ########################################
  def test_the_mesh_bakes_the_sizing_module_envelope(self):
    # the bake must not carry its own copy of the number
    try:
      _mesh = _load("_probe_star_catalog_src", os.path.join(_MESH_DIR, "star_catalog.py"))
      baked = _mesh.QUAD_HALF_ANGLE_DEG
    except Exception:                       # needs numpy/orkengine — read the source
      src = open(os.path.join(_MESH_DIR, "star_catalog.py")).read()
      self.assertIn("from ork.hypergraph.assets.mesh._splat_sizing import "
                    "QUAD_HALF_ANGLE_DEG", src)
      self.assertNotIn("\nQUAD_HALF_ANGLE_DEG =", src)
      return
    self.assertEqual(baked, _sz.QUAD_HALF_ANGLE_DEG)

  ########################################
  def test_floored_gaussian_fits_at_the_worst_case_pixel(self):
    px = _px_q(_sz.WORST_CASE_PIXEL_DEG)
    sig = _sz.floored_sigma_q(0.0, px)
    self.assertTrue(_sz.fits_envelope(sig),
                    "cutoff radius %.4f q at the declared worst case" %
                    (_sz.CUTOFF_SIGMA * sig))

  ########################################
  def test_the_640x360_field_configuration_is_in_band(self):
    # THE REGRESSION. This exact configuration rendered a magenta sky; if a
    # future edit narrows the envelope or the worst case back under it, this
    # fails by name instead of shipping a broken sky.
    cutoff = _sz.CUTOFF_SIGMA * _sz.floored_sigma_q(0.0, _px_q(FIELD_PIXEL_DEG))
    self.assertLessEqual(cutoff, 1.0,
                         "640x360 at %g deg fov = %.4f deg/px would REFUSE "
                         "(cutoff %.3f of the quad)"
                         % (FOV_DEG, FIELD_PIXEL_DEG, cutoff))

  ########################################
  def test_every_deployed_configuration_is_in_band(self):
    for label, fov, height in DEPLOYED_CONFIGS:
      pixel_deg = fov / float(height)
      cutoff = _sz.CUTOFF_SIGMA * _sz.floored_sigma_q(0.0, _px_q(pixel_deg))
      self.assertLessEqual(cutoff, 1.0,
                           "%s (%.4f deg/px) would REFUSE at cutoff %.3f"
                           % (label, pixel_deg, cutoff))

  ########################################
  def test_the_declared_minimum_is_the_design_point(self):
    # the envelope is sized TO the stated minimum configuration — it fits there,
    # and not by a wide margin (overdraw scales with the square, so a large
    # margin here is a silent fill cost rather than a safety).
    cutoff = _sz.CUTOFF_SIGMA * _sz.floored_sigma_q(
      0.0, _px_q(_sz.WORST_CASE_PIXEL_DEG))
    self.assertLessEqual(cutoff, 1.0)
    self.assertGreater(cutoff, 0.85, "envelope is padded well past the stated "
                                     "minimum configuration (fill cost)")

  ########################################
  def test_the_stated_minimum_configuration_is_the_constant(self):
    # the comment beside WORST_CASE_PIXEL_DEG states 360 px at 90 deg; keep the
    # number and the stated assumption from drifting apart.
    self.assertAlmostEqual(_sz.WORST_CASE_PIXEL_DEG, 90.0 / 360.0, places=9)

  ########################################
  def test_the_refusal_is_not_vacuous(self):
    # a configuration coarser than declared must actually trip the shader's
    # envelope test — otherwise the "loud refusal" could never fire and the
    # envelope law would be untested at runtime.
    px = _px_q(_sz.WORST_CASE_PIXEL_DEG * 1.5)
    self.assertFalse(_sz.fits_envelope(_sz.floored_sigma_q(0.0, px)))

  ########################################
  def test_a_wide_physical_sigma_also_trips_the_refusal(self):
    # the other overflow route: a user cranking the star's angular size past
    # what the envelope can hold, at any resolution.
    self.assertFalse(_sz.fits_envelope(
      _sz.floored_sigma_q(0.5, _px_q(FINE_PIXEL_DEG))))


###############################################################################
class TestSigmaFloor(unittest.TestCase):
  """The floor is COMPUTED from the projected pixel footprint — never a tuned
  constant — and it only ever WIDENS."""

  ########################################
  def test_a_point_source_is_widened_to_exactly_the_pixel_floor(self):
    for pixel_deg in (COARSE_PIXEL_DEG, FINE_PIXEL_DEG, _sz.WORST_CASE_PIXEL_DEG):
      px = _px_q(pixel_deg)
      sig = _sz.floored_sigma_q(_sz.sigma_q_from_deg(5.6e-4, _sz.QUAD_HALF_ANGLE_DEG), px)
      self.assertAlmostEqual(sig / px, _sz.MIN_SIGMA_PX, places=9,
                             msg="pixel_deg=%r" % pixel_deg)

  ########################################
  def test_a_resolved_star_keeps_its_physical_sigma(self):
    px = _px_q(FINE_PIXEL_DEG)
    phys = _sz.MIN_SIGMA_PX * px * 3.0
    self.assertEqual(_sz.floored_sigma_q(phys, px), phys)

  ########################################
  def test_the_floor_tracks_resolution(self):
    # finer pixels -> a smaller angular floor. This is the whole reason the
    # floor cannot be a baked constant.
    coarse = _sz.floored_sigma_q(0.0, _px_q(COARSE_PIXEL_DEG))
    fine = _sz.floored_sigma_q(0.0, _px_q(FINE_PIXEL_DEG))
    self.assertAlmostEqual(coarse / fine, 1080.0 / 480.0, places=6)

  ########################################
  def test_the_floor_never_narrows_a_star(self):
    px = _px_q(COARSE_PIXEL_DEG)
    for phys in (0.0, 1e-6, 0.01, 0.1, 0.5, 1.0):
      self.assertGreaterEqual(_sz.floored_sigma_q(phys, px), phys)


###############################################################################
class TestEnergyConservation(unittest.TestCase):
  """amplitude x 2*pi*sigma_x*sigma_y is invariant — across resolutions and,
  the case that matters, across the floor's widening clamp."""

  FLUX = 3.84                                # Sirius, in Pogson relative units

  ########################################
  def _leg(self, pixel_deg, sigma_ang_deg):
    """(integrated luminance, amplitude, sigma_px) for one configuration, along
    the same chain the material's DSL expression walks."""
    px = _px_q(pixel_deg)
    phys = _sz.sigma_q_from_deg(sigma_ang_deg, _sz.QUAD_HALF_ANGLE_DEG)
    sig = _sz.floored_sigma_q(phys, px)
    sig_px = sig / px
    amp = _sz.amplitude(self.FLUX, sig_px)
    return _sz.integrated_luminance(amp, sig_px), amp, sig_px

  ########################################
  def test_closed_form_returns_the_flux(self):
    for sigma_px in (0.25, 0.9, 1.0, 2.5, 7.75):
      amp = _sz.amplitude(self.FLUX, sigma_px)
      self.assertAlmostEqual(_sz.integrated_luminance(amp, sigma_px),
                             self.FLUX, places=12, msg="sigma_px=%r" % sigma_px)

  ########################################
  def test_invariant_across_resolution_at_the_floor(self):
    lo = self._leg(COARSE_PIXEL_DEG, 5.6e-4)
    hi = self._leg(FINE_PIXEL_DEG, 5.6e-4)
    self.assertAlmostEqual(lo[0], hi[0], places=12)
    # both floored -> the same pixel footprint, so the same peak
    self.assertAlmostEqual(lo[2], _sz.MIN_SIGMA_PX, places=12)
    self.assertAlmostEqual(hi[2], _sz.MIN_SIGMA_PX, places=12)
    self.assertAlmostEqual(lo[1], hi[1], places=12)

  ########################################
  def test_invariant_ACROSS_THE_CLAMP(self):
    # THE case the floor exists to break: an angular sigma that the coarse
    # configuration floors (widens) and the fine one does not. The pixel
    # footprints differ, the peaks differ — the integral must not.
    sigma_ang = 0.5 * (_sz.MIN_SIGMA_PX * (COARSE_PIXEL_DEG + FINE_PIXEL_DEG))
    lo = self._leg(COARSE_PIXEL_DEG, sigma_ang)
    hi = self._leg(FINE_PIXEL_DEG, sigma_ang)
    self.assertAlmostEqual(lo[2], _sz.MIN_SIGMA_PX, places=9,
                           msg="the coarse leg must be FLOORED")
    self.assertGreater(hi[2], _sz.MIN_SIGMA_PX * 1.05,
                       "the fine leg must be PHYSICAL (above the floor)")
    self.assertAlmostEqual(hi[2], sigma_ang / FINE_PIXEL_DEG, places=9)
    self.assertAlmostEqual(lo[0], hi[0], places=12)

  ########################################
  def test_widening_costs_amplitude_by_exactly_the_square(self):
    px = _px_q(COARSE_PIXEL_DEG)
    narrow = _sz.MIN_SIGMA_PX * px
    for factor in (1.5, 2.0, 4.0):
      a0 = _sz.amplitude(self.FLUX, narrow / px)
      a1 = _sz.amplitude(self.FLUX, (narrow * factor) / px)
      self.assertAlmostEqual(a0 / a1, factor * factor, places=9)

  ########################################
  def test_flux_scales_linearly_with_luminance(self):
    px = _px_q(FINE_PIXEL_DEG)
    sig_px = _sz.floored_sigma_q(0.0, px) / px
    for lum in (6.5e-4, 1.0, 3.84):
      amp = _sz.amplitude(lum, sig_px)
      self.assertAlmostEqual(_sz.integrated_luminance(amp, sig_px), lum, places=12)


###############################################################################
# The MATERIAL — that the emitted shader is built from the same law, blends
# additively, and reads the raw per-vertex payload. Needs orkengine.core (the
# DSL's vector types); no GPU, no shader compile.
###############################################################################

def _material_text():
  sys.path.insert(0, os.path.abspath(_SCRIPTS))
  import orkengine.core                                   # noqa: F401 (import-order law)
  from ork.hypergraph.assets.materials.star_splat import StarSplat
  from ork.hypergraph.ptex3d.dsl import materialize_ptex3d_full
  from ork.hypergraph.ptex3d.fxv2_template import _dslcache_dir
  path, specs, _lobes, _caps = materialize_ptex3d_full(StarSplat, name_hint="starsplat")
  fname = os.path.basename(path)
  text = open(os.path.join(_dslcache_dir("ptex3d"), fname)).read()
  return text, {n: d for (n, _g, d) in specs}


try:
  _TEXT, _PARAMS = _material_text()
  _MATERIAL_ERR = None
except Exception as e:                                     # no orkengine -> skip
  _TEXT, _PARAMS, _MATERIAL_ERR = None, None, repr(e)


@unittest.skipIf(_TEXT is None, "orkengine unavailable: %s" % _MATERIAL_ERR)
class TestEmittedMaterial(unittest.TestCase):

  ########################################
  def test_blend_is_additive_and_depth_is_read_only(self):
    # ADDITIVE = (SrcClr*1)+(FBClr*1): order-independent, so 9096 overlapping
    # splats need no sort. Depth WRITE off (never occlude the sky), depth TEST
    # on (terrain still occludes stars), no cull.
    self.assertIn("BlendMode = ADDITIVE;", _TEXT)
    self.assertIn("DepthMask = OFF;", _TEXT)
    self.assertIn("DepthTest = LEQUALS;", _TEXT)
    self.assertIn("CullTest  = OFF;", _TEXT)

  ########################################
  def test_the_binormal_payload_reaches_the_surface_unnormalized(self):
    # the whole point of ctx.B_payload: the stock VS normalize would throw the
    # star's luminance away.
    self.assertIn("frg_obin    = binormal;", _TEXT)
    self.assertNotIn("frg_obin    = normalize(binormal);", _TEXT)
    self.assertIn(", vec3 obinr)", _TEXT)

  ########################################
  def test_the_sigma_floor_is_computed_from_screen_derivatives(self):
    # resolution/fov dependence comes from the pixel footprint, not a constant
    self.assertIn("dFdx(uv)", _TEXT)
    self.assertIn("dFdy(uv)", _TEXT)
    self.assertIn("max(SsSigQ.x,", _TEXT)          # max(physical, floor)
    self.assertIn("(SsMinSigPx.x *", _TEXT)

  ########################################
  def test_the_amplitude_carries_the_gaussian_normalization(self):
    self.assertIn("6.283185307179586", _TEXT)      # 2*pi
    self.assertIn("(SsFlux.x /", _TEXT)

  ########################################
  def test_the_profile_is_the_shared_conic_gaussian(self):
    self.assertIn('import "orkshader://conictools.i2";', _TEXT)
    self.assertIn("lib_conic", _TEXT)
    self.assertIn("conic_gaussian2d(", _TEXT)

  ########################################
  def test_the_envelope_refusal_is_emitted(self):
    self.assertIn("step(1.0,", _TEXT)
    self.assertIn("vec3(8.0, 0.0, 8.0)", _TEXT)

  ########################################
  def test_param_defaults_come_from_the_sizing_law(self):
    from ork.hypergraph.assets.materials import star_splat as _mat
    self.assertAlmostEqual(_PARAMS["SsMinSigPx"], _sz.MIN_SIGMA_PX, places=12)
    self.assertAlmostEqual(_PARAMS["SsCutSig"], _sz.CUTOFF_SIGMA, places=12)
    self.assertAlmostEqual(
      _PARAMS["SsSigQ"],
      _sz.sigma_q_from_deg(_mat.STAR_SIGMA_DEG, _sz.QUAD_HALF_ANGLE_DEG),
      places=12)

  ########################################
  def test_nothing_temporal_and_nothing_sample_rate_dependent(self):
    # VR law: no clock, no per-sample anything in the star path.
    body = _TEXT.split("SurfaceOut ptex_surface", 1)[1].split("\n}", 1)[0]
    for banned in ("Time", "gl_SampleID", "gl_SampleMask", "gl_NumSamples",
                   "sampleCount"):
      self.assertNotIn(banned, body)


###############################################################################

if __name__ == '__main__':
  _result = unittest.main(exit=False, verbosity=2).result
  _ok = _result.wasSuccessful()
  print("STARSPLAT-UNIT: %s (%d run, %d fail, %d error)"
        % ("PASS" if _ok else "FAIL", _result.testsRun,
           len(_result.failures), len(_result.errors)), flush=True)
  sys.exit(0 if _ok else 1)
