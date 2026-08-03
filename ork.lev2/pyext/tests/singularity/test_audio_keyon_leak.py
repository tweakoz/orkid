#!/usr/bin/env ork.python
################################################################################
# test_audio_keyon_leak — per-note-on memory growth (RT leak fence).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   A note-on/note-off cycle returns everything it took. The synth instantiates a
#   ControllerInst per controller per note on the audio thread; those instances
#   used to be raw `new`ed and NEVER deleted (layer.cpp's own "todo pool
#   controllers"), so a finale keyon storm bled the process one instance per note
#   forever. The paired fix pools the storage and gives every instance an owner
#   (Layer::_ctrlBlock) that releases it at the next keyOn boundary.
#
#   Measurement is peak RSS (resource.getrusage), sampled AFTER a warmup that has
#   already grown every pool to its concurrency peak, then again after N more
#   identical cycles. A per-note leak shows up as a linear ramp; a bounded pool
#   shows up as zero. Peak (rather than current) RSS is the portable signal and
#   never under-reports a leak.
#
#   Asserts:
#     (a) the child ran the whole schedule and exited 0;
#     (b) growth over the measured window is <= LEAK_BUDGET bytes per note-on.
#
#   PROVEN RED: on the pre-fix tree this reports 157 bytes/keyon (786432 bytes
#   over 5000 notes). Peak RSS UNDER-reports — glibc mallinfo2 on the same run
#   shows the true 352 bytes/note (one RateLevelEnvInst) landing in already
#   resident heap — so the budget below is a fence, not a measurement.
#
# SHAPE (mirrors test_audio_keyon_rtalloc.py): the driver (no --role) is pure
# stdlib and spawns the engine boot as its OWN ork.python subprocess with
# ORKID_DRM_MODE stripped, so a measurement can never depend on / touch inherited
# display state. Portable: no interposer, no platform-specific tooling.
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

DT          = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
WARMUP      = 250           # note cycles run before the first RSS sample
NMEAS       = 5000          # measured note cycles
DRAIN       = 12            # pumps after each keyOff (release is 0.05s)
LEAK_BUDGET = 8.0           # bytes of peak-RSS growth per note-on


def _peak_rss_bytes():
  # ru_maxrss is KILOBYTES on linux, BYTES on darwin (documented divergence).
  import resource
  raw = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
  return raw if sys.platform == "darwin" else raw * 1024


################################################################################
# CHILD — one engine boot, audio-only headless, many note cycles.
################################################################################

def _build_probe_program(S):
  # the wav-render patch: deterministic, asset-free, 2 stages / 3 blocks / 1
  # controller. one controller per note is the smallest leak this can see.
  bank = S.BankData()
  prog = bank.newProgram("LEAKPROBE")
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
  ampenv.addSegment("rel", 0.05, 0.0, 0.5)
  dspstg.appendDspBlock("OscilSine", "sine")
  ampblk = ampstg.appendDspBlock("AmpAdaptive", "amp")
  ampblk.paramByName("gain").mods.src1 = ampenv
  ampblk.paramByName("gain").mods.src1scale = 1.0
  return bank, prog


def _role_child():
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0
      self.i = 0
      self.rss0 = 0
      self.rss1 = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioKeyOnLeakTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          freerun=True,
      )

    def _cycle(self):
      # one full note cycle: keyOn -> one pump -> keyOff -> drain.
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      note = 60 + (self.i % 12)
      voice = syn.keyOn(note, 127, self.prog, None)
      dev.advanceTime(DT)
      syn.keyOff(voice, note, 127)
      for _ in range(DRAIN):
        dev.advanceTime(DT)
      # CONSUME the rendered audio. nothing plays it back here, and the SYNC
      #  device's capture buffer would otherwise grow by the cycle's whole
      #  output (6240 frames == 49920 bytes) — 140x the leak being measured.
      dev.extractSamples(dev.availableSamples())

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      ############################################
      if self.phase == 0:   # build the patch; no audio time elapses here
        syn.masterGain = 1.0
        self.bank, self.prog = _build_probe_program(S)
        syn.programbus.uiprogram = self.prog
        dev.advanceTime(DT)
        self.phase = 1
        return
      ############################################
      if self.phase == 1:   # warmup: every pool grows to its peak here
        self._cycle()
        self.i += 1
        if self.i >= WARMUP:
          self.i = 0
          self.rss0 = _peak_rss_bytes()
          self.phase = 2
        return
      ############################################
      self._cycle()         # phase 2: the measured window
      self.i += 1
      if self.i >= NMEAS:
        self.rss1 = _peak_rss_bytes()
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  ok = (app.rss0 > 0 and app.rss1 > 0)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_RSS0=%d" % app.rss0, flush=True)
  print("CHILD_RSS1=%d" % app.rss1, flush=True)
  print("CHILD_NMEAS=%d" % NMEAS, flush=True)
  sys.exit(0 if ok else 3)


################################################################################
# DRIVER
################################################################################

def _spawn(timeout=1800):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "child"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep_int(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      try:
        return int(line.split("=", 1)[1].strip())
      except ValueError:
        return -1
  return -1


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out = _spawn()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child run timed out (possible wedge)"))

  print(out)

  rss0 = _grep_int(out, "CHILD_RSS0")
  rss1 = _grep_int(out, "CHILD_RSS1")
  n    = _grep_int(out, "CHILD_NMEAS")
  ran_ok = (rc == 0 and rss0 > 0 and rss1 > 0 and n == NMEAS)
  per_keyon = ((rss1 - rss0) / float(n)) if ran_ok else -1.0
  leak_ok = ran_ok and (per_keyon <= LEAK_BUDGET)

  passed = (ran_ok and leak_ok)
  detail = ("rc=%d n=%d rss0=%d rss1=%d growth=%d bytes_per_keyon=%.2f(<=%.1f) "
            "| ran=%s leak=%s"
            % (rc, n, rss0, rss1, (rss1 - rss0), per_keyon, LEAK_BUDGET,
               ran_ok, leak_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "child":
    _role_child()
  else:
    _main_driver()
