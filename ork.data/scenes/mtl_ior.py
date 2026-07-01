#!/usr/bin/env ork.python
###############################################################################
# ior.py — PBR2 Phase 2.4 acid test for KHR_materials_ior.
#
# Three identical white dielectric saddles, varying ior:
#   left   (x=-7): ior = 1.0  → F0 = 0           (no dielectric reflection)
#   center (x=0):  ior = 1.5  → F0 = 0.04        (default glTF / water-like)
#   right  (x=+7): ior = 2.4  → F0 ≈ 0.17        (diamond-like)
#
# Visible signature: face-on reflectance grows with ior. At grazing the
# Schlick Fresnel approaches 1.0 for all three (so silhouettes converge);
# the diagnostic angle is face-on / 30°-off, where Schlick is dominated
# by F0 and the three IOR values produce distinct brightness.
#
# Run:
#   ork.scene.viewer.py -i ior
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class IorScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/tozenv_nebula.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.05))

    ##########################
    # Shared sphere SDF — spheres give the cleanest read for Fresnel
    # variation (smooth curvature presents every viewing angle).
    ##########################

    sphere_sdf = self.asset.SphereSdf("sphere_sdf", radius=2.0)

    ##########################
    # All three: same near-white dielectric base, low roughness so
    # specular reflection dominates. Only ior differs.
    ##########################

    base_color = vec4(0.9, 0.9, 0.9, 1.0)
    base_metallic  = 0.0
    base_roughness = 0.25  # smooth-ish dielectric so F0 matters visually

    def make_mat(name, ior):
      return self.asset.PbrMaterial(
          name,
          base_color = base_color,
          metallic   = base_metallic,
          roughness  = base_roughness,
          ior        = ior)

    def make_drawable(name, mat):
      return self.asset.VdbGridToDrawable(name, grid=sphere_sdf, material=mat, iso=0.0)

    mat_low   = make_mat("ior_mat_low",   1.0)   # F0 = 0
    mat_mid   = make_mat("ior_mat_mid",   1.5)   # F0 = 0.04 (glTF default)
    mat_high  = make_mat("ior_mat_high",  2.4)   # F0 ≈ 0.17 (diamond)

    self.entity("sphere_ior_low",
      transform=Transform(translation=vec3(-5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_ior_low", mat_low)},
      })])
    self.entity("sphere_ior_mid",
      transform=Transform(translation=vec3( 0, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_ior_mid", mat_mid)},
      })])
    self.entity("sphere_ior_high",
      transform=Transform(translation=vec3(+5.5, 0, 0)),
      components=[self.spinner(), SG.component(nodes={
        "n": {"drawable": make_drawable("dr_ior_high", mat_high)},
      })])
