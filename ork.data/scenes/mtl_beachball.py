###############################################################################
# mtl_beachball.py — procedural inflatable beach ball (GEOV2 ptex3d).
#
# Vertical colored gore panels + recessed welded seams (bump), glossy vinyl.
#
#   ork.scene.viewer.py mtl_beachball
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import BeachBall

lev2_pyexdir.addToSysPath()


class BeachBallScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("beach_mtl",
                   dsl_class=BeachBall,
                   panels=6,
                   seam=0.025)
    drw = A.IcoSphere("beach_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("beachball",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
