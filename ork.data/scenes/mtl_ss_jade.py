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
from ork.hypergraph.ecs.scene import Scene, Transform as XF

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

    base_color  = vec4(0.25, 0.25, 0.25, 1.0)   # pale green stone
    base_rough  = 0.80                       # polished
    jade_radius = vec3(0.15, 0.55, 0.05)     # m; G scatters farthest
    jade_tint   = vec3(0.25, 0.70, 0.40)     # deep jade green
    # Backlit transmission (KHR_materials_diffuse_transmission, PBR2 P5):
    # light entering the BACK of a jade piece emerges as vivid green on the
    # camera-facing side. Per-light wrap-around N·L applied in the forward
    # shader — separate from screen-space SSSS but works on the same material.
    jade_trans_color = vec3(0.30, 0.85, 0.50)  # vivid translucent jade
    jade_trans_fac   = 0.3                     # strong (jade is markedly translucent)
    # Rough specular transmission (KHR_materials_transmission, PBR2 P4):
    # tiny refractive contribution that picks up the material's roughness
    # automatically. Adds the "stone-glass" quality jade has — light bends
    # slightly through it. Combined with Beer-Lambert attenuation_color the
    # transmission inherits a green tint over distance.
    jade_spec_trans      = 0.075                # smidgen — jade isn't glass
    jade_transmission_roughness = 0.5
    jade_ior             = 1.6                # close to typical stone IOR
    jade_atten_color     = vec3(0.40, 0.95, 0.55)
    jade_atten_distance  = 5.0                 # m — Beer-Lambert per meter

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_off = self.asset.PbrMaterial(
        "jade_mat_off",
        base_color = base_color, metallic = 0.0, roughness = base_rough)

    # Nested form (new). Same fields, grouped per glTF extension.
    # See assets.py:_NESTED_LOBES and the TypedDicts (SubsurfaceLobe,
    # TransmissionLobe, ...) for the closed inner-key vocabulary.
    mat_half = self.asset.PbrMaterial("jade_mat_half",
        base = {"color": base_color, "metallic": 0.0, "roughness": base_rough},
        subsurface = {
            "color":  jade_tint,
            "radius": jade_radius,
            "factor": 0.5},
        diffuse_transmission = {
            "color":  jade_trans_color,
            "factor": jade_trans_fac * 0.5},
        transmission = {
            "factor":    jade_spec_trans * 0.5,
            "roughness": jade_transmission_roughness},
        ior = jade_ior,
        volume = {
            "attenuation_color":    jade_atten_color,
            "attenuation_distance": jade_atten_distance},
        iridescence = {"factor": 0.055})

    mat_full = self.asset.PbrMaterial(
        "jade_mat_full",
        base_color        = base_color,
        metallic          = 0.0,
        roughness         = base_rough,
        subsurface_color  = jade_tint,
        subsurface_radius = jade_radius,
        subsurface_factor = 1.0,
        diffuse_transmission_factor = jade_trans_fac,
        diffuse_transmission_color  = jade_trans_color,
        transmission_factor  = jade_spec_trans,
        transmission_roughness = jade_transmission_roughness,
        ior                  = jade_ior,
        attenuation_color    = jade_atten_color,
        attenuation_distance = jade_atten_distance,
        iridescence_factor = 0.055,
        clearcoat_factor     = 0.08,
        clearcoat_roughness  = 0.00)

    self.entity("sphere_jade_off",
      transform=XF(translation=vec3(-5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_off", mat_off)},
      })])
    self.entity("sphere_jade_half",
      transform=XF(translation=vec3(0, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_half", mat_half)},
      })])
    self.entity("sphere_jade_full",
      transform=XF(translation=vec3(+5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_jade_full", mat_full)},
      })])
