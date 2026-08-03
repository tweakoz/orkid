#!/usr/bin/env ork.python
################################################################################
# test_audio_ecs_bindings — canary for the ECS audio data model (slice A0d).
#
# WHAT IT PROVES (needs NO audio hardware, NO display):
#   The ECS audio systems (StochWavSoundEmitter, SimpleSoundEmitter,
#   GlobalSynth, SoundField) are reachable and correct from Python. Before this
#   test, the
#   pyinit_stochwav / pyinit_simplesound / pyinit_globalsynth bindings had ZERO
#   users anywhere in the repo — a whole registered subsystem with no exercise.
#
#   Per check:
#     (a) declareSystem() resolves all three SystemData class names (reflection
#         registration, reflection_init.cpp) and pybind hands back the DERIVED
#         python type, not a bare SystemData;
#     (b) declareComponent() does the same for both emitter ComponentDatas and
#         for the SoundFieldProbeData (the ambisonic probe field, SF1);
#     (c) every bound property round-trips through its setter/getter pair (a
#         swapped or mistyped accessor shows up as a read-back mismatch);
#     (d) the map ops (addSound / removeSound / clearSounds / addSoundGroup /
#         addBusConfig) mutate the container the readonly property exposes;
#     (e) the whole graph SERIALIZES: every audio class name appears in the JSON
#         and no object emits `"class": ""` — the silent signature of a class
#         that was never touched by the ClassToucher.
#
# SHAPE (mirrors test_audio_device_fallback.py): the driver (no --role) is pure
# stdlib and spawns the engine-booting check as its OWN ork.python subprocess,
# with ORKID_DRM_MODE stripped so it can never touch the physical display.
#
# EXIT-CODE LAW (was defect #12, fixed): the child MUST exit rc=0. Historically
# ecs.headless_init() left the last loader-Context shared_ptr inside the never-run
# GfxEnv::initializeWithContext op parked in mainSerialQueue (a queue with zero
# worker threads), so ~VkContext -> ~VkGpuSliceTimer ran at atexit against an
# already-unloaded Vulkan loader and SIGSEGV'd AFTER headless_exit() returned.
# The op now captures the context weakly and VkContext::_doShutdown() destroys
# the slice timer, so the context dies inside stopLoaderThread() with the driver
# live. This driver accepts PASS ONLY — no teardown-bug tolerance — because that
# exit code is the only signal a regression here would produce. The child still
# emits its verdict BEFORE teardown (the #57 protocol) and prints the
# CHILD_TEARDOWN_ENTERED/RETURNED markers, now purely as crash-window diagnostics.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

# every class the audio data model must round-trip through serialization.
EXPECT_CLASSES = (
    "StochWavSoundEmitterSystemData",
    "StochWavSoundEmitterData",
    "StochWavSound",
    "StochSoundGroup",
    "SimpleSoundEmitterSystemData",
    "SimpleSoundEmitterData",
    "SimpleSoundData",
    "GlobalSynthSystemData",
    "SynthBusConfig",
    "PannerSpatializerData",
    "SoundFieldSystemData",
    "SoundFieldProbeData",
)


################################################################################
# CHILD — one headless ECS boot: build the audio data model, check it, report.
################################################################################

def _role_bindings():
  import orkengine.core                     # core FIRST (import-order law)
  from orkengine import lev2, ecs
  from ork.testing import verdict

  checks = []                               # (name, ok) — all must be True

  def check(name, ok):
    checks.append((name, bool(ok)))

  def near(a, b):
    return abs(float(a) - float(b)) < 1e-4

  # data-only ECS init: reflection registration without an EzApp / window.
  ecs.headless_init()
  scene = ecs.SceneData()

  ##############################################################
  # (a) the three SystemDatas resolve by class name, downcast
  ##############################################################

  sysdatas = {}
  for name in ("StochWavSoundEmitterSystemData",
               "SimpleSoundEmitterSystemData",
               "GlobalSynthSystemData",
               "SoundFieldSystemData"):
    sd = scene.declareSystem(name)
    sysdatas[name] = sd
    check("declare_" + name, sd is not None and type(sd).__name__ == name)

  swsys = sysdatas["StochWavSoundEmitterSystemData"]
  sfsys = sysdatas["SoundFieldSystemData"]
  sssys = sysdatas["SimpleSoundEmitterSystemData"]
  gssys = sysdatas["GlobalSynthSystemData"]

  ##############################################################
  # (c) StochWavSound — every bound property, set then read back
  ##############################################################

  snd = ecs.StochWavSound()
  # both spellings must land: scene authors write bare "data://" literals, the
  # hypergraph lowering hands over a real Path.
  snd.wavFilePath = orkengine.core.Path("data://tests/audio/gust.wav")
  path_form_ok = str(snd.wavFilePath) == "data://tests/audio/gust.wav"
  snd.wavFilePath = "data://tests/audio/wind.wav"
  try:
    snd.wavFilePath = 17                    # neither str nor Path -> loud, not silent
    rejects_junk = False
  except Exception:
    rejects_junk = True
  check("stochwavsound_wavpath_forms", path_form_ok and rejects_junk)
  snd.burstRate = 3.5
  snd.burstCountMin = 2
  snd.burstCountMax = 7
  snd.intraBurstRate = 12.0
  snd.selectionWeight = 0.25
  snd.pitchVarianceCents = 150.0
  snd.gainMinDB = -12.0
  snd.gainMaxDB = -3.0
  snd.fadeInTime = 0.1
  snd.fadeOutTime = 0.4
  check("stochwavsound_props",
        str(snd.wavFilePath) == "data://tests/audio/wind.wav"
        and near(snd.burstRate, 3.5)
        and snd.burstCountMin == 2
        and snd.burstCountMax == 7
        and near(snd.intraBurstRate, 12.0)
        and near(snd.selectionWeight, 0.25)
        and near(snd.pitchVarianceCents, 150.0)
        and near(snd.gainMinDB, -12.0)
        and near(snd.gainMaxDB, -3.0)
        and near(snd.fadeInTime, 0.1)
        and near(snd.fadeOutTime, 0.4))

  ##############################################################
  # (c)+(d) StochSoundGroup — props + the sounds map ops
  ##############################################################

  grp = ecs.StochSoundGroup()
  grp.outputBusName = "ambience"
  grp.masterGainDB = -6.0
  grp.maxVoicesPerGroup = 8
  grp.minSpacing = 0.05
  grp.fadeInTime = 0.2
  grp.fadeOutTime = 0.3
  check("stochsoundgroup_props",
        grp.outputBusName == "ambience"
        and near(grp.masterGainDB, -6.0)
        and grp.maxVoicesPerGroup == 8
        and near(grp.minSpacing, 0.05)
        and near(grp.fadeInTime, 0.2)
        and near(grp.fadeOutTime, 0.3))

  grp.addSound("wind", snd)
  grp.addSound("scratch", ecs.StochWavSound())
  after_add = set(grp.sounds.keys())
  grp.removeSound("scratch")
  after_remove = set(grp.sounds.keys())
  scratch_grp = ecs.StochSoundGroup()
  scratch_grp.addSound("x", ecs.StochWavSound())
  scratch_grp.clearSounds()
  check("stochsoundgroup_mapops",
        after_add == {"wind", "scratch"}
        and after_remove == {"wind"}
        and len(scratch_grp.sounds) == 0)

  ##############################################################
  # StochWavSoundEmitterSystemData — groups map + spatializer
  ##############################################################

  swsys.addSoundGroup("weather", grp)
  swsys.addSoundGroup("scratch", ecs.StochSoundGroup())
  swsys.removeSoundGroup("scratch")
  spat = lev2.singularity.PannerSpatializerData()
  spat.refDistance = 2.0
  swsys.spatializer = spat
  check("stochwavsys_groups_spatializer",
        set(swsys.soundGroups.keys()) == {"weather"}
        and swsys.spatializer is not None
        and near(swsys.soundGroups["weather"].masterGainDB, -6.0))

  ##############################################################
  # SimpleSound* — sound payload, system map, emitter component
  ##############################################################

  ssnd = ecs.SimpleSoundData()
  ssnd.wavfile_path = "data://tests/audio/blip.wav"
  ssnd.looping = True
  ssnd.spatialize = True
  ssnd.gainDB = -4.0
  ssnd.pitchOffsetCents = 25.0
  ssnd.fadeInTime = 0.01
  ssnd.fadeOutTime = 0.02
  ssnd.outputBusName = "sfx"
  check("simplesounddata_props",
        str(ssnd.wavfile_path) == "data://tests/audio/blip.wav"
        and ssnd.looping is True
        and ssnd.spatialize is True
        and near(ssnd.gainDB, -4.0)
        and near(ssnd.pitchOffsetCents, 25.0)
        and near(ssnd.fadeInTime, 0.01)
        and near(ssnd.fadeOutTime, 0.02)
        and ssnd.outputBusName == "sfx")

  sssys.addSound("blip", ssnd)
  sssys.addSound("scratch", ecs.SimpleSoundData())
  sssys.removeSound("scratch")
  sssys.spatializer = lev2.singularity.PannerSpatializerData()
  check("simplesoundsys_sounds",
        set(sssys.sounds.keys()) == {"blip"}
        and sssys.spatializer is not None)

  ##############################################################
  # GlobalSynthSystemData — bus configs
  ##############################################################

  bus = ecs.SynthBusConfig()
  bus.effectPreset = "none"
  bus.gainDB = -2.0
  bus.pan = 0.5
  bus.mute = False
  bus.solo = True
  gssys.addBusConfig("main", bus)
  gssys.addBusConfig("scratch", ecs.SynthBusConfig())
  gssys.removeBusConfig("scratch")
  gssys.masterGainDB = -1.5
  check("globalsynth_busconfigs",
        set(gssys.busConfigs.keys()) == {"main"}
        and gssys.busConfigs["main"].effectPreset == "none"
        and near(gssys.busConfigs["main"].gainDB, -2.0)
        and near(gssys.busConfigs["main"].pan, 0.5)
        and gssys.busConfigs["main"].mute is False
        and gssys.busConfigs["main"].solo is True
        and near(gssys.masterGainDB, -1.5))

  ##############################################################
  # SoundFieldSystemData — the ambisonic probe field's scene surface
  ##############################################################

  sfsys.masterGainDB = -3.0
  sfsys.slewTime = 0.4
  check("soundfieldsys_props",
        near(sfsys.masterGainDB, -3.0)
        and near(sfsys.slewTime, 0.4))

  ##############################################################
  # (b) the emitter + probe ComponentDatas on an archetype
  ##############################################################

  arch = scene.declareArchetype("audio_arch")

  swc = arch.declareComponent("StochWavSoundEmitterData")
  check("declare_StochWavSoundEmitterData",
        swc is not None and type(swc).__name__ == "StochWavSoundEmitterData")
  swc.groupName = "weather"
  swc.pitchOffsetCents = 10.0
  swc.gainOffsetDB = -1.0
  swc.rateScale = 2.0
  swc.enabled = True
  check("stochwavemitter_props",
        swc.groupName == "weather"
        and near(swc.pitchOffsetCents, 10.0)
        and near(swc.gainOffsetDB, -1.0)
        and near(swc.rateScale, 2.0)
        and swc.enabled is True)

  ssc = arch.declareComponent("SimpleSoundEmitterData")
  check("declare_SimpleSoundEmitterData",
        ssc is not None and type(ssc).__name__ == "SimpleSoundEmitterData")
  ssc.soundName = "blip"
  ssc.autoPlay = True
  ssc.enabled = True
  ssc.gainOffsetDB = -2.0
  ssc.pitchOffsetCents = 5.0
  ssc.initialFadeGain = 0.5
  check("simplesoundemitter_props",
        ssc.soundName == "blip"
        and ssc.autoPlay is True
        and ssc.enabled is True
        and near(ssc.gainOffsetDB, -2.0)
        and near(ssc.pitchOffsetCents, 5.0)
        and near(ssc.initialFadeGain, 0.5))

  sfc = arch.declareComponent("SoundFieldProbeData")
  check("declare_SoundFieldProbeData",
        sfc is not None and type(sfc).__name__ == "SoundFieldProbeData")
  sfc.ambixAsset = "data://tests/audio/ambience_foa.wav"
  path_form_ok = str(sfc.ambixAsset) == "data://tests/audio/ambience_foa.wav"
  sfc.ambixAsset = orkengine.core.Path("data://tests/audio/forest_foa.wav")
  try:
    sfc.ambixAsset = 17                     # neither str nor Path -> loud, not silent
    sf_rejects_junk = False
  except Exception:
    sf_rejects_junk = True
  sfc.gain = 0.75
  sfc.loop = False
  sfc.startPaused = True
  sfc.mode = 0                              # RADIAL (ZONE arrives at SF3)
  sfc.refDistance = 3.0
  sfc.maxDistance = 40.0
  sfc.rolloff = 2.0
  check("soundfieldprobe_props",
        path_form_ok and sf_rejects_junk
        and str(sfc.ambixAsset) == "data://tests/audio/forest_foa.wav"
        and near(sfc.gain, 0.75)
        and sfc.loop is False
        and sfc.startPaused is True
        and sfc.mode == 0
        and near(sfc.refDistance, 3.0)
        and near(sfc.maxDistance, 40.0)
        and near(sfc.rolloff, 2.0))

  ##############################################################
  # (e) serialize the whole graph — reflection completeness
  ##############################################################

  js = scene.serializeJson()
  missing = [c for c in EXPECT_CLASSES if ('"class": "%s"' % c) not in js]
  check("serialize_classes", len(missing) == 0)
  check("serialize_no_empty_class", '"class": ""' not in js)
  print("CHILD_JSON_BYTES=%d" % len(js), flush=True)
  print("CHILD_MISSING_CLASSES=%s" % (",".join(missing) if missing else "none"), flush=True)

  ##############################################################
  # verdict BEFORE teardown (#57): a teardown crash must never be able to erase
  # what we observed. The driver scores the exit code separately.
  ##############################################################

  failed = [name for name, ok in checks if not ok]
  print("CHILD_CHECKS=%d CHILD_FAILED=%s"
        % (len(checks), ",".join(failed) if failed else "none"), flush=True)
  rc = verdict(len(failed) == 0,
               "checks=%d failed=%s" % (len(checks), ",".join(failed) if failed else "none"))

  print("CHILD_TEARDOWN_ENTERED", flush=True)
  ecs.headless_exit()
  print("CHILD_TEARDOWN_RETURNED", flush=True)
  sys.exit(rc)


################################################################################
# DRIVER — pure stdlib; spawns the child and classifies.
################################################################################

def _spawn_child(timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "bindings"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _main_driver():
  import subprocess
  from ork.testing import verdict, read_verdict
  from ork.testing.verdict import PASS

  try:
    rc, out = _spawn_child()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child timed out (possible wedge in headless ecs init)"))

  print(out)

  child_class = read_verdict(out, rc)
  teardown_ok = "CHILD_TEARDOWN_RETURNED" in out

  # PASS demands BOTH: the child's own checks passed, AND it walked out with
  # rc=0. read_verdict downgrades a passing child with rc!=0 to
  # PASS_WITH_TEARDOWN_BUG, which is a FAILURE here — that is the #12 tripwire.
  passed = (child_class == PASS) and (rc == 0) and teardown_ok

  sys.exit(verdict(passed, "child=%s rc=%d teardown_returned=%s"
                   % (child_class, rc, teardown_ok)))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "bindings":
    _role_bindings()
  else:
    _main_driver()
