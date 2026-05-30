###############################################################################
# geov2_phase4c_lilies.py — round lily-pad / pillar relief (GEOV2 ptex3d POM).
#
# A full-hemisphere voronoi dome (radial .f1 profile) reads as round pads /
# pillars rather than cobblestones — saved here as its own look. Procedural
# parallax occlusion (EXPENSIVE/TEMPORARY, GEOV2 §18.5).
#
#   ork.scene.viewer.py geov2_phase4c_lilies
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import LilyPadsPOM

lev2_pyexdir.addToSysPath()


class LiliesScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset            = "ForwardPBR",
      skybox_path       = "<ork_envmaps2>/blender_forest.xir",
      SkyboxIntensity   = 1.0,
      DiffuseIntensity  = 1.0,
      SpecularIntensity = 1.0,
      AmbientLight      = vec3(0.06))

    mat = self.asset.Ptex3d("lily_mtl", dsl_class=LilyPadsPOM, cell_scale=1.0, radius=0.6, steps=16, relief=.15)
    drw = self.asset.IcoSphere("lily_sphere", radius=2.5, subdivisions=5, material=mat)

    self.entity(
      "lilyball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [SG.component(nodes={"n": {"drawable": drw}})])
