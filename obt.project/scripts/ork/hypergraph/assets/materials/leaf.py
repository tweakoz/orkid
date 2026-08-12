###############################################################################
# LeafProc / LeafCard — the PROCEDURAL broadleaf material, in two forms sharing
# ONE look definition (`leaf_fields`, the card-bake contract — see
# ptex3d/card_bake.py):
#
#   leaf_fields  the broadleaf blade — serrated/lobed silhouette, pinnate
#                secondary-vein network, fine lamina reticulation, petiole→tip
#                gradient, plus a TRANSLUCENCY mask and a cavity/AO field —
#                written against the shared P-op protocol so the SAME BODY
#                drives both the live GLSL emission and the numpy card bake
#                (zero drift; editing it re-keys the bake cache).
#   LeafProc     LIVE analytic form (per-pixel ALU; the look-tuning surface and
#                the reference).
#   LeafCard     STORED form: samples the baked card texture (r=shade,
#                g=translucency, b=cavity, a=silhouette — card.v2 layout) —
#                per-pixel cost is one texture fetch. Tint, the per-leaf Cd.y
#                brightness and the sun backlight stay LIVE (never baked). Bind
#                the bake via sampler_textures={"CardTex": bake_card(leaf_fields,
#                res=512, params=LEAF_PRESETS[...])}.
#
# The recipe (industry-standard leaf-atlas authoring): silhouette = margin
# half-width function modulated by lobing + a serration triangle wave; veins =
# midrib + a pinnate secondary family in sheared (v - |u|·slope) coordinates
# (veins darker AND less translucent, inter-vein lamina MORE translucent);
# hi-freq = a fine sine-lattice reticulation. The stored consumers then apply
# the classic real-time vegetation TRANSMISSION term: sun-wrap backlight =
# translucency · (wrap + (1-wrap)·⟨V·sunTravel⟩^p), i.e. the transmitted lobe
# continues along the sun travel direction and is strongest when that
# continuation reaches the eye. All backlight/tint tweakables are ctx.param
# (live-rebindable), never baked constants.
#
# SPECIES ARE DATA: LEAF_PRESETS parameter sets — each re-keys the bake cache
# into its own card. No species zoo in code.
#
# Both forms render ALPHA-TO-COVERAGE (two-sided, order-independent — needs an
# MSAA RtGroup), hard 0.5 discard, and alpha_cutout=0.5 for the masked depth
# prepass (leaf holes neither z-occlude nor cast solid-card shadows). Card
# geometry comes from Hypermesh.leaves(); u = width (symmetric about 0.5),
# v = length (0 petiole .. 1 tip). Per-leaf brightness varies off Cd.y.
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from orkengine.core import vec3

TWO_PI = 6.2831853


def leaf_fields(P, u, v, ramp,
                width=0.44,        # peak half-width of the blade (u units)
                base_skew=0.62,    # <1 skews the widest point toward the base
                serr_freq=15.0,    # marginal serrations along the blade length
                serr_amp=0.018,    # serration tooth depth (u units)
                lobes=0.0,         # periodic margin lobing count (0 = entire margin)
                lobe_amp=0.0,      # lobe pinch depth (fraction of half-width)
                veins=8.0,         # secondary (pinnate) vein pair count
                vein_slope=0.55,   # vein angle: how far toward the tip they sweep
                vein_w=0.035,      # secondary vein core width (vein-period units)
                ret_freq=88.0,     # fine lamina reticulation frequency (hi-freq detail)
                ret_amp=0.14,      # reticulation shade amplitude
                edge_soft=0.0025): # silhouette AA half-band (u units; mips own the rest)
  """Broadleaf blade fields (both realizations). Returns the card.v2 tuple
  (alpha, shade, transl, cavity), each 0..1:
    shade  albedo structure — veins dark, lamina billows bright, tip deepens
    transl thin-blade translucency — lamina high, veins/petiole block
    cavity AO — vein grooves + crowded blade base"""
  half = P.abs(u - 0.5)                          # 0 at midrib .. 0.5 at card edge
  pv   = 0.085                                   # petiole length in v
  vb   = P.saturate((v - pv) / (1.0 - pv))       # blade-relative length coord
  # ---- silhouette: half-width profile, lobed then serrated ----
  hw   = width * P.sin(P.pow(vb, base_skew) * 3.14159265)
  hw   = hw * (1.0 - lobe_amp * (0.5 + 0.5 * P.cos(vb * lobes * TWO_PI)))
  tri  = P.abs(P.fract(vb * serr_freq) - 0.5) * 2.0 - 0.5      # -0.5..0.5 teeth
  hw   = hw + serr_amp * tri * P.sin(vb * 3.14159265)          # fade teeth at base+tip
  blade = 1.0 - ramp(half, hw - edge_soft, hw + edge_soft)
  # petiole: thin stem below the blade, fading into the midrib
  pet  = (1.0 - ramp(half, 0.006, 0.013)) * (1.0 - ramp(v, pv + 0.02, pv + 0.05))
  alpha = P.max(blade, pet)
  pet_only = pet * (1.0 - blade)
  # ---- vein network: midrib + pinnate secondary family ----
  mid  = 1.0 - ramp(half, 0.0, 0.030 - 0.018 * vb)             # midrib, tapering tipward
  s    = (vb - half * vein_slope) * veins                      # sheared vein coordinate
  vd   = P.abs(P.fract(s) - 0.5)                               # 0 at a vein center
  sec  = 1.0 - ramp(vd, vein_w, vein_w * 2.4)                  # secondary vein mask
  rel  = half / (hw + 0.02)
  sec  = sec * (1.0 - ramp(rel, 0.85, 1.0))                    # veins stop at the margin
  sec  = sec * ramp(vb, 0.0, 0.06)                             # not below the blade base
  bil  = ramp(vd, vein_w * 2.0, 0.42)                          # 0 on vein .. 1 mid-cell
  # fine lamina reticulation (tertiary vein lattice — the hi-freq content)
  ret  = (P.sin(u * ret_freq * TWO_PI + 2.7 * P.sin(vb * ret_freq * 2.3))
          * P.sin(vb * ret_freq * 5.217 + 2.1 * P.sin(u * ret_freq * 1.7)))
  # ---- shade: veins dark, lamina billows, petiole→tip gradient ----
  sh   = P.mix(1.0, 0.58, mid)
  sh   = sh * P.mix(1.0, 0.80, sec)
  sh   = sh * P.mix(0.95, 1.045, bil)
  sh   = sh * (1.0 + ret_amp * 0.5 * ret)
  sh   = sh * P.mix(1.02, 0.86, vb)
  sh   = P.mix(sh, 0.42, pet_only)                             # woody petiole
  shade = P.saturate(sh * 0.94)
  # ---- translucency: lamina passes light, veins/petiole/base block ----
  tr   = 0.60 + 0.28 * bil
  tr   = tr * P.mix(1.0, 0.22, sec)
  tr   = tr * P.mix(1.0, 0.08, mid)
  tr   = tr * P.mix(0.60, 1.0, vb)                             # thick base, thin tip
  transl = P.saturate(P.mix(tr, 0.05, pet_only))
  # ---- cavity/AO: vein grooves + crowded base ----
  cav  = P.saturate(1.0 - 0.45 * mid - 0.28 * sec)
  cav  = cav * P.mix(0.80, 1.0, ramp(vb, 0.0, 0.30))
  cavity = P.saturate(P.mix(cav, 0.75, pet_only))
  return alpha, shade, transl, cavity


# species variants are DATA — a param set re-keys the bake cache into its own card
LEAF_PRESETS = {
  # beech-like: entire-to-finely-serrate ovate blade, dense straight vein pairs
  "beech": dict(width=0.40, base_skew=0.70, serr_freq=15.0, serr_amp=0.014,
                lobes=0.0, lobe_amp=0.0, veins=9.0, vein_slope=0.62),
  # birch-like: broader blade, coarse double-serrate margin, fewer veins
  "birch": dict(width=0.45, base_skew=0.52, serr_freq=23.0, serr_amp=0.026,
                lobes=0.0, lobe_amp=0.0, veins=7.0, vein_slope=0.48),
  # maple-like: lobed margin, sparse steep veins, coarse teeth
  "maple": dict(width=0.49, base_skew=0.45, serr_freq=9.0, serr_amp=0.030,
                lobes=2.0, lobe_amp=0.32, veins=5.0, vein_slope=0.95),
}


# the injected tail both forms share: hard mask (silhouette holds even without MSAA)
# + the two-sided normal resolve. A card is drawn cull=off, so ONE of its two facings
# must be chosen for lighting — and that choice must be VIEW-INDEPENDENT (owner ruling
# 2026-08-09): gl_FrontFacing is per-view screen winding, so a card near edge-on picks
# opposite normals in the two eyes (and flips under head motion) — one leaf, two
# discrete tones. The SUN side is the world-space equivalent and is what a thin
# translucent blade reads as anyway. sun_dir is the ublk_sun member (xyz = sunlight
# TRAVEL direction, w = has_sun); a pass that does not bind the block (impostor atlas
# capture, masked depth prepass) reads has_sun 0 and keeps the authored normal — which
# is what makes every baked hemi-oct view agree on the normal it stores.
_SURF_TAIL = ("if (s.opacity < 0.5) discard;\n"
              "if ((sun_dir.w > 0.5) && (dot(s.normal, -sun_dir.xyz) < 0.0)) s.normal = -s.normal;")


def foliage_surface(mat, ctx, *, shade, transl, cavity, alpha,
                    albedo, roughness, metallic,
                    lamina_tint=(1.22, 1.07, 0.52), lamina_tint_amt=0.55,
                    transmit_tint=(0.62, 1.05, 0.30),
                    backlit_gain=2.0, backlit_power=6.0, backlit_wrap=0.18):
  """The SHARED foliage surface composition (leaf + needle, live + stored forms
  — one body so the four consumers cannot drift). Inputs are the four card.v2
  fields as SurfNode expressions (analytic or card-sampled) plus the per-type
  tint. Composes:
    * albedo   = tint · Cd.y jitter · shade, hue-varied by the translucency
                 field (translucent lamina warms toward LaminaTint — the card
                 goes color-bearing; the per-type tint is modulation, not the
                 sole color source)
    * ao       = cavity
    * emissive = the sun-wrap BACKLIGHT (thin-blade transmission): the
                 transmitted lobe continues along the sun travel direction,
                 strongest when it continues into the eye —
                 transl · (wrap + (1-wrap)·⟨V·sun⟩^p) · sun radiance,
                 gated by has_sun (impostor capture / prepass bake none of it).
  Every tweakable is a ctx.param (live-rebindable via bindParam), never a baked
  constant."""
  base   = albedo * P.mix(0.82, 1.18, ctx.Cd.y)      # per-leaf brightness variation
  lam_t  = ctx.param("LaminaTint",    lamina_tint)
  lam_a  = ctx.param("LaminaTintAmt", lamina_tint_amt)
  color  = base * shade * P.mix(P.vec3(1.0, 1.0, 1.0), lam_t, transl * lam_a)
  gain   = ctx.param("BacklitGain",  backlit_gain)
  power  = ctx.param("BacklitPower", backlit_power)
  wrap   = ctx.param("BacklitWrap",  backlit_wrap)
  t_tint = ctx.param("TransmitTint", transmit_tint)
  to_eye = P.normalize(ctx.eye - ctx.P)
  vdl    = P.saturate(P.dot(to_eye, ctx.sun_dir))    # sun travel continuing into the eye
  lobe   = wrap + (1.0 - wrap) * P.pow(vdl, power)
  glow   = ctx.has_sun * gain * (1.0 / 3.14159265) * transl * lobe
  emissive = (base * t_tint) * (ctx.sun_color * (ctx.sun_intensity * glow))
  mat.surface(albedo=color, opacity=alpha, emissive=emissive, ao=cavity,
              roughness=roughness, metallic=metallic,
              alpha_to_coverage=True, cull="off", alpha_cutout=0.5)
  mat._surf_body_append = _SURF_TAIL


class LeafProc(Ptex3d):
  """LIVE analytic broadleaf (the original; kept as the tuning/reference form).
  Field params (a LEAF_PRESETS set) pass straight through to leaf_fields."""
  def __init__(self, ctx, *, albedo=vec3(0.24, 0.52, 0.17), roughness=0.55, metallic=0.0,
               **field_params):
    uv = ctx.uv
    alpha, shade, transl, cavity = leaf_fields(
        P, uv.x, uv.y,
        lambda x, e0, e1: self.aa_ramp(x, e0, e1),
        **field_params)
    foliage_surface(self, ctx, shade=shade, transl=transl, cavity=cavity, alpha=alpha,
                    albedo=albedo, roughness=roughness, metallic=metallic)


class LeafCard(Ptex3d):
  """STORED broadleaf: samples the shared baked card (one texture for every
  leaf in the forest; card.v2: r=shade, g=translucency, b=cavity, a=silhouette).
  Tint, per-leaf brightness and the sun backlight applied live."""
  def __init__(self, ctx, *, albedo=vec3(0.24, 0.52, 0.17), roughness=0.55, metallic=0.0):
    card = ctx.tex("CardTex")                      # bind via sampler_textures
    foliage_surface(self, ctx, shade=card.x, transl=card.y, cavity=card.z, alpha=card.w,
                    albedo=albedo, roughness=roughness, metallic=metallic)


__all__ = ["LeafProc", "LeafCard", "leaf_fields", "LEAF_PRESETS", "foliage_surface"]
