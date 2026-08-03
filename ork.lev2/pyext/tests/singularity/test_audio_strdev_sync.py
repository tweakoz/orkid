#!/usr/bin/env ork.python
################################################################################
# test_audio_strdev_sync — StrAudioDevice SYNC_NONREALTIME under a graphics app.
#
# WHAT IT PROVES (needs NO audio hardware, NO display):
#   A lockstep (virtual-time) EzApp with graphics AND audio_stream_sync drives the
#   StrAudioDevice SYNC pump itself: ezapp.cpp's lockstep update loop enqueues
#   advanceTime(update_delta) onto auxSerialQueue every update, while the render
#   side keeps drawing. The checks:
#     (a) the device handed to Python IS a StrAudioDevice in SYNC_NONREALTIME mode
#         (a real/PortAudio device here would be both a wrong device AND audible);
#     (b) the synth is live and bound to that device;
#     (c) the aux-queue pump actually advanced audio time — virtual audio seconds
#         and captured sample count both track the update count (a silently dead
#         pump would leave both at zero while the app happily rendered);
#     (d) graphics kept updating alongside the audio pump (gpu-update count > 0);
#     (e) shutdown is CLEAN — rc 0, "StrAudioDevice shutdown" reached, no crash
#         signature. This is the standing regression guard for the shutdown UAF
#         (aux-queue advanceTime -> synth::compute racing _audioExit's tearDown)
#         that surfaced as `free(): invalid pointer` in StrAudioDevice::shutdown().
#
# BOUNDED SELF-EXIT (law: no test runs unbounded): the child exits after UPDATES
# lockstep updates (default 240 == 4s of virtual time at 60 UPS), with a main-loop
# iteration cap as a second bound and an ork.testing Watchdog as the hard deadline —
# a wedge samples all thread stacks and hard-exits rc=111 instead of running forever.
# Overridable via ORKID_STRDEV_TEST_UPDATES / ORKID_STRDEV_TEST_DEADLINE.
#
# HEADLESS BY DEFAULT (consent law): the driver strips ORKID_DRM_MODE from the child
# env (offscreen alone does NOT stop DRM scanout) and pins ORKID_AUDIO_IOCLASS=STREAM
# so no host audio device can ever be opened. --real-device is the ONLY way to get
# on-glass output / an un-scrubbed environment, and it is OFF by default.
#
# SHAPE (mirrors test_audio_device_fallback.py / test_audio_wav_render.py): the
# driver (no --role) is pure stdlib, spawns the engine boot as its own ork.python
# subprocess, and emits the machine verdict line.
#
# NO CEILING ON ORKID_STRDEV_TEST_UPDATES (the former one is fixed): advanceTime
# used to accumulate its per-update sample count from a float dt (800.00006 rather
# than 800 at 60 UPS / 48kHz), so around update ~16.7k it emitted a chunk of 801 —
# and synth::compute, whose buffers are sized to exactly that count, always ran
# whole 32-frame control passes, so a non-multiple-of-32 chunk wrote 31 floats past
# the end -> heap corruption aborting at teardown (`free(): invalid next size
# (fast)`, 3/3 at UPDATES=24000). Both sides are hardened now: advanceTime snaps an
# interval that is a whole sample count within float precision, and the synth
# clamps its tail control pass. Long runs are safe (verified 24000 and 32000); the
# standing guard for the accounting itself is test_audio_stream_chunk_bounds.py.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
# NOTE: subprocess/time are imported lazily in the driver-only paths — the child
# must not pre-load hashlib/ssl-adjacent stdlib before orkengine.core (see the
# note in test_audio_wav_render.py).

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

UPS = 60.0
UPDATES = int(os.environ.get("ORKID_STRDEV_TEST_UPDATES", "240"))   # 4s virtual @60UPS
DEADLINE_S = float(os.environ.get("ORKID_STRDEV_TEST_DEADLINE", "90"))
STALL_S = DEADLINE_S * 0.66   # soft wall cap: a stalled update thread still exits
                              # gracefully (with a FAIL detail) before the watchdog
SR_NOMINAL = 48000         # StrAudioDevice fixed rate (audiodevice_stream.cpp)
AUDIO_FRACTION = 0.5       # audio must reach at least this much of the virtual span

# the child must observe exactly this device/mode — anything else means a host
# audio device was opened (audible) or the STREAM sync path was not selected.
EXPECT_DEVTYPE = "StrAudioDevice"
EXPECT_MODE = "SYNC_NONREALTIME"

# C++ tells the run MUST show (audiodevice_stream.cpp log channel "audio.STREAM").
EXPECT_LOG = ("StrAudioDevice::startup", "mode=SYNC", "StrAudioDevice shutdown")

# tells of a REAL device having been opened — none may appear headless.
FORBIDDEN_DEVICE = ("[audio.PA]", "[PA_DIAG]", "PCM name:", "[audio.PIPEWIRE]")

# crash signatures; the shutdown UAF regression shows up as the first of these.
CRASH_SIGS = ("free(): invalid pointer", "double free", "Segmentation fault",
              "SIGSEGV", "terminate called after throwing")


################################################################################
# CHILD — one engine boot: lockstep offscreen graphics + SYNC stream audio, run
# to the update bound, report what was observed, exit cleanly.
################################################################################

def _role_sync(real_device):
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import — setting it afterwards is
  # invisible to AppInitData and silently falls back to the host audio device.
  if not real_device:
    os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
    os.environ.pop("ORKID_DRM_MODE", None)

  import orkengine.core                    # core FIRST (import-order law)
  from orkengine.core import vec4
  from orkengine import lev2
  from ork.testing import armed

  import math
  import time

  class App(object):
    def __init__(self):
      self.updates = 0
      self.gpu_updates = 0
      self.iters = 0
      self.t_start = None
      self.abstime = 0.0
      self.devtype = "none"
      self.mode = "none"
      self.synth_ok = False
      self.audio_time = -1.0
      self.audio_samples = -1
      self.ezapp = lev2.OrkEzApp.create(
          self,
          enable_audio=True,
          enable_audio_output=True,
          enable_audio_synth=True,
          audio_stream_sync=True,
          enable_graphics=True,
          offscreen=not real_device,
          freerun=real_device,
          target_ups=UPS,
          target_fps=UPS,
          width=1280,
          height=720)
      # UI grid: the render-side load this test carries so the audio pump is
      # exercised against a live drawing app, not an idle one.
      self.ezapp.topWidget.enableUiDraw()
      lg_group = self.ezapp.topLayoutGroup
      lg_group.clearColorGuide = vec4(0.8, 0.6, 0.2, 1)
      self.griditems = lg_group.makeGrid(
          width=2,
          height=2,
          margin=4,
          uiclass=lev2.ui.Box,
          args=["label", vec4(0.1, 0.1, 0.3, 1)])
      lg_group.margin = 4

    def onGpuInit(self, ctx):
      dev = self.ezapp.audio_device
      synth = self.ezapp.audio_synth
      self.devtype = type(dev).__name__
      self.mode = str(getattr(dev, "mode", "none")).split(".")[-1]
      self.synth_ok = (synth is not None)

    def onUpdate(self, updinfo):
      self.updates += 1
      self.abstime = updinfo.absolutetime

    def onGpuUpdate(self, ctx):
      self.gpu_updates += 1
      t = self.abstime
      for i, item in enumerate(self.griditems):
        f = 1.0 + (i * 0.4)
        item.widget.color = vec4(
            math.sin(t * 1.00 * f * math.pi) * 0.5 + 0.5,
            math.sin(t * 1.31 * f * math.pi) * 0.5 + 0.5,
            math.sin(t * 1.51 * f * math.pi) * 0.5 + 0.5, 1)

    def onRunLoopIteration(self):
      self.iters += 1
      if self.t_start is None:
        self.t_start = time.time()
      stalled = (time.time() - self.t_start) > STALL_S
      if self.updates < UPDATES and not stalled:
        return
      dev = self.ezapp.audio_device
      if dev is not None and hasattr(dev, "currentTime"):
        self.audio_time = dev.currentTime()
        self.audio_samples = dev.availableSamples()
      self.ezapp.signalExit()

  app = App()
  with armed(DEADLINE_S, label="strdev_sync"):
    app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_DEVTYPE=%s" % app.devtype, flush=True)
  print("CHILD_MODE=%s" % app.mode, flush=True)
  print("CHILD_SYNTH=%s" % app.synth_ok, flush=True)
  print("CHILD_UPDATES=%d" % app.updates, flush=True)
  print("CHILD_GPUUPDATES=%d" % app.gpu_updates, flush=True)
  print("CHILD_AUDIO_TIME=%.6f" % app.audio_time, flush=True)
  print("CHILD_AUDIO_SAMPLES=%d" % app.audio_samples, flush=True)
  sys.exit(0)


################################################################################
# DRIVER — pure stdlib; spawns the child with a scrubbed env and classifies.
################################################################################

def _spawn_sync(real_device):
  import subprocess
  import time
  env = dict(os.environ)
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"        # C++ device logs must be captured
  if not real_device:
    env.pop("ORKID_DRM_MODE", None)         # offscreen alone does not stop scanout
    env["ORKID_AUDIO_IOCLASS"] = "STREAM"   # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "sync"]
  if real_device:
    cmd.append("--real-device")
  t0 = time.time()
  p = subprocess.run(cmd, capture_output=True, text=True,
                     timeout=DEADLINE_S + 60.0, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or ""), time.time() - t0


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


def _num(out, key, cast, default):
  raw = _grep(out, key)
  try:
    return cast(raw)
  except (TypeError, ValueError):
    return default


def _main_driver(real_device):
  import subprocess
  from ork.testing import verdict

  try:
    rc, out, elapsed = _spawn_sync(real_device)
  except subprocess.TimeoutExpired as e:
    combined = (e.stdout or "") + (e.stderr or "")
    print(combined if isinstance(combined, str) else combined.decode("utf-8", "replace"))
    sys.exit(verdict(False, "child exceeded the hard bound (watchdog did not fire) "
                            "deadline=%.0fs" % DEADLINE_S))

  print(out)

  updates = _num(out, "CHILD_UPDATES", int, -1)
  gpu_updates = _num(out, "CHILD_GPUUPDATES", int, -1)
  audio_time = _num(out, "CHILD_AUDIO_TIME", float, -1.0)
  audio_samples = _num(out, "CHILD_AUDIO_SAMPLES", int, -1)

  expected_secs = UPDATES / UPS
  dev_ok = (_grep(out, "CHILD_DEVTYPE") == EXPECT_DEVTYPE)
  mode_ok = (_grep(out, "CHILD_MODE") == EXPECT_MODE)
  synth_ok = (_grep(out, "CHILD_SYNTH") == "True")
  bound_ok = (updates >= UPDATES and elapsed <= DEADLINE_S)
  gfx_ok = (gpu_updates > 0)
  pump_ok = (audio_time >= expected_secs * AUDIO_FRACTION
             and audio_samples >= int(expected_secs * SR_NOMINAL * AUDIO_FRACTION))
  log_ok = all(tok in out for tok in EXPECT_LOG)
  clean_ok = (rc == 0 and not any(sig in out for sig in CRASH_SIGS))
  headless_ok = real_device or not any(tok in out for tok in FORBIDDEN_DEVICE)

  passed = (dev_ok and mode_ok and synth_ok and bound_ok and gfx_ok
            and pump_ok and log_ok and clean_ok and headless_ok)
  detail = ("rc=%d wall=%.1fs bound=%s(updates=%d/%d) dev=%s mode=%s synth=%s "
            "gfx=%s(gpuupd=%d) pump=%s(t=%.3f/%.3f samples=%d) log=%s clean=%s headless=%s"
            % (rc, elapsed, bound_ok, updates, UPDATES, dev_ok, mode_ok, synth_ok,
               gfx_ok, gpu_updates, pump_ok, audio_time, expected_secs, audio_samples,
               log_ok, clean_ok, headless_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--real-device", action="store_true",
                  help="CONSENT-GATED: run on-glass (freerun window) with the host "
                       "environment intact. OFF by default; the default run is "
                       "offscreen and can never open a host audio device.")
  args, _ = ap.parse_known_args()

  if args.role == "sync":
    _role_sync(args.real_device)
  else:
    _main_driver(args.real_device)
