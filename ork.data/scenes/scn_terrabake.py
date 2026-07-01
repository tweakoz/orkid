#!/usr/bin/env python3
###############################################################################
# scn_terrabake.py — Phase-0 spike for the TERRAIN PROCTEX TEXTURE-BAKE.
#
# Authors the erodeflow terrain (same DSL + render_dimension as scn_eflow, so it REUSES the cached
# heightfield bake) but with capture=True, so the terrain's ptex3d material additionally carries
# the FWD_SSBO_CUSTOM_CAPTURE technique. Run with ORKID_TERRAIN_TEXBAKE_DUMP=1 to bake the live
# proctex over the planar UV domain into a PBR atlas (albedo / world-normal+ao / metal-rough) and
# dump PNGs to /tmp/orkid_terrabake/ — for the baked-vs-live eyeball that proves surface() evaluates
# correctly in the UV-raster context, the world/UV math matches the live mesh, the world normal is
# usable, and the channel samplers (FlowMetrics/Basin/…) bake losslessly.
#
#   ORKID_TERRAIN_TEXBAKE_DUMP=1 ork.scene.viewer.py scn_terrabake
#   (optional) ORKID_TERRAIN_TEXBAKE_RES=4096   # atlas resolution (default 2048)
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()


class TerraBakeScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 2.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0),
        DepthPrepass      = True,
        msaa              = 3)

    self.terrain(
        "erodeflow",
        dsl_file  = "erodeflow",
        render_dimension = 1600,
        mode      = "stored",   # Phase-1: capture the proctex to <assetcache>/ptex3d_capture/<key>/ (cached)
        bake_res  = 4096
        )       # (terrain still RENDERS proc until the reconstruction lands — Increment 2)


__all__ = ["TerraBakeScene"]
