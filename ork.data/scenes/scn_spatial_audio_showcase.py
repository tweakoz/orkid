###############################################################################
# scn_spatial_audio_showcase.py — SPATIAL AUDIO SHOWCASE: prove the general
# ECS sound system in one walkable-scale world. "Wind going through the trees,
# streams, urban streets" — ambience lives at LOCATIONS:
#
#   * TREELINE (north edge, z=-40): WIND — the clearly-SYNTHESIZED source, a
#     hypersound filtered-noise patch (S.noise -> gust-LFO-modulated lowpass)
#     materialized LIVE by the HOST layer (WindLayer below) and spatialized
#     with a per-frame-driven PANNER2D stage. Plus 6 stochastic BIRD emitters
#     (StochWav one-shot bursts, band 2.5-5 kHz).
#   * SPRING POOL (0, 15): a stream you approach and pass — GRANULAR StochWav
#     water bed (overlapping 1.6-2.1 s burble grains, long crossfades — the
#     granular-loop idiom) + a SimpleSound looped stream (written now, plays
#     when the SimpleSoundEmitter revival lands).
#   * URBAN CORNER (45, -15): machine-hum loop (SimpleSound, revival-gated),
#     metallic CLANK bursts + distant traffic wash (StochWav).
#   * RUNTIME-SPAWNED CHIMES: from t=45 s the scene's own PythonSystem script
#     (_spatial_audio_showcase_sys.py) spawns short-lived chime entities at
#     randomized positions — entities that did not exist at scene-author time,
#     each carrying a StochWav emitter, auto-despawned by spawner lifetime.
#
# BUS PLAN (A0c named busses via GlobalSynthSystem): "ambience" (wind,
# traffic, hum), "water" (stream grains + loop), "points" (birds, chimes,
# clanks). Spatialization: shared PannerSpatializerData -> per-voice PANNER2D.
#
# All sampled assets are procedurally baked, spectrally banded, by
# ork.spatialaudio.bakewavs.py (bands are the harness measurement contract).
#
# CLASSES (pass --class to disambiguate):
#   SpatialAudioShowcase — visual scene: skybox + ground grid + location
#     markers + full audio. For the players / .ecs artifact.
#   AudioHarnessScene    — same audio + camera-only scenegraph, no drawables.
#     For ork.spatialaudio.harness.py (headless WAV-render gate).
#
#   ork.scene.tojson.py -i scn_spatial_audio_showcase \
#       --class SpatialAudioShowcase -o /tmp/spatial_audio_showcase.ecs
#   ork.ecs.player.exe /tmp/spatial_audio_showcase.ecs --audio   # (no wind:
#       the wind host layer needs a python host — use ork.spatialaudio.demo.py)
###############################################################################

import math
import os

from orkengine.core import vec3
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene import Scene, Transform

###############################################################################
# WORLD LAYOUT (meters; y up; ground plane y=0; human eye 1.7 m).
# Keep in sync with _spatial_audio_showcase_sys.py (chime constants) — the
# PythonSystem script runs in a sub-interpreter and cannot import this module.
###############################################################################

WAV_BASE = "data://sounds/showcase/"

# treeline row along z=-40
TREELINE_Z = -40.0
BIRD_POSITIONS = [vec3(x, 6.0, TREELINE_Z) for x in (-25.0, -15.0, -5.0, 5.0, 15.0, 25.0)]
WIND_ANCHOR = vec3(0.0, 8.0, TREELINE_Z)   # canopy-height wind centroid

# AMBISONIC BED (the scene's one SoundFieldProbe): a 4ch FOA AmbiX loop
# anchored in the canopy just below the wind centroid, radially weighted off
# the LISTENER position. maxDistance is deliberately smaller than the walk's
# extent so seg A/B (55-80 m out) sit fully outside it — the bed's band is
# silent there, which is what makes "the probe is what you hear" measurable.
AMBIX_BED_POS = vec3(0.0, 6.0, TREELINE_Z)
AMBIX_BED_REF = 8.0                        # full weight inside this radius
AMBIX_BED_MAX = 45.0                       # silent at or beyond it

# spring pool (the "stream you approach and pass")
POOL_CENTER = vec3(0.0, 0.3, 15.0)
POOL_EMITTERS = [vec3(-1.5, 0.3, 15.0), vec3(1.5, 0.3, 15.0)]

# urban corner
URBAN_HUM_POS = vec3(45.0, 2.0, -15.0)     # machine hum (SimpleSound loop)
URBAN_CLANK_POSITIONS = [vec3(48.0, 1.0, -18.0), vec3(43.0, 1.0, -20.0)]
URBAN_TRAFFIC_POS = vec3(55.0, 3.0, -10.0)

# runtime-spawned chimes (script-owned; constants duplicated in the sys script)
# — centered near the seg-E hold point (30,1.7,-12) so the runtime chimes ring
# 8-16 m from the held listener.
CHIME_CENTER = vec3(25.0, 2.0, -14.0)
CHIME_START_TIME = 45.0
CHIME_PERIOD = 6.0
CHIME_LIFETIME = 6.0

# SimpleSoundEmitter content is written but the system is mid-revival by the
# engine lane; the env knob lets hosts drop it while the installed build's
# SimpleSoundEmitterSystem is dead (ORK_SA_SIMPLESOUND=0).
ENABLE_SIMPLESOUND = os.environ.get("ORK_SA_SIMPLESOUND", "1") == "1"


###############################################################################
# audio data helpers
###############################################################################

def _sound(wav, **props):
  snd = ecs.StochWavSound()
  snd.wavFilePath = WAV_BASE + wav
  for key, val in props.items():
    setattr(snd, key, val)
  return snd


def _group(bus, master_db, max_voices, min_spacing, fade_in, fade_out, sounds):
  grp = ecs.StochSoundGroup()
  grp.outputBusName = bus
  grp.masterGainDB = master_db
  grp.maxVoicesPerGroup = max_voices
  grp.minSpacing = min_spacing
  grp.fadeInTime = fade_in
  grp.fadeOutTime = fade_out
  for name, snd in sounds.items():
    grp.addSound(name, snd)
  return grp


def _spatializer():
  spat = lev2.singularity.PannerSpatializerData()
  spat.refDistance = 2.0     # full level within 2 m of a source
  spat.maxDistance = 150.0
  spat.rolloff = 1.0
  spat.minGainDB = -60.0
  spat.headShadowMix = 1.0
  return spat


###############################################################################
# declare_audio(scene) — the shared audio composition, used by BOTH classes.
###############################################################################

def declare_audio(scene):

  # debug bisect ladder (harness-only): 0=nothing 1=busses 2=+groups
  # 3=+emitters 4=+chime arch 5=full (pysys script). default full.
  level = int(os.environ.get("ORK_SA_LEVEL", "5"))
  if level < 1:
    return

  # ---- busses (A0c named sends) --------------------------------------------
  gsys = scene.system_data("GlobalSynthSystem", masterGainDB=0.0)
  for bus_name, gain_db in (("ambience", 0.0), ("water", 0.0), ("points", 0.0)):
    cfg = ecs.SynthBusConfig()
    cfg.effectPreset = "none"
    cfg.gainDB = gain_db
    cfg.pan = 0.0
    gsys.sub_calls.append(("addBusConfig", (bus_name, cfg), {}))

  # ---- the ambisonic bed: one SoundFieldProbe (SF1) ------------------------
  # The probe sums into the engine's single B-format mix point, which owns its
  # own "soundfield" OutputBus — so it needs none of the named busses above.
  #
  # SoundFieldSystemData MUST be declared HERE. SoundFieldProbeData has a
  # DoRegisterWithScene that registers it, but that hook has no live call site
  # (ArchComposer::Register, its only caller, is commented out in
  # archetype.inl) and Scene.build lowers only explicitly declared systems.
  # Omit this line and the component links with a null _system, _onLink still
  # returns true, and _onActivate SEGVs on entity activation with nothing
  # logged (observed; filed).
  scene.system_data("SoundFieldSystemData",
                    masterGainDB=-20.0,   # the bed sits under the point sources
                    slewTime=0.25)
  scene.entity("ambix_bed_e",
               transform=Transform(translation=AMBIX_BED_POS),
               components=[scene.declare_component(
                   "SoundFieldProbeData",
                   ambixAsset=WAV_BASE + "treeline_ambix.wav",
                   gain=1.0,
                   loop=True,
                   startPaused=False,
                   refDistance=AMBIX_BED_REF,
                   maxDistance=AMBIX_BED_MAX,
                   rolloff=1.0)])

  # ---- SimpleSound system: loops (revival-gated) ---------------------------
  if ENABLE_SIMPLESOUND:
    sssys = scene.system_data("SimpleSoundEmitterSystem")
    sssys.kwargs["spatializer"] = _spatializer()

    stream_loop = ecs.SimpleSoundData()
    stream_loop.wavfile_path = WAV_BASE + "stream_loop.wav"
    stream_loop.looping = True
    stream_loop.spatialize = True
    stream_loop.gainDB = -8.0
    stream_loop.fadeInTime = 0.5
    stream_loop.fadeOutTime = 0.5
    stream_loop.outputBusName = "water"
    sssys.sub_calls.append(("addSound", ("stream_loop", stream_loop), {}))

    hum = ecs.SimpleSoundData()
    hum.wavfile_path = WAV_BASE + "hum_loop.wav"
    hum.looping = True
    hum.spatialize = True
    hum.gainDB = -6.0
    hum.fadeInTime = 0.5
    hum.fadeOutTime = 0.5
    hum.outputBusName = "ambience"
    sssys.sub_calls.append(("addSound", ("machine_hum", hum), {}))

    scene.entity("stream_loop_e",
                 transform=Transform(translation=POOL_CENTER),
                 components=[scene.declare_component(
                     "SimpleSoundEmitterData",
                     soundName="stream_loop",
                     autoPlay=True, enabled=True)])
    scene.entity("machine_hum_e",
                 transform=Transform(translation=URBAN_HUM_POS),
                 components=[scene.declare_component(
                     "SimpleSoundEmitterData",
                     soundName="machine_hum",
                     autoPlay=True, enabled=True)])

  if level < 2:
    return

  # ---- StochWav system: groups + spatializer -------------------------------
  swsys = scene.system_data("StochWavSoundEmitterSystem")
  swsys.kwargs["spatializer"] = _spatializer()

  water = _group(
      bus="water", master_db=-4.0, max_voices=10, min_spacing=0.05,
      fade_in=0.25, fade_out=0.5,
      sounds={
        "grain_a": _sound("water_grain_a.wav", burstRate=1.2, burstCountMin=1,
                          burstCountMax=2, intraBurstRate=2.0,
                          selectionWeight=1.0, pitchVarianceCents=100.0,
                          gainMinDB=-4.0, gainMaxDB=-1.0),
        "grain_b": _sound("water_grain_b.wav", burstRate=1.2, burstCountMin=1,
                          burstCountMax=2, intraBurstRate=2.0,
                          selectionWeight=1.0, pitchVarianceCents=100.0,
                          gainMinDB=-4.0, gainMaxDB=-1.0),
        "grain_c": _sound("water_grain_c.wav", burstRate=1.2, burstCountMin=1,
                          burstCountMax=2, intraBurstRate=2.0,
                          selectionWeight=1.0, pitchVarianceCents=100.0,
                          gainMinDB=-4.0, gainMaxDB=-1.0),
      })
  swsys.sub_calls.append(("addSoundGroup", ("water", water), {}))

  birds = _group(
      bus="points", master_db=-4.0, max_voices=6, min_spacing=0.1,
      fade_in=0.01, fade_out=0.05,
      sounds={
        "bird_a": _sound("bird_a.wav", burstRate=0.35, burstCountMin=2,
                         burstCountMax=5, intraBurstRate=2.5,
                         selectionWeight=1.0, pitchVarianceCents=300.0,
                         gainMinDB=-10.0, gainMaxDB=-4.0),
        "bird_b": _sound("bird_b.wav", burstRate=0.35, burstCountMin=2,
                         burstCountMax=5, intraBurstRate=2.5,
                         selectionWeight=1.0, pitchVarianceCents=300.0,
                         gainMinDB=-10.0, gainMaxDB=-4.0),
        "bird_c": _sound("bird_c.wav", burstRate=0.35, burstCountMin=2,
                         burstCountMax=5, intraBurstRate=2.5,
                         selectionWeight=1.0, pitchVarianceCents=300.0,
                         gainMinDB=-10.0, gainMaxDB=-4.0),
      })
  swsys.sub_calls.append(("addSoundGroup", ("birds", birds), {}))

  chimes = _group(
      bus="points", master_db=0.0, max_voices=4, min_spacing=0.25,
      fade_in=0.005, fade_out=0.3,
      sounds={
        "chime": _sound("chime.wav", burstRate=1.0, burstCountMin=1,
                        burstCountMax=2, intraBurstRate=0.8,
                        selectionWeight=1.0, pitchVarianceCents=50.0,
                        gainMinDB=-4.0, gainMaxDB=0.0),
      })
  swsys.sub_calls.append(("addSoundGroup", ("chimes", chimes), {}))

  clanks = _group(
      bus="points", master_db=0.0, max_voices=4, min_spacing=0.15,
      fade_in=0.005, fade_out=0.1,
      sounds={
        "clank_a": _sound("clank_a.wav", burstRate=1.6, burstCountMin=2,
                          burstCountMax=4, intraBurstRate=4.0,
                          selectionWeight=1.0, pitchVarianceCents=80.0,
                          gainMinDB=-2.0, gainMaxDB=0.0),
        "clank_b": _sound("clank_b.wav", burstRate=1.6, burstCountMin=2,
                          burstCountMax=4, intraBurstRate=4.0,
                          selectionWeight=1.0, pitchVarianceCents=80.0,
                          gainMinDB=-2.0, gainMaxDB=0.0),
      })
  swsys.sub_calls.append(("addSoundGroup", ("clanks", clanks), {}))

  traffic = _group(
      bus="ambience", master_db=-10.0, max_voices=3, min_spacing=0.5,
      fade_in=0.3, fade_out=0.5,
      sounds={
        "wash": _sound("traffic_wash.wav", burstRate=0.35, burstCountMin=1,
                       burstCountMax=1, intraBurstRate=1.0,
                       selectionWeight=1.0, pitchVarianceCents=80.0,
                       gainMinDB=-6.0, gainMaxDB=-3.0),
      })
  swsys.sub_calls.append(("addSoundGroup", ("traffic", traffic), {}))

  # ---- emitter entities ----------------------------------------------------

  if level < 3:
    return

  # ENGINE QUIRK (measured, spectrum-verified): StochWav/SimpleSound preload
  # SampleData with _rootKey=60 but _originalPitch = C3 (261.63*0.5), so a
  # note-60 trigger plays every wav at exactly 2x rate (+1 octave). A
  # pitchOffsetCents=-1200 counter proved RACY (deferred per-voice pitch —
  # some voices ring at 2x anyway), so the compensation lives in the BAKE:
  # ork.spatialaudio.bakewavs.py resamples every asset one octave down.
  # Emitters therefore trigger at plain root pitch here.
  UNITY_PITCH_CENTS = 0.0

  def stoch_emitter(name, pos, group, rate_scale=1.0, gain_off=0.0):
    scene.entity(name,
                 transform=Transform(translation=pos),
                 components=[scene.declare_component(
                     "StochWavSoundEmitterData",
                     groupName=group,
                     enabled=True,
                     rateScale=rate_scale,
                     gainOffsetDB=gain_off,
                     pitchOffsetCents=UNITY_PITCH_CENTS)])

  for i, pos in enumerate(POOL_EMITTERS):
    stoch_emitter("water_e%d" % i, pos, "water")

  for i, pos in enumerate(BIRD_POSITIONS):
    stoch_emitter("bird_e%d" % i, pos, "birds")

  for i, pos in enumerate(URBAN_CLANK_POSITIONS):
    stoch_emitter("clank_e%d" % i, pos, "clanks")

  stoch_emitter("traffic_e0", URBAN_TRAFFIC_POS, "traffic")

  if level < 4:
    return

  # ---- runtime-spawned chime archetype + inactive spawner ------------------
  # spawner=False: the archetype exists ONLY for dynamic spawns; the explicit
  # spawner is autospawn=False with a lifetime so every runtime chime
  # self-despawns (StochWav keyOffs ride component deactivation).
  chime_ent = scene.entity("chime",
                           spawner=False,
                           components=[scene.declare_component(
                               "StochWavSoundEmitterData",
                               groupName="chimes",
                               enabled=True,
                               rateScale=2.5,
                               pitchOffsetCents=UNITY_PITCH_CENTS)])
  sp = scene.spawner("chime_spawner", chime_ent._arch,
                     autospawn=False, lifetime=CHIME_LIFETIME)

  if level < 5:
    return

  # ---- the PythonSystem script that owns runtime spawning ------------------
  script = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "_spatial_audio_showcase_sys.py")
  scene._set_primary_system_script(script)


###############################################################################
# WIND — the runtime-SYNTHESIZED, host-driven, positioned source.
#
# Recipe (the classic subtractive wind / SoundSeed-Air idiom): broadband noise
# through a gust-LFO-modulated lowpass, long attack, sustained. Lowered via the
# hypersound (A1a) emitter onto singularity, then a PANNER2D stage is appended
# (the same block the ECS emitters use) whose ANGLE/DISTANCE data params the
# host writes per frame — DspParam's default evaluator reads _data->_coarse
# LIVE each control pass, so data-param writes are the A8 params-are-data path.
#
# HOST-LAYER BY NECESSITY: no ECS component today can trigger a singularity
# synth PROGRAM at an entity position (StochWav/SimpleSound are sampler-only,
# and the ecssim sub-interpreter cannot import lev2.singularity). Filed as a
# feature request; until then any python host (harness / demo) calls
# WindLayer.setup()/update() and the pure-C++ player plays everything BUT wind.
###############################################################################

WIND_BUS = "ambience"
WIND_NOTE = 60
WIND_VEL = 110
# wind is a BIG diffuse source: compress reported distance so the treeline is
# audible across the vale (games' "spread/size" trick), floor 2 m.
WIND_DIST_SCALE = 0.20


def _wind_patch_class():
  """Deferred import + class build (ork.hypergraph.sound imports nothing from
  the engine at module scope, but keep scene import light anyway)."""
  from ork.hypergraph.sound.dsl import S, SoundPatch

  class ShowcaseWind(SoundPatch):
    def __init__(self):
      gust = S.lfo(rate=0.11, label="gustlfo")
      sig = S.noise(label="windnoise")
      sig = S.lowpass2(sig, cutoff=S.mod(gust, scale=95.0, base=185.0),
                       label="windlp")
      sig = S.lowpass2(sig, cutoff=320.0, label="windlp2")
      env = S.env([("atk", 2.5, 1.0, 0.5), ("sus", 1.0, 1.0, 0.5)],
                  sustain=1, label="windenv")
      self.output(sig, amp=env)

  return ShowcaseWind


class WindLayer:
  """Host-side wind voice: build once after the synth is live, update per
  frame with the current listener matrix (camera->world)."""

  def __init__(self, anchor=None):
    self.anchor = anchor if anchor is not None else WIND_ANCHOR
    self._mat = None          # MaterializedSound (owns bank+program)
    self._angle_param = None
    self._dist_param = None
    self._voice = None

  def setup(self, synth):
    from ork.hypergraph.sound.emitter import materialize_sound_instance
    from orkengine.lev2 import singularity as sing
    patch = _wind_patch_class()()
    self._mat = materialize_sound_instance(patch, name="showcase_wind")
    layer = self._mat.layer
    stage = layer.appendStage("PAN")
    stage.ioconfig.inputs = [0, 1]
    stage.ioconfig.outputs = [0, 1]
    panner = stage.appendDspBlock("AmpPanner2D", "wind_panner")
    self._angle_param = panner.paramByName("ANGLE")
    self._dist_param = panner.paramByName("DISTANCE")
    self._angle_param.coarse = 0.0
    self._dist_param.coarse = 30.0
    bus = synth.outputBus(WIND_BUS)
    if bus is None:
      bus = synth.createOutputBus(WIND_BUS)
    kmod = sing.KeyOnModifiers()
    kmod.outputbus = bus
    self._voice = synth.keyOn(WIND_NOTE, WIND_VEL, self._mat.program, kmod)
    return self._voice is not None

  def update(self, synth):
    """Recompute ANGLE/DISTANCE from the live listener matrix — the same
    view-space convention the ECS emitters use (angle = -atan2(x, z))."""
    if self._angle_param is None:
      return
    inv_listener = synth.listener_matrix.inverse   # world -> view
    rel = self.anchor.transform(inv_listener)      # vec3.transform: w=1
    dist = max(1.0, math.sqrt(rel.x * rel.x + rel.y * rel.y + rel.z * rel.z))
    angle = -math.atan2(rel.x, rel.z)
    self._angle_param.coarse = angle
    self._dist_param.coarse = max(2.0, dist * WIND_DIST_SCALE)

  def teardown(self, synth):
    if self._voice is not None:
      synth.keyOff(self._voice, WIND_NOTE, 0)
      self._voice = None


###############################################################################
# LISTENER WALK — the scripted camera path both the harness (headless WAV
# gate) and any scripted preview drive. Segments are the measurement contract:
#
#   seg  t(s)    motion                                  gate
#   ---  ------  --------------------------------------  ----------------------
#   A    0-12    (0,1.7,40) -> (0,1.7,16), face north    water approach ratio
#   B    12-26   strafe east x:-21 -> +21 @ z=18,        water L/R flip
#                face north (pool passes R -> L... pool
#                at x=0: starts LEFT of path start? see
#                note below — flip sign is MEASURED, the
#                gate is the SIGN REVERSAL itself)
#   C    26-40   strafe west x:+25 -> -25 @ z=-36,       wind L/R flip,
#                face north (through the treeline)       bird transients
#   D    40-54   (-25,1.7,-36) -> (45,1.7,-18),          clank approach ratio
#                face along travel
#   E    54-72   hold at (30,1.7,-12), face north        chime band silent
#                                                        before t=45, present
#                                                        after (runtime spawn)
###############################################################################

EYE_HEIGHT = 1.7
WALK_TOTAL = 72.0

_SEGS = [
  # (t0, t1, eye0, eye1, face_mode)  face_mode: "north" | "travel"
  (0.0, 12.0, (0.0, EYE_HEIGHT, 40.0), (0.0, EYE_HEIGHT, 16.0), "north"),
  (12.0, 26.0, (-21.0, EYE_HEIGHT, 18.0), (21.0, EYE_HEIGHT, 18.0), "north"),
  (26.0, 40.0, (25.0, EYE_HEIGHT, -36.0), (-25.0, EYE_HEIGHT, -36.0), "north"),
  (40.0, 54.0, (-25.0, EYE_HEIGHT, -36.0), (45.0, EYE_HEIGHT, -18.0), "travel"),
  (54.0, WALK_TOTAL, (30.0, EYE_HEIGHT, -12.0), (30.0, EYE_HEIGHT, -12.0), "north"),
]


def listener_pose(t):
  """(eye, tgt) at time t along the walk. North = -Z."""
  t = max(0.0, min(t, WALK_TOTAL - 1e-4))
  for t0, t1, e0, e1, face in _SEGS:
    if t0 <= t <= t1:
      u = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
      eye = vec3(e0[0] + (e1[0] - e0[0]) * u,
                 e0[1] + (e1[1] - e0[1]) * u,
                 e0[2] + (e1[2] - e0[2]) * u)
      if face == "travel" and (e1 != e0):
        d = vec3(e1[0] - e0[0], 0.0, e1[2] - e0[2]).normalized
        tgt = eye + d * 10.0
      else:
        tgt = eye + vec3(0.0, 0.0, -10.0)   # face north
      return eye, tgt
  eye = vec3(*_SEGS[-1][3])
  return eye, eye + vec3(0.0, 0.0, -10.0)


###############################################################################
# scene classes
###############################################################################

class SpatialAudioShowcase(Scene):
  """Visual + audio: skybox, ground grid, location markers, full soundscape."""

  def __init__(self):
    super().__init__(default_sg=False)

    SG = self.scenegraph(
        preset="ForwardPBR",
        skybox_path="<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity=1.0,
        DiffuseIntensity=1.5,
        SpecularIntensity=1.0,
        AmbientLight=vec3(0.02),
        msaa=2,
        ssaa=0)

    # ground reference grid (self-shaded, reflected, zero-material)
    grid = lev2.GridDrawableData()
    grid.shader_suffix = "_V4"
    grid.modcolor = vec3(1.0)
    grid.intensityA = 1.2
    grid.intensityB = 0.6
    grid.intensityC = 0.4
    grid.intensityD = 0.2
    grid.lineWidth = 0.04
    grid.extent = 80.0
    grid.majorTileDim = 10.0
    grid.minorTileDim = 1.0
    grid.minor_fade_begin = 15.0
    grid.minor_fade_end = 40.0
    self.entity("ground",
                components=[SG.component(nodes={
                    "grid": {"drawable": grid},
                })])

    # location markers: one calib model per audio landmark, scaled to read at
    # a glance (the audio positions are the ground truth — markers visualize).
    def marker(name, pos, scale):
      self.entity(name,
                  transform=Transform(translation=pos, scale=scale),
                  components=[SG.component(nodes={
                      "m": {"drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
                  })])

    marker("mk_pool", POOL_CENTER, 1.5)
    marker("mk_wind", WIND_ANCHOR, 2.5)
    for i, pos in enumerate(BIRD_POSITIONS):
      marker("mk_bird%d" % i, pos, 0.6)
    marker("mk_hum", URBAN_HUM_POS, 1.2)
    for i, pos in enumerate(URBAN_CLANK_POSITIONS):
      marker("mk_clank%d" % i, pos, 0.9)
    marker("mk_traffic", URBAN_TRAFFIC_POS, 1.2)
    marker("mk_chime", CHIME_CENTER, 0.8)

    declare_audio(self)


class AudioHarnessScene(Scene):
  """Audio + camera-only scenegraph (no drawables) — the headless WAV-gate
  vehicle. The SceneGraphSystem exists so the LISTENER rides the real camera
  path (UpdateCamera notify -> SceneGraphSystem._camera -> synth listener),
  exactly as in live play."""

  def __init__(self):
    super().__init__(default_sg=False)
    self.scenegraph(
        preset="ForwardPBR",
        skybox_path="<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity=0.5,
        DiffuseIntensity=1.0,
        SpecularIntensity=0.0,
        AmbientLight=vec3(0.0),
        ssaa=0)
    declare_audio(self)
