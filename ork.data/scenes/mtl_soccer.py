###############################################################################
# mtl_soccer.py — procedural soccer ball / football (GEOV2 ptex3d).
#
# Truncated-icosahedron: 12 black pentagons + 20 white hexagons, recessed seams
# (bump), matte synthetic-leather PBR. (32-point Voronoi on the sphere.)
#
#   ork.scene.viewer.py mtl_soccer
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import SoccerBall

lev2_pyexdir.addToSysPath()


class SoccerBallScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("soccer_mtl",
                   dsl_class=SoccerBall,
                   seam=0.02)
    drw = A.IcoSphere("soccer_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("soccerball",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
