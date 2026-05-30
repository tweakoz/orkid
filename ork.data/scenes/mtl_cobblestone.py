###############################################################################
# geov2_phase4c.py — cobblestone with PROCEDURAL PARALLAX OCCLUSION (GEOV2 §18).
#
# The classic POM showcase (AMD ToyShop street), voronoi-based so it maps cleanly
# onto a sphere: rounded domed stones with deep gaps that visibly occlude at
# grazing angles. Uses the reusable CobblestonePOM material.
#
#   *** TEMPORARY / EXPENSIVE *** — the march re-evaluates the procedural height
#   `steps`x/fragment (GEOV2 §18.5). Lower `steps` / SSAA if heavy; a baked-height
#   texture replaces the march once auto-uv lands.
#
# Bindable: base_color / gap / warp / dome / bump_scale / relief.
#
# Run:
#   ork.scene.viewer.py geov2_phase4c
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CobblestonePOM

lev2_pyexdir.addToSysPath()


class CobblestoneScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset            = "ForwardPBR",
      skybox_path       = "<ork_envmaps2>/blender_forest.xir",
      SkyboxIntensity   = 1.0,
      DiffuseIntensity  = 1.0,
      SpecularIntensity = 1.0,
      AmbientLight      = vec3(0.06))

    mat = self.asset.Ptex3d("cobble_mtl", 
                            dsl_class=CobblestonePOM, 
                            base_color=vec3(0.47, 0.42, 0.43),
                            cell_scale=3.0,
                            gap=0.005,
                            gap_color=vec3(0.27, 0.22, 0.23), 
                            relief=0.02, 
                            steps=16)
    drw = self.asset.IcoSphere("cobble_sphere", radius=2.5, subdivisions=5, material=mat)

    self.entity(
      "cobbleball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [SG.component(nodes={"n": {"drawable": drw}})])
