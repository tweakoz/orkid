###############################################################################
# SdfBoolean — E.7/M2 demo: the full SDF boolean chain, rendered as a real,
# ANIMATED hypermesh. A box and a sphere become dense SDF bricks; the sphere is
# CSG-subtracted from the box on the GPU; marching tetrahedra turns the result
# into an INDEXED GpuMesh that the standard triangulator renders. The sphere
# slides back and forth on X (sine), carving a moving bite — re-voxelized,
# re-CSG'd and re-meshed every frame, all GPU-resident.
#
#   box      sdf_eval ┐
#                     ├─ csg(subtract) ─ sdf_to_mesh ─ (render)
#   sphere   sdf_eval ┘   (sphere.offset animated each frame)
#
# RESOLUTION is the marching grid (the SDF brick dim — sdf_to_mesh meshes the
# brick's cells, so this IS the mesh resolution). Raise it for finer geometry
# (cost rises ~dim³ per frame, since the whole chain re-evaluates while animated).
#
#   ./ork.hypermesh.viewer.py sdf_boolean
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.dflow import sdf as S
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.core import vec3


class SdfBoolean(Hypermesh):
  MATERIAL_CLASS = Solid
  RESOLUTION = 256     # SDF brick dim = the marching-tets mesh resolution (bump for finer geometry)
  EXTENT     = 5.0    # world cube the brick covers (must contain the box + the sphere's sweep)
  AMPLITUDE  = 1.2   # sphere X travel (both directions)
  SPEED      = 0.2    # radians/sec of the sine

  def __init__(self):
    super().__init__()
    s            = self.sdf(dim=self.RESOLUTION, extent=self.EXTENT)
    box          = s.box(size=(1.0, 1.0, 1.0))
    self._sphere = s.sphere(0.90)
    self._sphere2 = s.sphere(0.90)
    self._sphere3 = s.sphere(0.90)
    #n = box
    n            = box - self._sphere
    n            = n | self._sphere2
    n            = n - self._sphere3
    n = n.to_mesh(weld=True)    # MeshSort shared-vertex dedup (indexed manifold). NOTE: it's a 32-pass
                                 # radix sort over every corner EACH frame here (animated) — lower
                                 # RESOLUTION if it gets heavy; weld=False falls back to the (already
                                 # watertight, smooth-shaded) soup.
    #n = self.face_normals(n)                                    # displace moves P only; refresh shading
    #n = self.smooth_normals(n)                                    # displace moves P only; refresh shading
    #n = n.temporalSmooth(alpha=0.01)
    self.output(n)

  def onUpdate(self, updinfo):
    # poke ONLY the sphere's runtime offset plug; the whole chain re-evaluates on
    # the GPU next frame (no recompile). sine -> slides both ways across the box.
    x = math.sin(updinfo.absolutetime * self.SPEED) * self.AMPLITUDE
    y = math.sin(updinfo.absolutetime * self.SPEED*1.7) * self.AMPLITUDE
    z = math.sin(updinfo.absolutetime * self.SPEED*2.1) * self.AMPLITUDE
    self._sphere.inputs.offset = vec3(x, 0.75, 0.0)
    self._sphere2.inputs.offset = vec3(0.0, y, 0.0)
    self._sphere3.inputs.offset = vec3(0.0, 0, z)
