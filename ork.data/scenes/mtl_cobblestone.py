###############################################################################
# mtl_cobblestone.py — cobblestone with PROCEDURAL PARALLAX OCCLUSION (GEOV2 §18).
#
# The classic POM showcase (AMD ToyShop street), voronoi-based so it maps cleanly
# onto a sphere: flat voronoi cells with rounded rims + thin gaps that occlude at
# grazing angles. Uses the reusable CobbleStone material (pom_steps > 0).
#
#   *** TEMPORARY / EXPENSIVE *** — the march re-evaluates the procedural height
#   `pom_steps`x/fragment (GEOV2 §18.5). Lower `pom_steps` / SSAA if heavy; a
#   baked-height texture replaces the march once auto-uv lands.
#
# Bindable: base_color / gap / warp / dome / bump_scale / relief.
#
# Run:
#   ork.scene.viewer.py geov2_phase4c
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CobbleStone

lev2_pyexdir.addToSysPath()


class CobbleStoneScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("cobble_mtl",
                   dsl_class=CobbleStone,
                   base_color=vec3(0.47, 0.42, 0.43),
                   cell_scale=3.0,
                   gap=0.005,
                   gap_color=vec3(0.27, 0.22, 0.23),
                   relief=0.02,
                   pom_steps=16)
    drw = A.IcoSphere("cobble_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity(
      "cobbleball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [self.SG.component(nodes={"n": {"drawable": drw}})])
