###############################################################################
# baked — STORED-mode reconstruction material for the terrain proctex texture-bake.
#
# Samples the baked PBR atlas (Albedo / Normal+ao / MetalRough) that a proctex's
# FWD_SSBO_CUSTOM_CAPTURE technique produced over the planar terrain UV (see
# terrain_chunk_drawable.cpp terrainTexBake). GENERAL-PURPOSE: any terrain authored with
# self.terrain(mode="stored") whose atlas is cached renders through THIS instead of the live
# proctex — lit through the SAME forward PBR. The baked normal is WORLD-space (the bake stored
# worldNormal*0.5+0.5), so it is decoded and handed straight to lighting (same as the impostor
# reconstruction). The three samplers are bound from the deterministic cache path at author time
# (Ptex3d(sampler_textures=...)); the geometry comes from the shared TerrainChunkVertexSource.
#
#   from ork.hypergraph.assets.materials.terrain.baked import BakedTerrain
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P


class BakedTerrain(Ptex3d):
  # samplers (bound from the cache via sampler_textures): Albedo.rgb / Normal.xyz(+ao) / MetalRough.rg
  def __init__(self, ctx, *, height_scale=2000.0):   # height_scale forwarded by the terrain wrapper; unused
    uv  = ctx.uv
    alb = ctx.tex("Albedo", uv).xyz
    nrm = P.normalize(ctx.tex("Normal", uv).xyz * 2.0 - 1.0)   # decode WORLD-space normal
    mr  = ctx.tex("MetalRough", uv)
    self.surface(albedo=alb, normal=nrm, metallic=mr.x, roughness=mr.y, ao=1.0)


__all__ = ["BakedTerrain"]
