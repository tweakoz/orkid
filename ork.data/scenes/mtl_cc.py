#!/usr/bin/env ork.python
###############################################################################
# clearcoat.py — PBR2 Phase 2.2 acid test for KHR_materials_clearcoat.
#
# Two saddles side-by-side, both rough gold metal:
#   - Side A (left, x=-4): NO clearcoat — looks satin / brushed.
#   - Side B (right, x=+4): WITH clearcoat — sharp lacquer reflection
#                            layered on top, energy-conserved against base.
#
# Clearcoat is most visible when the base is rough (so the base reflection
# is dull) but clearcoat_roughness is low (so the lacquer reflection is
# sharp). The contrast between dull metal and sharp lacquer is the
# diagnostic. With energy conservation, the dull base on the right is
# also attenuated by (1 - clearcoat_factor * F_cc) so total reflectance
# stays ≤ 1.
#
# Run:
#   ork.scene.viewer.py -i clearcoat
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class ClearcoatScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph + HDRI skybox (nebula has bright streaks — clearcoat
    # reflections of those streaks are the obvious diagnostic).
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/tozenv_nebula.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.08))

    ##########################
    # Shared saddle SDF — saddle.py's standard 12m-wide hyperbolic
    # paraboloid, scaled down for side-by-side comparison.
    ##########################

    saddle_sdf = self.asset.ThickSaddleSdf(
        "saddle_sdf",
        half_extent_x = 3.0,
        half_extent_z = 3.0,
        saddle_coef   = 0.3,
        thickness     = 0.35,
        voxel_size    = 0.04)

    ##########################
    # Side A — rough gold metal, NO clearcoat. Brushed-finish look.
    ##########################

    mat_brushed = self.asset.PbrMaterial(
        "saddle_mat_brushed",
        base_color = vec4(0.95, 0.75, 0.30, 1.0),
        metallic   = 1.0,
        roughness  = 0.95)

    drawable_brushed = self.asset.VdbGridToDrawable(
        "saddle_drawable_brushed",
        grid     = saddle_sdf,
        material = mat_brushed,
        iso      = 0.0)

    self.entity("saddle_brushed",
      transform=Transform(translation=vec3(-4, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_brushed},
      })])

    ##########################
    # Side B — same rough gold base, WITH clearcoat. Sharp lacquer
    # reflection (clearcoat_roughness=0.05) layered on top.
    ##########################

    mat_lacquered = self.asset.PbrMaterial(
        "saddle_mat_lacquered",
        base_color           = vec4(0.95, 0.75, 0.30, 1.0),
        metallic             = 1.0,
        roughness            = 0.95,
        clearcoat_factor     = 1.0,
        clearcoat_roughness  = 0.05)

    drawable_lacquered = self.asset.VdbGridToDrawable(
        "saddle_drawable_lacquered",
        grid     = saddle_sdf,
        material = mat_lacquered,
        iso      = 0.0)

    self.entity("saddle_lacquered",
      transform=Transform(translation=vec3(+4, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_lacquered},
      })])
