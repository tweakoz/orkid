#!/usr/bin/env ork.python
###############################################################################
# geov2_phase4a.py — cracked mud (GEOV2 ptex3d) as an ECS scene.
#
# Uses the reusable CrackedMud material (ork.hypergraph.assets.materials): a
# cellular cracked-mud surface with the cheap ANALYTIC cellular bump (GEOV2 §18
# option 1 — relief from one gradient-voronoi eval, no finite-difference taps).
# Runs under the ECS viewer (tonemapping / exposure / postfx) and round-trips.
#
# The ptex3d asset wrapper materializes the DSL to a cached .fxv2 and packs a
# PbrMaterialGenData (shaderpath + bindable param defaults: base_color / crack /
# warp / bump_scale); the IcoSphere asset bakes the .ogeo and references the
# material by name — same round-trip path as mtl_showcase's PbrMaterial spheres.
# For real crack DEPTH (parallax) see geov2_phase4b.
#
# Run:
#   ork.scene.viewer.py geov2_phase4a
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CrackedMud

lev2_pyexdir.addToSysPath()


###############################################################################

class CrackedMudScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset            = "ForwardPBR",
      skybox_path       = "<ork_envmaps2>/blender_forest.xir",
      SkyboxIntensity   = 1.0,
      DiffuseIntensity  = 1.0,
      SpecularIntensity = 1.0,
      AmbientLight      = vec3(0.06))

    mat = self.asset.Ptex3d("mud_mtl", dsl_class=CrackedMud, cell_scale=5.0)
    drw = self.asset.IcoSphere("mud_sphere", radius=2.5, subdivisions=5, material=mat)

    self.entity(
      "mudball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [SG.component(nodes={"n": {"drawable": drw}})])
