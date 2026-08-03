#!/usr/bin/env python3
###############################################################################
# moon_phase.py — unit tests for the procedural sky's MOON PHASE geometry
# (the per-pixel lit-limb test inside skyEvalMoonDisc, shaders/fxv2/skytools.i2).
#
# Runs STANDALONE with plain CPython + numpy — no engine, no GPU, no staging:
#     python3 obt.project/unittests/moon_phase.py
#     python3 obt.project/unittests/moon_phase.py --ascii   # eyeball the shapes
#
# WHAT THIS IS: a re-implementation of the shader's terminator test, asserted
# against geometry that is known independently of it. It CANNOT catch a typo in
# the GLSL (only a rendered frame can); what it locks down is the MATH the GLSL
# is written from — every property below is what makes the disc read as a moon:
#
#   * the ellipse form the shader uses (u > k*sqrt(1-s^2), measured in disc
#     radii) is the SAME predicate as the surface-normal form
#     (dot(N,sun) > 0, N = f*t - sqrt(1-f^2)*moon), which is the reason the
#     cheap form is allowed to stand in for the physical one;
#   * the lit AREA fraction is exactly (1-k)/2 — analytic, so a wrong axis or a
#     wrong sign shows up as a wrong phase rather than as a plausible one;
#   * the cusps sit on the diameter PERPENDICULAR to the bright-limb axis and
#     the lune hugs the sunward limb, i.e. the horns point AWAY from the sun;
#   * full/new degenerate the bright-limb axis, and the sign of k alone must
#     still give all-lit / all-dark (the shader takes no branch there);
#   * nothing depends on the roll of the bright limb around the view axis.
#
# CONVENTION (same as the C++/GLSL): M = unit direction observer->moon,
# S = unit direction observer->sun, k = dot(M,S). k = -1 full, 0 quarter,
# +1 new. Elongation is the M-to-S angle, so k = cos(elongation).
###############################################################################

import math
import sys
import unittest

import numpy as np

# Disc radius used by the tests. The real disc is 0.265 degrees; the phase math
# is scale-free in the fractional radius f, and a wider disc simply gives the
# sampling grid more pixels to integrate over.
RADIUS_DEG = 6.0

# Sampling grid across the disc bounding box. The analytic-area assertions are
# only as tight as this: the terminator cuts pixels, so the error is O(1/N).
GRID_N = 201
AREA_TOL = 0.015

# Terminator softness, as a fraction of the disc radius (SkyMoonDisc.w).
SOFT = 0.1


###############################################################################
# the predicate under test — a line-for-line mirror of skyEvalMoonDisc's phase
# block, vectorized over view directions.
###############################################################################

def phase_terms(view_dirs, moon_dir, sun_dir, radius):
  """Returns (inside_disc, f, term, ndl) for each view direction.

  term : the shader's ellipse form  u - k*sqrt(1-s^2)   (lit where > 0)
  ndl  : the surface-normal form    dot(N, sun_dir)     (lit where > 0)"""
  moon_dir = moon_dir / np.linalg.norm(moon_dir)
  sun_dir = sun_dir / np.linalg.norm(sun_dir)
  ct = view_dirs @ moon_dir
  inside = ct >= math.cos(radius)
  off = view_dirs - np.outer(ct, moon_dir)
  off_len = np.linalg.norm(off, axis=1)
  safe = np.maximum(off_len, 1.0e-9)
  tang = off / safe[:, None]
  f = np.clip(off_len / max(math.sin(radius), 1.0e-6), 0.0, 1.0)
  k = float(np.clip(np.dot(moon_dir, sun_dir), -1.0, 1.0))
  limb = sun_dir - moon_dir * k
  limb_len = float(np.linalg.norm(limb))
  # degenerate at full/new: the axis is undefined AND irrelevant, u drops out
  limb_axis = (limb / limb_len) if limb_len > 1.0e-4 else np.zeros(3)
  u = f * (tang @ limb_axis)
  s2 = np.maximum(f * f - u * u, 0.0)
  term = u - k * np.sqrt(np.maximum(1.0 - s2, 0.0))
  normal = tang * f[:, None] - np.outer(np.sqrt(np.maximum(1.0 - f * f, 0.0)), moon_dir)
  ndl = normal @ sun_dir
  return inside, f, term, ndl


def smoothstep(e0, e1, x):
  t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
  return t * t * (3.0 - 2.0 * t)


###############################################################################
# geometry helpers
###############################################################################

def moon_sun_dirs(elongation_deg, roll_deg):
  """M along +Z; S at `elongation_deg` from M, rolled by `roll_deg` about M so
  the bright limb can point anywhere on the disc."""
  m = np.array([0.0, 0.0, 1.0])
  e = math.radians(elongation_deg)
  a = math.radians(roll_deg)
  s = np.array([math.sin(e) * math.cos(a), math.sin(e) * math.sin(a), math.cos(e)])
  return m, s / np.linalg.norm(s)


def disc_samples(radius, n=GRID_N, extent=1.0):
  """View directions over the disc's bounding box (M along +Z), plus the disc-plane
  coordinates each one came from."""
  sr = math.sin(radius) * extent
  axis = np.linspace(-sr, sr, n)
  xx, yy = np.meshgrid(axis, axis)
  x = xx.ravel()
  y = yy.ravel()
  z = np.sqrt(np.maximum(1.0 - x * x - y * y, 0.0))
  v = np.stack([x, y, z], axis=1)
  return v / np.linalg.norm(v, axis=1)[:, None], x, y


def lit_stats(elongation_deg, roll_deg, radius_deg=RADIUS_DEG):
  """Hard-edge lit fraction of the visible disc + the lit centroid, in disc-plane
  coordinates normalized so the limb axis is +x."""
  radius = math.radians(radius_deg)
  m, s = moon_sun_dirs(elongation_deg, roll_deg)
  v, px, py = disc_samples(radius)
  inside, f, term, _ = phase_terms(v, m, s, radius)
  lit = inside & (term > 0.0)
  n_in = int(inside.sum())
  frac = float(lit.sum()) / max(n_in, 1)
  # centroid, projected onto the bright-limb axis as seen in the disc plane
  a = math.radians(roll_deg)
  axis2d = np.array([math.cos(a), math.sin(a)])
  if lit.sum():
    cx = float(px[lit].mean())
    cy = float(py[lit].mean())
    centroid_along_axis = float(np.dot([cx, cy], axis2d)) / math.sin(radius)
  else:
    centroid_along_axis = 0.0
  return frac, centroid_along_axis, n_in


###############################################################################

PHASES = [
    # label,      elongation, expected lit fraction (1-k)/2
    ("new",         0.0,   0.0),
    ("thin",       10.0,   (1.0 - math.cos(math.radians(10.0))) / 2.0),
    ("crescent",   45.0,   (1.0 - math.cos(math.radians(45.0))) / 2.0),
    ("quarter",    90.0,   0.5),
    ("gibbous",   135.0,   (1.0 - math.cos(math.radians(135.0))) / 2.0),
    ("full",      180.0,   1.0),
]

ROLLS = [0.0, 37.0, 90.0, 215.0]


class MoonPhaseGeometry(unittest.TestCase):

  ########################################
  def test_ellipse_form_matches_surface_normal(self):
    # The whole reason the shader may use the cheap screen-space form. Samples
    # within a hair of the terminator are excluded: both forms cross zero there
    # and only their SIGN AGREEMENT away from the crossing is the claim.
    radius = math.radians(RADIUS_DEG)
    for elong in (0.0, 10.0, 45.0, 90.0, 135.0, 170.0, 180.0):
      for roll in ROLLS:
        m, s = moon_sun_dirs(elong, roll)
        v, _, _ = disc_samples(radius)
        inside, f, term, ndl = phase_terms(v, m, s, radius)
        both = inside & (np.abs(term) > 1.0e-3) & (np.abs(ndl) > 1.0e-3)
        disagree = both & ((term > 0.0) != (ndl > 0.0))
        self.assertEqual(
            int(disagree.sum()), 0,
            "ellipse and surface-normal forms disagree at elongation %.1f roll %.1f"
            % (elong, roll))

  ########################################
  def test_lit_fraction_is_analytic(self):
    # area of {u > k*sqrt(1-s^2)} over the unit disc is exactly (1-k)*pi/2, so
    # the lit fraction is (1-k)/2 — a wrong sign or a wrong axis cannot hide.
    for label, elong, expected in PHASES:
      for roll in ROLLS:
        frac, _, n_in = lit_stats(elong, roll)
        self.assertGreater(n_in, 1000)
        self.assertAlmostEqual(
            frac, expected, delta=AREA_TOL,
            msg="%s (elongation %.1f roll %.1f): lit fraction %.4f, expected %.4f"
                % (label, elong, roll, frac, expected))

  ########################################
  def test_full_moon_is_whole_disc_and_new_moon_is_dark(self):
    # BOTH degenerate the bright-limb axis (limb_len ~ 0), where the shader takes
    # no branch — the sign of k alone has to carry it. The full moon's tolerance
    # is not slop: the exact rim is grazing illumination (term -> 0 there), so a
    # hairline of limb samples is legitimately unlit at any phase.
    for roll in ROLLS:
      self.assertGreater(lit_stats(180.0, roll)[0], 0.999)
      self.assertAlmostEqual(lit_stats(0.0, roll)[0], 0.0, delta=1.0e-6)

  ########################################
  def test_quarter_is_the_sunward_half(self):
    for roll in ROLLS:
      frac, centroid, _ = lit_stats(90.0, roll)
      self.assertAlmostEqual(frac, 0.5, delta=AREA_TOL)
      # centroid of a half-disc sits at 4/(3pi) ~ 0.424 radii along the axis
      self.assertAlmostEqual(centroid, 4.0 / (3.0 * math.pi), delta=0.05)

  ########################################
  def test_lune_hugs_the_sunward_limb(self):
    # crescents (k > 0) are pulled toward the sun, gibbous phases (k < 0) away
    # from it: the sign of the lit centroid along the bright-limb axis IS the
    # "which side is lit" claim.
    for elong in (10.0, 45.0, 80.0):
      for roll in ROLLS:
        self.assertGreater(lit_stats(elong, roll)[1], 0.05)
    for elong in (100.0, 135.0, 170.0):
      for roll in ROLLS:
        self.assertLess(abs(lit_stats(elong, roll)[1]), 0.42)

  ########################################
  def test_sunward_limb_lit_antisunward_limb_dark(self):
    radius = math.radians(RADIUS_DEG)
    for elong in (10.0, 45.0, 90.0, 135.0, 170.0):
      for roll in ROLLS:
        m, s = moon_sun_dirs(elong, roll)
        a = math.radians(roll)
        # points just inside the limb, on and against the bright-limb axis. It has
        # to be JUST inside: a thin crescent's lit sliver only exists beyond
        # f = k (0.985 at 10 degrees elongation), which is the crescent BEING thin.
        near = 0.9995 * math.sin(radius)
        p_sun = np.array([near * math.cos(a), near * math.sin(a), 0.0])
        p_anti = -p_sun
        pts = []
        for p in (p_sun, p_anti):
          w = np.array([p[0], p[1], math.sqrt(max(1.0 - p[0]**2 - p[1]**2, 0.0))])
          pts.append(w / np.linalg.norm(w))
        inside, _, term, _ = phase_terms(np.array(pts), m, s, radius)
        self.assertTrue(bool(inside[0]) and bool(inside[1]))
        self.assertGreater(term[0], 0.0, "sunward limb dark at elongation %.1f" % elong)
        self.assertLess(term[1], 0.0, "anti-sunward limb lit at elongation %.1f" % elong)

  ########################################
  def test_cusps_sit_on_the_perpendicular_diameter(self):
    # the terminator ellipse u = k*sqrt(1-s^2) passes through (u=0, s=+-1) at
    # EVERY phase — that is where a crescent's horns are, and it is why the horns
    # point away from the sun rather than along some tuned direction.
    radius = math.radians(RADIUS_DEG)
    for elong in (10.0, 45.0, 90.0, 135.0, 170.0):
      for roll in ROLLS:
        m, s = moon_sun_dirs(elong, roll)
        a = math.radians(roll) + math.pi * 0.5   # perpendicular to the limb axis
        r = math.sin(radius)
        pts = []
        for sign in (1.0, -1.0):
          p = np.array([sign * r * math.cos(a), sign * r * math.sin(a),
                        math.sqrt(max(1.0 - r * r, 0.0))])
          pts.append(p / np.linalg.norm(p))
        _, _, term, _ = phase_terms(np.array(pts), m, s, radius)
        self.assertAlmostEqual(float(term[0]), 0.0, delta=2.0e-3)
        self.assertAlmostEqual(float(term[1]), 0.0, delta=2.0e-3)

  ########################################
  def test_phase_is_monotone_in_elongation(self):
    previous = -1.0
    for elong in range(0, 181, 5):
      frac = lit_stats(float(elong), 0.0)[0]
      self.assertGreaterEqual(frac + 1.0e-6, previous)
      previous = frac

  ########################################
  def test_roll_invariance(self):
    # the phase depends on the sun/moon ANGLE, never on how the disc is rolled
    for label, elong, _ in PHASES:
      fracs = [lit_stats(elong, roll)[0] for roll in ROLLS]
      self.assertLess(max(fracs) - min(fracs), AREA_TOL,
                      "%s: lit fraction varies with the bright-limb roll" % label)

  ########################################
  def test_soft_terminator_is_monotone_and_bounded(self):
    # the rendered lit factor: smoothstep across the terminator, in disc radii.
    radius = math.radians(RADIUS_DEG)
    m, s = moon_sun_dirs(90.0, 0.0)
    r = math.sin(radius)
    xs = np.linspace(-0.95 * r, 0.95 * r, 64)
    pts = np.stack([xs, np.zeros_like(xs), np.sqrt(np.maximum(1.0 - xs * xs, 0.0))], axis=1)
    pts = pts / np.linalg.norm(pts, axis=1)[:, None]
    _, _, term, _ = phase_terms(pts, m, s, radius)
    lit = smoothstep(-SOFT, SOFT, term)
    self.assertTrue(bool(np.all(np.diff(lit) >= -1.0e-9)))
    self.assertAlmostEqual(float(lit[0]), 0.0, delta=1.0e-6)
    self.assertAlmostEqual(float(lit[-1]), 1.0, delta=1.0e-6)


###############################################################################
# eyeball aid: the five named phases as ascii, bright-limb axis toward +x
###############################################################################

def dump_ascii(n=44):
  radius = math.radians(RADIUS_DEG)
  for label, elong, expected in PHASES:
    m, s = moon_sun_dirs(elong, 0.0)
    v, px, py = disc_samples(radius, n=n, extent=1.15)
    inside, _, term, _ = phase_terms(v, m, s, radius)
    lit = smoothstep(-SOFT, SOFT, term)
    print("=== %-9s elongation %5.1f  lit fraction %.3f (sun toward +x)"
          % (label, elong, expected))
    for row in range(n):
      line = []
      for col in range(n):
        i = row * n + col
        line.append(' .:-=+*#@'[min(8, int(lit[i] * 8.999))] if inside[i] else ' ')
      print('  ' + ''.join(line))


###############################################################################

if __name__ == '__main__':
  if "--ascii" in sys.argv:
    dump_ascii()
    sys.exit(0)
  _result = unittest.main(exit=False, verbosity=2).result
  _ok = _result.wasSuccessful()
  print("VERDICT: moon_phase %s (%d tests)"
        % ("PASS" if _ok else "FAIL", _result.testsRun))
  sys.exit(0 if _ok else 1)
