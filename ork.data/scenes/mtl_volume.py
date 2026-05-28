#!/usr/bin/env ork.python
###############################################################################
# volume.py — PBR2 Phase 2.8 acid test for KHR_materials_volume.
#
# Pairs with transmission. Beer-Lambert: T = exp(-σ * d), where σ comes
# from attenuation_color + attenuation_distance, and d is volume_thickness.
# Tints the transmitted light by absorbing wavelength-dependent amounts
# (red wine, amber, blue ocean glass).
#
# Three transmissive spheres (all IOR=1.5, transmission_factor=1.0):
#
#   left   (x=-5.5): no volume → clear glass (control)
#   center (x=0):    volume amber  — attenuation_color = (1.0, 0.55, 0.15)
#   right  (x=+5.5): volume cobalt — attenuation_color = (0.10, 0.30, 0.85)
#
# Diagnostic: orbiting the camera, the center sphere should look like
# amber whisky / honey, the right like deep cobalt glass. Both visibly
# tinted vs the clear left sphere — but only on the TRANSMITTED light
# (Fresnel rim reflections should stay neutral/sky-colored).
#
# Run:
#   ork.scene.viewer.py -i volume
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class VolumeScene(Scene):

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

    base_color = vec4(0.1,0.1,0.1, 1.0)
    base_rough = 0.0  # very smooth so refraction is sharp
    ior = 1.6
    tfact = 0.90

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    # Left — clear glass, no volume.
    mat_clear = self.asset.PbrMaterial(
        "vol_mat_clear",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = ior,
        transmission_factor = tfact)

    # Center — amber. attenuation_distance small => strong tint at the
    # configured thickness; warm amber color absorbs blue / passes red.
    mat_amber = self.asset.PbrMaterial(
        "vol_mat_amber",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = ior,
        transmission_factor      = tfact,
        volume_thickness_factor  = 1.0,                       # thick (matches sphere diameter)
        attenuation_color        = vec3(1.0, 0.55, 0.15),     # amber tint
        attenuation_distance     = 1.0)                       # rapid attenuation

    # Right — cobalt blue.
    mat_cobalt = self.asset.PbrMaterial(
        "vol_mat_cobalt",
        base_color = base_color, 
        metallic = 0.0, 
        roughness = base_rough,
        ior        = ior,
        transmission_factor      = tfact,
        volume_thickness_factor  = 5.0,
        attenuation_color        = vec3(0.10, 0.30, 0.85),
        attenuation_distance     = 1.0)

    self.entity("sphere_vol_clear",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_vol_clear", mat_clear)},
      })])
    self.entity("sphere_vol_amber",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_vol_amber", mat_amber)},
      })])
    self.entity("sphere_vol_cobalt",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_vol_cobalt", mat_cobalt)},
      })])
