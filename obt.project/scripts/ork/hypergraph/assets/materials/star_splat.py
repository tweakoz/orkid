###############################################################################
# StarSplat — the CATALOG night sky: one analytic 2D gaussian per Bright Star
# Catalogue star, shaded on the splat quads baked by assets/mesh/star_catalog.py.
# Replaces the procedural Fibonacci-lattice field (materials/star_dome.py) on the
# star dome; paired with Scene.stars().
#
#   from ork.hypergraph.assets.materials.star_splat import StarSplat
#   mtl = self.asset.Ptex3d("stars_mtl", dsl_class=StarSplat)
#
# WHAT THE MESH HANDS OVER, per vertex (all four verts of a star carry the same
# values, so every interpolation below is exact):
#   ctx.uv        — the quad coordinate q, +-1 at the envelope edge. THE gaussian
#                   argument; nothing rebuilds a tangent basis in the shader.
#   ctx.B_payload — LINEAR RADIANCE, peak-normalized blackbody tint times the
#                   exact Pogson relative luminance. NOT a tangent: this is the
#                   ptex3d DSL's raw per-vertex float3 payload atom (the stock VS
#                   would normalize the binormal and throw the luminance away —
#                   four decades of it, which no byte vertex color can hold).
#                   Because the tint peaks at 1, luminance == max(r,g,b) exactly.
#   ctx.N_object  — the star's unit direction in dome object space.
#
# SIZE IS COMPUTED, NEVER TUNED. A star is a point source; what sets its
# on-screen size is the PIXEL, so the sigma floor is derived per fragment from
# the live projected pixel footprint (screen derivatives of q) and therefore
# tracks resolution, fov and headset render scale with no constant to re-tune per
# machine. A star whose physical angular sigma projects below the floor is
# WIDENED to it.
#
# ENERGY IS CONSERVED ACROSS THAT WIDENING. Amplitude is flux / (2*pi*sigma_px^2),
# so the footprint's pixel SUM stays equal to the star's flux however wide the
# floor pushes it — see _splat_sizing.py for the closed form, unittests/
# star_splat.py for the math and llgfx/test_star_splat_energy_gate.py for the
# rendered proof.
#
# THE ENVELOPE REFUSAL. The quad is a BAKED angular size; the floor is a runtime
# pixel size. In a configuration coarser than the declared worst case
# (_splat_sizing.WORST_CASE_PIXEL_DEG) the floored gaussian would not fit inside
# the quad and would be cut by its rectangle — square-cornered blobs and silently
# missing energy. That case does not clamp quietly: it paints REFUSAL_RADIANCE
# over the whole quad, bypassing the night and horizon fades so it cannot hide.
#
# BLEND IS ADDITIVE, and that is a correctness claim, not a look: starlight ADDS,
# and additive blending is order-independent, so 9096 overlapping splats need no
# depth sort. Nothing here reads depth-order, sample count or the clock.
#
# NO TWINKLE, NO TEMPORAL ANYTHING: the field is static in the dome's frame (the
# entity's sidereal turn is the only motion), and the profile is analytic per
# fragment, so it is identical at every msaa level.
#
# TWILIGHT + HORIZON fades are CARRIED VERBATIM from star_dome.py, semantics
# included — the max(elevation, intensity) reading is a known defect owned by the
# exposure slice, deliberately not touched here. The one mechanical change is
# where they multiply: under alpha they scaled opacity, under additive they scale
# the RADIANCE, which is the same "fade to nothing" with no sort dependency.
#
# NOT MODELED: the Milky Way band, atmospheric extinction near the horizon,
# moonlight / sky-glow washout (exposure slice), cloud occlusion — stars will
# later consume the shared cloud-transmittance term, and the place it multiplies
# is marked below. Nothing here samples clouds.
###############################################################################

import math

from ork.hypergraph.assets.mesh import _splat_sizing as _sz
from ork.hypergraph.ptex3d import Ptex3d, P

# ---- brightness ------------------------------------------------------------
# Radiance a star of unit Pogson luminance deposits in total (its pixel SUM, not
# its peak). Sirius (L = 3.84) then peaks near 0.75 at the sigma floor. The
# absolute level is the exposure slice's to own.
FLUX_GAIN = 1.0

# A star's PHYSICAL angular sigma, in degrees. Real stars are unresolved point
# sources; ~2 arcsec is the atmospheric seeing disc, which projects far below the
# pixel floor in every configuration — so the floor is what is normally in force,
# and this knob is here for the mechanism (and for anyone who wants bloomier
# stars) rather than for the look.
STAR_SIGMA_DEG = 5.6e-4

# Faintest LINEAR LUMINANCE drawn; 0 = the whole catalog (V=7.96 is 6.5e-4).
# Raising it is the limiting-magnitude knob (and how a test isolates one star).
LUM_CUT = 0.0

# What the envelope refusal paints. Deliberately out of gamut for a night sky and
# far above any real star's peak, so it survives any exposure the frame applies.
REFUSAL_RADIANCE = (8.0, 0.0, 8.0)

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

# ---- horizon mask (view-direction y) — verbatim from star_dome.py ----------
HORIZON_LO      = 0.00
HORIZON_HI      = 0.05     # ~3 degrees of fade above the geometric horizon

# Radiance below which a fragment is dropped outright. Most of a star dome is
# empty sky and additive blending still pays for it. Not a look value (it is four
# decades under the faintest star's peak), so it bakes into the body append.
DISCARD_RADIANCE = 1.0e-6

# Guard against a degenerate screen derivative (a fully off-screen or zero-area
# quad) turning the floor into a divide by zero. Structural, so it bakes.
_MIN_PX_Q = 1.0e-6

_TWO_PI = 2.0 * math.pi


class StarSplat(Ptex3d):

  def __init__(self, ctx, *,
               flux_gain           = FLUX_GAIN,
               star_sigma_deg      = STAR_SIGMA_DEG,
               quad_half_angle_deg = _sz.QUAD_HALF_ANGLE_DEG,
               min_sigma_px        = _sz.MIN_SIGMA_PX,
               cutoff_sigma        = _sz.CUTOFF_SIGMA,
               lum_cut             = LUM_CUT,
               threshold_c         = THRESHOLD_C,
               threshold_b         = THRESHOLD_B,
               threshold_k         = THRESHOLD_K,
               sunless_night       = SUNLESS_NIGHT,
               horizon_lo          = HORIZON_LO,
               horizon_hi          = HORIZON_HI):
    # Every look value is a bindable plug (A8). vec2 params are NOT usable (the
    # reflection varmap codec has no fvec2 encoder — the uniform silently never
    # binds), so pairs stay two floats.
    p_flux   = ctx.param("SsFlux",    flux_gain)
    # the physical sigma is bound in QUAD units: the shader's coordinate is q, and
    # folding the degrees->quad conversion here keeps trig out of the fragment.
    # `quad_half_angle_deg` MUST match the baked envelope — both default from
    # _splat_sizing, which is why Scene.stars() overrides neither.
    p_sigq   = ctx.param("SsSigQ",
                         _sz.sigma_q_from_deg(star_sigma_deg, quad_half_angle_deg))
    p_minsig = ctx.param("SsMinSigPx", min_sigma_px)
    p_cutsig = ctx.param("SsCutSig",   cutoff_sigma)
    p_lumcut = ctx.param("SsLumCut",   lum_cut)
    p_thc     = ctx.param("SsThrC", threshold_c)
    p_thb     = ctx.param("SsThrB", threshold_b)
    p_thk     = ctx.param("SsThrK", threshold_k)
    p_sunless = ctx.param("SsNoSun",   sunless_night)
    p_hz_lo   = ctx.param("SsHorizLo", horizon_lo)
    p_hz_hi   = ctx.param("SsHorizHi", horizon_hi)

    # ---- the per-star payload ----
    rad = ctx.B_payload                                   # linear radiance
    lum = P.max(P.max(rad.x, rad.y), rad.z)               # == luminance, exactly
    q   = ctx.uv                                          # quad coord, +-1 at the edge

    # ---- the RUNTIME pixel footprint, in quad units ----
    # The Jacobian of q over the screen. The splat quad always faces the dome
    # centre, so its projection is isotropic to well within a pixel; the LONGER
    # axis is taken, which can only widen (never under-floor) the gaussian.
    px_q = P.max(P.max(P.length(P.dFdx(q)), P.length(P.dFdy(q))), _MIN_PX_Q)

    # ---- THE SIGMA FLOOR ----
    # sigma_q = max(physical, min_sigma_px pixels). Everything downstream reads
    # the floored value, so a widened star is a wider star, not a brighter one.
    sigma_q  = P.max(p_sigq, p_minsig * px_q)
    sigma_px = sigma_q / px_q

    # ---- ENERGY CONSERVATION ----
    # sum over pixels of amp*exp(-r^2/2sigma^2) == amp * 2*pi*sigma_px^2, so this
    # amplitude makes the footprint integrate to `flux` at any resolution and on
    # either side of the floor's clamp. _splat_sizing.amplitude() is the same line.
    amp = p_flux / (_TWO_PI * sigma_px * sigma_px)

    # the profile itself, from the shader corpus' generic conic library:
    # conic_gaussian2d(d, k) = exp2(-0.7213475205*k*dot(d,d)), and k = 1 is
    # exactly exp(-|d|^2/2) — the unit gaussian in sigma-normalized offsets.
    g = P.func("conic_gaussian2d({0}, {1})", [q / sigma_q, 1.0], rtype="float",
               inherits=("lib_conic",), imports=("orkshader://conictools.i2",))

    keep = P.step(p_lumcut, lum)                          # limiting magnitude

    # VISIBILITY BY CONTRAST against the measured sky background. A star is
    # visible when it out-shines the sky behind it, so this is a Weber ratio of
    # the star's OWN catalog luminance to the measured background — per star, so
    # the field fills in from the brightest down as the sky darkens, and
    # monotone in the background, so nothing can go backwards through twilight.
    # It replaces a max() of an elevation band and an intensity reading that
    # crossed each other between -4.5 and -7 degrees (stars briefly went OUT as
    # the sun set), and it is not keyed to whichever body holds ublk_sun — the
    # moon raising the background IS the washout, not a special case.
    thresh  = p_thc * P.pow(P.max(ctx.sky_luminance, 1e-9), p_thb)
    r       = P.pow(lum / P.max(thresh, 1e-12), p_thk)
    night   = r / (1.0 + r)
    night   = P.mix(p_sunless, night, P.step(0.5, ctx.has_sun))

    # below-horizon half of the shell: the dome is a FULL sphere so the pole tilt
    # can never expose an unstarred wedge, and this masks the half under the
    # ground with or without terrain in the way.
    view  = P.normalize(ctx.P - ctx.eye)
    horiz = P.smoothstep(p_hz_lo, p_hz_hi, view.y)

    # THE RADIANCE MULTIPLICATION POINT. Every extinction term the sky applies to
    # starlight multiplies here — the shared cloud-transmittance term lands
    # alongside `night * horiz` when it exists. (This material samples no clouds.)
    star = rad * (amp * g * keep * night * horiz)

    # ---- THE ENVELOPE REFUSAL (loud, never a silent clamp) ----
    # cutoff*sigma must land inside the quad (|q| <= 1). It cannot fail in any
    # configuration at or above _splat_sizing.WORST_CASE_PIXEL_DEG — that is what
    # sized the envelope — so if it does fail, the configuration is outside the
    # declared support and the sky says so instead of quietly clipping the
    # gaussian's tail (and its energy) against the quad rectangle. Bypasses both
    # fades on purpose: a refusal that only shows at night is not a refusal.
    overflow = P.step(1.0, p_cutsig * sigma_q)
    color    = P.mix(star, P.vec3(*REFUSAL_RADIANCE), overflow)

    # ADDITIVE: (SrcClr*1) + (FBClr*1). Order-independent — no sort, no depth
    # write — and physically what overlapping starlight does. Source alpha is
    # unused by this blend, so opacity stays 1 and every fade is in the color.
    self.unlit(color, 1.0,
               blend       = "additive",
               depth_test  = "leq",
               depth_write = False,
               cull        = "off")
    # empty sky is most of the dome — drop those fragments before the blend
    self._surf_body_append = (
      "if (max(max(s.emissive.r, s.emissive.g), s.emissive.b) < %s) discard;"
      % repr(float(DISCARD_RADIANCE)))


__all__ = ["StarSplat"]
