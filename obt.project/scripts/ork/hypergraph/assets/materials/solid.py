###############################################################################
# solid — a plain SOLID-COLOR PBR material (a live lev2.PBRMaterial).
#
# Non-terrain, non-ptex3d: a thin builder for instanced drawables / simple props
# that just need a flat color (e.g. scattered trees). Accepts hsv()/wavelength()/
# colortemp() lazy colors, a vec3, a vec4, or an (r,g,b[,a]) tuple — coercing to the
# rgba vec4 the PBRMaterial.baseColor setter wants (the lazy colors don't auto-coerce
# at a raw pybind setter, so we call .to_vec4() explicitly).
#
#   from ork.hypergraph.assets.materials import Solid
#   mtl = Solid(ctx, hsv(120, 0.5, 0.5))        # green, instancing-ready
###############################################################################
from orkengine.core import vec3, vec4
from orkengine import lev2


def _coerce_rgba(color):
  """hsv()/lazy color -> vec4; vec3 -> vec4(rgb,1); vec4 -> as-is; (r,g,b[,a]) -> vec4."""
  if hasattr(color, "to_vec4"):                 # hypergraph.colors lazy colors (hsv/wavelength/...)
    return color.to_vec4()
  if isinstance(color, vec4):
    return color
  if isinstance(color, vec3):
    return vec4(color.x, color.y, color.z, 1.0)
  c = list(color)
  return vec4(float(c[0]), float(c[1]), float(c[2]), float(c[3]) if len(c) > 3 else 1.0)


def Solid(ctx, color=vec4(1.0, 1.0, 1.0, 1.0), *, roughness=0.9, metallic=0.0,
          instancing_matrices_only=False):
  """Build a live solid-color PBRMaterial (instancing-ready). `color` accepts hsv()/vec3/vec4/
  tuple. `ctx` is the live render Context.

  instancing_matrices_only: when instanced, use the matrices-only dynamic block (FWD_CT_NM_IM_NI_MO)
  — per-instance transforms from a single (count-sizeable) SSBO, NO per-instance color (baseColor
  shows). For instanced drawables that only need transforms (e.g. terrain scatter). Set before gpuInit."""
  white = lev2.Image.createFromFile("src://effect_textures/white.dds")
  nrm   = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
  mtl = lev2.PBRMaterial()
  mtl.assignImages(ctx, color=white, normal=nrm, mtlruf=white, doConform=True)
  mtl.baseColor       = _coerce_rgba(color)
  mtl.roughnessFactor = float(roughness)
  mtl.metallicFactor  = float(metallic)
  mtl.instanceMatricesOnly = bool(instancing_matrices_only)   # before gpuInit (resolves _parInstanceBlock)
  mtl.gpuInit(ctx)
  return mtl


__all__ = ["Solid"]
