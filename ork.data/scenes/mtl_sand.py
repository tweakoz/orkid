###############################################################################
# mtl_sand.py — procedural volumetric Sand on a sphere (GEOV2 ptex3d).
#
# Organic ground material composed from the function library; 3D fields read at
# the surface (no triplanar). All variation knobs are runtime params.
#
#   ork.scene.viewer.py mtl_sand
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Sand

lev2_pyexdir.addToSysPath()


class SandScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("sand_mtl",
                   dsl_class=Sand)
    drw = A.IcoSphere("sand_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("sand",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
