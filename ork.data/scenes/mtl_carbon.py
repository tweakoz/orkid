###############################################################################
# mtl_carbon.py — woven carbon-fiber composite on a sphere (GEOV2 ptex3d).
#
# Demonstrates the decomposition: the reusable carbon_weave (2D greyscale) +
# triplanar projection map the weave onto the sphere with NO UVs — the same
# functions drop onto any shape unchanged.
#
#   ork.scene.viewer.py mtl_carbon
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CarbonFiber

lev2_pyexdir.addToSysPath()


class CarbonScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("carbon_mtl",
                   dsl_class=CarbonFiber,
                   scale=12.0)
    drw = A.IcoSphere("carbon_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("carbon",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
