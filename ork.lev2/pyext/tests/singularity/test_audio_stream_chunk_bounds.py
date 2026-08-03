#!/usr/bin/env ork.python
################################################################################
# test_audio_stream_chunk_bounds — SYNC stream chunk accounting + synth bounds.
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   Two independent hardenings of the StrAudioDevice SYNC_NONREALTIME pump, both
#   of which were broken and jointly produced heap corruption at ~24k updates:
#
#   (a) EXACT SAMPLE ACCOUNTING — advanceTime() takes dt as a float, so the
#       lockstep 1/60s step arrives as 800.0000417 samples at 48kHz. The old
#       fractional accumulator integrated that bias until one call asked for 801
#       samples instead of 800. This test pumps a fixed dt PUMP_UPDATES times
#       (well past the ~24k crossing) and requires the cumulative sample count to
#       be EXACTLY updates*frames_per_update — one spurious sample fails it.
#
#   (b) SYNTH CHUNK BOUNDS — synth::compute sizes every buffer (master, busses,
#       layer dsp) to exactly inumframes but walks the chunk in fixed 32-frame
#       control passes, so any chunk size that is not a multiple of 32 used to
#       write up to 31 floats past the end of all of them (the 801 chunk above is
#       exactly that case; the abort surfaced later as `free(): invalid next size
#       (fast)` during teardown). This test additionally feeds a spread of
#       deliberately pathological chunk sizes (1..1023 frames, none a multiple of
#       32) through a LIVE voice+bus graph and requires a clean exit — a synth
#       that clamps its tail pass survives them, one that does not smashes the
#       heap and aborts.
#
# BOUNDED SELF-EXIT (law: no test runs unbounded): the child pumps a fixed number
# of advanceTime calls and exits; an ork.testing Watchdog is the hard deadline.
# Overridable via ORKID_CHUNKBOUNDS_TEST_UPDATES / ORKID_CHUNKBOUNDS_TEST_DEADLINE.
#
# HEADLESS (consent law): audio-only subsystems (no gpu), ORKID_AUDIO_IOCLASS is
# pinned to STREAM in BOTH the driver spawn env and the child before the engine
# import (genviron snapshots `environ` at library load), so no host audio device
# can ever be opened and nothing is audible.
#
# SHAPE (mirrors test_audio_strdev_sync.py / test_audio_wav_render.py): the driver
# (no --role) is pure stdlib, spawns the engine boot as its own ork.python
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

UPS = 60.0
# past the ~24k-update crossing where the float dt bias used to emit a 801 chunk.
UPDATES = int(os.environ.get("ORKID_CHUNKBOUNDS_TEST_UPDATES", "26000"))
DEADLINE_S = float(os.environ.get("ORKID_CHUNKBOUNDS_TEST_DEADLINE", "300"))
PUMPS_PER_ITER = 250          # batch per run-loop iteration: loop overhead only

# chunk sizes (in frames) that are NOT multiples of frames_per_controlpass(32);
# each is requested as an exact frames/SR interval.
ODD_CHUNKS = (1, 7, 31, 33, 63, 129, 801, 1023)

CRASH_SIGS = ("free(): invalid next size", "free(): invalid pointer", "double free",
              "malloc(): ", "corrupted", "Segmentation fault", "SIGSEGV",
              "terminate called after throwing")

# tells of a REAL device having been opened — none may appear headless.
FORBIDDEN_DEVICE = ("[audio.PA]", "[PA_DIAG]", "PCM name:", "[audio.PIPEWIRE]")


################################################################################
# CHILD — one engine boot: audio-only headless, a live sine voice, then the two
# pump phases (fixed-dt long haul, then the pathological chunk sizes).
################################################################################

def _build_sine_program(S):
  # deterministic voice with NO sample/asset dependency (same patch as
  # test_audio_wav_render): PolyBLEP sine through an amp envelope. A LIVE voice
  # matters here — the tail-pass overrun is in the voice/bus mix path.
  bank = S.BankData()
  prog = bank.newProgram("CHUNKBOUNDS")
  lyr = prog.newLayer()
  dspstg = lyr.appendStage("DSP")
  ampstg = lyr.appendStage("AMP")
  dspstg.ioconfig.inputs = [0, 1]
  dspstg.ioconfig.outputs = [0, 1]
  ampstg.ioconfig.inputs = [0]
  ampstg.ioconfig.outputs = [0, 1]
  pch = dspstg.appendDspBlock("Pitch", "pitch")
  lyr.pitchBlock = pch
  lyr.panmode = 0
  lyr.pan = 7
  ampenv = lyr.appendController("RateLevelEnv", "AMPENV")
  ampenv.ampenv = True
  ampenv.bipolar = False
  ampenv.sustainSegment = 1
  ampenv.addSegment("atk", 0.01, 1.0, 0.5)
  ampenv.addSegment("sus", 1.0, 1.0, 0.5)
  ampenv.addSegment("rel", 0.2, 0.0, 0.5)
  dspstg.appendDspBlock("OscilSine", "sine")
  ampblk = ampstg.appendDspBlock("AmpAdaptive", "amp")
  ampblk.paramByName("gain").mods.src1 = ampenv
  ampblk.paramByName("gain").mods.src1scale = 1.0
  return bank, prog


def _role_pump():
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import — setting it afterwards is
  # invisible to AppInitData and silently falls back to the host audio device.
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  import orkengine.core                    # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S
  from ork.testing import armed

  class App(object):
    def __init__(self):
      self.phase = 0          # 0=build patch, 1=fixed-dt pump, 2=odd chunks
      self.pumped = 0
      self.odd_done = 0
      self.sr = 0
      self.dt = 0.0
      self.frames_per_update = 0
      self.total_frames = 0
      self.available = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioStreamChunkBoundsTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                     # -> StrAudioDevice SYNC
          freerun=True)

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      ##########################################
      if self.phase == 0:
        syn.masterGain = 1.0
        self.bank, self.prog = _build_sine_program(S)
        syn.programbus.uiprogram = self.prog
        self.voice = syn.keyOn(60, 127, self.prog, None)
        # SR from the device itself — the dt values below must be exact frame
        # counts at whatever rate the engine is actually running.
        self.sr = dev.extractSamples(0).sample_rate
        self.frames_per_update = int(round(self.sr / UPS))
        self.dt = 1.0 / UPS
        self.phase = 1
        return
      ##########################################
      if self.phase == 1:
        for i in range(PUMPS_PER_ITER):
          if self.pumped >= UPDATES:
            break
          self.total_frames = dev.advanceTime(self.dt)
          self.pumped += 1
        if self.pumped >= UPDATES:
          self.phase = 2
        return
      ##########################################
      if self.phase == 2:
        frames = ODD_CHUNKS[self.odd_done]
        self.total_frames = dev.advanceTime(float(frames) / float(self.sr))
        self.odd_done += 1
        if self.odd_done >= len(ODD_CHUNKS):
          self.available = dev.availableSamples()
          self.ezapp.signalExit()

  app = App()
  with armed(DEADLINE_S, label="stream_chunk_bounds"):
    app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_SR=%d" % app.sr, flush=True)
  print("CHILD_FRAMES_PER_UPDATE=%d" % app.frames_per_update, flush=True)
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_ODD_DONE=%d" % app.odd_done, flush=True)
  print("CHILD_TOTAL_FRAMES=%d" % app.total_frames, flush=True)
  print("CHILD_AVAILABLE=%d" % app.available, flush=True)
  sys.exit(0)


################################################################################
# DRIVER — pure stdlib; spawns the child with a scrubbed env and classifies.
################################################################################

def _spawn_pump():
  import subprocess
  import time
  env = dict(os.environ)
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"        # C++ device logs must be captured
  env.pop("ORKID_DRM_MODE", None)           # never touch the physical display
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"     # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "pump"]
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


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out, elapsed = _spawn_pump()
  except subprocess.TimeoutExpired as e:
    combined = (e.stdout or "") + (e.stderr or "")
    print(combined if isinstance(combined, str) else combined.decode("utf-8", "replace"))
    sys.exit(verdict(False, "child exceeded the hard bound (watchdog did not fire) "
                            "deadline=%.0fs" % DEADLINE_S))

  print(out)

  sr = _num(out, "CHILD_SR", int, -1)
  fpu = _num(out, "CHILD_FRAMES_PER_UPDATE", int, -1)
  pumped = _num(out, "CHILD_PUMPED", int, -1)
  odd_done = _num(out, "CHILD_ODD_DONE", int, -1)
  total = _num(out, "CHILD_TOTAL_FRAMES", int, -1)
  available = _num(out, "CHILD_AVAILABLE", int, -1)

  # the whole point: EXACT, not approximate. one drifted sample fails.
  expected = (UPDATES * fpu + sum(ODD_CHUNKS)) if fpu > 0 else -1
  bound_ok = (pumped == UPDATES and odd_done == len(ODD_CHUNKS))
  sr_ok = (sr > 0 and fpu > 0 and (sr % int(UPS)) == 0)
  exact_ok = (expected > 0 and total == expected and available == expected)
  clean_ok = (rc == 0 and not any(sig in out for sig in CRASH_SIGS))
  headless_ok = not any(tok in out for tok in FORBIDDEN_DEVICE)

  passed = (bound_ok and sr_ok and exact_ok and clean_ok and headless_ok)
  detail = ("rc=%d wall=%.1fs bound=%s(pumped=%d/%d odd=%d/%d) sr=%s(%d fpu=%d) "
            "exact=%s(total=%d available=%d expected=%d) clean=%s headless=%s"
            % (rc, elapsed, bound_ok, pumped, UPDATES, odd_done, len(ODD_CHUNKS),
               sr_ok, sr, fpu, exact_ok, total, available, expected,
               clean_ok, headless_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "pump":
    _role_pump()
  else:
    _main_driver()
