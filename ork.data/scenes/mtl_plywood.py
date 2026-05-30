###############################################################################
# mtl_plywood.py — procedural Plywood on a sphere (GEOV2 ptex3d, volumetric wood grain).
#
# Growth rings live in object space (the trunk), so the cut pattern emerges on the
# surface with no triplanar. All grain knobs are runtime params.
#
#   ork.scene.viewer.py mtl_plywood
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Plywood

lev2_pyexdir.addToSysPath()


class PlywoodScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("plywood_mtl",
                   dsl_class=Plywood)
    drw = A.IcoSphere("plywood_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("plywood",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
