###############################################################################
# mtl_zebra.py — procedural volumetric Zebra skin on a sphere (GEOV2 ptex3d).
#
# The pattern is a 3D function read at the surface (no triplanar); every marking
# edge is analytic-AA. All knobs are runtime params.
#
#   ork.scene.viewer.py mtl_zebra
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Zebra

lev2_pyexdir.addToSysPath()


class ZebraScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("zebra_mtl",
                   dsl_class=Zebra)
    drw = A.IcoSphere("zebra_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("zebra",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
