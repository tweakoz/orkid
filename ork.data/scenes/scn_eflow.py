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

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.3,
        DiffuseIntensity  = 2.5,
        SpecularIntensity = 1.5,
        AmbientLight      = vec3(0),
        CullFrustumScale   = 1.3,   # TEMP A/B TEST: narrow cull frustum (cull-more) — revert after
        DepthPrepass       = True,   # resolves a single-sample depth (the HZB occlusion source) + early-Z
        msaa              = 3)

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
