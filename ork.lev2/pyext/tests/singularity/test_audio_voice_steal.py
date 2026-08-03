#!/usr/bin/env ork.python
################################################################################
# test_audio_voice_steal — voice stealing under pool exhaustion (slice A0b).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   The synth survives being asked for more voices than the 512-layer pool holds.
#   Before A0b, allocLayer's `assert(it != _freeVoices.end())` killed the process
#   on the 513th layer; now the worst-ranked ACTIVE voice is keyed off and
#   reclaimed in place, and synthesis continues.
#
#   The load is a 4-LAYER sustaining program, keyed on and never released, so the
#   voice pool is the binding constraint (4 layers per note => the 512 layers run
#   out at 128 notes while the 512-instance programInst pool is only a quarter
#   used). Driving the note pool to exhaustion instead would test a different
#   allocator on a different thread — not this slice.
#
#   Child role "steal" (default PRIORITY policy), asserts:
#     (a) the run completes: no assert, no crash, rc == 0;
#     (b) the active-voice count NEVER exceeds kmaxlayerspersynth (512);
#     (c) the pool actually saturated (peak active == 512) — proves the steal
#         path was reached rather than silently skipped;
#     (d) synth.stealCount > 0, and matches the layers that had to be stolen;
#     (e) audio continues AFTER the last keyOn: RMS of the freshly generated tail
#         is above a floor (a wedged/steal-corrupted synth goes silent).
#   Child role "off" (stealPolicy=OFF, load kept UNDER the pool), asserts the
#     legacy path is untouched: stealCount == 0 and every note is still sounding.
#   A third run repeats the steal load with voiceHeadroom set, asserting the
#     proactive-release path is bounded, still sounding, and never makes the hard
#     (audible) steal count WORSE than headroom=0 — the whole point of the knob.
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure stdlib
# and spawns each engine boot as its OWN ork.python subprocess with ORKID_DRM_MODE
# stripped, so a render can never depend on / touch inherited display state.
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

MAX_VOICES     = 512     # kmaxlayerspersynth (krztypes.h)
LAYERS_PER_PRG = 4
N_NOTES_STEAL  = 200     # 800 layers -> 288 steals once the pool saturates
N_NOTES_OFF    = 100     # 400 layers -> fits, so OFF must never steal
# Free-voice reserve the proactive-release pass defends. It has to cover the
# release latency at the load's consumption rate (here 4 layers per pump against
# a 0.2s release = 20 pumps, so ~80 voices in flight); below that the pool still
# empties and the hard cut still happens.
HEADROOM       = 128
DT             = 1.0 / 100.0   # 480 frames @48k: whole control passes (32/pass)
TAIL_ITERS     = 20            # pumps after the last keyOn, for the RMS probe
RMS_FLOOR      = 1.0e-4


################################################################################
# CHILD — one engine boot, audio-only headless.
################################################################################

def _build_sustain_program(S, nlayers):
  # Deterministic, asset-free voice (the wav-render patch), replicated across
  # nlayers. The amp envelope SUSTAINS at full level and is never keyed off, so
  # no voice ever frees itself — the pool can only be recovered by stealing.
  bank = S.BankData()
  prog = bank.newProgram("STEALTEST")
  for i in range(nlayers):
    lyr    = prog.newLayer()
    dspstg = lyr.appendStage("DSP")
    ampstg = lyr.appendStage("AMP")
    dspstg.ioconfig.inputs  = [0, 1]
    dspstg.ioconfig.outputs = [0, 1]
    ampstg.ioconfig.inputs  = [0]
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
    ampenv.addSegment("sus", 1.0,  1.0, 0.5)
    ampenv.addSegment("rel", 0.2,  0.0, 0.5)
    dspstg.appendDspBlock("OscilSine", "sine")
    ampblk = ampstg.appendDspBlock("AmpAdaptive", "amp")
    ampblk.paramByName("gain").mods.src1 = ampenv
    ampblk.paramByName("gain").mods.src1scale = 1.0
  return bank, prog


def _role_child(policy, nnotes, headroom):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0
      self.keyed = 0
      self.peak_active = 0
      self.steals = 0
      self.tail_rms = -1.0
      self.voices = []
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioVoiceStealTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          freerun=True,
      )

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      ############################################
      if self.phase == 0:   # setup: knobs land through the event queue
        syn.masterGain = 1.0
        syn.stealPolicy = policy
        syn.voiceHeadroom = headroom
        self.bank, self.prog = _build_sustain_program(S, LAYERS_PER_PRG)
        syn.programbus.uiprogram = self.prog
        dev.advanceTime(DT)              # drain the knob events before keying
        self.phase = 1
        return
      ############################################
      if self.phase == 1:   # one note per pump, so the RT tick can recycle
        self.voices.append(syn.keyOn(48 + (self.keyed % 24), 100, self.prog, None))
        self.keyed += 1
        dev.advanceTime(DT)
        self.peak_active = max(self.peak_active, syn.numActiveVoices)
        if self.keyed >= nnotes:
          dev.extractSamples(1 << 30)    # discard everything generated so far
          self.phase = 2
        return
      ############################################
      # phase 2: audio must still be flowing AFTER the last keyOn.
      for _ in range(TAIL_ITERS):
        dev.advanceTime(DT)
        self.peak_active = max(self.peak_active, syn.numActiveVoices)
      cap = dev.extractSamples(1 << 30)
      acc = 0.0
      for v in cap.left:
        acc += v * v
      self.tail_rms = (acc / len(cap.left)) ** 0.5 if len(cap.left) else -1.0
      self.steals = syn.stealCount
      self.final_active = syn.numActiveVoices
      self.ezapp.signalExit()

  app = App()
  app.final_active = 0
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_KEYED=%d" % app.keyed, flush=True)
  print("CHILD_PEAK_ACTIVE=%d" % app.peak_active, flush=True)
  print("CHILD_FINAL_ACTIVE=%d" % app.final_active, flush=True)
  print("CHILD_STEALS=%d" % app.steals, flush=True)
  print("CHILD_TAIL_RMS=%.9f" % app.tail_rms, flush=True)
  sys.exit(0 if app.keyed == nnotes else 3)


################################################################################
# DRIVER
################################################################################

def _spawn(policy, nnotes, headroom=0, timeout=600):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "child",
         "--policy", str(policy), "--notes", str(nnotes),
         "--headroom", str(headroom)]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key, cast=int, missing=-1):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return cast(line.split("=", 1)[1].split()[0])
  return missing


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc_steal, out_steal = _spawn(3, N_NOTES_STEAL)               # 3 == PRIORITY
    rc_off, out_off     = _spawn(0, N_NOTES_OFF)                 # 0 == OFF
    rc_hr, out_hr       = _spawn(3, N_NOTES_STEAL, HEADROOM)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child run timed out (possible wedge)"))

  print(out_steal)
  print(out_off)
  print(out_hr)

  keyed   = _grep(out_steal, "CHILD_KEYED")
  peak    = _grep(out_steal, "CHILD_PEAK_ACTIVE")
  steals  = _grep(out_steal, "CHILD_STEALS")
  rms     = _grep(out_steal, "CHILD_TAIL_RMS", float, -1.0)

  off_keyed  = _grep(out_off, "CHILD_KEYED")
  off_active = _grep(out_off, "CHILD_FINAL_ACTIVE")
  off_steals = _grep(out_off, "CHILD_STEALS")

  hr_keyed  = _grep(out_hr, "CHILD_KEYED")
  hr_peak   = _grep(out_hr, "CHILD_PEAK_ACTIVE")
  hr_steals = _grep(out_hr, "CHILD_STEALS")
  hr_rms    = _grep(out_hr, "CHILD_TAIL_RMS", float, -1.0)

  # (a) survived, (b) never over the pool bound, (c) pool actually saturated,
  # (d) steals happened and cover every layer past the bound, (e) still sounding
  survived_ok = (rc_steal == 0 and keyed == N_NOTES_STEAL)
  bound_ok    = (0 < peak <= MAX_VOICES)
  saturate_ok = (peak == MAX_VOICES)
  expect_steal = (N_NOTES_STEAL * LAYERS_PER_PRG) - MAX_VOICES
  steal_ok    = (steals >= expect_steal)
  rms_ok      = (rms > RMS_FLOOR)

  # OFF policy under the bound: the legacy path, untouched
  off_ok = (rc_off == 0 and off_keyed == N_NOTES_OFF and off_steals == 0
            and off_active == N_NOTES_OFF * LAYERS_PER_PRG)

  # headroom: same load, bounded, still sounding, and STRICTLY fewer hard (i.e.
  # audible) cuts than headroom=0 — that reduction is the knob's whole purpose.
  hr_ok = (rc_hr == 0 and hr_keyed == N_NOTES_STEAL and 0 < hr_peak <= MAX_VOICES
           and hr_rms > RMS_FLOOR and hr_steals < steals)

  passed = (survived_ok and bound_ok and saturate_ok and steal_ok and rms_ok
            and off_ok and hr_ok)
  detail = ("rc=%d keyed=%d peak=%d(<=%d) steals=%d(>=%d) tailrms=%.6f | "
            "OFF rc=%d keyed=%d active=%d steals=%d | "
            "HR%d rc=%d keyed=%d peak=%d steals=%d tailrms=%.6f | "
            "survived=%s bound=%s saturated=%s stole=%s sounding=%s off=%s headroom=%s"
            % (rc_steal, keyed, peak, MAX_VOICES, steals, expect_steal, rms,
               rc_off, off_keyed, off_active, off_steals,
               HEADROOM, rc_hr, hr_keyed, hr_peak, hr_steals, hr_rms,
               survived_ok, bound_ok, saturate_ok, steal_ok, rms_ok, off_ok, hr_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--policy", type=int, default=3)
  ap.add_argument("--notes", type=int, default=N_NOTES_STEAL)
  ap.add_argument("--headroom", type=int, default=0)
  args, _ = ap.parse_known_args()

  if args.role == "child":
    _role_child(args.policy, args.notes, args.headroom)
  else:
    _main_driver()
