#!/usr/bin/env ork.python
################################################################################
# test_audio_strdev_async — StrAudioDevice ASYNC_REALTIME under a freerun app.
#
# WHAT IT PROVES (needs NO audio hardware, NO display):
#   The other half of the StrAudioDevice contract from test_audio_strdev_sync: with
#   audio_stream_sync=False the device runs its OWN audio thread, generating synth
#   audio paced by the wall clock into the capture buffers while a freerun graphics
#   app renders. The checks:
#     (a) the device handed to Python IS a StrAudioDevice in ASYNC_REALTIME mode;
#     (b) the synth is live and bound to that device;
#     (c) the audio thread actually produces at REALTIME rate — captured samples
#         accumulate at ~48kHz over the measured window (a dead thread leaves the
#         count flat; a free-spinning one races far past realtime, and both used to
#         pass unnoticed because nothing ever measured the rate);
#     (d) shutdown is CLEAN — the audio thread is joined ("Audio thread stopped"),
#         rc 0, no crash signature.
#
# BOUNDED SELF-EXIT (law: no test runs unbounded): the child exits RUN_S wall
# seconds after the audio device comes up (default 4s), with an ork.testing Watchdog
# as the hard deadline — a wedge samples all thread stacks and hard-exits rc=111
# instead of running forever. Overridable via ORKID_STRDEV_TEST_SECONDS /
# ORKID_STRDEV_TEST_DEADLINE.
#
# HEADLESS BY DEFAULT (consent law): ASYNC mode is exactly where an unpinned ioclass
# becomes AUDIBLE. genviron snapshots `environ` at shared-library load, so the old
# in-script `os.environ[...] = "STREAM"` AFTER the orkengine import was invisible to
# AppInitData and the run fell back to the host PortAudio device. The pin now happens
# in the spawning driver's env AND before the child's engine import. --real-device is
# the ONLY way to get a host device / on-glass output, and it is OFF by default.
#
# SHAPE (mirrors test_audio_device_fallback.py / test_audio_wav_render.py): the
# driver (no --role) is pure stdlib, spawns the engine boot as its own ork.python
# subprocess, and emits the machine verdict line.
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

RUN_S = float(os.environ.get("ORKID_STRDEV_TEST_SECONDS", "4.0"))
DEADLINE_S = float(os.environ.get("ORKID_STRDEV_TEST_DEADLINE", "90"))
SETTLE_S = 0.5             # audio-thread spin-up excluded from the rate window
STALL_S = DEADLINE_S * 0.66   # soft wall cap: a device that never comes up still
                              # exits gracefully (with a FAIL detail) pre-watchdog
SR_NOMINAL = 48000         # StrAudioDevice fixed rate (audiodevice_stream.cpp)
RATE_LO = 0.5 * SR_NOMINAL # realtime pacing tolerance: neither dead nor free-running
RATE_HI = 1.5 * SR_NOMINAL

# the child must observe exactly this device/mode — anything else means a host
# audio device was opened (audible) or the STREAM async path was not selected.
EXPECT_DEVTYPE = "StrAudioDevice"
EXPECT_MODE = "ASYNC_REALTIME"

# C++ tells the run MUST show (audiodevice_stream.cpp log channel "audio.STREAM").
EXPECT_LOG = ("StrAudioDevice::startup", "mode=ASYNC",
              "Audio thread started", "Audio thread stopped")

# tells of a REAL device having been opened — none may appear headless.
FORBIDDEN_DEVICE = ("[audio.PA]", "[PA_DIAG]", "PCM name:", "[audio.PIPEWIRE]")

CRASH_SIGS = ("free(): invalid pointer", "double free", "Segmentation fault",
              "SIGSEGV", "terminate called after throwing")


################################################################################
# CHILD — one engine boot: freerun offscreen graphics + ASYNC stream audio, run to
# the wall-clock bound measuring the audio thread's production rate, exit cleanly.
################################################################################

def _role_async(real_device):
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import — setting it afterwards is
  # invisible to AppInitData and silently falls back to the host audio device.
  if not real_device:
    os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
    os.environ.pop("ORKID_DRM_MODE", None)

  import orkengine.core                    # core FIRST (import-order law)
  from orkengine import lev2
  from ork.testing import armed

  import time

  class App(object):
    def __init__(self):
      self.iters = 0
      self.t_boot = None        # first main-loop iteration (device may not exist yet)
      self.t_start = None       # first iteration that saw a live device
      self.t_early = None
      self.samples_early = -1
      self.samples_late = -1
      self.window_s = 0.0
      self.devtype = "none"
      self.mode = "none"
      self.synth_ok = False
      self.ezapp = lev2.OrkEzApp.create(
          self,
          enable_audio=True,
          enable_audio_output=True,
          enable_audio_synth=True,
          audio_stream_sync=False,
          enable_graphics=True,
          offscreen=not real_device,
          freerun=True,
          width=640,
          height=480)

    def onGpuInit(self, ctx):
      dev = self.ezapp.audio_device
      synth = self.ezapp.audio_synth
      self.devtype = type(dev).__name__
      self.mode = str(getattr(dev, "mode", "none")).split(".")[-1]
      self.synth_ok = (synth is not None)

    def onRunLoopIteration(self):
      self.iters += 1
      now = time.time()
      if self.t_boot is None:
        self.t_boot = now
      dev = self.ezapp.audio_device
      if dev is None or not hasattr(dev, "availableSamples"):
        if (now - self.t_boot) > STALL_S:
          self.ezapp.signalExit()
        return
      if self.t_start is None:
        self.t_start = now
        return
      elapsed = now - self.t_start
      # sample the counter only at the two window edges: availableSamples takes
      # the device's buffer mutex, and this loop iterates ~500k times/sec.
      if self.t_early is None:
        if elapsed < SETTLE_S:
          return
        self.t_early = now
        self.samples_early = dev.availableSamples()
        return
      if elapsed < RUN_S:
        return
      self.samples_late = dev.availableSamples()
      self.window_s = now - self.t_early
      self.ezapp.signalExit()

  app = App()
  with armed(DEADLINE_S, label="strdev_async"):
    app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  rate = -1.0
  if app.samples_late >= 0 and app.samples_early >= 0 and app.window_s > 0.0:
    rate = (app.samples_late - app.samples_early) / app.window_s
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_DEVTYPE=%s" % app.devtype, flush=True)
  print("CHILD_MODE=%s" % app.mode, flush=True)
  print("CHILD_SYNTH=%s" % app.synth_ok, flush=True)
  print("CHILD_ITERS=%d" % app.iters, flush=True)
  print("CHILD_SAMPLES_EARLY=%d" % app.samples_early, flush=True)
  print("CHILD_SAMPLES_LATE=%d" % app.samples_late, flush=True)
  print("CHILD_WINDOW_S=%.6f" % app.window_s, flush=True)
  print("CHILD_RATE=%.1f" % rate, flush=True)
  sys.exit(0)


################################################################################
# DRIVER — pure stdlib; spawns the child with a scrubbed env and classifies.
################################################################################

def _spawn_async(real_device):
  import subprocess
  import time
  env = dict(os.environ)
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"        # C++ device logs must be captured
  if not real_device:
    env.pop("ORKID_DRM_MODE", None)         # offscreen alone does not stop scanout
    env["ORKID_AUDIO_IOCLASS"] = "STREAM"   # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "async"]
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
    rc, out, elapsed = _spawn_async(real_device)
  except subprocess.TimeoutExpired as e:
    combined = (e.stdout or "") + (e.stderr or "")
    print(combined if isinstance(combined, str) else combined.decode("utf-8", "replace"))
    sys.exit(verdict(False, "child exceeded the hard bound (watchdog did not fire) "
                            "deadline=%.0fs" % DEADLINE_S))

  print(out)

  rate = _num(out, "CHILD_RATE", float, -1.0)
  window_s = _num(out, "CHILD_WINDOW_S", float, -1.0)
  samples_late = _num(out, "CHILD_SAMPLES_LATE", int, -1)

  dev_ok = (_grep(out, "CHILD_DEVTYPE") == EXPECT_DEVTYPE)
  mode_ok = (_grep(out, "CHILD_MODE") == EXPECT_MODE)
  synth_ok = (_grep(out, "CHILD_SYNTH") == "True")
  bound_ok = (window_s > 0.0 and elapsed <= DEADLINE_S)
  rate_ok = (RATE_LO <= rate <= RATE_HI)
  log_ok = all(tok in out for tok in EXPECT_LOG)
  clean_ok = (rc == 0 and not any(sig in out for sig in CRASH_SIGS))
  headless_ok = real_device or not any(tok in out for tok in FORBIDDEN_DEVICE)

  passed = (dev_ok and mode_ok and synth_ok and bound_ok and rate_ok
            and log_ok and clean_ok and headless_ok)
  detail = ("rc=%d wall=%.1fs bound=%s(window=%.2fs) dev=%s mode=%s synth=%s "
            "rate=%s(%.0f/s vs %d nominal, samples=%d) log=%s clean=%s headless=%s"
            % (rc, elapsed, bound_ok, window_s, dev_ok, mode_ok, synth_ok,
               rate_ok, rate, SR_NOMINAL, samples_late, log_ok, clean_ok, headless_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--real-device", action="store_true",
                  help="CONSENT-GATED: run on-glass with the host environment intact, "
                       "so the engine may open a REAL audio device and be AUDIBLE. "
                       "OFF by default; the default run is offscreen and STREAM-pinned.")
  args, _ = ap.parse_known_args()

  if args.role == "async":
    _role_async(args.real_device)
  else:
    _main_driver(args.real_device)
