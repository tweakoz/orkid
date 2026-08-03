#!/usr/bin/env ork.python
################################################################################
# test_audio_pan_law — panBlend() constant-power law, measured through the REAL
# voice mixdown (no analytic-only shortcut, no old-law A/B).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   panBlend() (alg_pan.inl, consumed by Layer::currentMixGains / replaceBus /
#   AMPU_AMPL) implements the quarter-sine equal-power law that PANNER,
#   PANNER2D and PANNER2DU::compute already use:
#       pos = 0.5 + inp*0.5 ; lmix = cos(pos*PI/2) ; rmix = sin(pos*PI/2)
#   A linear crossfade (the pre-2026-07 law) satisfies the endpoints but NOT the
#   unity-sum-of-squares invariant, so it fails the mid/center rows below.
#
#   Each pan position is its own engine boot (audio-only headless EzApp, the
#   test_audio_wav_render.py idiom) rendering a deterministic pure-oscillator
#   voice to a WAV, panned via panmode 4 (Fixed floatPan) so the sweep hits
#   -1/-0.5/0/+0.5/+1 EXACTLY (integer lyr.pan only steps in 1/7ths).
#
#   Per-channel gain == per-channel WAV RMS divided by the unpanned source RMS.
#   The source reference is taken from a HARD-PANNED endpoint, where the law's
#   hot-side gain is exactly 1 (cos 0 == 1) — deriving it from the center render
#   instead would make the center assertion circular.
#
#   The driver asserts:
#     (a) both hard-pan endpoints agree on the source RMS (cross-boot
#         determinism of the render itself) and it is above a silence floor;
#     (b) endpoints exact: silent channel RMS == 0, hot channel gain == 1;
#     (c) every swept position matches the quarter-sine analytic within 1%;
#     (d) lmix^2+rmix^2 == 1 within 1% at every position (the constant-power law
#         proper — this is the row a linear crossfade cannot pass);
#     (e) center == 0.7071 per side within 1%.
#
# SHAPE: driver (no --role) is pure stdlib + numpy and spawns each engine-booting
# render as its OWN ork.python subprocess with ORKID_DRM_MODE stripped, parses
# the float WAV by hand (python's wave module cannot), emits the machine verdict.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import math
# NOTE: subprocess / struct / tempfile stay lazy inside driver-only functions —
# the CHILD path must not pre-load system crypto/ssl before orkengine.core
# (see test_audio_wav_render.py's note on the staged libssl version check).

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

# Deterministic pump, same fixed step as the WAV render canary: 0.01s * 48kHz is
# a whole 480 frames per call. Short on purpose — the envelope sustains, so RMS
# is dominated by steady state and the whole sweep stays inside a few seconds.
DT = 1.0 / 100.0
N_ITERS = 120

PANS = [-1.0, -0.5, 0.0, 0.5, 1.0]
TOL = 0.01           # 1% of full scale, absolute on gains and on the sumsq
SILENT_EPS = 1.0e-6  # a muted channel is EXACTLY zero, not merely small
SRC_FLOOR = 0.02     # source RMS floor (a silent render must not pass vacuously)


def _analytic(inp):
  pos = 0.5 + inp * 0.5
  return math.cos(pos * math.pi * 0.5), math.sin(pos * math.pi * 0.5)


################################################################################
# CHILD — one engine boot: build+keyOn a deterministic sine voice at one fixed
# pan position, pump advanceTime N_ITERS times teeing to --wav, exit cleanly.
################################################################################

def _build_sine_program(S, fpan):
  # Mirrors test_audio_wav_render.py's patch (no sample/asset dependency) with
  # panmode 4 so the pan position is a float, not a 1/7th step.
  bank = S.BankData()
  prog = bank.newProgram("PANTEST")
  lyr = prog.newLayer()
  dspstg = lyr.appendStage("DSP")
  ampstg = lyr.appendStage("AMP")
  dspstg.ioconfig.inputs = [0, 1]
  dspstg.ioconfig.outputs = [0, 1]
  ampstg.ioconfig.inputs = [0]        # single input -> AmpAdaptive duplicates mono to L+R
  ampstg.ioconfig.outputs = [0, 1]
  pch = dspstg.appendDspBlock("Pitch", "pitch")
  lyr.pitchBlock = pch
  lyr.panmode = 4                    # Fixed (floatPan)
  lyr.floatPan = fpan                # -1 .. +1 straight into panBlend
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
  return bank, prog       # keep the bank alive alongside the program


def _role_render(wav_path, fpan):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0
      self.pumped = 0
      self.total_frames = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioPanLawTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          wav_output_path=wav_path,
          freerun=True,
      )

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      if self.phase == 0:
        syn.masterGain = 1.0
        self.bank, self.prog = _build_sine_program(S, fpan)
        syn.programbus.uiprogram = self.prog
        self.voice = syn.keyOn(60, 127, self.prog, None)
        self.phase = 1
        return
      if self.pumped < N_ITERS:
        self.total_frames = dev.advanceTime(DT)
        self.pumped += 1
      if self.pumped >= N_ITERS:
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_TOTAL_FRAMES=%d" % app.total_frames, flush=True)
  sys.exit(0 if (app.pumped == N_ITERS and app.total_frames > 0) else 3)


################################################################################
# DRIVER helpers — hand-rolled RIFF/WAVE parse (python `wave` can't read float).
################################################################################

def _wav_channel_rms(path):
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
  channels = struct.unpack("<HHIIHH", fmt[:16])[1]
  if channels != 2:
    raise ValueError("expected 2ch, got %d" % channels)
  fl = np.frombuffer(audio, dtype="<f4")
  L = fl[0::2]
  R = fl[1::2]
  return float(np.sqrt(np.mean(L * L))), float(np.sqrt(np.mean(R * R)))


def _spawn_render(wav_path, fpan, timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path, "--pan", repr(fpan)]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


################################################################################
# DRIVER — one render per pan position, then classify against the law.
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="panlaw_")
  measured = {}      # fpan -> (rmsL, rmsR)
  try:
    for fpan in PANS:
      wav = os.path.join(tmp, "pan%+.1f.wav" % fpan)
      try:
        rc, out = _spawn_render(wav, fpan)
      except subprocess.TimeoutExpired as e:
        print((e.stdout or "") + (e.stderr or ""))
        shutil.rmtree(tmp, ignore_errors=True)
        sys.exit(verdict(False, "child render timed out at pan=%+.1f (possible wedge)" % fpan))
      if rc != 0 or not os.path.isfile(wav):
        print(out)
        shutil.rmtree(tmp, ignore_errors=True)
        sys.exit(verdict(False, "child render failed at pan=%+.1f rc=%d exists=%s"
                                % (fpan, rc, os.path.isfile(wav))))
      measured[fpan] = _wav_channel_rms(wav)
  finally:
    shutil.rmtree(tmp, ignore_errors=True)

  # (a) source reference from the hard-pan endpoints, where the law's hot side is
  #     exactly 1. Both endpoints are independent engine boots, so their
  #     agreement is also the render's cross-boot determinism check.
  src_l = measured[-1.0][0]      # full left  -> L carries everything
  src_r = measured[+1.0][1]      # full right -> R carries everything
  src = 0.5 * (src_l + src_r)
  src_ok = (src > SRC_FLOOR and abs(src_l - src_r) <= TOL * src)

  ends_ok = True
  analytic_ok = True
  sumsq_ok = True
  center_ok = True
  rows = []
  for fpan in PANS:
    rl, rr = measured[fpan]
    ml = rl / src if src > 0 else 0.0
    mr = rr / src if src > 0 else 0.0
    al, ar = _analytic(fpan)
    ss = ml * ml + mr * mr
    rows.append("%+.1f:meas=%.4f/%.4f ref=%.4f/%.4f sumsq=%.4f" % (fpan, ml, mr, al, ar, ss))
    # (c) matches the shipped quarter-sine law
    if abs(ml - al) > TOL or abs(mr - ar) > TOL:
      analytic_ok = False
    # (d) constant power — the invariant a linear crossfade violates
    if abs(ss - 1.0) > TOL:
      sumsq_ok = False
    # (b) endpoints: muted side EXACTLY zero, hot side full scale
    if abs(fpan) == 1.0:
      hot, muted = (mr, ml) if fpan > 0 else (ml, mr)
      if abs(hot - 1.0) > TOL or muted > SILENT_EPS:
        ends_ok = False
    # (e) center is the equal-power center, not the linear one
    if fpan == 0.0:
      c = math.sqrt(0.5)
      if abs(ml - c) > TOL or abs(mr - c) > TOL:
        center_ok = False

  passed = (src_ok and ends_ok and analytic_ok and sumsq_ok and center_ok)
  detail = ("src=%s(%.4f L=%.4f R=%.4f) endpoints=%s analytic=%s sumsq_unity=%s center=%s | %s"
            % (src_ok, src, src_l, src_r, ends_ok, analytic_ok, sumsq_ok, center_ok,
               " ".join(rows)))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  ap.add_argument("--pan", type=float, default=0.0)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.wav, args.pan)
  else:
    _main_driver()
