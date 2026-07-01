###############################################################################
# mtl_leopard.py — procedural volumetric Leopard skin on a sphere (GEOV2 ptex3d).
#
# The pattern is a 3D function read at the surface (no triplanar); every marking
# edge is analytic-AA. All knobs are runtime params.
#
#   ork.scene.viewer.py mtl_leopard
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Leopard

lev2_pyexdir.addToSysPath()


class LeopardScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("leopard_mtl",
                   dsl_class=Leopard)
    drw = A.IcoSphere("leopard_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("leopard",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
