#!/usr/bin/env ork.python
###############################################################################
# mtl_ss_skin.py — PBR2 Phase 3 (P3.D) acid test for KHR_materials_subsurface,
# **skin variant**: warm pink scatter tint, R bleeds farthest (ear-bleed look).
#
# Three white dielectric spheres varying subsurface_factor:
#
#   left   (x=-5.5): subsurface_factor = 0.0  → no SSSS (control)
#   center (x=0):    subsurface_factor = 0.5  → moderate scatter
#   right  (x=+5.5): subsurface_factor = 1.0  → full scatter
#
# All three share subsurface_radius (1.4, 0.5, 0.3) — skin baseline.
# Per-channel weights in the Jimenez 11-tap kernel + warm subsurface_color
# produce red-shifted bleed at silhouettes (cheekbone backlight look on a head).
#
# Diagnostic: with a backlight or grazing-angle envmap term, the right
# sphere's silhouette should glow warmer (R bleeds farthest); the left
# (control) should look like a regular matte dielectric.
#
# Auto-attach: any PbrMaterial with has_subsurface=true triggers the
# Scene DSL to register PostFxNodeSSSS in SceneGraphSystemData's
# postfx_nodes map + appends "ssss" to postfx_order. The post-fx
# survives JSON round-trip.
#
# See mtl_ss_jade.py for the cool / green-shifted SSS variant.
#
# Run:
#   ork.scene.viewer.py mtl_ss_skin
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class SubsurfaceScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,   # P3.D bring-up — kill spec so diffuse SSSS is visible
        AmbientLight       = vec3(0.0))

    sphere_sdf = self.asset.SphereSdf("sphere_sdf", radius=2.0)

    # Realistic skin tint baseline:
    #   - base_color: pale warm flesh (not over-saturated)
    #   - subsurface_color: soft warm pink — pushes the diffuse a small amount
    #     toward warm flesh, NOT toward bright red (former (0.85, 0.40, 0.30)
    #     was a demo-vivid value tuned for visibility, not physical realism)
    #   - subsurface_radius: relative per-channel scatter depths from
    #     Jimenez paper (R/G/B ratio ≈ 5:2:1). Absolute scale ≪ before;
    #     visible effect at typical strength = subtle warm halo at silhouettes,
    #     not the dominant material look
    base_color  = vec4(0.5,0.5,0.5,1)   # warm flesh
    base_rough  = 0.85
    skin_radius = vec3(0.50, 0.20, 0.10)*5.1        # m; R bleeds farthest
    skin_tint   = vec3(1.00, 0.65, 0.55)        # soft warm pink

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_off = self.asset.PbrMaterial(
        "sss_mat_off",
        base_color = base_color, metallic = 0.0, roughness = base_rough)

    mat_half = self.asset.PbrMaterial(
        "sss_mat_half",
        base_color        = base_color,
        metallic          = 0.0,
        roughness         = base_rough,
        subsurface_color  = skin_tint,
        subsurface_radius = skin_radius,
        subsurface_factor = 0.5)

    mat_full = self.asset.PbrMaterial(
        "sss_mat_full",
        base_color        = base_color,
        metallic          = 0.0,
        roughness         = base_rough,
        diffuse_transmission_factor = 0.25,
        diffuse_transmission_color  = skin_tint,
        subsurface_color  = skin_tint,
        subsurface_radius = skin_radius,
        subsurface_factor = 0.75)

    self.entity("sphere_sss_off",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_sss_off", mat_off)},
      })])
    self.entity("sphere_sss_half",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_sss_half", mat_half)},
      })])
    self.entity("sphere_sss_full",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": make_drawable("dr_sss_full", mat_full)},
      })])
