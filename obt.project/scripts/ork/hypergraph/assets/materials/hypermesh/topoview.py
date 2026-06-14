###############################################################################
# TopoView — hypermesh TOPOLOGY view material.
#
# Shade every FACE a distinct color so the mesh's polygon topology reads at a glance (mixed tri/quad/
# ngon all show). Neighboring faces are pushed maximally apart in hue (golden-ratio spread of the
# face id), so adjacent faces never blend.
#
# It reads a per-triangle FACE-ID buffer (sif_triface) filled by the hypermesh render triangulator,
# indexed by gl_PrimitiveID (the FS triangle index). Since every triangle of a face carries the same
# id, the whole face shades as one flat color. Declared via fragment_storage(): the FS storage block
# is inherited by the surface fragment, and the GLSL is appended AFTER ptex_surface() so it overrides
# the lit SurfaceOut `s` (gl_PrimitiveID is a fragment-stage builtin, only valid in the FS main).
#
# Assign as a hypermesh asset's MATERIAL_CLASS (like terrain materials), or force it on any asset with
# the viewer's --faces.
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d

# the per-triangle face-id buffer (filled by setupMeshRender's triangulator; bound to graphics-storage 5).
_TRIFACE_BLOCK = "storage_interface sif_triface (descriptor_set 0) { buffer layout(std430) hm_tfb { uint TFd[]; }; }"

# FS post (runs after ptex_surface(), overriding `s`): face id -> distinct golden-ratio-hue color.
_TOPO_FS = (
  "uint  _fid = TFd[gl_PrimitiveID];\n"
  "float _h   = fract(float(_fid) * 0.61803399 + 0.12);\n"            # golden-ratio hue -> max neighbor contrast
  "vec3  _fc  = clamp(abs(fract(_h + vec3(0.0, 0.66667, 0.33333)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);\n"  # hue -> rgb
  "float _val = 0.6 + 0.4 * fract(float(_fid) * 0.31830989);\n"       # per-face value variation
  "s.albedo   = _fc * _val;\n"
  "s.emissive = _fc * 0.10;\n"                                        # gentle self-lit so faces read even unlit
  "s.metallic = 0.0;\n"
  "s.roughness= 0.8;\n")


class TopoView(Ptex3d):
  """Every FACE a distinct color (topology inspection). Reads the per-triangle face-id buffer via
  gl_PrimitiveID. `height_scale`/`albedo`/`roughness` are accepted-but-ignored (the FS post sets the
  surface), so the asset wrapper can forward its usual material params."""

  WANTS_FACE_ID = True   # -> make_drawable wires the per-triangle face-id buffer + binds sif_triface

  def __init__(self, ctx, *, height_scale=1.0, roughness=0.8, albedo=None, metallic=0.0, **kw):
    self.surface(albedo=vec3(0.5), roughness=roughness)   # baseline; the FS post overrides `s`
    self.fragment_storage(_TRIFACE_BLOCK, inherits=("sif_triface",), append=_TOPO_FS)


__all__ = ["TopoView"]
