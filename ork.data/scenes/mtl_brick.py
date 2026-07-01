###############################################################################
# mtl_brick.py — procedural volumetric brick masonry on a sphere (GEOV2 ptex3d).
#
# Demonstrates the reusable brick_lattice (native 3D running bond) wrapping a
# sphere as real 3D masonry — no triplanar, no projection seams. Every knob is a
# runtime param (bond / size / mortar / color / mottle / height), so one material
# spans many brick looks. Add pom_steps=24 for deep parallax relief.
#
#   ork.scene.viewer.py mtl_brick
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Brick

lev2_pyexdir.addToSysPath()


class BrickScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("brick_mtl",
                   dsl_class=Brick,
                   pom_steps=4,
                   brick_freq=vec3(3.1, 7.1, 3.1),
                   bond=0.5)
    drw = A.IcoSphere("brick_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("brick",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
