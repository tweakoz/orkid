#!/usr/bin/env ork.python
################################################################################
# test_audio_keyon_cost — the note-on WALL-CLOCK ratchet (RT deadline safety).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   A note-on costs BOUNDED TIME on the audio thread. test_audio_keyon_rtalloc
#   fences the allocator traffic of the same call; this fences the thing the
#   deadline actually cares about — microseconds. The two are not the same fact:
#   the pooling work that drove allocations to zero could be traded for a linear
#   scan and nobody would notice until a device underran.
#
#   Source of truth is the engine's own [KEYON_PROF] phase table (keyon_prof.h),
#   armed by ORKID_KEYON_PROF in the child's environment — the SAME instrument
#   that attributed the xrun spikes on hardware, so the number this gate ratchets
#   is the number the audio lane reasons about. The ". layerkeyon" row is
#   Layer::keyOn: controller instantiation, dsp grid instantiation and block
#   keyOn — the whole per-note construction, and the dominant term of the
#   event-drain phase that runs inside the device callback.
#
#   The SYNC_NONREALTIME device makes the measurement exact: dev.advanceTime()
#   runs the audio-thread control+compute pass INLINE on this thread, so the
#   probes see the engine work and no host-api scheduling noise. The accumulator
#   is flushed (report CLEARS it) after warmup, so the reported average covers
#   the measured cycles ONLY — cold alg/pool construction cannot be laundered
#   into the average.
#
#   Asserts:
#     (a) the table is actually armed — the ". layerkeyon" row is present with
#         count == NMEAS. A missing row means ORKID_KEYON_PROF did not reach the
#         library before load (the genviron trap) and the gate FAILS rather than
#         passing on an empty measurement;
#     (b) steady-state avg < LAYERKEYON_AVG_US;
#     (c) no single note-on exceeds LAYERKEYON_MAX_US — one 5ms outlier is an
#         xrun on a 256-frame buffer even when the average is spotless;
#     (d) the dsp/controller instance recycler took NOTHING from the allocator
#         over the measured window (synth.dspPoolMisses delta == 0) — the same
#         steady-state expectation test_audio_keyon_rtalloc fences, restated here
#         because a timing regression and a pool-miss regression share a cause;
#     (e) the child ran the whole schedule and exited 0.
#
#   THRESHOLDS are fences, not targets. The achieved steady state after the
#   note-on optimization work is 26-60us depending on host load; the average
#   fence sits at 2.5x the top of that band. Tighten it when the achieved number
#   drops — never widen it to make a red run green.
#
# SHAPE (mirrors test_audio_keyon_rtalloc.py): the driver (no --role) is pure
# stdlib and spawns the engine boot as its OWN ork.python subprocess with
# ORKID_DRM_MODE stripped and the ioclass pinned to STREAM, so the measurement
# can never touch the physical display or a host audio device.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import re
import sys
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT     = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
WARMUP = 32            # note cycles run before the accumulator is flushed
NMEAS  = 64            # measured note cycles (the ratcheted window)
DRAIN  = 12            # pumps after each keyOff (release is 0.05s)

LAYERKEYON_AVG_US = 150.0    # 2.5x the achieved 26-60us band
LAYERKEYON_MAX_US = 1500.0   # any single note-on; 10x the average fence

# "[KEYON_PROF] . layerkeyon   total<..us> count<N> avg<..us> max<..us> worst<..>"
_ROW_RE = re.compile(
    r"^\[KEYON_PROF\]\s+\.\s+layerkeyon\s+total<([0-9.eE+-]+)us>\s+"
    r"count<(\d+)>\s+avg<([0-9.eE+-]+)us>\s+max<([0-9.eE+-]+)us>")

MARK_BEGIN = "CHILD_PROF_BEGIN"
MARK_END   = "CHILD_PROF_END"


################################################################################
# CHILD — one engine boot, audio-only headless, measured note cycles.
################################################################################

def _build_probe_program(S):
  # the wav-render patch: deterministic, asset-free, 2 stages / 3 blocks / 1
  # controller — the same grid test_audio_keyon_rtalloc measures, so the two
  # gates fence the same call over the same work.
  bank = S.BankData()
  prog = bank.newProgram("KEYONCOST")
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
  from orkengine import lev2
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0
      self.i = 0
      self.miss0 = -1
      self.miss1 = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioKeyOnCostTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          freerun=True,
      )

    def _cycle(self):
      # one full note cycle: keyOn -> one pump -> keyOff -> drain. the keyOn
      # event is DEFERRED to the pump, which is where the probe lives.
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      note = 60 + (self.i % 12)
      voice = syn.keyOn(note, 127, self.prog, None)
      dev.advanceTime(DT)
      syn.keyOff(voice, note, 127)
      for _ in range(DRAIN):
        dev.advanceTime(DT)

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
          # flush-and-clear: everything above is discarded from the table.
          lev2.audioKeyOnProfReport()
          self.miss0 = syn.dspPoolMisses
          self.phase = 2
        return
      ############################################
      self._cycle()         # phase 2: the measured window
      self.i += 1
      if self.i >= NMEAS:
        self.miss1 = syn.dspPoolMisses
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)

  print(MARK_BEGIN, flush=True)
  lev2.audioKeyOnProfReport()
  print(MARK_END, flush=True)
  print("CHILD_POOLMISSES=%d,%d" % (app.miss0, app.miss1), flush=True)
  print("CHILD_CYCLES=%d" % (WARMUP + app.i), flush=True)
  sys.exit(0)


################################################################################
# DRIVER
################################################################################

def _spawn(timeout=300):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"         # never open a host audio device
  env["ORKID_KEYON_PROF"] = "1"                 # arm the phase table (read at load)
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "child"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].strip()
  return None


def _prof_block(out):
  # only the report emitted AFTER the measured window counts; the warmup flush
  # went to the same stdout and must not be parsed.
  lines = out.splitlines()
  try:
    b = lines.index(MARK_BEGIN)
    e = lines.index(MARK_END, b)
  except ValueError:
    return None
  return lines[b + 1:e]


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out = _spawn()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child run timed out (possible wedge)"))

  print(out)

  block = _prof_block(out)
  row = None
  for line in (block or []):
    m = _ROW_RE.match(line.strip())
    if m:
      row = m
      break

  cycles = _grep(out, "CHILD_CYCLES")
  misses = _grep(out, "CHILD_POOLMISSES")

  ran_ok = (rc == 0 and cycles == str(WARMUP + NMEAS))

  count = int(row.group(2)) if row else -1
  avg_us = float(row.group(3)) if row else -1.0
  max_us = float(row.group(4)) if row else -1.0

  armed_ok = (row is not None) and (count == NMEAS)
  avg_ok   = armed_ok and (avg_us < LAYERKEYON_AVG_US)
  max_ok   = armed_ok and (max_us < LAYERKEYON_MAX_US)

  miss_delta = -1
  if misses:
    parts = misses.split(",")
    if len(parts) == 2:
      miss_delta = int(parts[1]) - int(parts[0])
  miss_ok = (miss_delta == 0)

  passed = (ran_ok and armed_ok and avg_ok and max_ok and miss_ok)
  detail = ("rc=%d cycles=%s layerkeyon_count=%d(==%d) avg=%.2fus(<%.0f) "
            "max=%.1fus(<%.0f) dspPoolMisses=%s delta=%d | "
            "ran=%s armed=%s avg_ok=%s max_ok=%s poolmiss=%s"
            % (rc, cycles, count, NMEAS, avg_us, LAYERKEYON_AVG_US,
               max_us, LAYERKEYON_MAX_US, misses, miss_delta,
               ran_ok, armed_ok, avg_ok, max_ok, miss_ok))
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
