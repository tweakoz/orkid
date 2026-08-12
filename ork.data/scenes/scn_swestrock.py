###############################################################################
# scn_swestrock.py — STRATA PROTOTYPE viewer (SWEST2 Phase 1). The swestrock
# layered-rock terrain standalone: mesa wall north, badlands blob east, open
# flat between. NO village wiring — this scene exists to judge the strata
# prototype by eye (four-vantage A/B against the before-plates).
#
# ALWAYS WALK MODE (owner aug08): the first-person walker, unconditionally
# (1.7 m eye; walker faces -Z = north at spawn). Orbit-cam flags are ignored —
# frame stills via SWROCK_SPAWN. VR is NOT a scene decision: run the player
# with --vr for the stereo render model, exactly like every other scene.
#
#   SWROCK_SPAWN=x,y,z     walker spawn (e.g. mesa foot, badlands edge)
#   SWROCK_DIM=N           render grid   (default 2048)
#   SWROCK_BAKEDIM=N       compute/bake grid (default = render grid)
#   SWROCK_ATLAS=N         stored-atlas texels (default 8192 — owner law)
#   SWROCK_MODE=proc|stored terrain material mode (default stored)
#   SWROCK_TOD=h           opening hour (default 15.5 — raking SW light
#                          models the mesa wall); clock FROZEN for stills
#   SWROCK_FLOW_ITERS / SWROCK_EROX_S — erosion cost knobs -> dsl_kwargs (A8)
#
#   ork.scene.tojson.py -i scn_swestrock -o /tmp/swrock.ecs
#   ork.ecs.player.exe /tmp/swrock.ecs -S out.png
###############################################################################

import os

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()

TERRA = "swrock_terra"


class SwestRockScene(Scene):

  def __init__(self):
    super().__init__()

    rdim     = int(os.environ.get("SWROCK_DIM", "2048"))
    bdim     = int(os.environ.get("SWROCK_BAKEDIM", str(rdim)))
    atlas    = int(os.environ.get("SWROCK_ATLAS", "8192"))
    mode     = os.environ.get("SWROCK_MODE", "stored")
    tod      = float(os.environ.get("SWROCK_TOD", "15.5"))
    # haze look: SWROCK_HAZE = preset name (clear/hazy_day/hazy_day2/
    # bladerunner); SWROCK_HAZE_SHADOW = 1/0 arms the terrain-shadowed march
    # (air in a caster's shadow stops in-scattering direct sun)
    haze = None
    hz_preset = os.environ.get("SWROCK_HAZE")
    hz_shadow = os.environ.get("SWROCK_HAZE_SHADOW")
    if hz_preset or hz_shadow is not None:
      haze = {}
      if hz_preset:
        haze["preset"] = hz_preset
      if hz_shadow is not None:
        haze["shadow"] = hz_shadow == "1"

    dsl_kwargs = {}
    if os.environ.get("SWROCK_FLOW_ITERS"):
      dsl_kwargs["iters_flow"] = int(os.environ["SWROCK_FLOW_ITERS"])
    if os.environ.get("SWROCK_EROX_S"):
      dsl_kwargs["erox_time_s"] = float(os.environ["SWROCK_EROX_S"])

    # the scn_swest sky, trimmed: same site latitude, frozen clock (stills
    # want a named hour), sun + shadows on, no moon/stars/clouds (prototype —
    # nothing in frame at 15.5 h needs them, and perf reads cleaner)
    # NO preset here: the render model is the HOST's call (--vr selects the
    # stereo one). A scene that declared it would take VR mode away from the
    # flag, which is what the always-walk change accidentally did.
    self.sky(
        skybox_path        = "<ork_envmaps2>/desert4k.xir",
        skybox_intensity   = 1.2,
        diffuse_intensity  = 1.5,
        specular_intensity = 1.0,
        ambient_light      = vec3(0.0),
        msaa               = 2,
        ssaa               = 0,
        latitude_deg  = 36.0,
        day_of_year   = 223.0,
        time_of_day   = tod,
        time_scale    = 0.0,
        celestial     = True,
        moon          = False,
        stars         = False,
        sun_color     = vec3(1.0, 0.82, 0.60),
        sun_intensity = 2.5,
        sun_params    = {"shadow_map_size": 4096,
                         "shadow_caster": True},
        haze          = haze)

    spawn = vec3(0.0, 260.0, 0.0)
    sp = os.environ.get("SWROCK_SPAWN")
    if sp:
      p = [float(c) for c in sp.replace(" ", "").split(",")]
      spawn = vec3(p[0], p[1], p[2])

    self.terrain(TERRA,
                 dsl_file         = "swestrock",
                 dsl_kwargs       = dsl_kwargs or None,
                 spawn            = spawn,
                 chunk            = 128,
                 render_dimension = rdim,
                 bake_dimension   = bdim,
                 bake_res         = atlas,
                 mode             = mode,
                 walkable         = True)


__all__ = ["SwestRockScene"]
