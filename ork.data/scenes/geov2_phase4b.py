#!/usr/bin/env ork.python
###############################################################################
# geov2_phase4b.py — cracked mud with PROCEDURAL PARALLAX OCCLUSION (GEOV2 §18).
#
# Same cracked-mud surface as geov2_phase4a, but instead of the cheap analytic
# cellular bump it uses the GENERAL self.displace(..., parallax=True) path: the
# height field is RAY-MARCHED along the view ray so the cracks show real depth +
# self-occlusion (parallax), and the surface is evaluated at the displaced hit
# point — not merely normal-perturbed.
#
#   *** TEMPORARY / EXPENSIVE ***
#   The march re-evaluates the procedural height (warp + voronoi) `steps`× per
#   fragment (GEOV2 §18.5 cost trap). This is the bring-up proof; a baked-height
#   texture sample replaces the procedural march once auto-uv lands. Keep
#   geov2_phase4a (cheap analytic bump, no parallax) for the affordable version.
#   Lower `steps` / SSAA if it's heavy.
#
# Bindable: base_color / crack / warp / bump_scale / relief (parallax depth).
#
# Run:
#   ork.scene.viewer.py geov2_phase4b
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CrackedMudPOM

lev2_pyexdir.addToSysPath()


###############################################################################

class CrackedMudPOMScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset            = "ForwardPBR",
      skybox_path       = "<ork_envmaps2>/blender_forest.xir",
      SkyboxIntensity   = 1.0,
      DiffuseIntensity  = 1.0,
      SpecularIntensity = 1.0,
      AmbientLight      = vec3(0.06))

    mat = self.asset.Ptex3d("mud_mtl", dsl_class=CrackedMudPOM, cell_scale=5.0)
    drw = self.asset.IcoSphere("mud_sphere", radius=2.5, subdivisions=5, material=mat)

    self.entity(
      "mudball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [SG.component(nodes={"n": {"drawable": drw}})])
