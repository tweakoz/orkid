###############################################################################
# mtl_bark.py — procedural Bark on a sphere (test the Bark material in isolation).
#   ork.scene.viewer.py mtl_bark
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials.bark import Bark

lev2_pyexdir.addToSysPath()


class BarkScene(Scene):

  def __init__(self):
    super().__init__()
    A = self.asset
    mat = A.Ptex3d("bark_mtl", dsl_class=Bark)
    drw = A.IcoSphere("bark_sphere", radius=2.5, subdivisions=6, material=mat)
    self.entity("bark",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
