###############################################################################
# scn_erodeflow.py — the erodeflow terrain as a HYPERECS scene. self.terrain() loads the
# REAL erodeflow DSL (zero reimplementation) and reads its OWN scale + look: EXTENT_M
# (16 km), HEIGHT_M (2.5 km), MATERIAL_CLASS (Material — banded strata + cracked-mud +
# flow-metric tint) and its SAMPLER_CHANNELS (FlowMetrics / FlowDischarge / FillDepth /
# Basin / CenterPit / Normal <- the baked flow/basin channels). Nothing restated here.
# Bakes (cook-cached) + plays zero-Python in the player.
#
# render_dimension is the render-mesh res; bake_dimension (default = it) is the compute/bake res
# (DSL is res-independent via EXTENT_M); bump bake_dimension for more detail
# (first bake is slow, then disk-cached).
#
#   ork.scene.viewer.py scn_erodeflow
#   ork.scene.tojson.py -i scn_erodeflow -o /tmp/erodeflow.ecs
#   ork.ecs.player.exe /tmp/erodeflow.ecs --camdist 8000 --camheight 3000
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()


class ErodeFlowScene(Scene):

  def __init__(self):
    super().__init__()

    # PROCEDURAL SKY (replaces the canned desert4k dome): Hillaire atmosphere +
    # celestial sun over a southwest-desert site, clock FROZEN at mid-afternoon —
    # a raking SW light that models the flow channels and basin walls the old
    # IBL-only lighting flattened. NO skybox_path: the sky's own refiltered
    # snapshot is the IBL (owner call, aug08 — a procedural-sky scene loads no
    # envmap). Exposure trio rebalanced to the procedural-sky family's numbers
    # (the 1.3/2.5/1.5 set was graded against the baked dome).
    # No moon/stars: the clock is frozen in daylight, so they never rise.
    self.sky(
        skybox_intensity   = 1.2,
        diffuse_intensity  = 1.5,
        specular_intensity = 1.0,
        ambient_light      = vec3(0.0),
        msaa               = 3,
        CullFrustumScale   = 1.3,   # TEMP A/B TEST: narrow cull frustum (cull-more) — revert after
        DepthPrepass       = True,   # resolves a single-sample depth (the HZB occlusion source) + early-Z
        latitude_deg  = 36.0,
        day_of_year   = 223.0,
        time_of_day   = 15.5,
        time_scale    = 0.0,
        celestial     = True,
        moon          = False,
        stars         = False,
        sun_color     = vec3(1.0, 0.82, 0.60),
        sun_intensity = 2.5,
        sun_params    = {"shadow_map_size": 4096,
                         "shadow_caster": True})

    self.terrain(
        "erodeflow",
        dsl_file  = "erodeflow",
        spawn=vec3(-630.3, 969.4, 282.5),
        render_dimension = 1600,
        mode      = "stored",   # Phase-1: capture the proctex to <assetcache>/ptex3d_capture/<key>/ (cached)
        bake_dimension  = 4096,
        bake_res  = 4096,
        walkable  = True)

    self.projectile_pool(fire=True)   # '/' shoots fireballs (gaze-aimed in VR)


__all__ = ["ErodeFlowScene"]
