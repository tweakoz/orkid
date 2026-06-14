###############################################################################
# mtl_dirt.py — procedural volumetric Dirt on a sphere (GEOV2 ptex3d).
#
# Organic ground material composed from the function library; 3D fields read at
# the surface (no triplanar). All variation knobs are runtime params.
#
#   ork.scene.viewer.py mtl_dirt
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Dirt

lev2_pyexdir.addToSysPath()


class DirtScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("dirt_mtl",
                   dsl_class=Dirt)
    drw = A.IcoSphere("dirt_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("dirt",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
