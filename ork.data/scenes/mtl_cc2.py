#!/usr/bin/env ork.python
###############################################################################
# clearcoat_car.py — PBR2 Phase 2.2 acid test, car-paint variant.
#
# Sleek car silhouette via analytical SDF: a long low rounded "body" box
# smooth-min'd with a smaller offset rounded "cab" box. No wheels — just
# the painted shell. Fire-engine red lacquer paint = dark-red dielectric
# base + sharp clearcoat. Two cars side-by-side, identical body and color,
# with vs without clearcoat — the clearcoat side should look "wet" with
# crisp env reflections; the plain side looks like flat-finish primer.
#
# Run:
#   ork.scene.viewer.py -i clearcoat_car
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from ork.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class ClearcoatCarScene(Scene):

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
    # Car silhouette SDF.
    #   body: 4m long × 1.8m wide × 0.6m tall, rounded
    #   cab : 2.5m long × 1.6m wide × 0.5m tall, sitting on top + slightly back
    #   smooth-min via polynomial blend (controls the windshield curve)
    ##########################

    car_shader = """
    vec3f@pp = getvoxelpws();
    float@px = vec3f@pp.x;
    float@py = vec3f@pp.y;
    float@pz = vec3f@pp.z;

    // ---- body: long low rounded box centered at (0, body_cy, 0)
    float@bx = abs(float@px)               - f$body_hx;
    float@by = abs(float@py - f$body_cy)   - f$body_hy;
    float@bz = abs(float@pz)               - f$body_hz;
    float@bxp = max(float@bx, 0.0);
    float@byp = max(float@by, 0.0);
    float@bzp = max(float@bz, 0.0);
    float@bm  = max(float@bx, max(float@by, float@bz));
    float@bdist = sqrt(float@bxp*float@bxp + float@byp*float@byp + float@bzp*float@bzp)
                + min(float@bm, 0.0) - f$body_r;

    // ---- cab: smaller rounded box, offset up + slightly back
    float@cx = abs(float@px - f$cab_cx)    - f$cab_hx;
    float@cy = abs(float@py - f$cab_cy)    - f$cab_hy;
    float@cz = abs(float@pz)               - f$cab_hz;
    float@cxp = max(float@cx, 0.0);
    float@cyp = max(float@cy, 0.0);
    float@czp = max(float@cz, 0.0);
    float@cm  = max(float@cx, max(float@cy, float@cz));
    float@cdist = sqrt(float@cxp*float@cxp + float@cyp*float@cyp + float@czp*float@czp)
                + min(float@cm, 0.0) - f$cab_r;

    // ---- smooth min (polynomial). h is the smoothstep-like blend weight.
    float@h  = max(0.0, min(1.0, 0.5 + 0.5 * (float@cdist - float@bdist) / f$blend));
    f@sdf    = float@cdist * (1.0 - float@h) + float@bdist * float@h
             - f$blend * float@h * (1.0 - float@h);
    """

    # Bbox sized so the activated region contains the whole car body+cab
    # with a margin. Voxel size 0.04m → ~ 250×60×50 voxels = ~750k voxels.
    half_x = 2.5   # half-length (X is forward/back)
    half_y = 1.4   # half-height
    half_z = 1.2   # half-width

    car_sdf = self.asset.ImplicitSdf(
        "car_sdf",
        shader     = car_shader,
        bbox_min   = vec3(-half_x, -half_y, -half_z),
        bbox_max   = vec3(+half_x, +half_y, +half_z),
        voxel_size = 0.04,
        params = {
          # Body — long, low.
          "body_hx": 2.0,   # half-length
          "body_hy": 0.30,  # half-height (thin slab)
          "body_hz": 0.85,  # half-width
          "body_cy": 0.35,  # vertical center (raises body off origin)
          "body_r":  0.18,  # corner rounding
          # Cab — smaller, on top, slightly back.
          "cab_hx":  1.0,
          "cab_hy":  0.30,
          "cab_hz":  0.70,
          "cab_cx":  -0.25, # cab offset toward -x (back of car)
          "cab_cy":  0.95,  # sits on top of body
          "cab_r":   0.12,
          # Smooth-min blend — controls the windshield + rear curve.
          "blend":   0.45,
        })

    ##########################
    # Side A (left, x=-3.5): fire-engine red, NO clearcoat — matte paint.
    ##########################

    mat_matte = self.asset.PbrMaterial(
        "car_mat_matte",
        base_color = vec4(0.5, 0.05, 0.03, 1.0),
        metallic   = 1.0,
        roughness  = 0.95)

    drawable_matte = self.asset.VdbGridToDrawable(
        "car_drawable_matte",
        grid     = car_sdf,
        material = mat_matte,
        iso      = 0.0)

    self.entity("car_matte",
      transform=Transform(translation=vec3(0, 0, -2.5)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_matte},
      })])

    ##########################
    # Side B (right, x=+3.5): same red, WITH clearcoat — wet lacquer.
    ##########################

    mat_lacquer = self.asset.PbrMaterial(
        "car_mat_lacquer",
        base_color           = vec4(0.5, 0.05, 0.03, 1.0),
        metallic             = 1.0,
        roughness            = 0.95,
        clearcoat_factor     = 0.85,
        clearcoat_roughness  = 0.05)

    drawable_lacquer = self.asset.VdbGridToDrawable(
        "car_drawable_lacquer",
        grid     = car_sdf,
        material = mat_lacquer,
        iso      = 0.0)

    self.entity("car_lacquer",
      transform=Transform(translation=vec3(0, 0, +2.5)),
      components=[SG.component(nodes={
        "n": {"drawable": drawable_lacquer},
      })])
