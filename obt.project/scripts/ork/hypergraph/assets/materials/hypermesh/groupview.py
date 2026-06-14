###############################################################################
# GroupView — hypermesh SELECTION-GROUP view material.
#
# Shade every FACE by its selection groups: the 5 "working" bits (0..4) of the mesh's uint32 __tags FACE
# channel decode as an RGB+intensity color so a human can read the named groups directly:
#   bit 0 -> R          bit 1 -> G          bit 2 -> B          (1 bit each: the color channel)
#   bits 3..4 -> intensity (2 bits): 0->0.25, 1->0.50, 2->0.75, 3->1.0   (scales the rgb)
# So group 0 = red, group 1 = green, group 2 = blue, groups 3/4 brighten; unselected (no color bit) is
# black. e.g. groups(0,1)=yellow, groups(0,3)=brighter red, groups(0,1,2,3,4)=full white.
#
# Like TopoView it reads the per-triangle source-face-id buffer (sif_triface, TFd[gl_PrimitiveID]) to
# find each fragment's face; it then additionally reads that face's __tags from a second FACE-channel
# buffer (sif_seltags, STd) bound to graphics-storage slot 6 + refreshed each frame by setupMeshRender
# (tag_viz=True). Declared via fragment_storage(): both blocks are inherited by the surface fragment
# and the GLSL is appended AFTER ptex_surface() so it overrides the lit SurfaceOut `s`.
#
# Assign as a hypermesh asset's MATERIAL_CLASS, or force it on any asset with the viewer's --groups.
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d

# the per-triangle face-id buffer (slot 5) + the __tags FACE channel (slot 6), both maintained by
# setupMeshRender(tag_viz=True). STd is indexed by the per-triangle face id (TFd), giving the face's tags.
_TRIFACE_BLOCK = "storage_interface sif_triface (descriptor_set 0) { buffer layout(std430) hm_tfb { uint TFd[]; }; }"
_SELTAGS_BLOCK = "storage_interface sif_seltags (descriptor_set 0) { buffer layout(std430) hm_stb { uint STd[]; }; }"

# FS post (runs after ptex_surface(), overriding `s`): working-bit field -> RGB(bits0..2) * intensity(bits3..4).
# `roughness`/`metallic` are baked in. `emissive` (default True) ALSO self-lights the group color so it reads
# exactly regardless of scene lighting (diagnostic). emissive=False routes the group color to ALBEDO ONLY, so
# metallic/roughness produce a real PBR response (metallic=1/roughness=0 -> a group-colored MIRROR; band 0 ->
# black albedo -> black; brighter bands -> brighter F0). The group color always drives albedo either way.
def _group_fs(roughness, metallic, emissive):
  return (
    "uint _fid  = TFd[gl_PrimitiveID];\n"
    "uint _band = STd[_fid] & 0x1Fu;\n"                                # working bits 0..4
    "vec3  _rgb = vec3(float(_band & 1u), float((_band >> 1u) & 1u), float((_band >> 2u) & 1u));\n"  # bit0/1/2 -> R/G/B
    "float _int = float(((_band >> 3u) & 3u) + 1u) * 0.25;\n"          # bits 3..4 -> 0.25/0.50/0.75/1.0
    "vec3  _gc  = _rgb * _int;\n"                                      # no color bit -> black
    "s.albedo   = _gc;\n"                                             # group color -> albedo (== F0 when metallic)
    + ("s.emissive = _gc;\n" if emissive else "s.emissive = vec3(0.0);\n") +  # self-lit (diag) vs pure PBR
    "s.metallic = " + ("%f" % float(metallic)) + ";\n"
    "s.roughness= " + ("%f" % float(roughness)) + ";\n")


class GroupView(Ptex3d):
  """Every FACE colored by its selection-group working bits (__tags & 0x1F): bits 0/1/2 -> R/G/B,
  bits 3..4 -> intensity (0.25/0.50/0.75/1.0); unselected (no color bit) = black. Reads the per-triangle
  face-id buffer + the __tags FACE channel. `roughness` (default 1) + `metallic` (default 0) tune the PBR
  response. `emissive` (default True) self-lights the group color so it reads exactly under any lighting
  (diagnostic); emissive=False routes the color to ALBEDO ONLY so metallic/roughness actually show (e.g.
  metallic=1/roughness=0 -> group-colored mirror, band 0 -> black). `albedo`/`height_scale` ignored."""

  WANTS_FACE_ID = True   # -> make_drawable wires the per-triangle face-id buffer (slot 5)
  WANTS_TAGS    = True   # -> also bind the __tags FACE channel (slot 6) + setupMeshRender(tag_viz=True)

  def __init__(self, ctx, *, height_scale=1.0, roughness=1.0, albedo=None, metallic=0.0, emissive=True, **kw):
    self.surface(albedo=vec3(0.5), roughness=roughness, metallic=metallic)   # baseline; FS post overrides `s`
    self.fragment_storage(_TRIFACE_BLOCK + "\n" + _SELTAGS_BLOCK,
                          inherits=("sif_triface", "sif_seltags"), append=_group_fs(roughness, metallic, emissive))


__all__ = ["GroupView"]
