###############################################################################
# _cloud_deck.py — the CLOUD DECK library: the layered-texture sky decks that
# the cloud gauge grew (jul24/jul25) and scn_forest imported from it,
# promoted out of the scene file so any scene can raise the same decks with one
# call. The material, the shell mesh and the per-layer look tables below are
# that scene's VERBATIM; CloudDeckMixin.cloud_decks() is the per-scene
# instantiation boilerplate both scenes were copying.
#
# RETIRED (jul28): ork.data/scenes/scn_cloudgauge.py is gone — the content it
# existed to hold is right here, and the scene had become "a scope label that
# outlived its commit, not a decision — the same shape that let the family
# drift". Its shared launch-state module (_cloudgauge_input.py) stays in
# ork.data/scenes: it is a LIVE dependency of this file (see _cgi() below) and
# must remain a plain scenes-dir module because the player appends it as a
# system script.
#
# Three deck altitudes to spec (cirrus ~8 km / altocumulus ~4 km / cumulus ~1.5
# km), coverage from the R (rank) channel through the CHANNELS.md remap
# (ork.data/src/environ/clouds/CHANNELS.md).
#
# CONTROL BUS (data-only, no engine change): all runtime knobs ride the plane
# ENTITY TRANSFORMS — translation.y encodes the coverage threshold (the shader
# derives t from world altitude), uniform scale sets tile world-size (UVs are
# object-space), translation.xz is the wind scroll. Look constants are
# ctx.param plugs (A8). The gamepad layer drives that same bus, so the
# launch-state transform math here MUST match _cloudgauge_input.py.
###############################################################################

import math
import os
import sys

import numpy as np

from orkengine.core import vec3, CrcStringProxy

from ork.hypergraph.ptex3d import Ptex3d as Ptex3dBase, P
from ork.hypergraph.ptex3d.dsl import Param as PtexParam
from ork.hypergraph.ptex3d.functions import (beer_transmittance,
                                             fractional_occlusion)
from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import (MeshAsset, build_geometry,
                                                binormals_from_normals)
from ork.hypergraph.ecs.scene._helpers import Transform


_tokens = CrcStringProxy()

# Rec.709 luma weights — the ONE reduction from an rgb radiance/throughput to
# the single number the deck's fades and its overcast chroma pull work in.
_LUMA = P.vec3(0.2126, 0.7152, 0.0722)


def _workspace_dir():
  """The checkout under test. No fallback by law: a guessed workspace resolves
  deck textures (and the deck-state module) out of SOME OTHER tree, which shows
  up as a wrong-looking sky rather than an error. Set by init_env.py for every
  project env; ork/path.py hard-requires it the same way."""
  ws = os.environ.get("ORKID_WORKSPACE_DIR", "")
  if not ws:
    raise RuntimeError(
      "ORKID_WORKSPACE_DIR is unset — the cloud-deck library resolves its "
      "textures and its deck-state module from the workspace, and it must "
      "point at the checkout under test. Run inside a project env "
      "(init_env.py sets it); there is deliberately no default.")
  return ws


_TEXDIR = os.path.join(_workspace_dir(), "ork.data/platform_lev2/textures")

CLOUD_TEX = {
  "cirrus":     os.path.join(_TEXDIR, "clouds_cirrus_1024.png"),
  "alto":       os.path.join(_TEXDIR, "clouds_altocumulus_1024.png"),
  "cumulus1k":  os.path.join(_TEXDIR, "clouds_cumulus_1024.png"),
  "cumulus2k":  os.path.join(_TEXDIR, "clouds_cumulus_2048.png"),
}


###############################################################################
# Cloud-layer material — CHANNELS.md remap on a ptex3d lit translucent surface.
# One shared generated shader; the four instances differ only in bound texture
# + param plug values. Threshold/tile/wind arrive over the TRANSFORM BUS:
#   t   = (wpos.y - AltLo) * InvSweep      (script moves the plane +-sweep/2)
#   uv  = object_pos.xz * InvTile          (entity scale => tile world-size;
#                                           entity translation => wind scroll)
###############################################################################

# Reference intensity the live sun's radiometric weight is normalized against —
# Scene.sun()'s declared default. It is a NORMALIZER, not a look knob: real
# scene-exposure normalization belongs to the exposure slice, which supersedes
# this. A moon declares a much lower intensity (Scene.moon 0.4), so the same
# ratio makes moonlit decks land near a tenth of daylight on their own.
# NOTE (W10-S1): this is the DECK-BODY anchor only (the ratified daytime body
# radiance rides it). The SKY/haze side needs no normalizer at all since the
# aerial perspective became the engine's own march: the horizon radiance the
# deck fades toward is the atmosphere's, in the atmosphere's units.
SUN_BASE_INTENSITY = 4.0

# DAY MID-SKY RADIANCE — the engine's own unit bridge between the atmosphere's
# radiance scale and the deck model's normalized sky irradiance (a_body = 1.0
# at full day). The sky-view LUT stores radiance scaled by _sunIlluminance, and
# at the default (1,1,1) the day mid-sky lands near 2e-2 (sky_atmosphere.h states
# it twice: the presentation OUTPUT SCALE note and the night-emission
# derivation ladder, "day mid-sky 5.0e+3 cd/m^2 (engine 2e-2)"). The trio's own
# shipped magnitudes were DERIVED against this same anchor, so dividing by it
# recovers exactly the night:day irradiance fraction the trio authored — a
# ratio, so every gain the day sky and the night floor share (sky exposure,
# skybox intensity, display grade) cancels by construction. A constant of the
# earth-like default MEDIUM, not a look knob (SUN_BASE_INTENSITY precedent).
DAY_MID_SKY_RADIANCE = 2.0e-2


def night_floor_irradiance(atmo=None):
  """The night-ambient trio's summed SOURCELESS irradiance (airglow +
  starlight), expressed in the deck model's sky-irradiance unit (1.0 = the
  full-day sky) — THE value the CgNightAmb consumable slot exists to carry
  (W15-S1 wiring of the W10-S1 design note).

  atmo — the scene's declared SkyAtmosphereData, or None for the engine's
  earth-like default (the same object the renderer attaches when a scene
  declares none, so an undeclared scene reads the trio the sky actually
  renders with). The moon-Rayleigh term is deliberately EXCLUDED: it converts
  the declared moon light's illuminance into sky radiance, and the deck's
  ambient already scatters that same crossfaded beam (amb_scatter x e_body) —
  the moon path rides the beam term, phase-scaled upstream, never this
  sourceless slot (adding it here would double-count AND freeze the phase).

  Per-channel: the day sky scales with sun_illuminance per channel while the
  trio's emission is spectrally flat, so the fraction divides per channel
  (identity at the default (1,1,1))."""
  if atmo is None:
    from orkengine.lev2 import SkyAtmosphereData
    atmo = SkyAtmosphereData()
  glow = float(atmo.airglow_intensity) + float(atmo.starlight_intensity)
  si   = atmo.sun_illuminance
  return tuple(glow / max(DAY_MID_SKY_RADIANCE * float(c), 1.0e-9)
               for c in (si.x, si.y, si.z))


class CloudLayerMtl(Ptex3dBase):

  def __init__(self, ctx, *,
               tile_base_m  = 12000.0,   # WORLD meters per texture tile at tile-mult x1
               mesh_scale   = 125.0,     # entity BASE scale: local plane meters -> world
                                         # (the mesh pipeline mangles multi-km vertex
                                         # coords, so planes are authored ~400m local
                                         # and blown up by entity scale)
               alt_m        = 1500.0,    # nominal shell altitude
               sweep_m      = 600.0,     # altitude band encoding threshold 0..1
               soft         = 0.035,     # CHANNELS.md contact-sheet default
               erode        = 0.10,      #   "
               prox_gain    = 5.0,       # edge-proximity falloff for the B erosion
               detail_scale = 1.53,      # B sampled at a different tiling (decorrelated)
               tex_res      = 1024.0,    # bound texture resolution (2048 for cumulus-2k)
               aa_gain      = 0.04,      # analytic AA knee: rank-units per texel-per-pixel
               px_rad       = 0.0015,    # radians per pixel (~65deg/720px; VR similar)
               opacity_max  = 0.95,
               alpha_sigma  = 2.5,       # Beer-Lambert ALPHA: how fast optical depth (G)
                                         # saturates opacity — interior gradient instead of
                                         # a flat plateau (owner jul25: layer-combine
                                         # posterization from near-binary deck alphas)
               alpha_floor  = 0.35,      # opacity share at zero core (feathered fringes)
               ramp_mix     = 0.0,       # 0 = EDGE mode (smoothstep band at t: defined
                                         # billowy silhouettes) .. 1 = VEIL mode (Decima
                                         # coverage-remap: density=(R-t)/(1-t), the
                                         # opacity gradient spans the WHOLE feature —
                                         # spatially soft even when features are SMALL,
                                         # where a rank-unit feather collapses to a few
                                         # texels; owner jul25: high layers stayed sharp)
               dens_pow     = 1.0,       # veil-density gamma (coverage-gamma idiom):
                                         # <1 lifts + widens the faint fringes — gentler
                                         # falloff across a strand (owner jul25: "the
                                         # wavy bits need less falloff still")
               blur_tx      = 0.0,       # SPATIAL prefilter radius FLOOR (texels) on the
                                         # rank field: 9-tap disc emulating the missing
                                         # mip/blur chain (seam S1) — the only lever that
                                         # widens falloff in PIXELS regardless of the
                                         # texture's local gradient (owner jul25:
                                         # "reduce falloff in pixels by at least 4x")
               blur_px      = 0.0,       # ADAPTIVE feather target (SCREEN pixels): the
                                         # radius scales with texels-per-pixel (the AA
                                         # tpp estimate), so the feather holds its pixel
                                         # width under foreshortening at low elevations
                                         # — a fixed texel radius collapses exactly
                                         # where the deck is most-viewed (mip idiom:
                                         # footprint-proportional filtering)
               beer_sigma   = 1.2,       # Beer-Lambert extinction on the G optical-depth
                                         # proxy: tops stay sun-lit, cores shade to base
               lit_color    = (0.60, 0.575, 0.55), # sun-lit cloud radiance (matches the
                                                   #  current dim procsky exposure)
               shadow_color = (0.155, 0.170, 0.20),# shadowed-base radiance (bluish)
               silver_gain  = 2.0,       # silver-lining strength (thin deck near sun)
               silver_pow   = 24.0,      # forward-scatter lobe tightness
               silver_sigma = 2.0,       # how fast thickness kills the lining
               occ_sigma    = 8.0,       # OCCLUSION extinction (the one transmittance
                                         # term's sigma for what the deck hides): sets
                                         # how fast a thick core extinguishes the sun
                                         # disc / sky / stars behind it. Independent of
                                         # the look sigmas ON PURPOSE — opacity_max is a
                                         # look cap, this is physics (A8: a UBO plug, so
                                         # it retunes with no recompile).
               occ_floor    = 0.12,      # minimum depth proxy inside the coverage mask:
                                         # thin ice reads G~0 yet still dims the sun
               sun_dir      = (0.5, 0.53, 0.68),   # FALLBACK unit vector TOWARD the sun,
                                                   #  used only when the scene declares no
                                                   #  directional light (has_sun == 0); a
                                                   #  live sun overrides it every frame
               # ---- celestial irradiance model (W10-S1 owner ruling: ONE
               # continuous model, no night branch — night is what the model
               # produces as the incident light goes to zero) ----
               sun_hi_deg   = 10.0,      # above this elevation the ACTIVE body's beam is
                                         # unextinguished (full daytime radiance)
               sun_lo_deg   = -12.0,     # elevation where the beam extinction bottoms out
               dusk_level   = 0.35,      # beam extinction with the body exactly on the
                                         # horizon — the ratified sunset anchor
               beam_floor   = 0.0,       # beam extinction at/below sun_lo. 0 is physics
                                         # (a set body beams nothing). This slot was the
                                         # old night_floor GATE (which ctx.sun_intensity=0
                                         # defeated to black anyway); moonless radiance
                                         # now comes from night_ambient below.
               dusk_tint    = (1.25, 0.80, 0.55),  # warm low-body tint (peaks at the
                                         # horizon; applies to a rising moon too)
               night_tint   = (0.55, 0.62, 0.85),  # cool tint at/below the floor
               amb_scatter  = 1.0,       # sky-ambient response to the incident beam (the
                                         # scattered-light fraction), normalized so 1.0 =
                                         # the ratified daytime deck/haze balance
               night_ambient = None,     # rgb: SOURCELESS night-sky ambient irradiance
                                         # as a fraction of the daytime sky. None (the
                                         # default) = the WIRED value: the night-ambient
                                         # trio's summed airglow+starlight irradiance
                                         # (night_floor_irradiance; cloud_decks() fills
                                         # it from the scene's declared atmosphere). A
                                         # tuple overrides — the manual data-law knob.
                                         # The pre-wiring placeholder (0.007,0.009,0.014,
                                         # ~1% of day) left night decks ~35x brighter
                                         # than the entire clear night sky (W15-S1).
               wind_mps     = (0.0, 0.0),# WORLD-m/s wind vector — scrolls the UVs over
                                         # the STATIC shell on the GPU clock (owner jul25:
                                         # translating the shell made its curvature/veil
                                         # structures sweep the sky = the "swimming")
               evo_uv       = (0.0, 0.0),# Time-driven B-channel drift (uv/sec) —
                                         # CHANNELS.md evolution: B advects vs R so
                                         # silhouettes churn (independent of wind pause)
               cov_scale    = 1.0,       # per-layer share of the GLOBAL coverage knob:
                                         # t_layer = 1-(1-t)*cov_scale — cirrus stays in
                                         # its sparse feathery regime while cumulus fills
               haze_max     = 0.96,      # how completely distance melts a deck into sky:
                                         # the cap on the OPACITY melt (the color melt is
                                         # the atmosphere's own transmittance, uncapped).
                                         # The fade DISTANCE is no longer a knob — it
                                         # falls out of the medium's scale-height physics
                                         # through the shared aerial-perspective seam
               overcast_color = (0.295, 0.305, 0.325), # CHROMATICITY the overcast sky
                                         # converges to (luminance stays the
                                         # atmosphere's — see the graying below)
               gray_on      = 0.45,      # cover fraction where graying begins
               gray_gain    = 2.2,       # how fast cover desaturates past the onset
               veil_max     = 0.0,       # overcast HORIZON VEIL strength (cumulus only):
                                         # at high cover a gray wall swallows the sunset
                                         # strip near the horizon (owner jul25: sky colors
                                         # must couple to cloud cover; true coupling =
                                         # engine seam S6, this is the gauge approximation)
               veil_lo      = 100.0,     # veil band start (LOCAL radius)
               rim_lo       = 186.0,     # dome-rim opacity clamp start (LOCAL units,
               rim_hi       = 198.0,     #   shell half-extent = 200) — hides the mesh edge
               tint         = (1.0, 1.0, 1.0)):
    # object-space (LOCAL) tiling: world tile = tile_base_m * tile_mult, and
    # entity scale = mesh_scale * tile_mult, so local tile = tile_base/mesh_scale
    inv_tile  = ctx.param("CgInvTile",  mesh_scale / tile_base_m)
    alt_lo    = ctx.param("CgAltLo",    alt_m - 0.5 * sweep_m)
    inv_sweep = ctx.param("CgInvSweep", 1.0 / sweep_m)
    p_soft    = ctx.param("CgSoft",     soft)
    p_erode   = ctx.param("CgErode",    erode)
    p_pgain   = ctx.param("CgProxGain", prox_gain)
    p_dscale  = ctx.param("CgDetail",   detail_scale)
    p_texres  = ctx.param("CgTexRes",   tex_res)
    p_aa_k    = ctx.param("CgAaGain",   aa_gain)
    p_px_rad  = ctx.param("CgPxRad",    px_rad)
    p_omax    = ctx.param("CgOpacMax",  opacity_max)
    p_asig    = ctx.param("CgAlphaSig", alpha_sigma)
    p_afloor  = ctx.param("CgAlphaFlr", alpha_floor)
    p_rampmix = ctx.param("CgRampMix",  ramp_mix)
    p_dpow    = ctx.param("CgDensPow",  dens_pow)
    p_blur    = ctx.param("CgBlurTx",   blur_tx)
    p_bpx     = ctx.param("CgBlurPx",   blur_px)
    p_sigma   = ctx.param("CgBeerSig",  beer_sigma)
    p_lit     = ctx.param("CgLitCol",   lit_color)
    p_shad    = ctx.param("CgShadCol",  shadow_color)
    p_sl_gain = ctx.param("CgSlGain",   silver_gain)
    p_sl_pow  = ctx.param("CgSlPow",    silver_pow)
    p_sl_sig  = ctx.param("CgSlSig",    silver_sigma)
    p_occ_sig = ctx.param("CgOccSig",   occ_sigma)
    p_occ_flr = ctx.param("CgOccFlr",   occ_floor)
    p_sun     = ctx.param("CgSunDir",   sun_dir)
    # beam-extinction band, packed in one vec4: the shader tests sin(elevation)
    # (= the to-body vector's y) against the two edge SINES, then interpolates
    # the horizon anchor and the below-horizon floor.
    p_sunband = ctx.param("CgSunBand",  (math.sin(math.radians(sun_hi_deg)),
                                         math.sin(math.radians(sun_lo_deg)),
                                         dusk_level, beam_floor))
    p_dusk_c  = ctx.param("CgDuskTint",  dusk_tint)
    p_night_c = ctx.param("CgNightTint", night_tint)
    p_ambsc   = ctx.param("CgAmbScat",  amb_scatter)
    # None = the trio's summed sourceless irradiance at the ENGINE-DEFAULT
    # atmosphere (a scene-declared medium reaches here through cloud_decks(),
    # which resolves it before constructing the material).
    _namb     = (night_floor_irradiance() if night_ambient is None
                 else night_ambient)
    p_namb    = ctx.param("CgNightAmb", _namb)
    # WIND/EVO ride VEC3 params (z unused): the reflection varmap codec has NO
    # fvec2 encoder (codec.inl: float/int/bool/fvec3/fvec4/string/crcstr only)
    # — a vec2 param serializes as "null:", the uniform never binds, and the
    # GPU reads stale UBO memory: ZERO offscreen (clouds froze) but JUNK in the
    # VR path (huge garbage wind x live CgTime = the owner's "high speed
    # swimming"). Engine seam filed (S8); vec3 is the data-side fix.
    p_wind    = ctx.param("CgWindVec",  (wind_mps[0], wind_mps[1], 0.0))
    p_evo     = ctx.param("CgEvoUV",    (evo_uv[0], evo_uv[1], 0.0))
    p_covs    = ctx.param("CgCovScale", cov_scale)
    # engine-fed scene clock (fx_pipeline named-param provider, the L-system
    # Wind precedent) — powers the differential-evolution drift below.
    p_time    = PtexParam("CgTime", "float", _tokens.RCFD_TIME)
    p_haze_k  = ctx.param("CgHazeMax",  haze_max)
    p_rim_lo  = ctx.param("CgRimLo",    rim_lo)
    p_rim_hi  = ctx.param("CgRimHi",    rim_hi)
    p_ovc_c   = ctx.param("CgOvcCol",   overcast_color)
    p_gray_on = ctx.param("CgGrayOn",   gray_on)
    p_gray_k  = ctx.param("CgGrayGain", gray_gain)
    p_veil_mx = ctx.param("CgVeilMax",  veil_max)
    p_veil_lo = ctx.param("CgVeilLo",   veil_lo)
    p_tint    = ctx.param("CgTint",     tint)

    # entity scale from screen derivatives (exact per-triangle for a uniform,
    # unrotated entity) — needed for the UV wind scroll AND the threshold decode
    s_ent = P.length(P.dFdx(ctx.P)) / P.max(P.length(P.dFdx(ctx.P_object)), 1e-9)

    # view geometry + texel footprint FIRST (the adaptive prefilter below and
    # the AA knee both need texels-per-pixel): stable under head motion —
    # depends only on world distance + grazing angle, never on fwidth().
    d_eye = P.length(ctx.P - ctx.eye)
    view  = P.normalize(ctx.P - ctx.eye)
    # LIVE BODY: the engine's per-frame directional light (ublk_sun) whenever the
    # scene declares one; the frozen CgSunDir kwarg is the fallback for a sunless
    # scene. ctx.sun_dir is the sunlight TRAVEL direction, so the to-sun vector is
    # its negation — CgSunDir is already a to-sun vector. Declared HERE, ahead of
    # the radiometry that consumes it, because the aerial-perspective march below
    # is illuminated by the same body.
    to_sun = P.mix(p_sun, P.normalize(-ctx.sun_dir), P.step(0.5, ctx.has_sun))
    texel_w = s_ent / (inv_tile * p_texres)          # one texel in WORLD meters
    tpp   = (d_eye * p_px_rad) / (texel_w * P.max(P.abs(view.y), 0.2))

    # EXPLICIT fract() tiling — the bound sampler CLAMPS (engine default), so
    # without this only the single [0,1] UV tile shows cloud; everything else
    # streaks the edge texels (the "one wedge of cloud" failure).
    # WIND rides the GPU clock as a UV scroll over the STATIC shell (canonical
    # skydome idiom) — world wind vec -> object space via /s_ent.
    # `raw` is the UNfracted tile coordinate: every derived lookup MUST tile
    # from raw, never from fract(raw) — fract(fract(raw)*k) puts a genuine
    # (frac k)-sized jump in the derived field at the wrap seam (the owner's
    # "discontinuity directly overhead": the seam parked at the apex at t=0,
    # since P_object.xz=0 there and fract is discontinuous at 0). The +0.5
    # bias parks the wrap seam HALF A TILE off the zenith, so the residual
    # one-texel clamp seam (engine seam S1: no REPEAT sampler) never sits at
    # the most-viewed point of the sky.
    raw   = (ctx.P_object.xz - p_wind.xy * p_time / s_ent) * inv_tile + P.vec2(0.5, 0.5)
    uv    = P.fract(raw)
    tex   = ctx.tex("CloudTex", uv)              # R=rank.hi G=core B=detail A=rank.lo
    # 16-BIT rank reconstruction (jul25 bake: A = low byte). Linear in both
    # channels, so bilinear filtering is exact — kills the 8-bit terracing.
    # SPATIAL PREFILTER (5-tap diagonal box, radius CgBlurTx texels): the
    # in-shader stand-in for the missing mip/blur chain (seam S1). Widening
    # the falloff in SCREEN PIXELS requires widening the FIELD's spatial
    # gradient — no rank-space remap can do it (owner jul25: ">=4x wider").
    # The blur is linear, so the 16-bit R+A reconstruction stays exact.
    # ADAPTIVE radius: max(floor texels, feather-target-px * tpp / 2) — holds
    # the feather's SCREEN width under foreshortening (fixed texel radii
    # collapse to a few px at mid/low elevation, where the deck is most seen).
    b_tx  = P.max(p_blur, 0.5 * p_bpx * tpp)
    b_uv  = b_tx / p_texres
    _ring = [( 1.0,  0.0), (-1.0,  0.0), ( 0.0,  1.0), ( 0.0, -1.0),
             ( 0.707,  0.707), (-0.707,  0.707), ( 0.707, -0.707), (-0.707, -0.707)]
    _taps = [tex] + [ctx.tex("CloudTex", P.fract(raw + P.vec2(dx, dy) * b_uv))
                     for (dx, dy) in _ring]
    r_hi, r_lo, c_sum = _taps[0].x, _taps[0].w, _taps[0].y
    for _tp in _taps[1:]:
      r_hi  = r_hi + _tp.x
      r_lo  = r_lo + _tp.w
      c_sum = c_sum + _tp.y
    rank  = (r_hi * 256.0 + r_lo) * (1.0 / (9.0 * 257.0))
    core  = c_sum * (1.0 / 9.0)
    # B (erosion detail) advects relative to R over TIME (CHANNELS.md: ~1.1-1.3x
    # the layer wind) — silhouettes churn instead of translating rigidly. Note:
    # this rides the SCENE CLOCK, so it keeps evolving through a wind pause.
    det   = ctx.tex("CloudTex",
                    P.fract(raw * p_dscale + P.vec2(0.37, 0.61) + p_evo.xy * p_time)).z

    # coverage threshold decoded from the shell APEX's WORLD altitude
    # (transform bus); the shell's own sag is subtracted via s_ent.
    apex_y = ctx.P.y - s_ent * ctx.P_object.y
    t_g   = P.clamp((apex_y - alt_lo) * inv_sweep, 0.0, 1.2)
    # per-layer coverage share of the global knob (1-t is sky-cover fraction)
    t     = 1.0 - (1.0 - t_g) * p_covs
    # GLOBAL cover fraction (the knob itself) — drives the overcast graying
    cover = P.clamp(1.0 - t_g, 0.0, 1.0)

    # CHANNELS.md: erode the boundary before thresholding for crinkly
    # silhouettes (rank is 16-bit now — no de-banding dither needed).
    prox  = 1.0 - P.clamp(P.abs(rank - t) * p_pgain, 0.0, 1.0)
    r_e   = rank - p_erode * det * prox
    # BANDLIMITED knee, ANALYTIC (owner jul25 "swimming" fix): the previous
    # fwidth() knee breathed with head motion (screen-space footprint changes
    # every frame -> opacity swims in VR). This estimate depends only on world
    # distance + grazing angle, so it is stable under view motion:
    #   texels/pixel ~ (d_eye * px_rad) / texel_world / |view.y|
    # (d_eye/view/texel_w/tpp computed above, before the prefilter taps)
    fw    = P.clamp(p_aa_k * tpp, 0.0, 0.5)
    # FEATHERED coverage falloff, CENTERED on t (owner jul25 "sharp falloffs ->
    # layer-combine posterization"): the old knee rose 0 -> omax over soft
    # (0.035!) rank units — near-binary deck alphas whose overlaps quantize
    # into discrete opacity bands. Wide s-curve centered on t keeps the
    # coverage knob honest (R is rank-equalized; mean cover stays 1-t).
    half  = 0.5 * (p_soft + fw)
    cov_e = P.smoothstep(t - half, t + half, r_e)
    # VEIL mode (Decima coverage-remap): density = rank excess normalized to
    # the remaining covered range — exact coverage (r < t stays clear) with an
    # opacity gradient across the WHOLE feature. The rank feather above is a
    # BAND at t whose spatial width is soft/|grad R|: tiny for the small
    # alto cloudlets / cirrus strands (steep local rank gradients), hence the
    # persistent sharp falloff on the high layers. This ramp is gradient-
    # independent: small features fade across their full extent.
    dens  = P.clamp((r_e - t) / P.max(1.0 - t + fw, 0.05), 0.0, 1.0)
    # coverage-gamma: dens^p with p<1 lifts the faint fringes, stretching the
    # visible gradient across the whole strand (max slope drops as p drops) —
    # the falloff gentleness dial for the veil layers.
    cov_r = P.pow(dens, p_dpow)
    cov   = P.mix(cov_e, cov_r, p_rampmix)
    # Beer-Lambert ALPHA on the G optical-depth proxy: interiors gain opacity
    # with core-ness instead of sitting at a flat plateau — the deck reads as
    # thick-center/thin-fringe, and overlapping decks blend continuously.
    a_core = 1.0 - beer_transmittance(core, p_asig)   # absorbed = 1 - transmitted
    cov    = cov * P.mix(p_afloor, 1.0, a_core)

    # AERIAL PERSPECTIVE — THE ENGINE SEAM (S6 CLOSED). The deck no longer fakes
    # distance haze with an e-folding distance and a frozen horizon color: it
    # calls skyAerialPerspective, the SAME 8-step march through the SAME Hillaire
    # medium the forward fragments run and the sky LUTs are baked from. Distance,
    # altitude, sun elevation and the artist ground-haze layer therefore reach the
    # deck exactly as they reach everything else, and a deck at the horizon melts
    # into the sky the atmosphere actually renders there — at every hour, with no
    # per-layer distance to keep in sync (scale-height physics sets it).
    #
    # NAMING the function is what arms it: the ptex3d template detects it in this
    # body and adds the skytools import + the lib_sky inherit, and the material's
    # pipelines pick up the sky-haze state lambda (declaring SkyRadii IS the
    # opt-in). A pass with no binder — impostor capture, masked prepass, the
    # cookie fill before the LUTs exist — reads the zero buffer, where
    # SkyHazeGeom.x (km per world unit) is 0, the ray collapses to zero length and
    # the seam returns transmittance 1 / in-scatter 0: no fade, never garbage.
    #
    # dir_to_sun is the deck's OWN to-sun vector rather than ublk_sky_atmo's
    # SkySunDirection: both are the scene's highest-priority directional light
    # (SkyFrameState publishes that same light negated), so they agree by
    # construction wherever both exist — and this one is finite in a scene that
    # declares no sun, where normalize() of the unbound sky vector would seed NaN
    # through every channel below.
    hz     = P.func("skyAerialPerspective({0}, {1}, {2})",
                    [ctx.eye, ctx.P, to_sun], rtype="HazeResult")
    hz_t   = P.func("{0}._transmittance", [hz], rtype="vec3")
    hz_s   = P.func("{0}._inscatter", [hz], rtype="vec3")
    # ONE scalar fade: the deck's opacity melt and its color melt are the same
    # loss of the deck, so the per-channel throughput reduces through the eye's
    # own weighting (Rec.709) instead of three separate fades.
    haze  = P.clamp(1.0 - P.dot(hz_t, _LUMA), 0.0, 1.0)
    # OVERCAST GRAYING: as cover rises the horizon loses its clear-sky
    # chromaticity and converges to the overcast gray. The pull is on CHROMA
    # ONLY — the luminance stays the in-scatter's, i.e. the atmosphere's — so
    # overcast_color can no longer set how BRIGHT the horizon is at any hour
    # (a static color that did was the jul30 glowing-night-deck defect).
    grayness = P.clamp((cover - p_gray_on) * p_gray_k, 0.0, 1.0)
    ovc_chr  = p_ovc_c * (1.0 / P.max(P.dot(p_ovc_c, _LUMA), 1.0e-5))
    haze_c   = P.mix(hz_s, P.dot(hz_s, _LUMA) * ovc_chr, grayness)
    # dome-rim clamp (LOCAL radius): guarantees the shell edge itself is gone
    # well before the mesh boundary / before the shell dips to eye level.
    rim   = 1.0 - P.smoothstep(p_rim_lo, p_rim_hi, P.length(ctx.P_object.xz))

    # OVERCAST HORIZON VEIL: a gray wall in the shell's outer band that rises
    # with cover^2 — swallows the exposed sunset strip near the horizon when
    # the sky closes up; vanishes when clear. Color is ~haze_c there anyway
    # (haze ~ 1 in that band), so only opacity needs the floor.
    veil  = cover * cover * p_veil_mx * P.smoothstep(p_veil_lo, p_rim_lo, P.length(ctx.P_object.xz))

    # PRESENCE = every fade that means "less deck here", i.e. `opac` without the
    # opacity_max LOOK CAP. Both the visible opacity and the occlusion below are
    # derived from this ONE expression so no fade can shape one and miss the
    # other — the jul28 'black fadeouts' regression was exactly that mismatch.
    if os.environ.get("ORK_CLOUDGAUGE_SOLID", "0") == "1":   # debug: geometry viz
      presence = rim * 0.7
      opac     = presence
    else:
      presence = P.max(cov * (1.0 - haze * p_haze_k), veil) * rim
      opac     = P.max(cov * p_omax * (1.0 - haze * p_haze_k), veil) * rim

    # ---------- CLOUD RADIOMETRY (custom, UNLIT — scene lighting bypassed) ---
    # ONE CONTINUOUS CELESTIAL-IRRADIANCE MODEL (W10-S1 owner ruling: no night
    # branch). Deck radiance = DIRECT + AMBIENT, both derived from the light
    # actually INCIDENT on the deck:
    #   direct  = sunlit-face color x incident beam  (the sun by day; the
    #             phase/elevation-scaled moon by night — the night policy folds
    #             both into the ONE crossfaded directional feed ublk_sun
    #             carries, so the moon's illumination gate arrives upstream)
    #   ambient = shadowed-base color x sky irradiance, where the sky is the
    #             scattered fraction of that same beam PLUS the sourceless
    #             night floor (CgNightAmb — WIRED since W15-S1 to the
    #             night-ambient trio's summed airglow+starlight irradiance,
    #             night_floor_irradiance above; moon-Rayleigh rides the beam)
    # The horizon the deck melts into is NOT modelled here at all any more: it
    # is the atmosphere's own in-scatter, from the shared seam above. A night
    # deck therefore cannot inherit a daytime-bright horizon (the jul30
    # night-brightness defect, when the fade target was a static color times a
    # scene-normalized weight) — it fades toward the night sky that renders
    # beside it, because it is the same integral.
    #
    # SEAM (single-feed): ublk_sun carries one body at a time — the policy
    # crossfades sun->moon exactly where the sun is down to ~1% — so the sun
    # and moon terms are time-multiplexed here, not simultaneous. The one
    # regime where both matter at once (dusk with a risen moon) is dominated
    # by twilight sky ambient anyway; true simultaneity needs the forward
    # prologue to publish the second directional light (engine seam, reported).
    # (to_sun — the LIVE body, or the frozen fallback vector — is declared up in
    # the view-geometry block: the aerial-perspective march needs it too.)
    mu    = P.clamp(P.dot(view, to_sun), 0.0, 1.0)
    trans = beer_transmittance(core, p_sigma)
    sl    = P.pow(mu, p_sl_pow) * beer_transmittance(core, p_sl_sig)
    # incident-beam weight — the live intensity over the historical SUN_BASE
    # normalizer: the deck-body weight the ratified daytime look was graded
    # against (identical arithmetic to the old lit_w, so the day look rides
    # unchanged). A sunless scene (has_sun = 0) keeps the frozen-kwarg full-day
    # behavior. The crossfaded intensity is also what makes the caster-handoff
    # DIRECTION snap invisible: ~1% at the -5.5deg handoff, then the moonlit deck
    # RAMPS in as the moon's own policy scale grows.
    _live  = P.step(0.5, ctx.has_sun)
    e_body = P.mix(1.0, P.clamp(ctx.sun_intensity * (1.0 / SUN_BASE_INTENSITY), 0.0, 1.0),
                   _live)
    # atmospheric shaping of the ACTIVE body's beam by its elevation: extinction
    # (1 in full day -> dusk_level at the horizon -> beam_floor below sun_lo)
    # and reddening (white -> dusk tint -> night tint). Valid for the moon too:
    # a rising moon is extinguished and reddened by the same airmass.
    s_elev = to_sun.y
    day    = P.smoothstep(0.0, p_sunband.x, s_elev)     # 0 at horizon .. 1 above sun_hi
    twi    = P.smoothstep(p_sunband.y, 0.0, s_elev)     # 0 at sun_lo  .. 1 at horizon
    ext    = P.mix(P.mix(p_sunband.w, p_sunband.z, twi), 1.0, day)
    btint  = P.mix(P.mix(p_night_c, p_dusk_c, twi), P.vec3(1.0), day)
    # sky irradiance (rgb) ON THE DECK: scattered beam + the sourceless night
    # floor. Deck-anchored (e_body), like the direct term it accompanies.
    a_body = p_ambsc * (e_body * ext) * btint + p_namb
    direct = (p_lit * trans + p_sl_gain * sl * p_lit) * (e_body * ext) * btint
    amb    = p_shad * (1.0 - trans) * a_body
    col    = (direct + amb) * p_tint
    col    = P.mix(col, haze_c, haze)   # aerial perspective last: the target is
                                        # the air's own in-scatter, in the
                                        # atmosphere's units (already carrying
                                        # the sky exposure), so no bridge weight
                                        # stands between the deck and the sky

    # ---------- SUN/SKY OCCLUSION (consumer of the ONE transmittance term) ---
    # The deck's `opac` is a LOOK value: capped at opacity_max, feathered, hazed.
    # Under straight ALPHA that same number also decides what survives BEHIND the
    # deck, so the background always kept >= (1-opacity_max) of its radiance —
    # invisible on sky, blinding on the sun disc, whose HDR intensity punches
    # through any 4% leak (the owner's "clouds not occluding sun"). PREMULTIPLIED
    # blending splits the two: the deck ADDS `col*opac` (its look, unchanged to
    # the bit) and the background survives by TRUE transmittance.
    #
    # PRESENCE vs DEPTH — the split this term lives or dies on (jul28 regression
    # 'cloud fadeouts are black'). `presence` is EVERY fade that says there is
    # LESS DECK HERE and is the exact set that shapes the visible opacity below,
    # minus the opacity_max look cap: the coverage feather (edge + veil ramp +
    # the a_core interior gradient, all already folded into `cov`), the
    # aerial-perspective loss, the overcast veil floor and the dome-rim clamp.
    # It enters LINEARLY (fractional_occlusion), so a fading edge dims and
    # un-occludes together and goes transparent, never black.
    #
    # DEPTH is how thick the deck is where it IS: the G proxy, floored, times the
    # slab slant (a slab at a grazing angle is deeper by 1/|cos|; the same 0.2
    # floor the AA footprint uses caps it at 5x). Depth alone is exponential.
    #
    # NOT in presence, on purpose: the elevation dimming / sun-intensity / tint
    # factors above are RADIANCE-only — a night cloud stops glowing but must go
    # on hiding the stars behind it.
    slant    = 1.0 / P.max(P.abs(view.y), 0.2)
    # FLOOR on the depth proxy: G reads ~0 on thin ice/wisp, but a deck fragment
    # you can SEE is a deck fragment that blocks light (which is why the look
    # model carries alpha_floor at all). Without it a coreless-but-covered
    # fragment would be perfectly transparent to the sun AND fall under the
    # near-zero discard below, punching holes in cirrus.
    occ_a    = fractional_occlusion(presence, P.max(core, p_occ_flr) * slant, p_occ_sig)

    # UNLIT emissive output; depth_test ON (leq) — terrain/geometry occludes
    # the decks. (Engine seam S3 — skybox z-write 0.9999 — FIXED @ 369f01e55:
    # the sky quad no longer writes depth, so depth-testing the decks is safe
    # at any range.) Drawn on std_transparent AFTER opaques; never z-writes.
    self.unlit(col * opac, occ_a,
               blend       = "prema",
               depth_test  = "leq",
               depth_write = False,
               cull        = "off")
    # kill near-zero fragments (fill savings). s.opacity is the OCCLUSION alpha
    # now; the depth floor above keeps it tied to deck presence, so this still
    # culls only fragments that neither add radiance nor hide anything.
    self._surf_body_append = (
      "if (s.opacity < 0.02) discard;")


###############################################################################
# CloudShellMesh — a DOME shell (paraboloid cap, apex at local y=0, sagging by
# `drop` toward the rim), centered at origin. The classic skydome recipe: the
# far deck descends toward eye level at a FINITE radius, which (a) bounds the
# grazing-angle anisotropy that made the flat plane shimmer near the horizon
# and (b) compresses the horizon naturally. The transform bus survives: the
# APEX rides translation.y (threshold decode subtracts scale*local_y
# in-shader), entity scale blows the 400m-local shell up to world size.
###############################################################################

@_register
class CloudShellMesh(MeshAsset):

  def __init__(self, *, extent_m=400.0, grid=24, drop=12.0, material=None):
    n    = int(grid) + 1
    half = 0.5 * extent_m
    xs = np.linspace(-half, half, n, dtype=np.float32)
    px, pz = np.meshgrid(xs, xs, indexing="ij")
    r2 = (px * px + pz * pz).ravel()
    k  = float(drop) / (half * half)          # y(r) = -k * r^2 (paraboloid sag)
    py = (-k * r2).astype(np.float32)
    verts = np.stack([px.ravel(), py, pz.ravel()], axis=1)
    tris = []
    for i in range(n - 1):
      for j in range(n - 1):
        a = i * n + j
        b = (i + 1) * n + j
        c = i * n + (j + 1)
        d = (i + 1) * n + (j + 1)
        tris += [[a, c, b], [b, c, d]]        # +Y-facing winding
    # analytic paraboloid normals: y=-k r^2 -> N ~ (2kx, 1, 2kz)
    nx = (2.0 * k * px.ravel()).astype(np.float32)
    nz = (2.0 * k * pz.ravel()).astype(np.float32)
    ny = np.ones(n * n, dtype=np.float32)
    norms = np.stack([nx, ny, nz], axis=1)
    norms /= np.linalg.norm(norms, axis=1, keepdims=True)
    bins = binormals_from_normals(norms)
    self._init(build_geometry(verts, tris, norms, bins), material)


# per-layer material look (WORLD meters at tile-mult x1). The shell MESH is a
# fixed PLANE_LOCAL_M-wide dome; world extent = local * base_scale (CGI.LAYERS)
# * tile-mult — big vertex coords never touch the mesh pipeline (it mangles
# multi-km coordinates). Dome sag: the rim descends DROP_FRAC of the layer
# altitude, so the far deck approaches (but never crosses) eye level.
PLANE_LOCAL_M = 400.0

# evolution-churn multiplier (owner "swimming" gauge): 0 = freeze silhouette
# churn entirely, 1 = default gentle rate.
EVO_MULT = float(os.environ.get("ORK_CLOUDGAUGE_EVO", "1.0"))
DROP_FRAC     = 1.10


_LAYER_LOOK = {
  # cirrus: heavy B-erosion shreds silhouettes into fibers; cov_scale keeps it
  # in the sparse feathery regime of the jul25 rebake while cumulus fills.
  # beer_sigma: ice is optically thin; cumulus is thick. evo_uv: B-channel
  # drift (uv/s along the layer wind) — the CHANNELS.md evolution churn.
  # soft: the FEATHER width (rank units, centered on t) — jul25 widened ~4-8x
  # from the contact-sheet knees (0.035-0.07) after the owner flagged sharp
  # falloffs posterizing where decks overlap. alpha_sigma/alpha_floor: the
  # Beer-alpha interior gradient (thin ice stays translucent -> low sigma,
  # high floor; cumulus saturates fast -> high sigma, low floor).
  # ramp_mix: EDGE mode (0, silhouette band at t) vs VEIL mode (1, Decima
  # full-range coverage remap). High layers (small blobs / wavy strands) go
  # veil-soft — their steep local rank gradients defeat any rank-unit feather;
  # cumulus keeps mostly-edge billows with a touch of interior ramp.
  # cirrus: erode dropped 0.20 -> 0.06 (the HF B-erosion carved crisp edges
  # back into the strands) + dens_pow 0.55 stretches the fade across the
  # whole strand — the "wavy bits" falloff dial.
  "cirrus": dict(soft=0.30,  erode=0.06, opacity_max=0.50, beer_sigma=0.5,
                 alpha_sigma=1.2, alpha_floor=0.55, ramp_mix=1.0,
                 dens_pow=0.55, blur_tx=8.0, blur_px=20.0,
                 silver_gain=1.6, cov_scale=0.55,
                 evo_uv=(0.00004, 0.0)),
  "alto":   dict(soft=0.18,  erode=0.10, opacity_max=0.85, beer_sigma=0.9,
                 alpha_sigma=2.0, alpha_floor=0.40, ramp_mix=0.75,
                 dens_pow=0.80, blur_tx=6.0, blur_px=16.0,
                 silver_gain=1.2, cov_scale=0.85,
                 evo_uv=(0.00006, 0.000025), veil_max=0.6),
  "cumulus": dict(soft=0.12, erode=0.10, opacity_max=0.96, beer_sigma=1.4,
                  alpha_sigma=2.5, alpha_floor=0.35, ramp_mix=0.25,
                  dens_pow=1.0, blur_tx=2.0, blur_px=6.0,
                  silver_gain=1.0, cov_scale=1.0,
                  evo_uv=(0.00002, -0.000012), veil_max=0.985),
}

# entity -> (LAYERS key, texture key). Declared TOP-DOWN (cirrus first) so the
# std_transparent draw order composites back-to-front for a viewer below.
_PLANES = [
  ("cloud_cirrus",     "cirrus",  "cirrus"),
  ("cloud_alto",       "alto",    "alto"),
  ("cloud_cumulus_2k", "cumulus", "cumulus2k"),
  ("cloud_cumulus_1k", "cumulus", "cumulus1k"),
]


###############################################################################
# The mixin.
###############################################################################

# The per-layer geometry table (altitudes, tile sizes, base scales, winds) and
# the launch-state math live in ork.data/scenes/_cloudgauge_input.py — ONE truth
# shared by the scene-author phase and the in-player gamepad script, which must
# stay a plain scenes-dir module (the player appends it as a system script).
# Scenes that raise decks already import it (which seeds sys.path); the fallback
# below covers a caller that didn't.
def _cgi():
  try:
    import _cloudgauge_input as CGI
  except ImportError:
    scenes_dir = os.path.join(_workspace_dir(), "ork.data/scenes")
    if scenes_dir not in sys.path:
      sys.path.insert(0, scenes_dir)
    import _cloudgauge_input as CGI
  return CGI


class CloudDeckMixin:

  ###########################################################################
  # THE IMPLIED DECK SET YIELDS TO AN AUTHORED ONE.
  #
  # Every procedural sky carries decks (see _sky.py), but a scene that says
  # cloud_decks() itself is the deck author: its call is the whole deck set,
  # never a second one merged onto the sky's. The implied set is therefore
  # STAGED at sky() time and declared at Scene.build(), only if nothing
  # explicit claimed the decks by then — the two sets share entity and asset
  # names, so declaring both is a name collision, and that collision guard is
  # what must keep firing for a scene that says cloud_decks() TWICE.
  ###########################################################################

  def stage_default_cloud_decks(self, **deck_kw):
    """Stage the sky's implied deck set (declared in Pass 2, or never)."""
    self._staged_cloud_decks = dict(deck_kw)

  def declare_staged_cloud_decks(self):
    """Pass-2 head: land the staged set unless the scene declared its own."""
    deck_kw = self._staged_cloud_decks
    self._staged_cloud_decks = None
    if deck_kw is not None and not self._cloud_decks_declared:
      self.cloud_decks(**deck_kw)

  def cloud_decks(self, *,
                  specs        = None,
                  looks        = None,
                  alt_offset_m = 0.0,
                  sag_datum_m  = 0.0,
                  cover        = None,
                  tile         = None,
                  sun_dir      = None,
                  colors       = None,
                  layer        = None,
                  cookie_layer = "sun_cookie"):
    """Declare the cloud-deck entities (one textured dome shell per deck).

    specs        — [(entity_name, layer key, texture key), ...]; default _PLANES
                   (declared TOP-DOWN so the std_transparent draw order
                   composites back-to-front for a viewer below).
    looks        — per-layer material look dict; default _LAYER_LOOK.
    alt_offset_m — raises every deck (AGL spec altitudes -> ASL over elevated
                   terrain).
    sag_datum_m  — the level the dome rim sags TOWARD (0 = sea level; a terrain's
                   mean height makes the ridgeline occlude the far rim).
    cover        — sky-cover fraction 0..1. None = the env launch state
                   (ORK_CLOUDGAUGE_T, the gauge's live-tunable threshold).
    tile         — tile-size multiplier. None = the env launch state.
    sun_dir      — FALLBACK unit vector TOWARD the sun for the deck radiometry;
                   used only when the scene declares no directional light.
    colors       — optional overrides for the material's radiance colors
                   (lit_color / shadow_color, plus overcast_color, which is a
                   CHROMATICITY now — a gain on it does nothing; also
                   night_ambient, the manual override for the auto-read trio
                   night floor — see night_floor_irradiance).
    layer        — render layer for the deck nodes; default std_transparent.
    cookie_layer — SECOND node per deck, on the layer the engine fills the sun
                   COOKIE from (cloud shadows on the ground + cloud occlusion of
                   the sun/moon discs). Default "sun_cookie", the role name the
                   forward node looks up; None declares no cookie node and the
                   decks cast nothing. It is a second NODE, not a second
                   drawable: the cookie pass draws the very same deck geometry
                   and material from a sun-aligned camera, which is what keeps
                   ONE transmittance model behind everything a cloud occludes.
                   Declaring it costs nothing until a sun sets
                   cloud_shadow_strength > 0 — no strength, no pass.

    Returns the list of entity handles, in `specs` order."""

    self._cloud_decks_declared = True
    CGI   = _cgi()
    st    = CGI.env_state()
    # THE DECKS' OWN RUNTIME STATE APPLIER, attached with the decks. It owns the control
    # bus these entities are driven through (coverage in deck altitude, tile in scale) and
    # is the only listener for the CloudSet message a host uses to move them — so a scene
    # that declares decks gets the thing that can actually change them, rather than decks
    # frozen at their launch transforms with a HUD row that cannot bite. Idempotent: its
    # first apply writes the very launch state the author-time transforms already carry.
    # Composable, like the sky clock: an APPENDED system script.
    self.append_system_script(os.path.join(_workspace_dir(), "ork.data/scenes/_cloudgauge_input.py"))
    specs = _PLANES      if specs is None else specs
    looks = _LAYER_LOOK  if looks is None else looks
    if layer is None:
      layer = os.environ.get("ORK_CLOUDGAUGE_NODELAYER", "std_transparent")
    thresh = st["thresh"] if cover is None else CGI.cover_to_thresh(cover)
    tile_mult = st["tile"] if tile is None else float(tile)
    # THE LAUNCH STATE, PUBLISHED AS SCENE DATA. The decks' runtime currency is the
    # deck TRANSFORM (coverage encoded as altitude), which no host can read a cover
    # back out of — so the numbers themselves ride the scenegraph params, next to
    # SkyAtmosphere and SkySource. That is what lets the player's CLOUDS rows come up
    # holding what THIS scene declared instead of a guessed default, which in turn is
    # what makes a saved cover a diff against the scene rather than against a constant.
    _sgdecl_params = self._systems.get("SceneGraphSystem")
    if _sgdecl_params is not None:
      for _call, _args, _kw in _sgdecl_params.sub_calls:
        if _call == "declareParams" and _args and isinstance(_args[0], dict):
          _args[0]["CloudCover"] = float(max(0.0, min(1.0, 1.0 - thresh)))
          _args[0]["CloudTile"]  = float(tile_mult)
          # the decks' ASL lift. The material's band is built at alt+offset, so a
          # runtime applier that does not know the offset would place the shell off
          # its own band (the author-time transform below adds it, the live applier
          # had no way to). It travels with the state it belongs to.
          _args[0]["CloudAltOffset"] = float(alt_offset_m)
          break

    A   = self.asset
    out = []

    # NIGHT-FLOOR AUTO-READ (W15-S1): the trio's summed sourceless irradiance
    # from THIS SCENE's declared atmosphere, with zero per-scene wiring. The
    # medium rides the scenegraph params as the "SkyAtmosphere" reflected
    # object and is only ever written when declared
    # (_sky_dome.py), so the LAST declareParams carrying the key is the live
    # one (setUserSceneParam is dict-assignment; sub_calls run in order). No
    # declaration = the engine attaches its earth-like default — read the same
    # default here so the deck floor and the rendered sky can never disagree.
    _atmo = None
    _sgdecl = self._systems.get("SceneGraphSystem")
    if _sgdecl is not None:
      for _call, _args, _kw in _sgdecl.sub_calls:
        if _call == "declareParams" and _args and isinstance(_args[0], dict):
          if "SkyAtmosphere" in _args[0]:
            _atmo = _args[0]["SkyAtmosphere"]
    night_floor = night_floor_irradiance(_atmo)

    for ent_name, lkey, texkey in specs:
      spec  = CGI.LAYERS[lkey]
      look  = looks[lkey]
      alt_m = spec["alt_m"] + alt_offset_m
      mtl_kw = dict(
          dsl_class    = CloudLayerMtl,
          tex_res      = 2048.0 if texkey == "cumulus2k" else 1024.0,
          tile_base_m  = spec["tile_base_m"],
          mesh_scale   = spec["base_scale"],
          alt_m        = alt_m,
          sweep_m      = spec["sweep_m"],
          soft         = look["soft"],
          erode        = look["erode"],
          opacity_max  = look["opacity_max"],
          alpha_sigma  = look["alpha_sigma"],
          alpha_floor  = look["alpha_floor"],
          ramp_mix     = look["ramp_mix"],
          dens_pow     = look["dens_pow"],
          blur_tx      = look["blur_tx"],
          blur_px      = look["blur_px"],
          beer_sigma   = look["beer_sigma"],
          silver_gain  = look["silver_gain"],
          wind_mps     = (spec["wind_dir"][0] * spec["wind_mps"] * CGI.WIND_MULT,
                          spec["wind_dir"][1] * spec["wind_mps"] * CGI.WIND_MULT),
          evo_uv       = (look["evo_uv"][0] * EVO_MULT,
                          look["evo_uv"][1] * EVO_MULT),
          cov_scale    = look["cov_scale"],
          veil_max     = look.get("veil_max", 0.0),
          night_ambient = night_floor,
          sampler_textures = {"CloudTex": CLOUD_TEX[texkey]})
      if sun_dir is not None:
        mtl_kw["sun_dir"] = sun_dir
      if colors:
        mtl_kw.update(colors)
      mtl = A.Ptex3d(ent_name + "_mtl", **mtl_kw)
      # dome sag: the far rim descends DROP_FRAC of the deck's height OVER THE
      # SAG DATUM, so it approaches (but never crosses) that level.
      plane = A.CloudShellMesh(ent_name + "_mesh",
                               extent_m = PLANE_LOCAL_M,
                               grid     = 24,
                               drop     = DROP_FRAC * (alt_m - sag_datum_m) / spec["base_scale"],
                               material = mtl)

      # node on the deck layer, NO auto depth-prepass (a translucent sky card
      # must never z-occlude the sky/other decks) — hence the manual
      # declareNodeOnLayer instead of SG.component(nodes=...).
      sgc = self.declare_component("SceneGraphComponent")
      sgc.sub_calls.append(("declareNodeOnLayer", (), {
          "name":                ent_name + "_node",
          "drawable":            plane.built,
          "layer":               layer,
          "drawable_asset_name": ent_name + "_mesh",
          "skip_auto_dpp":       True}))

      # ...and the same deck on the COOKIE layer. No compositing pass names
      # this layer, so it is invisible in the frame; the forward node draws it
      # once per frame from the sun's ortho camera to fill the transmittance
      # cookie. skip_auto_dpp for the same reason as above.
      if cookie_layer:
        sgc.sub_calls.append(("declareNodeOnLayer", (), {
            "name":                ent_name + "_cookienode",
            "drawable":            plane.built,
            "layer":               cookie_layer,
            "drawable_asset_name": ent_name + "_mesh",
            "skip_auto_dpp":       True}))

      # initial transform = the same state math the input script applies live. An
      # EMPTY sky parks every plane rather than drawing four fully-transparent shells
      # (the procedural-sky default is cover 0, so this is the common case).
      vis = (not CGI.decks_clear(thresh)) and CGI.layer_visible(
          CGI.plane_key(ent_name), st["res"], st["mode"])
      y   = (CGI.layer_y(spec, thresh) + alt_offset_m) if vis else CGI.HIDE_Y
      s   = spec["base_scale"] * tile_mult if vis else CGI.HIDE_SCALE
      out.append(self.entity(ent_name,
                             transform  = Transform(translation=vec3(0.0, y, 0.0), scale=s),
                             components = [sgc]))
    return out
