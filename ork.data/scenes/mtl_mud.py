###############################################################################
# mtl_mud.py — procedural volumetric Mud on a sphere (GEOV2 ptex3d).
#
# 3D fields read at the surface (no triplanar). All variation knobs runtime.
#
#   ork.scene.viewer.py mtl_mud
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Mud

lev2_pyexdir.addToSysPath()


class MudScene(Scene):

  def __init__(self):
    super().__init__()
    A = self.asset

    mat = A.Ptex3d("mud_mtl",
                   dsl_class=Mud)
    drw = A.IcoSphere("mud_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("mud",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
