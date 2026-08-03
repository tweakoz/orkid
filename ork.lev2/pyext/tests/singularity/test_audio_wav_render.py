#!/usr/bin/env ork.python
################################################################################
# test_audio_wav_render — deterministic offline audio render vehicle (slice T1).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   The StrAudioDevice SYNC_NONREALTIME path can tee its generated audio to a WAV
#   file. A headless AUDIO-ONLY EzApp (use_subsystems=['opq','core','audioO'], no
#   gpu subsystem so no graphics context is ever created — the sequencer.py idiom)
#   builds a deterministic pure-oscillator patch in Python, keyOns it, and pumps
#   advanceTime() a FIXED number of times. Every generated sample is captured to
#   the WAV at generation time, independent of any consumer drain.
#
#   The driver asserts, on the emitted file:
#     (a) the WAV exists;
#     (b) header == 2-channel IEEE-float WAV at the engine sample rate, with a
#         frame count EXACTLY equal to the deterministic pump total (+/- 0);
#     (c) non-silence: per-channel RMS above a floor;
#     (d) determinism: two independent child renders produce BYTE-IDENTICAL files.
#
# SHAPE (mirrors test_audio_device_fallback.py): the driver (no --role) is pure
# stdlib + numpy and spawns each engine-booting render as its OWN ork.python
# subprocess, with ORKID_DRM_MODE stripped from the child env so a render can
# never depend on / touch inherited display state. It parses the RIFF/WAVE header
# by hand (python's wave module cannot read float WAV) and emits the machine
# verdict line.
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


################################################################################
# CHILD — one engine boot: audio-only headless, build+keyOn a deterministic sine
# patch, pump advanceTime N_ITERS times teeing to --wav, then exit cleanly.
################################################################################

def _build_sine_program(S):
  # Simplest fully-deterministic voice with NO sample/asset dependency: a PolyBLEP
  # sine oscillator through an amp envelope. Mirrors ork.singularity.sampler's
  # createLayer topology but swaps the Sampler block for OscilSine (no keymap).
  bank = S.BankData()
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
  # keep the bank alive alongside the program
  return bank, prog


def _role_render(wav_path):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0            # 0=await synth, 1=pumping
      self.pumped = 0
      self.total_frames = 0
      self.sr = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioWavRenderTest",
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
        # build + key the deterministic patch; no audio time elapses here.
        syn.masterGain = 1.0
        self.bank, self.prog = _build_sine_program(S)
        syn.programbus.uiprogram = self.prog
        self.voice = syn.keyOn(60, 127, self.prog, None)
        self.phase = 1
        return
      # phase 1: deterministic fixed-step pump; advanceTime returns cumulative frames.
      if self.pumped < N_ITERS:
        self.total_frames = dev.advanceTime(DT)
        self.pumped += 1
      if self.pumped >= N_ITERS:
        self.sr = dev.extractSamples(0).sample_rate   # SR w/o consuming/affecting WAV
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_TOTAL_FRAMES=%d" % app.total_frames, flush=True)
  print("CHILD_SR=%d" % app.sr, flush=True)
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
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


def _spawn_render(wav_path, timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path]
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


################################################################################
# DRIVER — spawns two independent renders and classifies.
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="wavrender_")
  wavA = os.path.join(tmp, "renderA.wav")
  wavB = os.path.join(tmp, "renderB.wav")

  try:
    rcA, outA = _spawn_render(wavA)
    rcB, outB = _spawn_render(wavB)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child render timed out (possible wedge)"))

  print(outA)
  print(outB)

  child_frames = _grep(outA, "CHILD_TOTAL_FRAMES")
  child_sr     = _grep(outA, "CHILD_SR")
  child_frames = int(child_frames) if child_frames is not None else -1
  child_sr     = int(child_sr) if child_sr is not None else -1

  # (a) both files exist
  existsA = os.path.isfile(wavA)
  existsB = os.path.isfile(wavB)

  hdr_ok = frames_ok = rms_ok = det_ok = False
  detail_extra = ""
  rmsL = rmsR = -1.0
  wa = None
  if existsA and existsB and rcA == 0 and rcB == 0:
    wa = _parse_wav(wavA)
    # (b) header: 2ch IEEE-float at the engine SR, exact deterministic frame count
    expected = N_ITERS * int(round(DT * wa["sr"])) if wa["sr"] else -1
    hdr_ok = (wa["channels"] == 2 and wa["is_float"] and wa["sr"] == child_sr and wa["sr"] > 0)
    frames_ok = (wa["frames"] == expected and wa["frames"] == child_frames and expected > 0)
    # (c) non-silence per channel
    fl = wa["floats"]
    if fl.size >= 2:
      L = fl[0::2]; R = fl[1::2]
      rmsL = float(np.sqrt(np.mean(L * L)))
      rmsR = float(np.sqrt(np.mean(R * R)))
      rms_ok = (rmsL > RMS_FLOOR and rmsR > RMS_FLOOR)   # both channels: verifies L/R interleave
    # (d) determinism: byte-identical
    det_ok = (_sha256(wavA) == _sha256(wavB))
    detail_extra = ("ch=%d float=%s sr=%d frames=%d expected=%d rmsL=%.4f rmsR=%.4f"
                    % (wa["channels"], wa["is_float"], wa["sr"], wa["frames"],
                       expected, rmsL, rmsR))

  passed = (existsA and existsB and rcA == 0 and rcB == 0
            and hdr_ok and frames_ok and rms_ok and det_ok)
  detail = ("rcA=%d rcB=%d exists=%s/%s hdr=%s frames=%s rms=%s determinism=%s %s"
            % (rcA, rcB, existsA, existsB, hdr_ok, frames_ok, rms_ok, det_ok, detail_extra))
  shutil.rmtree(tmp, ignore_errors=True)
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.wav)
  else:
    _main_driver()
