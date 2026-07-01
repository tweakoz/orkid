###############################################################################
# mtl_oak.py — procedural Oak on a sphere (GEOV2 ptex3d, volumetric wood grain).
#
# Growth rings live in object space (the trunk), so the cut pattern emerges on the
# surface with no triplanar. All grain knobs are runtime params.
#
#   ork.scene.viewer.py mtl_oak
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Oak

lev2_pyexdir.addToSysPath()


class OakScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("oak_mtl",
                   dsl_class=Oak)
    drw = A.IcoSphere("oak_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("oak",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
