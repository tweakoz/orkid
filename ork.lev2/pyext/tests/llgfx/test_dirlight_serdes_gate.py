#!/usr/bin/env ork.python
###############################################################################
# Gate — a DirectionalLightData (the SUN) survives the reflection round trip
# with every cascade knob intact.
#
# The sun's whole tunable surface is reflected state on this one object:
# cascade count, map size, max distance, PCF dither, depth bias, the celestial
# body tag, the WORLD-BAND geometry (radius/ratio, the per-band resolution step
# and the per-snapshot jitter), and the SNAPSHOT INTERVAL that holds the cascade
# fit between ticks. Nothing
# covered it — a knob added to describeX but not to the header (or the other way
# round) serialized as a default, and a scene that authored it would render as
# if it had said nothing. Asserts:
#   1. DEFAULTS the engine ships (asserted, not authored): a fresh sun holds
#      interval 0 — cascades render EVERY frame unless a scene asks otherwise —
#      plus the 2048/4/250/1.0 cascade rig and the 10m/4x world bands every
#      procedural-sky scene is
#      balanced against. A silent default drift here is a silent behavior
#      change on every shadowed scene.
#   2. ENCODE: an off-default value for every python-settable reflected knob
#      reaches the JSON as that value (not "null:", not the default).
#   3. DECODE: deserialize + re-serialize reproduces the same property node —
#      which is what proves the values came BACK (a field written but never
#      read reappears as its default and this comparison fails);
#   3b. the RETIRED drift-trigger knobs (translation deadband / light-angle
#      threshold) are GONE from both surfaces. The declared interval is the sole
#      cadence, so a resurrected threshold would silently refit a scene that
#      asked for a one-minute shadow — the exact defect their removal fixed.
#
# ...and the DSL seam that reaches those knobs, because the interval is the one
# knob whose whole point is that a scene declares it ONCE for a whole SKY:
#   4. sky(shadow_snapshot_interval=) lands in sun_params (an unnamed kwarg would
#      fall through **dome_params into declareParams, where unknown keys are
#      SILENTLY DROPPED — it would look accepted and do nothing), and is loud on
#      being said twice or said to a sky with no sun;
#   5. BOTH ensemble lights carry it — the sun AND the moon that takes the
#      cascade off it at night. A cadence that died at the handoff would deliver
#      nothing for the whole night it was declared for.
#
# Non-render gate: no frames, no capture — serialization + declaration only.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # headless_appinit — bare imports serialize EMPTY (no class registration)
from orkengine.core import Object, vec3

# authored values — deliberately off every default
CASCADES        = 2
MAP_SIZE        = 1024
MAX_DISTANCE    = 137.5
PCF_DITHER      = 2.5
SHADOW_BIAS     = 0.00375
SNAP_INTERVAL   = 2.5     # the slice-4 knob: seconds between cascade snapshots
BAND_RADIUS     = 6.25    # world-band geometry (W7): innermost radius + ratio
BAND_RATIO      = 3.5
BAND_RES_RATIO  = 2.0     # per-band resolution step (1 = every band at the full dim)
JITTER_TEXELS   = 0.5     # per-snapshot sub-texel fit jitter (0 = the shipped fit)
BANDS_PER_FRAME = 1       # S2a: bands one frame may render (0 = all, shipped)
CROSSFADE_FRAMES = 8      #  and the flip crossfade window (0 = hard swap, shipped)
CROSSFADE_SECS   = 0.35   #  ...whose wall-clock length is the fps-invariant half
INTENSITY       = 3.25
PRIORITY        = 7.5
SKY_BODY        = 2       # moon
COLOR           = vec3(0.25, 0.5, 0.75)

# what the engine SHIPS. Asserted rather than authored:
#  * a 0 interval is the every-frame law — any other default silently holds
#    shadows on scenes that never asked for a hold;
#  * the rest is the cascade rig every shadowed scene's look and cost are
#    balanced against.
DEFAULT_SNAP_INTERVAL = 0.0
DEFAULT_CASCADES      = 4
DEFAULT_MAP_SIZE      = 2048
DEFAULT_MAX_DISTANCE  = 250.0
DEFAULT_PCF_DITHER    = 1.0
DEFAULT_BAND_RADIUS   = 10.0
DEFAULT_BAND_RATIO    = 4.0
# the quality pair ships DISARMED too: every band at the full map dim, and no
# jitter on the fit — a scene that says nothing renders the shipped cascade.
DEFAULT_BAND_RES_RATIO = 1.0
DEFAULT_JITTER_TEXELS  = 0.0
# the drift triggers these two fed are REMOVED (owner ruling jul30 — the
# declared interval is the cadence, not a ceiling). Named here so the gate can
# assert their ABSENCE by name.
RETIRED_PY_PROPS   = ("shadowTranslationDeadband", "shadowLightAngleThreshold")
RETIRED_JSON_PROPS = ("ShadowTranslationDeadband", "ShadowLightAngleThreshold")
# the amortization pair ships DISARMED: a scene that says nothing gets the
# whole snapshot in one frame and a hard swap — exactly what shipped before
# the double buffer existed (and no second set of shadow slices allocated).
DEFAULT_BANDS_PER_FRAME  = 0
DEFAULT_CROSSFADE_FRAMES = 0
# ...but the fade, once a scene ARMS it with frames, is a DURATION by default:
# a frame count alone made the blend 24ms offscreen and 200ms at 60Hz.
DEFAULT_CROSSFADE_SECS   = 0.2


def _find_light(node):
  """the DirectionalLightData property dict, wherever it sits in the tree"""
  if isinstance(node, dict):
    obj = node.get("object")
    if isinstance(obj, dict) and obj.get("class") == "DirectionalLightData":
      return obj["properties"]
    for v in node.values():
      found = _find_light(v)
      if found is not None:
        return found
  elif isinstance(node, list):
    for v in node:
      found = _find_light(v)
      if found is not None:
        return found
  return None


def _build_light():
  light = lev2.DirectionalLightData()
  assert abs(light.shadowSnapshotInterval - DEFAULT_SNAP_INTERVAL) < 1e-9, (
      "snapshot-interval default moved to %f — cascades no longer render every "
      "frame by default" % light.shadowSnapshotInterval)
  assert light.shadowCascadeCount == DEFAULT_CASCADES, (
      "cascade-count default moved to %d" % light.shadowCascadeCount)
  assert light.shadowMapSize == DEFAULT_MAP_SIZE, (
      "shadow-map-size default moved to %d" % light.shadowMapSize)
  assert abs(light.shadowMaxDistance - DEFAULT_MAX_DISTANCE) < 1e-6, (
      "shadow-max-distance default moved to %f" % light.shadowMaxDistance)
  assert abs(light.pcfDither - DEFAULT_PCF_DITHER) < 1e-6, (
      "pcf-dither default moved to %f" % light.pcfDither)
  assert abs(light.shadowBandRadius - DEFAULT_BAND_RADIUS) < 1e-6, (
      "band-radius default moved to %f" % light.shadowBandRadius)
  assert abs(light.shadowBandRatio - DEFAULT_BAND_RATIO) < 1e-6, (
      "band-ratio default moved to %f" % light.shadowBandRatio)
  assert abs(light.shadowBandResRatio - DEFAULT_BAND_RES_RATIO) < 1e-6, (
      "band-res-ratio default moved to %f — outer cascades no longer render at "
      "the full map dim by default" % light.shadowBandResRatio)
  assert abs(light.shadowJitterTexels - DEFAULT_JITTER_TEXELS) < 1e-6, (
      "jitter default moved to %f — snapshot fits are no longer unjittered by "
      "default" % light.shadowJitterTexels)
  for name in RETIRED_PY_PROPS:
    assert not hasattr(light, name), (
        "%s is back on DirectionalLightData — a drift trigger would make the "
        "declared snapshot interval advisory again" % name)
  assert light.shadowSnapshotBandsPerFrame == DEFAULT_BANDS_PER_FRAME, (
      "bands-per-frame default moved to %d — snapshots no longer render whole "
      "in one frame by default" % light.shadowSnapshotBandsPerFrame)
  assert light.shadowCrossfadeFrames == DEFAULT_CROSSFADE_FRAMES, (
      "crossfade-frames default moved to %d — snapshot flips no longer swap "
      "hard by default" % light.shadowCrossfadeFrames)
  assert abs(light.shadowCrossfadeSecs - DEFAULT_CROSSFADE_SECS) < 1e-6, (
      "crossfade-secs default moved to %f — an armed fade's wall-clock length "
      "is what makes the blend fps-invariant" % light.shadowCrossfadeSecs)
  light.color                  = COLOR
  light.intensity              = INTENSITY
  light.priority               = PRIORITY
  light.sky_body               = SKY_BODY
  light.shadowCaster           = True
  light.shadowBias             = SHADOW_BIAS
  light.shadowMapSize          = MAP_SIZE
  light.shadowCascadeCount     = CASCADES
  light.shadowMaxDistance      = MAX_DISTANCE
  light.pcfDither              = PCF_DITHER
  light.shadowSnapshotInterval = SNAP_INTERVAL
  light.shadowBandRadius          = BAND_RADIUS
  light.shadowBandRatio           = BAND_RATIO
  light.shadowBandResRatio        = BAND_RES_RATIO
  light.shadowJitterTexels        = JITTER_TEXELS
  light.shadowSnapshotBandsPerFrame = BANDS_PER_FRAME
  light.shadowCrossfadeFrames       = CROSSFADE_FRAMES
  light.shadowCrossfadeSecs         = CROSSFADE_SECS
  return light


def _assert_knobs(props, tag):
  assert props["ShadowCascadeCount"] == CASCADES, (tag, props)
  assert props["ShadowMapSize"] == MAP_SIZE, (tag, props)
  assert props["SkyBody"] == SKY_BODY, (tag, props)
  assert props["ShadowCaster"] in (True, 1, "true"), (tag, props)
  assert abs(props["ShadowMaxDistance"] - MAX_DISTANCE) < 1e-4, (tag, props)
  assert abs(props["PcfDither"] - PCF_DITHER) < 1e-6, (tag, props)
  assert abs(props["ShadowBias"] - SHADOW_BIAS) < 1e-7, (tag, props)
  assert abs(props["ShadowSnapshotInterval"] - SNAP_INTERVAL) < 1e-6, (tag, props)
  assert abs(props["ShadowBandRadius"] - BAND_RADIUS) < 1e-6, (tag, props)
  assert abs(props["ShadowBandRatio"] - BAND_RATIO) < 1e-6, (tag, props)
  assert abs(props["ShadowBandResRatio"] - BAND_RES_RATIO) < 1e-6, (tag, props)
  assert abs(props["ShadowJitterTexels"] - JITTER_TEXELS) < 1e-6, (tag, props)
  for name in RETIRED_JSON_PROPS:
    assert name not in props, (tag, "retired drift-trigger property still serialized", name)
  assert props["ShadowSnapshotBandsPerFrame"] == BANDS_PER_FRAME, (tag, props)
  assert props["ShadowCrossfadeFrames"] == CROSSFADE_FRAMES, (tag, props)
  assert abs(props["ShadowCrossfadeSecs"] - CROSSFADE_SECS) < 1e-6, (tag, props)
  assert abs(props["Intensity"] - INTENSITY) < 1e-6, (tag, props)
  assert abs(props["Priority"] - PRIORITY) < 1e-6, (tag, props)
  assert "Color" in props, (tag, props)   # encoding is the vec3 codec's business; identity is asserted below


###############################################################################
# DSL seam — the two light mixins against a STUB scene. sun()/moon()/sky() only
# touch the handful of Scene hooks stubbed below, so the forwarding can be
# asserted without authoring (or cooking) a real scene.
###############################################################################

class _StubSky:
  """captures what sky() forwards, in place of the real dome/ensemble calls"""

  def __init__(self):
    self.celestial_kwargs = None

  def sky_dome(self, **kw):
    return "handle"

  def cloud_decks(self, **kw):
    pass

  def append_system_script(self, *a, **kw):
    pass

  def celestial_sky(self, **kw):
    self.celestial_kwargs = kw


class _StubEnsemble:
  """captures the DirectionalLightData objects sun()/moon() actually build"""

  class _SG:
    def component(self, nodes=None):
      return ("sgnodes", nodes or {})

  def __init__(self):
    self.SG = _StubEnsemble._SG()
    self.lights = {}

  def _ensure_system(self, name):
    pass

  def declare_component(self, kind, **kw):
    return (kind, kw)

  def entity(self, name, transform=None, components=None):
    for c in components or []:
      if isinstance(c, tuple) and c[0] == "sgnodes":
        for node_name, node in c[1].items():
          self.lights[node_name] = node["drawable"]
    return name


def _assert_dsl_seam():
  from ork.hypergraph.ecs.scene._sky import SkyMixin
  from ork.hypergraph.ecs.scene._sun import SunMixin
  from ork.hypergraph.ecs.scene._moon import MoonMixin

  class _Sky(SkyMixin, _StubSky):
    pass

  class _Ensemble(SunMixin, MoonMixin, _StubEnsemble):
    pass

  # 4a — the named kwarg reaches Scene.sun() through sun_params
  s = _Sky()
  s.sky(shadow_snapshot_interval=SNAP_INTERVAL)
  fwd = (s.celestial_kwargs or {}).get("sun_params") or {}
  assert abs(fwd.get("shadow_snapshot_interval", -1) - SNAP_INTERVAL) < 1e-6, fwd
  # ...and an un-declared interval leaves sun_params alone
  s2 = _Sky()
  s2.sky()
  assert "shadow_snapshot_interval" not in ((s2.celestial_kwargs or {}).get("sun_params") or {})

  # 4b — loud on two spellings of one cadence
  try:
    _Sky().sky(shadow_snapshot_interval=1.0,
               sun_params={"shadow_snapshot_interval": 2.0})
    raise AssertionError("sky() accepted the cadence twice")
  except ValueError:
    pass

  # 4c — loud when there is no sun to carry it
  try:
    _Sky().sky(shadow_snapshot_interval=1.0, celestial=False)
    raise AssertionError("sky() accepted a cadence for a sky with no sun")
  except ValueError:
    pass

  # 5 — BOTH ensemble lights carry the declared cadence (the moon inherits it,
  # the same way it inherits the sun's site and clock)
  e = _Ensemble()
  e.sun(celestial={"latitude_deg": 36.0, "day_of_year": 223.0,
                   "time_of_day": 18.0, "time_scale": 480.0},
        shadow_snapshot_interval=SNAP_INTERVAL,
        shadow_band_radius=BAND_RADIUS,
        shadow_band_ratio=BAND_RATIO,
        shadow_band_res_ratio=BAND_RES_RATIO,
        shadow_jitter_texels=JITTER_TEXELS,
        shadow_snapshot_bands_per_frame=BANDS_PER_FRAME,
        shadow_crossfade_frames=CROSSFADE_FRAMES,
        shadow_crossfade_secs=CROSSFADE_SECS)
  e.moon()
  for which in ("sun", "moon"):
    got = e.lights[which].shadowSnapshotInterval
    assert abs(got - SNAP_INTERVAL) < 1e-6, (
        "%s cadence %f, declared %f" % (which, got, SNAP_INTERVAL))
    # the world-band rig is the SKY's too — declared on the sun, inherited by
    # the moon that renders into the same bands at night
    light = e.lights[which]
    for label, got, want in (
        ("band radius", light.shadowBandRadius, BAND_RADIUS),
        ("band ratio", light.shadowBandRatio, BAND_RATIO),
        ("band res ratio", light.shadowBandResRatio, BAND_RES_RATIO),
        ("jitter texels", light.shadowJitterTexels, JITTER_TEXELS),
        # how the snapshot is PAID FOR travels with the cadence: a moon that
        # inherited the interval but not the amortization would spike on every
        # night-time snapshot the sun was spreading over frames
        ("crossfade secs", light.shadowCrossfadeSecs, CROSSFADE_SECS),
        ("bands per frame", light.shadowSnapshotBandsPerFrame, BANDS_PER_FRAME),
        ("crossfade frames", light.shadowCrossfadeFrames, CROSSFADE_FRAMES)):
      assert abs(got - want) < 1e-6, (
          "%s %s %f, declared %f" % (which, label, got, want))

  # ...and with nothing declared, both stay on the every-frame default
  e2 = _Ensemble()
  e2.sun(name="sun2", celestial={"latitude_deg": 36.0, "day_of_year": 223.0,
                                 "time_of_day": 18.0, "time_scale": 480.0})
  e2.moon(name="moon2", sun="sun2")
  for which in ("sun2", "moon2"):
    got = e2.lights[which].shadowSnapshotInterval
    assert abs(got - DEFAULT_SNAP_INTERVAL) < 1e-9, (which, got)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ok = False
  try:
    js1 = _build_light().serializeJson()
    assert '"null:"' not in js1.replace(" ", ""), "a light property encoded as null: " + js1
    props1 = _find_light(json.loads(js1))
    assert props1 is not None, "no DirectionalLightData node in the json:\n" + js1
    _assert_knobs(props1, "encode")
    print("dirlight ENCODE PASS (%d reflected properties)" % len(props1), flush=True)

    js2    = Object.deserializeJson(js1).serializeJson()
    props2 = _find_light(json.loads(js2))
    assert props2 is not None, "round trip LOST the light:\n" + js2
    _assert_knobs(props2, "decode")
    assert props2 == props1, "round trip altered the sun"
    print("dirlight DECODE PASS (round trip identical)", flush=True)

    _assert_dsl_seam()
    print("dirlight DSL PASS (sky() -> sun_params, sun AND moon carry the cadence)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== dirlight serdes gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
