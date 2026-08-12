###############################################################################
# scn_procsky.py — SKYLIGHT lane B gauge scene: the PROCEDURAL sky on screen.
#
# Walkable terrain under a directional SUN that orbits, with the scene's sky
# source switched to "procedural": the skybox is the Hillaire sky-view LUT plus
# the analytic sun disc (FWD_SKYBOX_PROC), rebuilt from the center-eye position
# every composited frame. GAUGE: watch the sky redden and the disc redden with
# it as the orbit carries the sun toward the horizon, and check that the disc
# sits where the shadows say the sun is.
#
#   ork.scene.viewer.py scn_procsky            # window: WASD move, cursor turn
#   ork.scene.viewer.py scn_procsky --vr       # stereo (both eyes share one LUT)
#
# Pass sky_source="baked" to the self.sky() call below to A/B the same scene
# against the equirect envmap — that envmap is still what lights the scene
# either way (the IBL feed from the procedural sky is a later slice), so only
# the visible sky changes.
###############################################################################

from orkengine.core import vec3, vec4, lev2_pyexdir
from orkengine import ecs as _ecs

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()

TERRA    = "procsky_terra"
RELIEF_M = 100.0   # scatterhills default relief — navigable ~5-15deg slopes


class ProcSkyScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # The sky — library defaults: procedural sky source + the shared exposure,
    # plus the celestial ensemble (sun + moon + star dome) on the library's
    # observer-frame day, opening just before dawn so the run starts on sunrise.
    # The sun's direction is what the prologue bakes the sky-view LUT with, so
    # sky, disc and shadows all track together; the moon takes the cascade as
    # the sun sets (no POP — see _night_policy.py) and the star dome wheels once
    # about the pole overnight.
    #
    # CLOUD COVER 0.35 — the reference sky's second knob (the first is time of
    # day). Scattered-to-broken decks: enough cloud that the sky reads as a sky
    # and the decks show their silver lining, sunset colour and moonlit
    # undersides, while roughly two thirds of the dome stays open so the disc,
    # the moon and the star field — the work this scene exists to judge — are
    # never hidden behind an overcast. Owner re-cuts by eye; the whole range is
    # one number (0 = clear, 1 = overcast).
    ##########################

    self.sky(
        cloud_cover = 0.35,
        # IBL source (unchanged by the sky-source switch in this milestone)
        skybox_path = "<ork_envmaps2>/desert4k.xir",
        msaa        = 2,
        ssaa        = 0)

    ##########################
    # Walkable terrain — the lightest walkable terrain DSL in the repo (plain
    # fbm, no erosion loop), baked small so the scene loads fast and the horizon
    # line is real geometry.
    ##########################

    self.terrain(TERRA,
                 dsl_file         = "scatterhills",
                 spawn            = vec3(0.0, RELIEF_M + 10.0, 0.0),
                 chunk            = 128,
                 render_dimension = 512,
                 bake_dimension   = 512,
                 walkable         = True)

    ##########################
    # PBR gauge sphere — 1m MIRROR ball hovering 10m above the terrain at the
    # origin (same RELIEF_M+10 convention the walker spawn uses): a perfect
    # metallic mirror reflects the IBL feed raw, so refilter swaps, fade
    # quality and sky/disc alignment are judged directly on its surface.
    ##########################

    A = self.asset
    gauge_mtl = A.PbrMaterial("pbr_gauge_mtl",
                              base_color = vec4(1.0, 1.0, 1.0, 1.0),
                              metallic   = 1.0,
                              roughness  = 0.0)
    gauge_drw = A.IcoSphere("pbr_gauge_sphere",
                            radius       = 1.0,
                            subdivisions = 5,
                            material     = gauge_mtl)
    gauge_shape        = _ecs.BulletShapeSphereData()
    gauge_shape.radius = 1.0
    gauge_phys = self.declare_component(
        "BulletObjectComponent",
        shape          = gauge_shape,
        mass           = 5.0,
        friction       = 0.9,
        restitution    = 0.1,
        angularDamping = 0.8,       # settle where it lands; a mirror ball rolling downhill gauges nothing
        linearDamping  = 0.02)
    self.entity("pbr_gauge",
                transform  = {"translation": vec3(0.0, RELIEF_M + 10.0, 0.0)},
                components = [self.SG.component(nodes={"n": {"drawable": gauge_drw}}),
                              gauge_phys])


__all__ = ["ProcSkyScene"]
