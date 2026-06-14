###############################################################################
# mtl_rock.py — procedural volumetric Rock on a sphere (GEOV2 ptex3d).
#
# 3D fields read at the surface (no triplanar). All variation knobs runtime.
#
#   ork.scene.viewer.py mtl_rock
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Rock

lev2_pyexdir.addToSysPath()


class RockScene(Scene):

  def __init__(self):
    super().__init__()
    A = self.asset

    mat = A.Ptex3d("rock_mtl",
                   dsl_class=Rock)
    drw = A.IcoSphere("rock_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("rock",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
