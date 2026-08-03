###############################################################################
# scn_xxx3.py — the xxx3 terrain as a HYPERECS scene. self.terrain() loads the REAL xxx3
# DSL (zero reimplementation) and reads its OWN scale + look: EXTENT_M (32 km), HEIGHT_M
# (4 km), MATERIAL_CLASS (XXX3Mat — height/slope strata + FlowMap AO) and its
# SAMPLER_CHANNELS (FlowMap <- flow_discharge). Nothing about scale/material/samplers is
# restated here. Bakes (cook-cached) + plays zero-Python in the player.
#
# render_dimension is the render-mesh res; bake_dimension (default = it) is the compute/bake res
# (DSL is res-independent via EXTENT_M); bump bake_dimension for more detail (first bake slow, then cached).
#
#   ork.scene.viewer.py scn_xxx3
#   ork.scene.tojson.py -i scn_xxx3 -o /tmp/xxx3.ecs
#   ork.ecs.player.exe /tmp/xxx3.ecs --camdist 15000 --camheight 5000
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()


class XXX3Scene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 3.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.00),
        CullFrustumScale   = 1.0,   # TEMP A/B TEST: narrow cull frustum (cull-more) — revert after
        msaa = 3)

    self.terrain(
        "xxx3",
        dsl_file  = "xxx3",
        render_dimension = 2048,
        bake_dimension  = 4096,
        bake_res  = 4096,                
        walkable  = True)

    self.projectile_pool(fire=True)   # '/' shoots fireballs (gaze-aimed in VR)


__all__ = ["XXX3Scene"]
