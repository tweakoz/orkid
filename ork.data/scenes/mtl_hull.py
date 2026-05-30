###############################################################################
# mtl_hull.py — procedural sci-fi spaceship hull on a sphere (GEOV2 ptex3d).
#
# Demonstrates the decomposition: reusable panel_split (irregular plating) +
# greeble (sub-plates) + triplanar map onto the sphere with NO UVs — the same
# functions drop onto a real hull mesh unchanged. Military-grey finish; dial
# `greeble_cover` from 0 (clean panels) up for a busier hull.
#
#   ork.scene.viewer.py mtl_hull
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import SpaceshipHull

lev2_pyexdir.addToSysPath()


class HullScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("hull_mtl",
                   dsl_class=SpaceshipHull,
                   levels=6,
                   scale=1.0,
                   greeble_cover=0.30,
                   bump_scale=0.01,
                   seam=0.02)
    drw = A.IcoSphere("hull_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("hull",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
