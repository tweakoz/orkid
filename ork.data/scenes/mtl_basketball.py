###############################################################################
# mtl_basketball.py — procedural basketball (GEOV2 ptex3d).
#
# Orange pebbled rubber + near-black recessed seams (bump), matte PBR.
#
#   ork.scene.viewer.py mtl_basketball
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import BasketBall

lev2_pyexdir.addToSysPath()


class BasketBallScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("basket_mtl",
                   dsl_class=BasketBall)
    drw = A.IcoSphere("basket_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("basketball",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
