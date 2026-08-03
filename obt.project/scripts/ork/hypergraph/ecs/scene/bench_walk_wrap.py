###############################################################################
# bench_walk_wrap — the ORKEXP_SCENE_WRAP overlay that turns ANY walkable scene
# into the stereo frame-time bench:
#
#   ORKEXP_SCENE_WRAP=ork.hypergraph.ecs.scene.bench_walk_wrap:wrap \
#   ork.scene.viewer.py scn_forest_procsky --offscreen --no-devkeys
#
# TWO overlays, no scene edits:
#   1. preset -> FWDPBRVRDM. With no live XR runtime the SceneGraphSystem
#      auto-registers a NoVrDevice at 1280x1280 per eye, so the dual-mono VR
#      output node renders a 2560x1280 stereo surface — the VR frame cost on a
#      desktop GPU, measurable headless.
#   2. bench_walk_system.py appended to the PythonSystem. It publishes the camera
#      as a pure function of sim time (see that file); the scene keeps its own
#      primary script (walker input), which is order-independent of appended ones.
#
# The preset is forced on EVERY scenegraph() call, so a scene family that amends
# its scenegraph after declaring it (the sky library's dome half) cannot restore
# the mono preset behind our back.
#
# A/B KNOBS (both absent = the bench above, unchanged):
#
#   BENCH_PRESET=<preset>   use this preset instead of FWDPBRVRDM. The point of
#                           BENCH_PRESET=ForwardPBR is the MONO-VS-STEREO
#                           discriminator: DMVR renders two SEQUENTIAL per-eye
#                           passes into a 2560x1280 surface, mono renders ONE
#                           1280x1280 pass. PER-EYE PIXEL COUNT IS IDENTICAL, so
#                           the mono-to-stereo FPS ratio prices the second eye
#                           pass directly (a ratio near 2 means the second pass
#                           costs a whole frame; a ratio near 1 means it is
#                           shared work). Nothing else about the run changes —
#                           same tour, same witness, same scene state — so the
#                           two runs stay comparable frame for frame.
#
#   BENCH_MSAA=<n>          override the scene's msaa scenegraph param (forest
#                           hardcodes msaa=2). Comparability caveat: this changes
#                           what the frames LOOK like as well as what they cost —
#                           it is a cost discriminator, not an alternative
#                           baseline, and it must not be mixed into a look gate.
#
#   BENCH_IBL=live          restore sky-IBL refiltering. THE DEFAULT IS OFF (owner
#                           call, scoped "for now"): the refilter cycle is the
#                           current wedge trigger, and a bench that wedges mid-tour
#                           measures nothing. OFF is implemented as the PACING KNOB
#                           SkyAtmosphereData.ibl_chain_min_angle_deg raised past
#                           any reachable sun motion, which suppresses only the
#                           CHAINED refilter cycles. Everything else stays live:
#                           the world clock, the celestial motion, the shadow
#                           cascade refit, the cloud drift, and the FIRST IBL bake
#                           (the "never snapped" trigger is not an angle test). The
#                           cost is that the IBL stops tracking the sun, so the
#                           lighting slowly goes stale over a long run — which is
#                           why this is a measurement mode with an off switch and
#                           not a scene change. FOREST_TIME_SCALE=0 was rejected as
#                           the mechanism: freezing the clock also freezes the
#                           shadow refit and the sky, i.e. it changes the frame
#                           being measured.
#
# The atmosphere object this attaches is a DEFAULT-CONSTRUCTED SkyAtmosphereData,
# which is exactly what the engine attaches for itself when a procedural-sky scene
# declares none (fwdnode_impl_sub.cpp:1654) — so the medium is unchanged and only
# the one pacing knob differs.
###############################################################################
import os

_DRIVER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bench_walk_system.py")

# past any reachable sun delta (the trigger compares against an angle in 0..180)
_IBL_CHAIN_OFF_DEG = 1.0e6


def wrap(scene_class, args):

  preset = os.environ.get("BENCH_PRESET", "FWDPBRVRDM")
  msaa = os.environ.get("BENCH_MSAA", None)
  if msaa is not None:
    msaa = int(msaa)
  ibl_live = os.environ.get("BENCH_IBL", "off") == "live"

  class _BenchWalk(scene_class):

    def scenegraph(self, **kwargs):
      kwargs["preset"] = preset
      if msaa is not None:
        kwargs["msaa"] = msaa
      if not ibl_live:
        from orkengine.lev2 import SkyAtmosphereData
        atmo = kwargs.get("SkyAtmosphere") or SkyAtmosphereData()
        atmo.ibl_chain_min_angle_deg = _IBL_CHAIN_OFF_DEG
        kwargs["SkyAtmosphere"] = atmo
      print("[bench_walk_wrap] preset=%s msaa=%s ibl=%s" % (
          preset, msaa if msaa is not None else "scene", "live" if ibl_live else "chain-off"),
          flush=True)
      return super().scenegraph(**kwargs)

    def __init__(self, *a, **kw):
      super().__init__(*a, **kw)
      self.append_system_script(_DRIVER)

  _BenchWalk.__name__ = scene_class.__name__ + "_BenchWalk"
  return _BenchWalk
