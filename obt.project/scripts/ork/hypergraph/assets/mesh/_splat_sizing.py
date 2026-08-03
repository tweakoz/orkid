###############################################################################
# _splat_sizing — the ONE sizing law shared by the star-splat BAKE (the envelope
# quad in star_catalog.py) and the star-splat MATERIAL (the runtime gaussian in
# materials/star_splat.py), plus the closed-form energy math both are held to.
#
# STDLIB ONLY (no numpy, no orkengine) so the unit tests can load it by path with
# plain CPython — same rule as _bsc5.py.
#
# ---------------------------------------------------------------------------
# THE TWO SIZES, AND WHY THEY FIGHT
#
#   * The ENVELOPE is baked: a fixed ANGULAR half-size per star quad, chosen once
#     at bake time, which the runtime cannot grow.
#   * The GAUSSIAN is computed at runtime: its sigma has a FLOOR in PIXELS (below
#     ~1px a point source aliases into a crawling sparkle), and a pixel subtends
#     a different angle in every (resolution, fov, headset render-scale)
#     configuration. The COARSEST configuration therefore demands the LARGEST
#     angular footprint.
#
# If the demanded footprint exceeds the envelope the gaussian is cut by the quad
# rectangle: square-cornered blobs AND silently lost energy. So the envelope is
# derived FROM the coarsest declared configuration, not from a nominal view:
#
#     envelope_half_angle >= CUTOFF_SIGMA * MIN_SIGMA_PX * WORST_CASE_PIXEL_DEG
#
# and the shader re-checks the same inequality per fragment against the LIVE
# pixel footprint, refusing loudly (not clamping) if a configuration coarser than
# the declared worst case ever runs. See star_splat.py's refusal block.
#
# ---------------------------------------------------------------------------
# THE ENERGY LAW
#
# A star is a point source: its flux is fixed, so the SUM of the pixels it lights
# must not depend on how many pixels that is. With the gaussian written in pixel
# units, sum(pixels) = amplitude * 2*pi*sigma_px^2, so
#
#     amplitude = flux / (2*pi*sigma_px^2)
#
# keeps the sum equal to `flux` at every resolution AND across the sigma floor's
# widening (which raises sigma_px and lowers amplitude in exact proportion).
# `integrated_luminance` below is that closed form; the material's DSL expression
# is the shader spelling of the same three lines, and the render gate
# (test_star_splat_energy_gate.py) checks the shader against it on real pixels.
###############################################################################

import math

# ---- the sigma floor -------------------------------------------------------
# Minimum on-screen gaussian sigma, in PIXELS. Below ~0.5px a point source is at
# the sampling limit and scintillates as it drifts subpixel; 0.9 puts the worst
# subpixel peak swing at exp(-0.5/(2*0.81)) = 0.73 of the best, which reads as a
# steady dot. Larger = softer, fatter stars and a proportionally larger envelope.
MIN_SIGMA_PX = 0.9

# Where the gaussian is considered finished, in sigmas. The quad rectangle is the
# actual truncation, so this is what the envelope must FIT: 3 sigma leaves
# 1 - exp(-4.5) = 98.9% of the energy inside a circle of that radius (the corners
# of the rectangle reach sqrt(2) further, so the real capture is higher).
CUTOFF_SIGMA = 3.0

# The COARSEST pixel any supported configuration may subtend, in DEGREES — i.e.
# the MINIMUM SUPPORTED CONFIGURATION, stated so the next person reads the
# assumption instead of re-deriving it:
#
#     360 px of vertical render height, at a 90 degree vertical fov.
#     90/360 = 0.25 deg/px.
#
# WHY THOSE TWO NUMBERS:
#   * 360 px is the smallest height actually deployed. The engine enforces NO
#     minimum — ork.scene.viewer.py takes any -H — and 640x360 is a normal small
#     window (it is also the first entry in test_terrain_meshshader_ab's
#     REPRO_RESOLUTIONS). Below 360 is a deliberate micro-window, not a use.
#   * 90 deg is the widest fov the engine ships as a DEFAULT: the VR device
#     (SceneGraphSystem.cpp:466, and openxr_device.cpp's degenerate-fov fallback
#     lands on the same 90). Desktop scenes run 35..70 (the walker default is
#     65), and there is no fov clamp anywhere in the camera DSL, so the widest
#     shipped default is the honest ceiling to quote.
#
# The two worst cases are deliberately MULTIPLIED even though they do not
# co-occur — a 90 deg fov is a headset, and headset eye buffers are >1000 px, so
# nothing real is as coarse as 0.25. That product IS the margin: the observed
# field failure (640x360 at the 65 deg default = 0.181 deg/px) sits 38% inside
# it. A configuration coarser than this is still not silently mis-drawn — the
# shader refuses it, loudly, and that refusal is what caught this constant.
#
# HISTORY: this was 0.16 (640x480 at 77 deg), derived from a nominal desktop
# window rather than from a stated minimum. A 640x360 window rendered the entire
# sky as the magenta refusal.
WORST_CASE_PIXEL_DEG = 0.25


def required_half_angle_deg():
  """The SMALLEST envelope half-angle that fits the floored gaussian in the
  coarsest declared configuration. The baked quad must be at least this."""
  return CUTOFF_SIGMA * MIN_SIGMA_PX * WORST_CASE_PIXEL_DEG


# Baked envelope half-angle, in degrees — `required_half_angle_deg()` (0.675)
# rounded up with a little margin. Overdraw scales with its SQUARE, so it is not
# padded generously: at the 65deg/720px reference view this is a 7.8 px
# half-size, ~240 fragments per star (~2.2 M for the 9096-star field). That is
# 2.4x the fill of the previous 0.45 envelope, and the price of covering the
# stated minimum configuration above; the fragments outside the profile are
# discarded before the blend, so the cost is shading, not bandwidth.
QUAD_HALF_ANGLE_DEG = 0.70


###############################################################################
# The runtime math, in QUAD units: q is the material's interpolated quad
# coordinate, +-1 at the envelope edge, so 1 q == tan(half_angle) on the tangent
# plane == half_angle of arc (to 1e-5 at these angles).
###############################################################################

def sigma_q_from_deg(sigma_ang_deg, half_angle_deg):
  """A star's PHYSICAL angular sigma (degrees) expressed in quad units. Folded on
  the CPU so the shader needs no trig — the material binds the result as a plug."""
  return math.radians(sigma_ang_deg) / math.tan(math.radians(half_angle_deg))


def floored_sigma_q(sigma_phys_q, px_q, min_sigma_px=MIN_SIGMA_PX):
  """THE FLOOR. `px_q` is the live projected pixel footprint in quad units
  (max(|dFdx(q)|,|dFdy(q)|) in the shader). A star whose physical sigma projects
  below `min_sigma_px` pixels is WIDENED to exactly that many pixels."""
  return max(sigma_phys_q, min_sigma_px * px_q)


def amplitude(flux, sigma_px):
  """Peak radiance that makes the footprint integrate to `flux`."""
  return flux / (2.0 * math.pi * sigma_px * sigma_px)


def integrated_luminance(amp, sigma_px):
  """Closed-form footprint sum of a 2D gaussian: amp * 2*pi*sigma_x*sigma_y (the
  splat is isotropic, so sigma_x == sigma_y == sigma_px)."""
  return amp * 2.0 * math.pi * sigma_px * sigma_px


def fits_envelope(sigma_q, cutoff_sigma=CUTOFF_SIGMA):
  """The runtime envelope check, in the same form the shader evaluates: the
  cutoff radius must land inside the quad (|q| <= 1)."""
  return (cutoff_sigma * sigma_q) <= 1.0


__all__ = ["MIN_SIGMA_PX", "CUTOFF_SIGMA", "WORST_CASE_PIXEL_DEG",
           "QUAD_HALF_ANGLE_DEG", "required_half_angle_deg",
           "sigma_q_from_deg", "floored_sigma_q", "amplitude",
           "integrated_luminance", "fits_envelope"]
