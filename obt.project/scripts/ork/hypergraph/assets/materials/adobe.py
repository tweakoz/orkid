###############################################################################
# Adobe — earthen mud-plaster ptex3d surface for the scn_swest pueblo family.
#
# Adobe is LOCAL DIRT: the palette deliberately sits in the SAME tan/ochre hue
# family as the swestvale/erodeflow ground strata (buildings and terrain sharing
# one palette is the most authentic single move for the pueblo look target).
#
# OBJECT-SPACE, meter-calibrated per the terrain/ground.py discipline:
#   PATCH band  — mud-coat tonal patchiness over `patch_m` features (~2 m: the
#                 scale of one replastering coat / one dry-season stain)
#   GRAIN band  — near-field straw-and-sand fleck over `grain_m` (~0.25 m)
#   STREAK band — vertically-stretched fbm = rain-wash streaking down the walls
#   CRACKS      — hairline mud-crack wedges (the shared _cracked_mud_fields
#                 voronoi, albedo-darkening only; no displacement in v1 — bump
#                 on the instanced vertex source is untested, see report)
#
# Domains ride domain_xf (runtime mtx rows) — rescale live, no recompile (A8).
# `instance_variation` = per-placed-block tint drift via ctx.Cd.y (the E.2 seed
# channel) so scattered room-blocks stop looking cloned.
# Roughness ~0.93 — must exceed ~0.75 to read matte (PBR nonlinearity law).
#
#   from ork.hypergraph.assets.materials.adobe import Adobe
#   mat = self.asset.Ptex3d("wall", dsl_class=Adobe, instance_variation=0.14)
###############################################################################
from orkengine.core import vec3, quat, mtx4
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_xf, section_normal


def _scale_xf(s):
  return mtx4.composed(vec3(0, 0, 0), quat(), vec3(s, s, s))


class Adobe(Ptex3d):
  def __init__(self, ctx, *,
               albedo_lo=vec3(0.44, 0.33, 0.215),   # damp/shadowed mud
               albedo_hi=vec3(0.60, 0.47, 0.315),   # sun-dried plaster
               patch_m=2.2,                         # replaster-coat feature scale (meters)
               grain_m=0.26,                        # straw/sand fleck scale (meters)
               streak=0.10,                         # rain-wash streak depth (0..~0.3)
               patch_oct=3, grain_oct=5, streak_oct=3,  # fbm octave budgets (A8: loop
                                                        # RESTORED 07-22: owner wants finer
                                                        # texture read; 300+ fps headroom pays
                                                    # counts are data) — trimmed 07-22:
                                                    # walls fill whole frames in the
                                                    # terraced villages and per-pixel
                                                    # fbm ALU was the gauge's top cost
               crack_vis=0.0,                       # hairline crack darkening (0..1) — DEFAULT OFF:
                                                    # v1 rendered as a dominant multi-meter voronoi
                                                    # web on walls (owner-caught 07-22, "cracked mud
                                                    # on buildings"); plaster walls are smooth — the
                                                    # mud-crack vocabulary belongs to the playa GROUND
               cell_scale=1.4,                      # crack plates per meter-ish
               basal_dust=0.55,                     # B3 talus dust: ground-toned gradient
                                                    # over the battered wall base (0..1)
               roughness=0.93,
               instance_variation=0.0,
               aa=1.0):
    p_patch = domain_xf(ctx, ctx.P_object, "adb_patch", _scale_xf(1.0 / float(patch_m)))
    p_grain = domain_xf(ctx, ctx.P_object, "adb_grain", _scale_xf(1.0 / float(grain_m)))
    t = P.fbm_aa(p_patch, octaves=int(patch_oct), aa=aa)   # ~[0,1] coat patchiness
    d = P.fbm_aa(p_grain, octaves=int(grain_oct), aa=aa)   # ~[0,1] fleck field

    alo = ctx.param("albedo_lo", albedo_lo)
    ahi = ctx.param("albedo_hi", albedo_hi)
    stk = ctx.param("streak", float(streak))
    cv  = ctx.param("crack_vis", float(crack_vis))

    alb = P.mix(alo, ahi, t)
    alb = alb * (0.88 + d * 0.24)                    # fleck brightness, centered ~1

    # rain-wash: fbm compressed vertically -> horizontal-banded vertical streaks
    sf  = P.fbm_aa(ctx.P_object * P.vec3(0.55, 3.2, 0.55), octaves=int(streak_oct), aa=aa)
    alb = alb * (1.0 - stk + sf * (2.0 * stk))

    # B3 BASAL BAND (seating round 07-22): dustier GROUND-TONED gradient over the
    # battered wall base — the talus melts the building into the trampled earth.
    # Instanced ctx.P_object is WORLD-space (the filed seam), so a height-above-
    # base band cannot compose yet; the batter's own NORMALS can: N.y picks the
    # talus slope (plumb walls ~0, roof decks ~1 — both outside the band). Tone
    # matches the terrain's packed-earth blend so wall base and plaza floor meet
    # in one hue.
    bd  = ctx.param("basal_dust", float(basal_dust))
    tal = P.smoothstep(0.18, 0.45, ctx.N.y) * (1.0 - P.smoothstep(0.80, 0.92, ctx.N.y))
    alb = P.mix(alb, P.vec3(0.575, 0.465, 0.335) * (0.90 + 0.20 * d), tal * bd)

    # OPTIONAL hairline mud cracks (bake-time gate: crack_vis == 0 emits NO
    # voronoi code — the safe default; see the ctor note)
    if float(crack_vis) > 0.0:
      mudf = self._cracked_mud_fields(ctx, cell_scale=float(cell_scale),
                                      base_color=(1.0, 1.0, 1.0), crack=0.035,
                                      warp=0.55, bump_scale=0.0,
                                      crack_color=(1.0, 1.0, 1.0), aa=aa)
      alb = alb * P.mix(1.0 - 0.5 * cv, 1.0, mudf["plate"])

    # E.4 frg_clr: per-instance tint drift (Cd.y = seed01) — the Solid convention
    if instance_variation > 0.0:
      v   = float(instance_variation)
      alb = alb * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)

    rgh = ctx.param("roughness", float(roughness)) * (0.96 + t * 0.04)
    self.surface(albedo=alb, metallic=0.0, roughness=rgh)


###############################################################################
# AdobeStored — O3 STORED-MODE capture variant of Adobe (the proc class above is
# byte-untouched). Same grain, but BAKED per section into a texture array:
#   * capture SectionAlbedo (albedo.xyz) + SectionParams (roughness, metallic, ao)
#     — the schema the SectionArrayPBR sampler reconstructs from.
#   * surface_stored() reconstructs from those arrays (also makes it standalone-valid;
#     in the dual-role bake map only its captures are baked, the sampler is drawn).
#
# instance_variation DISPOSITION: it is PER-INSTANCE (ctx.Cd.y, the E.2 seed) and
# CANNOT bake into a section-shared array. Cheapest honest option: bake WITHOUT it
# (force 0 into the proc) and re-apply the tint drift LIVE in the forward on the
# SAMPLED albedo — the base grain is baked, the per-block drift stays a forward
# multiply. (Flagged for owner look-review: the drift scales the baked albedo, not
# the live proc albedo — visually a uniform per-instance tint, near-identical.)
###############################################################################

class AdobeStored(Adobe):
  PTEX_MODE    = "stored"
  PTEX_CAPTURE = True
  ALBEDO = "SectionAlbedo"
  PARAMS = "SectionParams"
  NORMAL = "SectionNormal"

  def __init__(self, ctx, *, instance_variation=0.0, **kw):
    self._sctx = ctx
    self._iv   = float(instance_variation)
    kw.pop("instance_variation", None)
    super().__init__(ctx, instance_variation=0.0, **kw)  # bake WITHOUT per-instance drift

  def surface(self, *, albedo, metallic=0.0, roughness, **rest):
    ctx = self._sctx
    self.capture(self.ALBEDO, albedo)
    self.capture(self.PARAMS, P.vec3(roughness, metallic, 1.0))   # x=rough y=metal z=ao
    # SECTION NORMAL (target 2): the FAITHFUL tangent-space shading normal (n*0.5+0.5). Adobe v1 authors
    # no normal detail (no displace/normal channel), so the shading normal IS the geometric normal ctx.N ->
    # transpose(tbn)*N -> flat (0.5,0.5,1.0) — parity-safe. A future relief-authoring variant would pass a
    # perturbed world normal (functions.world_bump_normal / section_normal_from_albedo) instead.
    self.capture(self.NORMAL, section_normal(ctx, ctx.N))
    alb = ctx.texArray(self.ALBEDO, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    if self._iv > 0.0:                                            # per-instance drift, LIVE (not baked)
      v   = self._iv
      alb = alb * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    par = ctx.texArray(self.PARAMS, uv=ctx.uv, layer=ctx.layer, default=0.0)
    self.surface_stored(albedo=alb, roughness=par.x, metallic=par.y, ao=par.z)


__all__ = ["Adobe", "AdobeStored"]
