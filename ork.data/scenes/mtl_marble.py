###############################################################################
# mtl_marble.py — procedural veined marble on a sphere (GEOV2 ptex3d).
#
# Demonstrates vein_field: domain-warped turbulence run volumetrically, so the
# veins run continuously through the sphere like real quarried stone — no
# triplanar, no projection seams. Polished and flat by default (white Carrara);
# pass relief=True for eroded stone. All vein knobs are runtime params.
#
#   ork.scene.viewer.py mtl_marble
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Marble

lev2_pyexdir.addToSysPath()


class MarbleScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("marble_mtl",
                   dsl_class=Marble,
                   vein_freq=2.0,
                   warp=0.8)
    drw = A.IcoSphere("marble_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("marble",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
