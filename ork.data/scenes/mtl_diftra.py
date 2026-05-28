#!/usr/bin/env ork.python
###############################################################################
# diffuse_transmission.py — PBR2 Phase 2.3 acid test for
# KHR_materials_diffuse_transmission.
#
# Two thick wavy "leaf/paper" slabs side-by-side:
#   - Side A (-z): plain green dielectric — opaque leaf.
#   - Side B (+z): same green, plus diffuse_transmission_factor +
#                  warm transmission color — backlit translucent leaf.
#
# The effect is most visible when the env diffuse map differs front vs
# back. With a starry/nebula sky, the back-side irradiance differs
# from the front-side, giving a clear "the back is lit differently"
# signature.
#
# Run:
#   ork.scene.viewer.py -i diffuse_transmission
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class DiffuseTransmissionScene(Scene):

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
    # Thin wavy slab — same "billowy" SDF as sheen.py. Good for
    # diffuse-transmission because: thin, lots of curved front/back
    # surfaces visible from any view angle.
    ##########################

    leaf_shader = """
    vec3f@pp = getvoxelpws();
    float@xx = vec3f@pp.x;
    float@yy = vec3f@pp.y;
    float@zz = vec3f@pp.z;

    float@h_a = f$amp * sin(float@xx * f$freq) * cos(float@zz * f$freq * 0.7);
    float@h_b = f$amp * 0.35 * sin(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@h   = float@h_a + float@h_b;

    float@dhx_a =  f$amp * f$freq       * cos(float@xx * f$freq)       * cos(float@zz * f$freq * 0.7);
    float@dhz_a = -f$amp * f$freq * 0.7 * sin(float@xx * f$freq)       * sin(float@zz * f$freq * 0.7);
    float@dhx_b =  f$amp * 0.35 * f$freq * 1.6 * cos(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@dhz_b =  f$amp * 0.35 * f$freq * 0.4 * cos(float@xx * f$freq * 1.6 + float@zz * f$freq * 0.4);
    float@dhx   = float@dhx_a + float@dhx_b;
    float@dhz   = float@dhz_a + float@dhz_b;

    float@sgn  = float@yy - float@h;
    float@gm   = sqrt(1.0 + float@dhx*float@dhx + float@dhz*float@dhz);
    float@slab = abs(float@sgn / float@gm) - f$thickness * 0.5;

    float@bx  = abs(float@xx) - f$ex;
    float@bz  = abs(float@zz) - f$ez;
    float@box = max(float@bx, float@bz);

    f@sdf = max(float@slab, float@box);
    """

    half_x    = 3.0
    half_z    = 3.0
    amp       = 0.5
    thickness = 0.10   # thinner than sheen.py — paper-like
    half_y    = amp + thickness * 0.5 + 0.5

    leaf_sdf = self.asset.ImplicitSdf(
        "leaf_sdf",
        shader     = leaf_shader,
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
    # Side A — opaque green leaf, no transmission. Reference.
    ##########################

    mat_opaque = self.asset.PbrMaterial(
        "leaf_mat_opaque",
        base_color = vec4(0.18, 0.45, 0.10, 1.0),
        metallic   = 0.0,
        roughness  = 0.9)

    drawable_opaque = self.asset.VdbGridToDrawable(
        "leaf_drawable_opaque",
        grid     = leaf_sdf,
        material = mat_opaque,
        iso      = 0.0)

    self.entity("leaf_opaque",
      transform=Transform(translation=vec3(0, 0, -4)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_opaque},
      })])

    ##########################
    # Side B — same green, with diffuse transmission. Should look
    # translucent — the back-side irradiance (env at -N) bleeds
    # through and tints toward the transmission_color. Set the
    # transmission_color to a warm tone so the "lit from behind"
    # contribution reads as a subtle glow.
    ##########################

    mat_translucent = self.asset.PbrMaterial(
        "leaf_mat_translucent",
        base_color                  = vec4(0.18, 0.45, 0.10, 1.0),
        metallic                    = 0.0,
        roughness                   = 0.9,
        diffuse_transmission_factor = 0.1,
        diffuse_transmission_color  = vec3(1.0, 0.0, 0.0))

    drawable_translucent = self.asset.VdbGridToDrawable(
        "leaf_drawable_translucent",
        grid     = leaf_sdf,
        material = mat_translucent,
        iso      = 0.0)

    self.entity("leaf_translucent",
      transform=Transform(translation=vec3(0, 0, +4)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_translucent},
      })])
