#!/usr/bin/env ork.python
###############################################################################
# specular.py — PBR2 Phase 2.5 acid test for KHR_materials_specular.
#
# Three white dielectric spheres (metallic=0, roughness=0.25), each
# with different specular_factor / specular_color settings:
#
#   left   (x=-5.5): specular_factor = 0.0
#                    → F0 forced to 0; sphere looks matte (no central
#                    reflection, tiny Fresnel rim only). Equivalent to
#                    an "anti-reflection coating" override.
#
#   center (x=0):    specular_factor = 1.0, specular_color = (1,1,1)
#                    → glTF default (no change from base PBR; F0=0.04
#                    for dielectric). Reference.
#
#   right  (x=+5.5): specular_factor = 1.0, specular_color = (1, 0.3, 0.2)
#                    → red-tinted dielectric specular. F0 dimmed in
#                    green/blue channels; reflection of the nebula
#                    sky becomes red-shifted.
#
# Diagnostic: left should be visibly less reflective than center; right
# should have a distinct red/orange cast on its specular reflection
# while keeping its diffuse white.
#
# Run:
#   ork.scene.viewer.py -i specular
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class SpecularScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/tozenv_nebula.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.05))

    sphere_sdf = self.asset.SphereSdf("sphere_sdf", radius=2.0)

    base_color = vec4(0.9, 0.9, 0.9, 1.0)
    base_metal = 0.0
    base_rough = 0.25

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    # Left — specular killed entirely.
    mat_off = self.asset.PbrMaterial(
        "spec_mat_off",
        base_color      = base_color,
        metallic        = base_metal,
        roughness       = base_rough,
        specular_factor = 0.0)

    # Middle — default (factor=1, color=white). Reference.
    mat_default = self.asset.PbrMaterial(
        "spec_mat_default",
        base_color      = base_color,
        metallic        = base_metal,
        roughness       = base_rough,
        specular_factor = 1.0,
        specular_color  = vec3(1.0, 1.0, 1.0))

    # Right — red-tinted dielectric specular.
    mat_tinted = self.asset.PbrMaterial(
        "spec_mat_tinted",
        base_color      = base_color,
        metallic        = base_metal,
        roughness       = base_rough,
        specular_factor = 1.0,
        specular_color  = vec3(1.0, 0.3, 0.2))

    self.entity("sphere_spec_off",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_spec_off", mat_off)},
      })])
    self.entity("sphere_spec_default",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_spec_default", mat_default)},
      })])
    self.entity("sphere_spec_tinted",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_spec_tinted", mat_tinted)},
      })])
