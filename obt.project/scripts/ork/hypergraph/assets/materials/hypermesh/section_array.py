###############################################################################
# SectionViz / SectionArray — the O3 per-section texture-ARRAY materials.
#
# SectionUnwrap (the hypermesh op) splits a gid-partitioned mesh into SECTIONS,
# unwraps each into its own 0-1 UV domain, and writes each section's dense LAYER
# index into UV0.z. These two materials consume that:
#
#   SectionViz   — colors each fragment by its section LAYER (ctx.layer). No texture;
#                  proves the unwrap+layer path reads correctly (distinct color per
#                  section) under the plain forward pipeline.
#   SectionArray — the O3 BAKED path (stage 2): a real ptex3d capture material. surface()
#                  is not needed — instead it declares a PROCEDURAL grain via self.capture()
#                  (baked per section into that section's texture-array LAYER by the GPU bake
#                  driver, hmdflow_render.cpp::prepareSectionBake) and reconstructs the forward
#                  from that array via surface_stored(ctx.texArray(...)). ONE material shades
#                  every section by sampling its OWN array LAYER — no per-gid draw buckets.
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import section_normal


class SectionViz(Ptex3d):
  """Emissive per-section color from ctx.layer (the section's UV0.z layer index). Distinct hue per
  integer layer so the SectionUnwrap partition reads directly. No samplers, no array — the isolation
  test for the unwrap+layer plumbing (deliverables 1+2)."""

  def __init__(self, ctx, *, height_scale=1.0, roughness=0.6, albedo=None, metallic=0.0, **kw):
    l   = ctx.layer
    # a distinct emissive hue per integer layer (phase-shifted sines -> 0.25..1.0 per channel).
    col = P.vec3(0.5 + 0.4 * P.sin(l * 1.7 + 0.0),
                 0.5 + 0.4 * P.sin(l * 1.7 + 2.1),
                 0.5 + 0.4 * P.sin(l * 1.7 + 4.2))
    self.surface(albedo=col, emissive=col, roughness=roughness, metallic=metallic)


class SectionArray(Ptex3d):
  """The O3 baked path (stage 2): a capture material mirroring the terrain proctex pattern. The GPU bake
  driver renders self.capture(...)'s procedural into each section's texture-array LAYER (via the material's
  FWD_SSBO_CUSTOM_CAPTURE technique + the section cap-VS); the forward pipeline reconstructs from that array
  with surface_stored(ctx.texArray(...)) at ctx.layer. Bind the assembled array via
  material.bindParam("SectionAlbedo", texarray). PTEX_MODE/PTEX_CAPTURE tell make_drawable to build it in
  stored+capture mode."""

  ARRAY_SAMPLER = "SectionAlbedo"   # the sampler2DArray uniform the asset binds a TextureArray to
  PTEX_MODE     = "stored"          # forward = surface_stored(); the named-capture technique bakes the array
  PTEX_CAPTURE  = True              # emit FWD_SSBO_CUSTOM_CAPTURE (the bake driver's render technique)

  def __init__(self, ctx, *, height_scale=1.0, roughness=0.6, albedo=None, metallic=0.0, **kw):
    l = ctx.layer
    p = ctx.P_object                                 # object-space position (vec3 — the fbm coordinate)
    # BAKED procedural surface — an Adobe-like grain: multi-octave value-noise fbm over object space plus a
    # high-freq speckle, tinted per section LAYER so each section bakes VISIBLY distinct content into its
    # array layer. (Author demo material — NOT the swest Adobe asset.) The cap-VS rasterizes this into the
    # section's 0-1 UV atlas; a function of ctx.P_object varies coherently across the section faces.
    grain = P.fbm(p * 6.0, 5)                        # coarse detail field (0..1-ish)
    fine  = P.fbm(p * 22.0, 3)                        # high-frequency speckle
    hue   = P.vec3(0.5 + 0.4 * P.sin(l * 1.7 + 0.0),
                   0.5 + 0.4 * P.sin(l * 1.7 + 2.1),
                   0.5 + 0.4 * P.sin(l * 1.7 + 4.2))
    col   = hue * (0.55 + 0.55 * grain) + P.vec3(0.18, 0.18, 0.18) * fine
    # BAKE TARGET: the section's procedural albedo (packs xyz -> MRT out_SectionAlbedo). Same name as the
    # forward array sampler -> the cache file / array association is 1:1 (report §5).
    self.capture(self.ARRAY_SAMPLER, col)
    # FORWARD: sample the baked per-section array at this section's layer (ctx.texArray -> sampler2DArray).
    alb = ctx.texArray(self.ARRAY_SAMPLER, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    self.surface_stored(albedo=alb, roughness=roughness, metallic=metallic)


class SectionArrayPBR(Ptex3d):
  """O3 stage 3 — the MULTI-TARGET stored SAMPLER material for the C++ player-path bake driver. Drawn ONCE
  over the whole gid'd mesh; reconstructs per-section PBR by sampling TWO texture arrays at the section's
  layer (ctx.layer): SectionAlbedo (albedo.xyz) + SectionParams (roughness.x, metallic.y, ao.z). This is
  the `material=` in drawable_data(section_bake=True); the per-gid BAKE MAP (materials={gid: AdobeStored /
  TimberStored}) fills each layer with that gid's material's captured content. Its OWN capture (a neutral
  surface) is the fallback baked for UNBOUND gids (never black) + defines the target schema the bake
  map materials must match. Author demo material — the swest recipe adopts it in the next lane."""

  ALBEDO = "SectionAlbedo"
  PARAMS = "SectionParams"
  NORMAL = "SectionNormal"
  PTEX_MODE    = "stored"
  PTEX_CAPTURE = True

  def __init__(self, ctx, *, roughness=0.7, instance_variation=0.0, **kw):
    # fallback capture (schema + unbound-gid content): a neutral mid surface. Every gate gid is bound, so
    # this is baked only for gids with no bake material (ops self-defend — never a black layer).
    self.capture(self.ALBEDO, P.vec3(0.5, 0.5, 0.5))
    self.capture(self.PARAMS, P.vec3(float(roughness), 0.0, 1.0))   # x=rough y=metal z=ao
    # SECTION NORMAL (target 2, MRT order): the FAITHFUL tangent-space shading normal (n*0.5+0.5). DEFINES the
    # target schema the bake-map materials (AdobeStored/TimberStored) must match. No authored normal detail ->
    # the shading normal is the geometric normal ctx.N -> flat (0.5,0.5,1.0) neutral fallback (unbound-gid).
    self.capture(self.NORMAL, section_normal(ctx, ctx.N))
    # FORWARD: sample both baked per-section arrays at this fragment's section layer.
    alb = ctx.texArray(self.ALBEDO, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    # The bake-map materials (AdobeStored/TimberStored) bake WITHOUT their per-instance tint drift (it is
    # per-instance ctx.Cd.y, unbakeable into a section-shared array). The drawn sampler re-applies it LIVE on
    # the SAMPLED albedo — the disposition the stored materials document. One value per draw (the dominant
    # gid's), so a scattered building's whole shell drifts as a unit. DEFAULT 0.0 emits NO code (the gate's
    # ren_section_bake sampler stays byte-identical).
    if float(instance_variation) > 0.0:
      v   = float(instance_variation)
      alb = alb * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    par = ctx.texArray(self.PARAMS, uv=ctx.uv, layer=ctx.layer, default=0.0)
    self.surface_stored(albedo=alb, roughness=par.x, metallic=par.y, ao=par.z)


__all__ = ["SectionViz", "SectionArray", "SectionArrayPBR"]
