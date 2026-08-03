###############################################################################
# Glass — window glazing via ALPHA-TO-COVERAGE (the ratified transparent path:
# surface(opacity=, alpha_to_coverage=True) — order-independent, VR-safe, needs
# an MSAA RtGroup, e.g. scenegraph(msaa=2) / viewer --msaa 2). Constant-opacity
# A2C dithers the coverage mask per sample; per-instance Cd.y varies brightness
# so scattered buildings' glazing doesn't read cloned.
#
#   from ork.hypergraph.assets.materials.glass import Glass
#   glass = self.asset.Ptex3d("glass", dsl_class=Glass,
#                             vertex_source=GpuMeshRenderSource(instanced=True))
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from orkengine.core import vec3


class Glass(Ptex3d):
  def __init__(self, ctx, *,
               height_scale=2000.0,                 # terrain-wrapper compat; unused
               tint=vec3(0.55, 0.66, 0.72),         # cool glazing tint
               opacity=0.45,                        # coverage fraction (0 clear .. 1 solid)
               roughness=0.06,
               metallic=0.0,
               instance_variation=0.15):
    albedo = tint
    if instance_variation > 0.0:                    # per-instance sparkle off the E.2 seed01
      v = float(instance_variation)
      albedo = albedo * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    self.surface(albedo=albedo,
                 metallic=metallic,
                 roughness=roughness,
                 opacity=opacity,
                 alpha_to_coverage=True)


__all__ = ["Glass"]
