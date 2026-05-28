#!/usr/bin/env ork.python
###############################################################################
# mtl_ss_jade.py — PBR2 Phase 3 (P3.D) SSSS acid test, **jade variant**.
#
# Same three-sphere structure as mtl_ss_skin (off / half / full
# subsurface_factor), but with cool / G-dominant scatter so the SSSS
# contribution pushes the diffuse toward translucent jade green instead
# of warm flesh.
#
# Jade is greenish dielectric stone:
#   - low roughness (polished)
#   - subsurface_color: deep jade green tint — pushes diffuse toward green
#   - subsurface_radius: G > R, B ≈ near zero (jade transmits green
#     light deepest, scatters very little blue)
#
# Run:
#   ork.scene.viewer.py mtl_ss_jade
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class JadeScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.0))

    sphere_sdf = self.asset.SphereSdf("sphere_sdf", radius=2.0)

    base_color  = vec4(0.5, 0.5, 0.5, 1.0)   # pale green stone
    base_rough  = 0.75                       # polished
    jade_radius = vec3(0.15, 0.55, 0.05)     # m; G scatters farthest
    jade_tint   = vec3(0.25, 0.70, 0.40)     # deep jade green

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_off = self.asset.PbrMaterial(
        "jade_mat_off",
        base_color = base_color, metallic = 0.0, roughness = base_rough)

    mat_half = self.asset.PbrMaterial(
        "jade_mat_half",
        base_color        = base_color,
        metallic          = 0.0,
        roughness         = base_rough,
        subsurface_color  = jade_tint,
        subsurface_radius = jade_radius,
        subsurface_factor = 0.5)

    mat_full = self.asset.PbrMaterial(
        "jade_mat_full",
        base_color        = base_color,
        metallic          = 0.0,
        roughness         = base_rough,
        subsurface_color  = jade_tint,
        subsurface_radius = jade_radius,
        subsurface_factor = 1.0)

    self.entity("sphere_jade_off",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_off", mat_off)},
      })])
    self.entity("sphere_jade_half",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_half", mat_half)},
      })])
    self.entity("sphere_jade_full",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_full", mat_full)},
      })])
