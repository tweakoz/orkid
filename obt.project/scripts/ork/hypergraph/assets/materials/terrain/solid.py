###############################################################################
# white — a pure white matte surface (no texture, no color classification).
#
# Shade a terrain with NOTHING the albedo can distract from, so the raw SHAPE
# reads under lighting alone. Used by the noise-primitive example assets
# (perlin / simplex / worleyf1 / voronoi).
#
#   from ork.hypergraph.assets.materials import White
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, rgb
from orkengine.core import vec3


class Solid(Ptex3d):
  """Pure white matte (albedo = 1,1,1; no samplers). `roughness` is the only visible knob.
  `height_scale` is accepted-but-unused: the terrain asset wrapper forwards it to every
  terrain material's ctor (elevation-based materials need it); pure white ignores it."""
  def __init__(self, ctx, *, 
               height_scale=2000.0, 
               albedo = vec3(1),
               metallic = 0.0,
               roughness=1.0,
               instance_variation=0.0 ):
    # E.4 — the first frg_clr consumer: instanced hypermeshes carry the E.2
    # typed per-instance attrs in Cd (x=type_id, y=seed01). variation v scales
    # albedo by (1-v .. 1+v) per instance — placed copies stop looking cloned.
    # 0.0 (default) = byte-identical generated shader for existing users.
    if instance_variation > 0.0:
      v = float(instance_variation)
      albedo = albedo * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    self.surface( albedo=albedo, 
                  metallic=metallic, 
                  roughness=roughness )


__all__ = ["Solid"]
