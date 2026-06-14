#!/usr/bin/env ork.python
###############################################################################
# transmission.py — PBR2 Phase 2.7 acid test for KHR_materials_transmission.
#
# Three dielectric spheres varying transmission_factor:
#
#   left   (x=-5.5): transmission_factor = 0.0    → opaque white (control)
#   center (x=0):    transmission_factor = 0.5    → half-transmissive
#   right  (x=+5.5): transmission_factor = 1.0    → fully transmissive
#
# All three use metallic=0, roughness=0.05 (smooth so refraction is
# sharp), base_color = near-white, ior=1.5 (default glTF).
#
# Diagnostic: right sphere should look glass-like — the nebula sky
# refracts through it, view-dependent. Snell at IOR=1.5 bends the
# sample direction inward; orbiting the camera sweeps different env
# regions visible through the sphere. The Fresnel rim should still
# show some front-reflection at the silhouette (where 1-F dims, base
# BRDF brightens). Left should stay opaque white for comparison.
#
# v1 is IBL-only: no scene-behind framebuffer copy — other objects
# placed BEHIND the glass would NOT show through. P3 adds that.
#
# Run:
#   ork.scene.viewer.py -i transmission
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class TransmissionScene(Scene):

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
    base_rough = 0.25  # very smooth — sharp refraction is the diagnostic

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_off = self.asset.PbrMaterial(
        "trans_mat_off",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = 1.0,
        transmission_factor = 0.75)

    mat_half = self.asset.PbrMaterial(
        "trans_mat_half",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = 1.5,
        transmission_factor = 0.75)

    mat_full = self.asset.PbrMaterial(
        "trans_mat_full",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = 2.0,
        transmission_factor = 0.75)

    self.entity("sphere_trans_off",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_trans_off", mat_off)},
      })])
    self.entity("sphere_trans_half",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_trans_half", mat_half)},
      })])
    self.entity("sphere_trans_full",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_trans_full", mat_full)},
      })])
