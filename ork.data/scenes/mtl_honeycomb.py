###############################################################################
# mtl_honeycomb.py — hex-tiled sphere via the reusable P.hexgrid (GEOV2 ptex3d).
#
# Demonstrates the decomposition: equirectangular projection + the reusable 2D
# hex grid. Per-cell color from hexgrid's .id, recessed seams from .edge, tile
# count from `freq` (runtime). (Equirect distorts the hexes near the poles.)
#
#   ork.scene.viewer.py mtl_honeycomb
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import HoneyComb

lev2_pyexdir.addToSysPath()


class HoneyCombScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("comb_mtl",
                   dsl_class=HoneyComb,
                   cells=400.0,
                   tile=0.2)
    drw = A.IcoSphere("comb_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity("honeycomb",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
