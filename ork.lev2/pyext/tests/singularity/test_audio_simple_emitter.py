#!/usr/bin/env ork.python
################################################################################
# test_audio_simple_emitter — canary for SimpleSoundEmitterSystem (spatial rung 0).
#
# WHAT IT PROVES (needs NO audio hardware, NO HMD, NO display):
#   SimpleSoundEmitterSystem::_onUpdate opened with a bare `return;` from
#   2026-04-21 until this lane: the whole system — listener refresh, auto-play
#   triggering, per-voice panner update, fade ramp — was dead code. Nothing in
#   the repo noticed, because a dead emitter is indistinguishable from a working
#   emitter with nothing to say. This canary is the noticing.
#
#   Legs (all in ONE child boot, offscreen GPU + SYNC non-realtime audio):
#     (a) UNIT_NOVR — synth.setListenerFromRigView() with NO device head pose
#         returns the caller's rig VIEW matrix BIT-FOR-BIT (the non-VR path may
#         never pay an identity compose);
#     (b) UNIT_VR   — with a head pose published, the listener is the CENTER head
#         view rig*base*hmd — the same composition Device::_updatePosesCommon
#         builds for the eye cameras (cmv = usermtx*base*hmd), so the ears sit
#         between the eyes, not on the walker rig;
#     (c) AUTOPLAY  — a spawned SimpleSoundEmitterComponent with autoPlay actually
#         triggers a voice: the captured window is NOT silent. This is the leg
#         that was impossible before the fix (proven RED against the unfixed tree);
#     (d) AZIMUTH   — moving the emitter from -X to +X inverts the L/R energy
#         asymmetry, i.e. the panner is being driven from the live entity position
#         through the listener matrix (and repeating the first position reproduces
#         the first result);
#     (e) HMD       — with the emitter parked and the HMD pose yawed 180 degrees,
#         the asymmetry inverts AGAIN. The head pose — not the rig — is steering
#         the ears. This is the headless stand-in for a real HMD: the NoVR device
#         accepts a pose through setPoseMatrix("hmd", ...), which is exactly the
#         slot a live device writes.
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure stdlib
# and spawns the engine boot as its OWN ork.python subprocess with ORKID_DRM_MODE
# stripped and ORKID_AUDIO_IOCLASS pinned to STREAM (in BOTH the spawn env and the
# child before the engine import — genviron snapshots environ at library load, so
# a later set is invisible).
#
# BOOT NOTE (why not ork.testing.headless_app): the harness boots through
# ecs.headless_appinit -> lev2appinit, whose kwarg surface carries no
# audio_stream_sync / freerun / wav_output_path (OrkEzApp.create's does). This test
# NEEDS the SYNC non-realtime device — audio time must be advanced by hand so a
# measurement window is a fixed sample count rather than a wall-clock race — so it
# boots through OrkEzApp.createEx and keeps the harness's guards (DRM pop, dir
# preflight) around it. The seam gap is reported, not worked around silently.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import math
# NOTE: subprocess / tempfile are imported lazily inside driver-only paths; the
# child must not pre-load hashlib/ssl-adjacent stdlib before orkengine.core.

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT             = 1.0 / 100.0   # 480 frames @48k per audio pump: whole control passes
PUMPS_SETTLE   = 60            # audio pumps discarded before each measurement
PUMPS_MEASURE  = 60            # audio pumps captured per measurement window
FRAMES_PER_WIN = 16            # render iterations interleaved with each half-window
FRAME_SLEEP_S  = 0.005         # let the freerun update thread tick between frames
VOICE_WAIT_S   = 120.0         # WALL-CLOCK bound on the auto-played voice keying on.
                               # Deliberately generous: the ECS side runs freerun on
                               # its own thread, so a loaded box (a parallel build)
                               # stretches staging — an iteration-counted wait turned
                               # that into a false silence verdict.
EMITTER_X      = 8.0           # emitter offset from the listener, meters
RMS_FLOOR      = 1.0e-4        # window energy floor: below this the system is mute
ASYM_FLOOR     = 5.0e-4        # |rmsL-rmsR| that counts as a real pan, not noise


################################################################################
# CHILD — one engine boot: offscreen GPU + SYNC audio + a minimal ECS scene.
################################################################################

def _role_emit():
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import — setting it afterwards is
  # invisible to AppInitData and silently falls back to the host audio device.
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  import time
  import struct
  import wave
  import tempfile

  from orkengine import core                  # core FIRST (import-order law)
  from orkengine import lev2
  from orkengine import ecs
  from orkengine.core import vec3, mtx4, CrcStringProxy

  tokens = CrcStringProxy()

  ##############################################################
  # deterministic, asset-free source material: a 500Hz mono tone,
  # looped by the emitter so a window always has signal in it.
  ##############################################################

  def gen_wav(path):
    sr = 48000
    frames = bytearray()
    for i in range(sr // 2):
      frames += struct.pack("<h", int(20000 * math.sin(2.0 * math.pi * 500.0 * i / sr)))
    with wave.open(path, "wb") as w:
      w.setnchannels(1)
      w.setsampwidth(2)
      w.setframerate(sr)
      w.writeframes(bytes(frames))
    return path

  tmpdir  = tempfile.mkdtemp(prefix="simple_emitter_")
  wavpath = gen_wav(os.path.join(tmpdir, "tone.wav"))

  ##############################################################
  # scene: SceneGraphSystem (owns the camera the listener rides)
  # + SimpleSoundEmitterSystem with one spatialized looping sound.
  ##############################################################

  def build_scene():
    scene = ecs.SceneData()
    sg = scene.declareSystem("SceneGraphSystem")
    sg.declareLayer("std_forward")
    sg.declareParams({"preset": "ForwardPBR"})
    ss = scene.declareSystem("SimpleSoundEmitterSystemData")
    snd = ecs.SimpleSoundData()
    snd.wavfile_path  = wavpath
    snd.looping       = True
    snd.spatialize    = True
    snd.gainDB        = 0.0
    snd.outputBusName = "main"
    ss.addSound("tone", snd)
    ss.spatializer = lev2.singularity.PannerSpatializerData()
    arch = scene.declareArchetype("EmitterArch")
    comp = arch.declareComponent("SimpleSoundEmitterData")
    comp.soundName = "tone"
    comp.autoPlay  = True
    comp.enabled   = True
    spawner = scene.declareSpawner("emit_spawner")
    spawner.archetype = arch
    spawner.autospawn = False
    return scene

  ##############################################################
  # boot: offscreen (no window, no DRM) + SYNC non-realtime audio
  ##############################################################

  class App(object):
    pass

  ezapp = lev2.OrkEzApp.createEx(
      App(), [ecs.ecsInitCallback],
      name="AudioSimpleEmitterTest",
      use_subsystems=['opq', 'core', 'gpu', 'audioO'],
      offscreen=True, width=128, height=96, ssaa=0,
      enable_audio_synth=True,
      audio_stream_sync=True,          # -> StrAudioDevice SYNC_NONREALTIME
      freerun=True)
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  if not ctx:
    print("CHILD_ERROR=bindGfxToCurrentThread returned null", flush=True)
    sys.exit(3)

  syn = ezapp.audio_synth
  dev = ezapp.audio_device

  ##############################################################
  # (a)+(b) the listener composition, unit-exercised with no ECS.
  # Order matters: the no-pose leg must run BEFORE any head pose
  # is published, since a posemap key cannot be un-published.
  ##############################################################

  rig = mtx4.lookAt(vec3(1, 2, 3), vec3(0, 0, 0), vec3(0, 1, 0))
  syn.setListenerFromRigView(rig)
  unit_novr = (syn.inv_listener_matrix == rig)

  vrdev  = lev2.orkidvr.novr_device()
  hmd90  = mtx4.rotMatrix(vec3(0, 1, 0), math.pi * 0.5)
  vrdev.setPoseMatrix("hmd", hmd90)
  syn.setListenerFromRigView(rig)
  # multiply_ltor(rig, multiply_ltor(base, hmd)) with base == identity, spelled
  # in python's rtol operator: (hmd*base)*rig.
  unit_vr = (syn.inv_listener_matrix == (hmd90 * rig))
  vrdev.setPoseMatrix("hmd", mtx4())   # head straight ahead for the ECS legs

  print("CHILD_UNIT_NOVR=%d" % int(unit_novr), flush=True)
  print("CHILD_UNIT_VR=%d" % int(unit_vr), flush=True)

  ##############################################################
  # the simulation
  ##############################################################

  ctrl = ecs.Controller()
  ctrl.bindScene(build_scene())
  ctrl.gpuInit(ctx)
  ctrl.createSimulation()
  ctrl.startSimulation()
  ctrl.installUpdateCallbackOnEzApp(ezapp)
  ctrl.installGpuUpdateCallbackOnEzApp(ezapp)
  ctrl.installRenderCallbackOnEzApp(ezapp)
  sys_sg = ctrl.findSystem("SceneGraphSystem")

  # listener at the origin looking down +Z: the emitter's world X then maps to a
  # pure left/right offset, whichever way the panner spells the sign.
  ctrl.systemNotify(sys_sg, tokens.UpdateCamera, {
      tokens.eye:  vec3(0, 0, 0),
      tokens.tgt:  vec3(0, 0, 1),
      tokens.up:   vec3(0, 1, 0),
      tokens.near: 0.1,
      tokens.far:  1000.0,
      tokens.fovy: 1.0})

  def spawn(x):
    SAD = ecs.SpawnAnonDynamic("emit_spawner")
    SAD.overridexf.translation = vec3(x, 0, 0)
    return ctrl.spawnEntity(SAD)

  def pump(nframes, npumps):
    # frames drive the ECS update side (freerun, own thread); advanceTime is the
    # ONLY thing that generates audio on a SYNC device, so a window is an exact
    # sample count no matter how the wall clock behaves.
    for _ in range(nframes):
      ezapp.mainThreadIter()
      time.sleep(FRAME_SLEEP_S)
    for _ in range(npumps):
      dev.advanceTime(DT)

  def wait_for_voice(label):
    # bounded: on a system that never triggers (the pre-fix dead update) this
    # falls through and the silence check below is what reports it.
    t0 = time.time()
    voiced = False
    while (time.time() - t0) < VOICE_WAIT_S:
      if syn.numActiveVoices > 0:
        voiced = True
        break
      pump(1, 2)
    print("CHILD_VOICEWAIT_%s=%d,%.2f" % (label, int(voiced), time.time() - t0), flush=True)
    return voiced

  def rms(vals):
    if not len(vals):
      return -1.0
    acc = 0.0
    for v in vals:
      acc += v * v
    return (acc / len(vals)) ** 0.5

  def measure(label):
    pump(FRAMES_PER_WIN, PUMPS_SETTLE)
    dev.extractSamples(1 << 30)               # discard the settle window
    pump(FRAMES_PER_WIN, PUMPS_MEASURE)
    cap = dev.extractSamples(1 << 30)
    l = rms(cap.left)
    r = rms(cap.right)
    print("CHILD_%s=%.9f,%.9f,%d" % (label, l, r, len(cap.left)), flush=True)
    return l, r

  pump(20, 10)                                # let the scene stage
  ent = spawn(-EMITTER_X)
  voiced = wait_for_voice("negx")
  print("CHILD_VOICED=%d" % int(voiced), flush=True)
  measure("W_NEGX")

  ctrl.despawnEntity(ent)
  pump(8, 20)
  ent = spawn(+EMITTER_X)
  wait_for_voice("posx")
  measure("W_POSX")

  ctrl.despawnEntity(ent)
  pump(8, 20)
  ent = spawn(-EMITTER_X)
  wait_for_voice("negx2")
  measure("W_NEGX2")

  # same emitter, same rig — turn the HEAD 180 degrees.
  vrdev.setPoseMatrix("hmd", mtx4.rotMatrix(vec3(0, 1, 0), math.pi))
  measure("W_HMD180")

  ##############################################################
  # teardown (#57: everything observable is already printed)
  ##############################################################

  ctrl.uninstallRenderCallbackOnEzApp(ezapp)
  ctrl.uninstallGpuUpdateCallbackOnEzApp(ezapp)
  ctrl.uninstallUpdateCallbackOnEzApp(ezapp)
  ctrl.stopSimulation()
  ctrl.terminateSimulation()
  print("CHILD_TEARDOWN_ENTERED", flush=True)
  ezapp.signalExit()
  ezapp.mainThreadEnd()
  print("CHILD_TEARDOWN_RETURNED", flush=True)
  sys.exit(0)


################################################################################
# DRIVER — pure stdlib; spawns the child and classifies.
################################################################################

def _spawn_child(timeout=600):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"       # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "emit"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


def _window(out, key):
  raw = _grep(out, "CHILD_" + key)
  if raw is None:
    return None
  parts = raw.split(",")
  return float(parts[0]), float(parts[1]), int(parts[2])


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out = _spawn_child()
  except subprocess.TimeoutExpired as e:
    # TimeoutExpired carries RAW BYTES even under text=True — decoding here keeps a
    # wedge diagnosable instead of drowning it in a TypeError from the report path.
    def _dec(b):
      if b is None:
        return ""
      return b.decode("utf-8", "replace") if isinstance(b, bytes) else b
    print(_dec(e.stdout) + _dec(e.stderr))
    sys.exit(verdict(False, "child timed out (possible wedge in the ECS/audio pump)"))

  print(out)

  unit_novr = _grep(out, "CHILD_UNIT_NOVR") == "1"
  unit_vr   = _grep(out, "CHILD_UNIT_VR") == "1"
  voiced    = _grep(out, "CHILD_VOICED") == "1"
  teardown  = "CHILD_TEARDOWN_RETURNED" in out

  wins = {k: _window(out, k) for k in ("W_NEGX", "W_POSX", "W_NEGX2", "W_HMD180")}
  have_all = all(w is not None for w in wins.values())

  sounding = asym = mirror = repeatable = hmd_ok = False
  detail_extra = ""
  if have_all:
    def diff(k):
      l, r = wins[k][0], wins[k][1]
      return l - r
    def energy(k):
      l, r = wins[k][0], wins[k][1]
      return max(l, r)
    dn, dp, dn2, dh = diff("W_NEGX"), diff("W_POSX"), diff("W_NEGX2"), diff("W_HMD180")

    # (c) the emitter is audible at all — the pre-fix dead update is pure silence
    sounding = all(energy(k) > RMS_FLOOR for k in wins)
    # (d) both positions pan HARD, and to opposite sides
    asym   = (abs(dn) > ASYM_FLOOR and abs(dp) > ASYM_FLOOR)
    mirror = (dn * dp < 0.0)
    # returning the emitter to the first position reproduces the first side
    repeatable = (dn * dn2 > 0.0)
    # (e) head turned 180 with the emitter parked: the pan must invert
    hmd_ok = (abs(dh) > ASYM_FLOOR and dh * dn2 < 0.0)
    detail_extra = ("negX(L=%.6f,R=%.6f) posX(L=%.6f,R=%.6f) negX2(L=%.6f,R=%.6f) "
                    "hmd180(L=%.6f,R=%.6f)"
                    % (wins["W_NEGX"][0], wins["W_NEGX"][1],
                       wins["W_POSX"][0], wins["W_POSX"][1],
                       wins["W_NEGX2"][0], wins["W_NEGX2"][1],
                       wins["W_HMD180"][0], wins["W_HMD180"][1]))

  passed = (rc == 0 and teardown and unit_novr and unit_vr and voiced
            and have_all and sounding and asym and mirror and repeatable and hmd_ok)
  detail = ("rc=%d teardown=%s unit_novr=%s unit_vr=%s voiced=%s sounding=%s "
            "asym=%s mirrored=%s repeatable=%s hmd=%s | %s"
            % (rc, teardown, unit_novr, unit_vr, voiced, sounding,
               asym, mirror, repeatable, hmd_ok, detail_extra))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "emit":
    _role_emit()
  else:
    _main_driver()
