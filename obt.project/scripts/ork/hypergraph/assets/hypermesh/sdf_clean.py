###############################################################################
# SdfClean — the SHAPE-AWARE clean-remesh demo (companion to SdfBlocky). A box is
# voxelized to a dense SDF brick, then extracted via sdf_to_mesh_clean: openvdb's
# curvature-ADAPTIVE volumeToMesh (flat faces -> few big polys, detail at curvature)
# instead of marching-tets' uniform ~voxel^2 soup, plus an xatlas UV unwrap for
# texture baking. This is the op the pueblo buildings adopt to turn ~1M-face
# marching-tets massing into clean, low-poly, UV'd meshes.
#
#   box -> mesh_to_sdf(dim) -> sdf_to_mesh_clean(adaptivity, unwrap=True) -> (render)
#
#   ./ork.hypermesh.viewer.py sdf_clean
#   _ork.hypermesh.validate.py -i .../sdf_clean.py -o /tmp/sdf_clean.obj
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.core import vec4


class SdfClean(Hypermesh):
  MATERIAL_CLASS  = Solid
  MATERIAL_PARAMS = {
      "color": vec4(0.70, 0.72, 0.78, 1.0),
      "roughness": 0.55,
      "metallic": 0.0,
  }

  DIM        = 96      # SDF brick resolution
  ADAPTIVITY = 0.5     # 0 = max detail, 1 = flattest
  SIZE       = 1.2     # box edge length
  EXTENT     = 2.4     # brick cube side (contains the box + narrow band)

  def __init__(self):
    super().__init__()
    b = self.box(size=0.5)                                     # unit cube (half-extent 0.5)
    b = self.transform(b, scale=(self.SIZE, self.SIZE, self.SIZE))
    sd = self.mesh_to_sdf(b, dim=self.DIM, extent=self.EXTENT, center=(0.0, 0.0, 0.0))
    self.output(self.sdf_to_mesh_clean(sd, adaptivity=self.ADAPTIVITY, unwrap=True))
