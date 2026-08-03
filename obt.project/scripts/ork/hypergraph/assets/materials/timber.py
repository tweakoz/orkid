###############################################################################
# Timber — weathered exterior wood for the scn_swest pueblo family: vigas,
# lintels, ladders (gid 4 TRIM) and door slabs / dark opening recesses (gid 2).
#
# Built on the shared volumetric wood_grain (materials/wood.py lineage) with a
# WEATHERING layer: decades of UV silver the surface toward gray, patchily
# (broad fbm mask), while the ring/grain structure stays legible underneath —
# the classic sun-bleached cottonwood/pine read of pueblo roof beams.
#
# Grain axis default = +Z (the viga long axis in the _pueblo recipe). Roughness
# high (matte weathered wood, > the 0.75 matte threshold). No displacement in
# v1 (instanced vertex source; see adobe.py note). `instance_variation` = the
# per-instance Cd.y tint drift (Solid convention).
#
#   from ork.hypergraph.assets.materials.timber import Timber
#   mat = self.asset.Ptex3d("trim", dsl_class=Timber, instance_variation=0.18)
#   # door-slab preset: darker + less silvered
#   mat = self.asset.Ptex3d("door", dsl_class=Timber,
#                           early_color=vec3(0.24, 0.17, 0.11),
#                           late_color=vec3(0.13, 0.095, 0.07),
#                           silver=0.12, roughness=0.9)
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import wood_grain, section_normal


class Timber(Ptex3d):
  def __init__(self, ctx, *,
               scale=1.0,
               axis=vec3(0.0, 0.0, 1.0),            # viga long axis
               early_color=vec3(0.46, 0.37, 0.27),
               late_color=vec3(0.28, 0.215, 0.155),
               silver=0.45,                         # weathered-gray takeover (0..1)
               ring_freq=9.0, ring_warp=0.55, warp_freq=1.3, ring_contrast=2.0,
               grain=0.22, grain_freq=34.0, octaves=4,
               roughness=0.82, rough_grain=0.12,
               instance_variation=0.0,
               aa=1.0):
    scl   = ctx.param("scale",         float(scale))
    ax    = ctx.param("axis",          axis)
    rfreq = ctx.param("ring_freq",     float(ring_freq))
    rwarp = ctx.param("ring_warp",     float(ring_warp))
    wfreq = ctx.param("warp_freq",     float(warp_freq))
    rcon  = ctx.param("ring_contrast", float(ring_contrast))
    gfreq = ctx.param("grain_freq",    float(grain_freq))
    grn   = ctx.param("grain",         float(grain))
    early = ctx.param("early_color",   early_color)
    late  = ctx.param("late_color",    late_color)
    sil   = ctx.param("silver",        float(silver))
    rough = ctx.param("roughness",     float(roughness))
    rgrn  = ctx.param("rough_grain",   float(rough_grain))

    p = ctx.P_object * scl
    w = wood_grain(p, ax, rfreq, rwarp, wfreq, int(octaves),
                   grain_freq=gfreq, ring_contrast=rcon)

    col = P.mix(early, late, w.band)
    col = col * P.mix(1.0 - grn, 1.0 + grn, w.grain)

    # weathering: patchy silver-gray takeover; grain streaks keep it woody
    wx   = P.fbm_aa(ctx.P_object * 0.8, 3, aa=aa)
    gray = P.vec3(0.42, 0.41, 0.385) * P.mix(0.85, 1.10, w.grain)
    col  = P.mix(col, gray, P.saturate(sil * (0.45 + wx * 0.9)))

    if instance_variation > 0.0:
      v   = float(instance_variation)
      col = col * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)

    self.surface(albedo=col,
                 metallic=0.0,
                 roughness=P.mix(rough, rough + rgrn, w.band))


###############################################################################
# TimberStored — O3 STORED-MODE capture variant of Timber (the proc class above is
# byte-untouched). Same weathered-wood grain, BAKED per section into a texture array
# with the SectionAlbedo / SectionParams schema, reconstructed via surface_stored().
# instance_variation is per-instance (not bakeable) -> re-applied LIVE in the forward
# on the sampled albedo (see the AdobeStored disposition note).
###############################################################################

class TimberStored(Timber):
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
    # SECTION NORMAL (target 2): the FAITHFUL tangent-space shading normal (n*0.5+0.5). Timber v1 authors no
    # normal detail, so the shading normal IS the geometric normal ctx.N -> flat (0.5,0.5,1.0) — parity-safe.
    # Schema matches AdobeStored / the SectionArrayPBR sampler (a relief variant would perturb the world normal).
    self.capture(self.NORMAL, section_normal(ctx, ctx.N))
    alb = ctx.texArray(self.ALBEDO, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    if self._iv > 0.0:                                            # per-instance drift, LIVE (not baked)
      v   = self._iv
      alb = alb * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    par = ctx.texArray(self.PARAMS, uv=ctx.uv, layer=ctx.layer, default=0.0)
    self.surface_stored(albedo=alb, roughness=par.x, metallic=par.y, ao=par.z)


__all__ = ["Timber", "TimberStored"]
