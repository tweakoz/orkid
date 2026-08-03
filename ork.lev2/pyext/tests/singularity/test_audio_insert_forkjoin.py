#!/usr/bin/env ork.python
################################################################################
# test_audio_insert_forkjoin — InsertGroup fork/join + per-voice sends (slice A0c).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   1. A parallel insert group computes ALL of its branches and sums them.
#      Before A0c the group ran branch 0 only and SILENTLY DROPPED the rest, so
#      the two-branch render was indistinguishable from the one-branch render —
#      G1 below is exactly that regression check.
#   2. The join is _mixGain-weighted.
#   3. Branches are independent (each computes with its OWN layerdata/params),
#      not branch 0 evaluated N times.
#   4. A voice's per-voice send (LayerData.sendBus / .sendLevel) puts a
#      level-weighted copy of the voice on a DIFFERENT bus — the shared-send-bus
#      shape the reverb-send decision needs.
#
# METHOD: the deterministic offline WAV-render vehicle of test_audio_wav_render
#   (audio-only headless EzApp, fixed-iteration advanceTime pump, WAV tee), run
#   once per scenario as its own child process, with the scenarios chosen so the
#   expected relationships are EXACT float arithmetic rather than eyeballed RMS:
#
#     base      1 branch  @ 0dB,  mixGain 1.0            -> b
#     fork2     2 branches @ 0dB,  mixGain 1.0           -> b+b  == 2*base EXACTLY
#     fork2half 2 branches @ 0dB,  mixGain 0.5           -> .5*(b+b) == base, BYTEWISE
#     fork2gain branches @ 0dB and -20dB, mixGain 1.0    -> ~1.1*base
#     sendoff   no inserts, sendBus=aux sendLevel 0.0    -> dry only
#     sendhalf  no inserts, sendBus=aux sendLevel 0.5    -> ~1.5*sendoff
#     sendfull  no inserts, sendBus=aux sendLevel 1.0    -> 2*sendoff EXACTLY
#
#   (x+x and 0.5*(x+x) are exact in binary floating point, which is what lets G1
#   and G2 assert bit equality instead of a tolerance.)
#   fork2 is rendered TWICE to assert byte-identical double-run determinism of
#   the fork/join path itself.
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure
# stdlib + numpy and spawns each engine-booting render as its OWN ork.python
# subprocess with ORKID_DRM_MODE stripped from the child env.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
# NOTE: subprocess / struct / hashlib / tempfile are imported lazily inside the
# driver-only functions. hashlib pre-loads the SYSTEM libcrypto, which then makes
# the staged libssl fail its OPENSSL_3.3.0 version check when the CHILD later
# imports orkengine.core — so the child path must touch none of them first.

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

# Deterministic pump: DT * engine-SR must be a whole number of frames per call so
# the total frame count is exact. At the engine's 48kHz, 0.01s -> 480 frames/call.
DT = 1.0 / 100.0
N_ITERS = 120
RMS_FLOOR = 0.02
REL_TOL = 1.0e-5

# scenario -> (branch gains in dB or None for "no insert group", group mixGain,
#              send level or None for "no send")
SCENARIOS = {
  "base":      ([0.0],        1.0, None),
  "fork2":     ([0.0, 0.0],   1.0, None),
  "fork2half": ([0.0, 0.0],   0.5, None),
  "fork2gain": ([0.0, -20.0], 1.0, None),
  "sendoff":   (None,         1.0, 0.0),
  "sendhalf":  (None,         1.0, 0.5),
  "sendfull":  (None,         1.0, 1.0),
  # live edits of an installed chain WHILE audio runs (the branch build is
  # off-RT, the list edit is an audio-thread event) — group topology is read
  # back after each edit's pump, and the render must survive all of it.
  "mutate":    ([0.0],        1.0, None),
}
SEND_BUS = "aux"
# pump index -> edit applied to the "mutate" scenario's chain
MUT_STEPS = [24, 48, 72, 96]


################################################################################
# CHILD — one engine boot: audio-only headless, build the scenario, pump
# advanceTime N_ITERS times teeing to --wav, then exit cleanly.
################################################################################

def _build_sine_program(S, bank):
  # Deterministic voice with NO sample/asset dependency: a PolyBLEP sine
  # oscillator through an amp envelope (verbatim from test_audio_wav_render).
  prog = bank.newProgram("WAVTEST")
  lyr  = prog.newLayer()
  dspstg = lyr.appendStage("DSP")
  ampstg = lyr.appendStage("AMP")
  dspstg.ioconfig.inputs  = [0, 1]
  dspstg.ioconfig.outputs = [0, 1]
  ampstg.ioconfig.inputs  = [0]        # single input -> AmpAdaptive duplicates mono to L+R
  ampstg.ioconfig.outputs = [0, 1]
  pch = dspstg.appendDspBlock("Pitch", "pitch")
  lyr.pitchBlock = pch
  lyr.panmode = 0     # Fixed
  lyr.pan = 7         # (pan-7)/7 == 0.0 -> centered: both channels get signal
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
  return prog, lyr


def _build_insert_branch(bank, index, gaindb):
  # One fork/join branch: a bus-processor layerdata whose alg is a single
  # stereo-in/stereo-out stage holding one static-gain block. The gain is what
  # makes a branch identifiable in the summed output.
  prog = bank.newProgram("INSERT%d" % index)
  lyr  = prog.newLayer()
  stg  = lyr.appendStage("FX")
  stg.ioconfig.inputs  = [0, 1]
  stg.ioconfig.outputs = [0, 1]
  blk = stg.appendDspBlock("AmpStereoGain", "gain")
  blk.paramByName("gain").coarse = gaindb   # dB; 0 -> unity
  return lyr


def _role_render(scenario, wav_path):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  branch_gains, mixgain, sendlevel = SCENARIOS[scenario]

  class App(object):
    def __init__(self):
      self.phase = 0            # 0=await synth, 1=pumping
      self.pumped = 0
      self.total_frames = 0
      self.sr = 0
      self.numgroups = -1
      self.numbranches = -1
      self.mutobs = []          # topology observed just before each live edit
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioInsertForkJoinTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          wav_output_path=wav_path,
          freerun=True,
      )

    def _mutate(self, step):
      # observe FIRST (the previous step's event has been through a pump), then
      # edit. the InsertGroup handle is re-fetched every time: it is a pointer
      # into the bus's group vector, which group removal shifts.
      ngroups = self.mainbus.numInsertGroups
      nbranch = self.mainbus.insertGroup(0).numLayers if ngroups else 0
      self.mutobs.append((ngroups, nbranch))
      if step == 0:
        self.mainbus.insertGroup(0).addLayer(self.spare)
      elif step == 1:
        self.mainbus.insertGroup(0).removeLayer(1)
      elif step == 2:
        self.mainbus.removeInsertGroup(1)
      elif step == 3:
        self.mainbus.clearInserts()

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      if self.phase == 0:
        # build + key the scenario; no audio time elapses here.
        syn.masterGain = 1.0
        self.bank = S.BankData()
        self.prog, self.lyr = _build_sine_program(S, self.bank)
        self.mainbus = syn.outputBus("main")
        if sendlevel is not None:
          # send bus must exist BEFORE the voice keys on: the send target is
          # resolved once, at keyOn, and a missing bus is a loud error.
          self.sendbus = syn.createOutputBus(SEND_BUS)
          self.lyr.sendBus = SEND_BUS
          self.lyr.sendLevel = sendlevel
        if branch_gains is not None:
          self.branches = [_build_insert_branch(self.bank, i, g)
                           for i, g in enumerate(branch_gains)]
          self.mainbus.addParallelInsert(self.branches, mixgain)
        if scenario == "mutate":
          # a second group to remove later, and a spare branch to add
          self.spare = _build_insert_branch(self.bank, 8, 0.0)
          self.mainbus.addParallelInsert([_build_insert_branch(self.bank, 9, 0.0)], 1.0)
        syn.programbus.uiprogram = self.prog
        self.voice = syn.keyOn(60, 127, self.prog, None)
        self.phase = 1
        return
      # phase 1: deterministic fixed-step pump; advanceTime returns cumulative frames.
      if self.pumped < N_ITERS:
        self.total_frames = dev.advanceTime(DT)
        self.pumped += 1
        if scenario == "mutate" and self.pumped in MUT_STEPS:
          self._mutate(MUT_STEPS.index(self.pumped))
      if self.pumped >= N_ITERS:
        self.sr = dev.extractSamples(0).sample_rate   # SR w/o consuming/affecting WAV
        # read back the installed topology (the install is an audio-thread event,
        # so this is only meaningful now, after the pump).
        self.numgroups = self.mainbus.numInsertGroups
        if self.numgroups > 0:
          self.numbranches = self.mainbus.insertGroup(0).numLayers
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_TOTAL_FRAMES=%d" % app.total_frames, flush=True)
  print("CHILD_SR=%d" % app.sr, flush=True)
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_GROUPS=%d" % app.numgroups, flush=True)
  print("CHILD_BRANCHES=%d" % app.numbranches, flush=True)
  print("CHILD_MUTOBS=%s" % ",".join("%d:%d" % gb for gb in app.mutobs), flush=True)
  sys.exit(0 if (app.pumped == N_ITERS and app.total_frames > 0 and app.sr > 0) else 3)


################################################################################
# DRIVER helpers — hand-rolled RIFF/WAVE parse (python `wave` can't read float).
################################################################################

def _parse_wav(path):
  import struct
  import numpy as np
  with open(path, "rb") as f:
    data = f.read()
  if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
    raise ValueError("not a RIFF/WAVE file")
  fmt = None
  audio = None
  pos = 12
  while pos + 8 <= len(data):
    cid = data[pos:pos + 4]
    csz = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    body = data[pos + 8:pos + 8 + csz]
    if cid == b"fmt ":
      fmt = body
    elif cid == b"data":
      audio = body
    pos += 8 + csz + (csz & 1)          # chunks are word-aligned
  if fmt is None or audio is None:
    raise ValueError("missing fmt/data chunk")
  fmt_tag, channels, sr, _byte_rate, block_align, bits = struct.unpack("<HHIIHH", fmt[:16])
  is_float = (fmt_tag == 3)             # WAVE_FORMAT_IEEE_FLOAT
  if fmt_tag == 0xFFFE and len(fmt) >= 26:   # EXTENSIBLE: subformat GUID prefix
    is_float = (struct.unpack("<H", fmt[24:26])[0] == 3)
  frames = len(audio) // block_align if block_align else 0
  floats = np.frombuffer(audio, dtype="<f4")
  return dict(channels=channels, sr=sr, is_float=is_float, bits=bits,
              frames=frames, floats=floats)


def _spawn_render(scenario, wav_path, timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "render", "--scenario", scenario, "--wav", wav_path]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _sha256(path):
  import hashlib
  h = hashlib.sha256()
  with open(path, "rb") as f:
    for blk in iter(lambda: f.read(1 << 16), b""):
      h.update(blk)
  return h.hexdigest()


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


def _maxreldiff(a, b, scale):
  """max |a - scale*b| relative to the peak of |scale*b| (0.0 == bit equality)."""
  import numpy as np
  ref = b * scale
  peak = float(np.max(np.abs(ref)))
  if peak <= 0.0:
    return 1.0e9
  return float(np.max(np.abs(a - ref))) / peak


################################################################################
# DRIVER — spawns one render per scenario and classifies.
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  # (label, scenario) — fork2 twice for the double-run determinism check.
  renders = [("base", "base"), ("fork2", "fork2"), ("fork2_dup", "fork2"),
             ("fork2half", "fork2half"), ("fork2gain", "fork2gain"),
             ("sendoff", "sendoff"), ("sendhalf", "sendhalf"), ("sendfull", "sendfull"),
             ("mutate", "mutate")]

  tmp = tempfile.mkdtemp(prefix="insertfj_")
  wavs = {label: os.path.join(tmp, label + ".wav") for label, _ in renders}
  rcs = {}
  outs = {}

  try:
    for label, scenario in renders:
      rcs[label], outs[label] = _spawn_render(scenario, wavs[label])
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child render timed out (possible wedge)"))

  for label, _ in renders:
    print("---- %s (rc=%d) ----" % (label, rcs[label]))
    print(outs[label])

  all_rc_ok = all(rc == 0 for rc in rcs.values())
  all_exist = all(os.path.isfile(p) for p in wavs.values())

  g0 = g1 = g2 = g3 = g4 = g5 = g6 = g7 = False
  detail_extra = ""
  if all_rc_ok and all_exist:
    W = {label: _parse_wav(wavs[label]) for label, _ in renders}
    ch = {label: W[label]["floats"] for label, _ in renders}
    expected = N_ITERS * int(round(DT * W["base"]["sr"])) if W["base"]["sr"] else -1
    child_frames = int(_grep(outs["base"], "CHILD_TOTAL_FRAMES") or -1)

    # G0: every render is a 2ch float WAV of the same exact length, and the
    #     baseline is not silence (an all-zero render would pass every ratio).
    same_len = len(set(len(ch[label]) for label, _ in renders)) == 1
    rms_base = float(np.sqrt(np.mean(ch["base"] * ch["base"])))
    g0 = (same_len
          and all(W[label]["channels"] == 2 and W[label]["is_float"] for label, _ in renders)
          and W["base"]["frames"] == expected and W["base"]["frames"] == child_frames
          and rms_base > RMS_FLOOR)

    # the group actually installed as N branches (topology, not just signal)
    g6 = (_grep(outs["fork2"], "CHILD_GROUPS") == "1"
          and _grep(outs["fork2"], "CHILD_BRANCHES") == "2"
          and _grep(outs["base"], "CHILD_BRANCHES") == "1")

    # G1: the fork/join SUMS the branches. pre-A0c the extra branch was dropped,
    #     which makes d_dropped ~0 and d_sum ~1 — i.e. this fails loudly.
    d_sum = _maxreldiff(ch["fork2"], ch["base"], 2.0)
    d_dropped = _maxreldiff(ch["fork2"], ch["base"], 1.0)
    g1 = (d_sum == 0.0) and (d_dropped > REL_TOL)

    # G2: _mixGain weights the join — 0.5*(b+b) is bit-identical to 1.0*b.
    g2 = (_sha256(wavs["fork2half"]) == _sha256(wavs["base"]))

    # G3: branches are independent — the -20dB branch contributes 0.1x, not 1x.
    d_indep = _maxreldiff(ch["fork2gain"], ch["base"], 1.1)
    g3 = (d_indep <= REL_TOL)

    # G4: per-voice send. level 0 leaves the target bus silent (== dry only),
    #     level 0.5 adds half the voice, level 1.0 doubles it exactly.
    d_half = _maxreldiff(ch["sendhalf"], ch["sendoff"], 1.5)
    d_full = _maxreldiff(ch["sendfull"], ch["sendoff"], 2.0)
    d_sendnull = _maxreldiff(ch["sendhalf"], ch["sendoff"], 1.0)
    g4 = (d_half <= REL_TOL) and (d_full == 0.0) and (d_sendnull > REL_TOL)

    # G5: double-run byte determinism of the fork/join path.
    g5 = (_sha256(wavs["fork2"]) == _sha256(wavs["fork2_dup"]))

    # G7: live chain edits (addLayer / removeLayer / removeInsertGroup /
    #     clearInserts) land in order and leave the chain empty — observed
    #     groups:branches BEFORE each successive edit.
    mutobs = _grep(outs["mutate"], "CHILD_MUTOBS")
    g7 = (mutobs == "2:1,2:2,2:1,1:1"
          and _grep(outs["mutate"], "CHILD_GROUPS") == "0")

    detail_extra = ("frames=%d expected=%d rms_base=%.4f d_sum=%.3e d_dropped=%.3e "
                    "d_indep=%.3e d_half=%.3e d_full=%.3e d_sendnull=%.3e mutobs=%s"
                    % (W["base"]["frames"], expected, rms_base, d_sum, d_dropped,
                       d_indep, d_half, d_full, d_sendnull, mutobs))

  passed = all_rc_ok and all_exist and g0 and g1 and g2 and g3 and g4 and g5 and g6 and g7
  detail = ("rc_ok=%s exists=%s G0_render=%s G1_forkjoin_sum=%s G2_mixgain=%s "
            "G3_branch_independence=%s G4_voice_send=%s G5_determinism=%s "
            "G6_topology=%s G7_live_edits=%s %s"
            % (all_rc_ok, all_exist, g0, g1, g2, g3, g4, g5, g6, g7, detail_extra))
  shutil.rmtree(tmp, ignore_errors=True)
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--scenario", default="base")
  ap.add_argument("--wav", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.scenario, args.wav)
  else:
    _main_driver()
