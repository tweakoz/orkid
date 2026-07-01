#!/usr/bin/env ork.python
###############################################################################
# iridescence.py — PBR2 Phase 2.6 acid test for KHR_materials_iridescence.
#
# Three white dielectric spheres varying iridescence_factor:
#
#   left   (x=-5.5): iridescence_factor = 0.0  → no iridescence (control)
#   center (x=0):    iridescence_factor = 0.5  → blended thin-film
#   right  (x=+5.5): iridescence_factor = 1.0  → full iridescent shift
#
# All three use metallic=0, roughness=0.15 (smooth, so the spec lobe
# clearly shows the F0 modulation). Film thickness fixed at 350nm and
# film IOR fixed at 1.3 in the shader (v1 hardcoded; P3 plumbs them).
#
# Diagnostic: orbiting the camera should produce a per-wavelength hue
# shift across the right (and to a lesser extent center) sphere's
# Fresnel rim — soap-bubble / jewel-beetle / oil-slick look. Left
# sphere should stay neutral (no hue rotation with view).
#
# Run:
#   ork.scene.viewer.py -i iridescence
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class IridescenceScene(Scene):

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

    base_color = vec4(0.0,0.0,0.0, 1.0)
    base_rough = 0.55  # smooth — sharper reflection makes hue shift visible
    trans_factor  = 0.95
    trans_ruf     = 0.0
    ior           = 1.01

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_off = self.asset.PbrMaterial(
        "irid_mat_off",
        base_color = base_color, metallic = 0.0, roughness = base_rough,
        iridescence_factor = 0.0)

    mat_half = self.asset.PbrMaterial(
        "irid_mat_half",
        base_color = base_color, metallic = 0.0, roughness = base_rough,
        iridescence_factor = 0.125)

    mat_full = self.asset.PbrMaterial(
        "irid_mat_full",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        transmission_factor    = trans_factor,
        transmission_roughness = trans_ruf,
        ior                    = ior,
        iridescence_factor     = 0.25)

    self.entity("sphere_irid_off",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_irid_off", mat_off)},
      })])
    self.entity("sphere_irid_half",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_irid_half", mat_half)},
      })])
    self.entity("sphere_irid_full",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_irid_full", mat_full)},
      })])
