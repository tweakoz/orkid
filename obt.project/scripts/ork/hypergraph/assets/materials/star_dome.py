###############################################################################
# StarDome — the PROCEDURAL night star field, UNLIT, on the inward-facing
# celestial-sphere shell (assets/mesh/stardome.py). Paired with Scene.stars().
#
#   from ork.hypergraph.assets.materials.star_dome import StarDome
#   mtl = self.asset.Ptex3d("stars_mtl", dsl_class=StarDome)
#
# NO STAR TEXTURE EXISTS, so the field is generated: two tiers of the spherical
# FIBONACCI lattice (P.spherecells — uniform on a sphere, no UV chart, no pole
# singularity), each cell holding a star when its hash passes the tier's density
# and shading a round point of the tier's angular size. A second hash sets the
# magnitude (hash^MAG_POWER — many faint, few bright, the Pogson-ish look) and a
# third the spectral tint (cool blue-white .. warm). Everything is evaluated in
# OBJECT space, so the entity orientation carries the sidereal rotation.
#
# NO TWINKLE: the field is static in the dome's frame by design (the sidereal
# turn is the only motion). Nothing here reads the clock.
#
# TWILIGHT FADE, BY CONTRAST: a star is visible when it out-shines the sky
# BEHIND it, so the fade is a contrast test against the MEASURED sky background
# luminance (ctx.sky_luminance) and nothing else. Weber form,
#   visibility = amp / (amp + CONTRAST_K * sky_luminance),
# per star: as the sky darkens, every star's visibility rises and none of them
# ever falls, the bright tier crosses first (it has the larger amp) and the
# faint field fills in behind it. There is no "is it night" term to get wrong,
# no elevation band to be off by a degree, and nothing keyed to whichever body
# holds ublk_sun — which is what an earlier max() of two independent readings
# was, and why stars briefly went OUT between -4.5 and -7 degrees as the two
# crossed. Gated on ctx.has_sun only because the measurement rides that block:
# a pass that never bound it falls back to SUNLESS_NIGHT.
#
# HORIZON FADE: the shell is a FULL sphere (so the pole tilt can never expose an
# unstarred wedge), so its lower half is masked here by the view direction's y —
# stars below the horizon are not visible, with or without terrain in the way.
#
# MOONLIGHT / SKY-GLOW WASHOUT comes free and is not modelled separately: a
# risen moon raises the measured sky luminance, which is the same denominator
# the contrast test already divides by.
#
# NOT MODELED: the Milky Way band, named constellations, atmospheric extinction
# near the horizon.
###############################################################################

from ork.hypergraph.ptex3d import Ptex3d, P

# ---- the two size tiers ----------------------------------------------------
# cells = lattice cell count over the WHOLE sphere; density = fraction of cells
# that hold a star; size = angular radius in radians (~0.0015 rad per pixel at
# 65deg/720px, so keep sizes >= ~2px or the points alias into flicker); gain =
# peak radiance of the tier's brightest star.
BRIGHT_CELLS   = 2400.0
BRIGHT_DENSITY = 0.30      # ~720 prominent stars
BRIGHT_SIZE    = 0.0035    # ~2.3 px radius
BRIGHT_GAIN    = 1.0

FAINT_CELLS    = 14000.0
FAINT_DENSITY  = 0.50      # ~7000 background stars
FAINT_SIZE     = 0.0020    # ~1.3 px radius
FAINT_GAIN     = 0.42

MAG_POWER      = 3.0       # magnitude distribution: brightness = hash^MAG_POWER
PROFILE_POWER  = 2.0       # point profile sharpness (1 = linear falloff)

# spectral tint endpoints, mixed per star by its own hash
COOL_TINT      = (0.78, 0.86, 1.00)
WARM_TINT      = (1.00, 0.87, 0.72)

# ---- twilight fade: CONTRAST THRESHOLD against the measured sky --------------
# A star is visible when it out-shines the sky BEHIND it, so visibility is a
# threshold on the star's own luminance against the MEASURED sky background
# (ctx.sky_luminance) — nothing else. The threshold is not proportional to the
# background: a real threshold-vs-intensity law steepens as the background
# brightens, which is exactly why the whole catalog is out on a moonless night
# and not one star of it survives noon. In this form,
#
#     threshold = THRESHOLD_C * sky ** THRESHOLD_B
#     r         = star_luminance / threshold
#     visible   = r**K / (1 + r**K)          (a logistic in log-ratio)
#
# every star's visibility is MONOTONE in the background, so nothing can go
# backwards through twilight; the bright tier crosses first because it has the
# larger numerator; and the field fills in smoothly behind it. A risen moon
# raises the background, so moon washout is the same term, not a special case.
#
# THE THREE NUMBERS, anchored on the sky ladder this engine actually measures
# (test_scene_adaptation_luminance_gate: day 9.4e-2, sun -4deg 5.5e-4, moonlit
# night 2.0e-3, moonless floor 1.8e-5):
#   THRESHOLD_C/THRESHOLD_B place the limiting magnitude at the FAINTEST catalog
#     star on the moonless floor, and steepen fast enough that the brightest
#     star is ~1/500 visible under a noon sky;
#   K is the softness — the transition spans a bit over a decade of ratio.
THRESHOLD_C     = 8610.0
THRESHOLD_B     = 1.5
THRESHOLD_K     = 1.5
SUNLESS_NIGHT   = 1.0      # fallback when ublk_sun is unbound (has_sun == 0)

# ---- horizon mask (view-direction y) ---------------------------------------
HORIZON_LO      = 0.00
HORIZON_HI      = 0.05     # ~3 degrees of fade above the geometric horizon

# fragments dimmer than this are discarded (fill savings — most of a star dome
# is empty sky)
DISCARD_OPACITY = 0.004

# The lattice pole is a mild singularity in the Fibonacci construction, and the
# dome's local +Y is the NORTH CELESTIAL POLE — the one patch of sky an observer
# stares at. This fixed axis permutation moves the lattice poles onto the
# celestial equator instead. STRUCTURAL (no user knob), so it bakes.
def _lattice_dir(d):
  return P.vec3(d.y, d.z, d.x)


def _hash_from(h, salt):
  """A decorrelated companion hash from a cell hash in [0,1)."""
  return P.fract(P.sin(h * 78.233 + salt) * 43758.5453)


class StarDome(Ptex3d):

  def __init__(self, ctx, *,
               bright_cells   = BRIGHT_CELLS,
               bright_density = BRIGHT_DENSITY,
               bright_size    = BRIGHT_SIZE,
               bright_gain    = BRIGHT_GAIN,
               faint_cells    = FAINT_CELLS,
               faint_density  = FAINT_DENSITY,
               faint_size     = FAINT_SIZE,
               faint_gain     = FAINT_GAIN,
               mag_power      = MAG_POWER,
               profile_power  = PROFILE_POWER,
               cool_tint      = COOL_TINT,
               warm_tint      = WARM_TINT,
               threshold_c     = THRESHOLD_C,
               threshold_b     = THRESHOLD_B,
               threshold_k     = THRESHOLD_K,
               sunless_night   = SUNLESS_NIGHT,
               horizon_lo      = HORIZON_LO,
               horizon_hi      = HORIZON_HI):
    # every look value is a bindable plug (A8) — vec2 params are NOT usable (the
    # reflection varmap codec has no fvec2 encoder: the uniform silently never
    # binds, see scn_cloudgauge's CgWindVec note), so pairs stay two floats.
    p_a_n    = ctx.param("SdCellsA",  bright_cells)
    p_a_d    = ctx.param("SdDensA",   bright_density)
    p_a_s    = ctx.param("SdSizeA",   bright_size)
    p_a_g    = ctx.param("SdGainA",   bright_gain)
    p_b_n    = ctx.param("SdCellsB",  faint_cells)
    p_b_d    = ctx.param("SdDensB",   faint_density)
    p_b_s    = ctx.param("SdSizeB",   faint_size)
    p_b_g    = ctx.param("SdGainB",   faint_gain)
    p_magp   = ctx.param("SdMagPow",  mag_power)
    p_prof   = ctx.param("SdProfPow", profile_power)
    p_cool   = ctx.param("SdCoolCol", cool_tint)
    p_warm   = ctx.param("SdWarmCol", warm_tint)
    p_thc    = ctx.param("SdThrC", threshold_c)
    p_thb    = ctx.param("SdThrB", threshold_b)
    p_thk    = ctx.param("SdThrK", threshold_k)
    p_sunless = ctx.param("SdNoSun",  sunless_night)
    p_hz_lo  = ctx.param("SdHorizLo", horizon_lo)
    p_hz_hi  = ctx.param("SdHorizHi", horizon_hi)

    lat = _lattice_dir(P.normalize(ctx.P_object))

    def tier(cells, density, size, gain):
      cell  = P.spherecells(lat, cells)
      h_sel = cell.id
      h_mag = _hash_from(h_sel, 11.13)
      h_col = _hash_from(h_sel, 27.71)
      # star present? density is the fraction of cells above the cut
      live  = P.step(1.0 - density, h_sel)
      # round point: 1 at the cell centre, 0 at `size` radians out
      prof  = P.pow(1.0 - P.smoothstep(0.0, size, cell.f1), p_prof)
      amp   = live * prof * P.pow(h_mag, p_magp) * gain
      return amp, P.mix(p_cool, p_warm, h_col)

    amp_a, col_a = tier(p_a_n, p_a_d, p_a_s, p_a_g)
    amp_b, col_b = tier(p_b_n, p_b_d, p_b_s, p_b_g)
    amp = amp_a + amp_b
    # amplitude-weighted tint (the tiers never overlap in practice; the divide
    # keeps the colour normalized where they do)
    col = (col_a * amp_a + col_b * amp_b) / P.max(amp, 1e-4)

    # CONTRAST against the measured sky background (see the header). Per star,
    # so the bright tier emerges first and the field fills in behind it; the
    # denominator can never be zero, so a pre-publish frame (sky_luminance 0)
    # reads as full visibility rather than as a blank dome.
    thresh  = p_thc * P.pow(P.max(ctx.sky_luminance, 1e-9), p_thb)
    r       = P.pow(amp / P.max(thresh, 1e-12), p_thk)
    night   = r / (1.0 + r)
    night   = P.mix(p_sunless, night, P.step(0.5, ctx.has_sun))

    # below-horizon half of the shell (see header)
    view = P.normalize(ctx.P - ctx.eye)
    horiz = P.smoothstep(p_hz_lo, p_hz_hi, view.y)

    self.unlit(col, P.clamp(amp * night * horiz, 0.0, 1.0),
               blend       = "alpha",
               depth_test  = "leq",
               depth_write = False,
               cull        = "off")
    # empty sky is most of the dome — drop those fragments outright
    self._surf_body_append = (
      "if (s.opacity < %s) discard;" % repr(float(DISCARD_OPACITY)))


__all__ = ["StarDome"]
