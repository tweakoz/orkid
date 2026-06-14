#!/usr/bin/env ork.python
###############################################################################
# sheen.py — PBR2 Phase 2.1 acid test for KHR_materials_sheen.
#
# Two thick wavy "cloth" slabs side-by-side; left is plain PbrMaterial,
# right has sheen_factor + sheen_color set. Sheen is most visible at
# grazing angles on rough materials, so:
#   - both slabs use roughness=0.9, metallic=0.0 (matte fabric)
#   - the wave geometry guarantees plenty of glancing-angle pixels
#   - sheen_color is warm red/orange for high visual signature
#
# Run:
#   ork.scene.viewer.py -i sheen
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class SheenScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph + HDRI skybox
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/tozenv_nebula.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.06))

    ##########################
    # Billowy SDF — thick wavy sheet via two-frequency sin/cos modulation
    # of the mid-Y surface. Box-clipped in x,z. Tangent-plane distance
    # approximation (|F| / |∇F|) for slab thickness.
    ##########################

    cloth_shader = """
    vec3f@pp = getvoxelpws();
    float@xx = vec3f@pp.x;
    float@yy = vec3f@pp.y;
    float@zz = vec3f@pp.z;

    // Wavy mid-surface: two-frequency sin/cos for cloth-like undulation.
    //   primary wave  — large amplitude, low frequency
    //   detail wave   — smaller amplitude, mixed-axis higher frequency
    float@h_a = f$amp * sin(float@xx * f$freq) * cos(float@zz * f$freq * 0.7);
    float@h_b = f$amp * 0.35 * sin(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@h   = float@h_a + float@h_b;

    // Tangent-plane distance to y = h(x,z).
    // F(p) = y - h(x,z),   ∇F = (-∂h/∂x, 1, -∂h/∂z).
    //   ∂h_a/∂x =  f$amp * f$freq        * cos(xx*f) * cos(zz*f*0.7)
    //   ∂h_a/∂z = -f$amp * f$freq * 0.7  * sin(xx*f) * sin(zz*f*0.7)
    //   ∂h_b/∂x =  f$amp * 0.35 * f$freq * 1.6 * cos(xx*f*1.6 + zz*f*0.4)
    //   ∂h_b/∂z =  f$amp * 0.35 * f$freq * 0.4 * cos(xx*f*1.6 + zz*f*0.4)
    float@dhx_a =  f$amp * f$freq       * cos(float@xx * f$freq)       * cos(float@zz * f$freq * 0.7);
    float@dhz_a = -f$amp * f$freq * 0.7 * sin(float@xx * f$freq)       * sin(float@zz * f$freq * 0.7);
    float@dhx_b =  f$amp * 0.35 * f$freq * 1.6 * cos(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@dhz_b =  f$amp * 0.35 * f$freq * 0.4 * cos(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@dhx   = float@dhx_a + float@dhx_b;
    float@dhz   = float@dhz_a + float@dhz_b;

    float@sgn  = float@yy - float@h;
    float@gm   = sqrt(1.0 + float@dhx*float@dhx + float@dhz*float@dhz);
    float@slab = abs(float@sgn / float@gm) - f$thickness * 0.5;

    // Box clip in x,z so the cloth is a finite rectangle.
    float@bx  = abs(float@xx) - f$ex;
    float@bz  = abs(float@zz) - f$ez;
    float@box = max(float@bx, float@bz);

    f@sdf = max(float@slab, float@box);
    """

    half_x      = 3.0
    half_z      = 3.0
    amp         = 0.6
    thickness   = 0.18
    half_y      = amp + thickness * 0.5 + 0.5

    cloth_sdf = self.asset.ImplicitSdf(
        "cloth_sdf",
        shader     = cloth_shader,
        bbox_min   = vec3(-half_x, -half_y, -half_z),
        bbox_max   = vec3(+half_x, +half_y, +half_z),
        voxel_size = 0.04,
        params = {
          "amp":       amp,
          "freq":      1.4,
          "thickness": thickness,
          "ex":        half_x,
          "ez":        half_z,
        })

    ##########################
    # Side A — plain matte material, no sheen. Reference.
    ##########################

    mat_plain = self.asset.PbrMaterial(
        "cloth_mat_plain",
        base_color = vec4(0.55, 0.05, 0.08, 1.0),
        metallic   = 0.0,
        roughness  = 0.9)

    drawable_plain = self.asset.VdbGridToDrawable(
        "cloth_drawable_plain",
        grid     = cloth_sdf,
        material = mat_plain,
        iso      = 0.0)

    self.entity("cloth_plain",
      transform=Transform(translation=vec3(-4, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_plain},
      })])

    ##########################
    # Side B — same base color + roughness, plus warm sheen. Should
    # show a red/orange glow at grazing angles on the wave crests.
    ##########################

    mat_sheen = self.asset.PbrMaterial(
        "cloth_mat_sheen",
        base_color      = vec4(0.55, 0.05, 0.08, 1.0),
        metallic        = 0.0,
        roughness       = 0.9,
        sheen_factor    = 1.0,
        sheen_color     = vec3(1.0, 1.0, 1.0),
        sheen_roughness = 0.25)

    drawable_sheen = self.asset.VdbGridToDrawable(
        "cloth_drawable_sheen",
        grid     = cloth_sdf,
        material = mat_sheen,
        iso      = 0.0)

    self.entity("cloth_sheen",
      transform=Transform(translation=vec3(+4, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_sheen},
      })])
