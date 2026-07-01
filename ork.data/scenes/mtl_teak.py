###############################################################################
# mtl_teak.py — procedural Teak on a sphere (GEOV2 ptex3d, volumetric wood grain).
#
# Growth rings live in object space (the trunk), so the cut pattern emerges on the
# surface with no triplanar. All grain knobs are runtime params.
#
#   ork.scene.viewer.py mtl_teak
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Teak

lev2_pyexdir.addToSysPath()


class TeakScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("teak_mtl",
                   dsl_class=Teak)
    drw = A.IcoSphere("teak_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("teak",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
