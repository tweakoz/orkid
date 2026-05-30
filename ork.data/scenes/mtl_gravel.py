###############################################################################
# mtl_gravel.py — procedural volumetric Gravel on a sphere (GEOV2 ptex3d).
#
# Organic ground material composed from the function library; 3D fields read at
# the surface (no triplanar). All variation knobs are runtime params.
#
#   ork.scene.viewer.py mtl_gravel
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Gravel

lev2_pyexdir.addToSysPath()


class GravelScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("gravel_mtl",
                   dsl_class=Gravel)
    drw = A.IcoSphere("gravel_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("gravel",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
