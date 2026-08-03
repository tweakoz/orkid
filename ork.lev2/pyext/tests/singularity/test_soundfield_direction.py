#!/usr/bin/env ork.python
################################################################################
# test_soundfield_direction — the SoundField encode/decode direction canary (SF1).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   A 4-channel AmbiX (FOA / ACN / SN3D) probe streamed into the engine's one
#   B-format mix point comes out of the Gerzon dual-band stereo decoder pointing
#   where it was authored to point. Before this test the whole encode ->
#   accumulate -> decode -> "soundfield" bus chain had no observable at all.
#
#   The fixtures are synthesized HERE, so the test depends on no asset: for a
#   plane wave of amplitude s at azimuth az (elevation 0), SN3D says
#       W = s/sqrt(2)   Y = s*sin(az)   Z = 0   X = s*cos(az)
#   written in ACN order (0=W 1=Y 2=Z 3=X) as a 4ch float32 48kHz WAV whose
#   length is a whole number of tone periods (a seamless loop).
#
#   AMBISONIC AZIMUTH IS LEFT-POSITIVE: +Y is the listener's LEFT, so az=+90
#   must decode left-dominant. The decoder is a coincident cardioid pair aimed
#   at +/-45deg (soundfield.h), giving
#       L/R = (1 + cos(az-45)) / (1 + cos(az+45))
#   for the low band. Ideal (full directivity) ratios:
#       az  -90 -> 0.1716    -45 -> 0.500     0 -> 1.000
#       az  +45 -> 2.000     +90 -> 5.828
#   The 100Hz probe tone sits below the 700Hz Gerzon shelf but not infinitely
#   below it, so the measured ratios land ~2% inside the ideal ones (5.713 /
#   0.175 / 1.995 / 0.501 / 1.000 as built); RATIO_TOL covers that.
#
#   Per check:
#     (a) each azimuth's decoded L/R RMS ratio matches the ideal within tol;
#     (b) the ratios are strictly ORDERED left-to-right across the five angles
#         (a decoder that dropped Y entirely would still pass a loose (a));
#     (c) +90 and -90 are reciprocal — the decode is laterally symmetric;
#     (d) the field streamed clean: zero ring underruns, and the render is
#         non-silent.
#
# HEADLESS (consent law): audio-only subsystems (no gpu), ORKID_AUDIO_IOCLASS
# pinned to STREAM in BOTH the driver spawn env and the child BEFORE the engine
# import (genviron snapshots `environ` at library load), so no host audio device
# is ever opened and nothing is audible.
#
# SHAPE (mirrors test_audio_stream_chunk_bounds.py): the driver (no --role) is
# pure stdlib + numpy and spawns each engine-booting render as its own
# ork.python subprocess with ORKID_DRM_MODE stripped.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
import struct
import argparse
# NOTE: subprocess / tempfile / numpy are imported lazily in the driver-only
# paths — the child must not pre-load hashlib/ssl-adjacent stdlib before
# orkengine.core (see the note in test_audio_wav_render.py).

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT          = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
# the weight slew is a ~250ms exponential; the pre-roll runs it to within 0.03%
# of unity BEFORE any audio time elapses, so the measured window sees a probe at
# a constant, known weight rather than a ramp.
PREROLL     = 200           # update() calls in phase 0 (no audio generated)
SKIP_PUMPS  = 20            # pumps discarded from the analysis window
N_ITERS     = 160           # total pumps per render (1.6s of audio)
TONE_HZ     = 100.0         # below the 700Hz Gerzon shelf
FIXTURE_S   = 1.0           # 100 whole periods -> a seamless loop
RATIO_TOL   = 0.10          # relative, covers the shelf's ~2% pull
RMS_FLOOR   = 0.02

# azimuth (deg) -> ideal low-band L/R ratio (1+cos(az-45))/(1+cos(az+45))
CASES = (
    (-90.0, 0.171573),
    (-45.0, 0.500000),
    (0.0, 1.000000),
    (45.0, 2.000000),
    (90.0, 5.828427),
)


################################################################################
# fixture synthesis — a 4ch float32 AmbiX WAV, written by hand (python's `wave`
# module cannot write float) so the test needs no soundfile/scipy dependency.
################################################################################

def write_ambix(path, az_deg, freq=TONE_HZ, seconds=FIXTURE_S, sr=48000, amp=0.5):
  n  = int(round(seconds * sr))
  az = math.radians(az_deg)
  W  = 1.0 / math.sqrt(2.0)
  X  = math.cos(az)
  Y  = math.sin(az)
  Z  = 0.0
  body = bytearray()
  for i in range(n):
    s = amp * math.sin(2.0 * math.pi * freq * i / sr)
    body += struct.pack("<4f", s * W, s * Y, s * Z, s * X)   # ACN order
  data        = bytes(body)
  ch, bits    = 4, 32
  block_align = ch * bits // 8
  fmt = struct.pack("<HHIIHH", 3, ch, sr, sr * block_align, block_align, bits)
  riff = (b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt
          + b"data" + struct.pack("<I", len(data)) + data)
  with open(path, "wb") as f:
    f.write(b"RIFF" + struct.pack("<I", len(riff)) + riff)


################################################################################
# CHILD — one engine boot: audio-only headless, one probe at full weight with an
# identity listener, pump advanceTime teeing to --wav, then exit.
################################################################################

def _role_render(wav_path, fixture_path, az_deg):
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import.
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  write_ambix(fixture_path, az_deg)

  import orkengine.core as core                # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase     = 0
      self.pumped    = 0
      self.weight    = -1.0
      self.slot      = -2
      self.underruns = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="SoundFieldDirectionTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                     # -> StrAudioDevice SYNC
          wav_output_path=wav_path,
          freerun=True)

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      if self.phase == 0:
        syn.masterGain = 1.0
        # identity listener: front is world -Z, right is world +X.
        syn.listener_matrix = core.mtx4()
        self.field = S.SoundField.instance()
        self.probe = self.field.createProbe(fixture_path, True)
        # listener sits AT the probe, well inside refDistance -> weight 1.
        self.field.setProbeParams(self.probe, core.vec3(0, 0, 0),
                                  refDistance=10.0, maxDistance=50.0,
                                  rolloff=1.0, gain=1.0)
        # no audio time elapses here: settle the weight slew before pumping.
        for _ in range(PREROLL):
          self.field.update(DT)
        self.weight = self.field.probeWeight(self.probe)
        self.slot   = self.field.probeSlot(self.probe)
        self.phase  = 1
        return
      self.field.update(DT)
      dev.advanceTime(DT)
      self.pumped += 1
      if self.pumped >= N_ITERS:
        self.underruns = self.field.underrunCount
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_WEIGHT=%.6f" % app.weight, flush=True)
  print("CHILD_SLOT=%d" % app.slot, flush=True)
  print("CHILD_UNDERRUNS=%d" % app.underruns, flush=True)
  sys.exit(0)


################################################################################
# DRIVER helpers
################################################################################

def _parse_wav(path):
  import numpy as np
  with open(path, "rb") as f:
    data = f.read()
  if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
    raise ValueError("not a RIFF/WAVE file")
  fmt = audio = None
  pos = 12
  while pos + 8 <= len(data):
    cid = data[pos:pos + 4]
    csz = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    body = data[pos + 8:pos + 8 + csz]
    if cid == b"fmt ":
      fmt = body
    elif cid == b"data":
      audio = body
    pos += 8 + csz + (csz & 1)
  if fmt is None or audio is None:
    raise ValueError("missing fmt/data chunk")
  _tag, channels, sr, _br, block_align, _bits = struct.unpack("<HHIIHH", fmt[:16])
  floats = np.frombuffer(audio, dtype="<f4")
  return dict(channels=channels, sr=sr,
              frames=(len(audio) // block_align if block_align else 0),
              floats=floats)


def _spawn_render(wav_path, fixture_path, az_deg, timeout=240):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"       # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path,
         "--fixture", fixture_path, "--az", repr(az_deg)]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


################################################################################
# DRIVER
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="sfdirection_")
  checks = []
  measured = {}
  detail_bits = []

  def check(name, ok):
    checks.append((name, bool(ok)))

  try:
    for az, ideal in CASES:
      tag = "%+03d" % int(az)
      wav = os.path.join(tmp, "render%s.wav" % tag)
      fix = os.path.join(tmp, "fixture%s.wav" % tag)
      try:
        rc, out = _spawn_render(wav, fix, az)
      except subprocess.TimeoutExpired as e:
        print((e.stdout or "") + (e.stderr or ""))
        sys.exit(verdict(False, "child render timed out at az=%s (possible wedge)" % az))
      print(out)

      if rc != 0 or not os.path.isfile(wav):
        check("render%s" % tag, False)
        detail_bits.append("az%s=RENDERFAIL(rc=%d)" % (tag, rc))
        continue

      underruns = _grep(out, "CHILD_UNDERRUNS")
      weight    = _grep(out, "CHILD_WEIGHT")
      check("stream_clean%s" % tag, underruns == "0")
      check("weight_full%s" % tag, weight is not None and float(weight) > 0.98)

      wa = _parse_wav(wav)
      fl = wa["floats"]
      L  = fl[0::2]
      R  = fl[1::2]
      k  = SKIP_PUMPS * int(round(DT * wa["sr"]))   # drop the slot's fade-in ramp
      rmsL = float(np.sqrt(np.mean(L[k:] * L[k:])))
      rmsR = float(np.sqrt(np.mean(R[k:] * R[k:])))
      check("nonsilent%s" % tag, max(rmsL, rmsR) > RMS_FLOOR)
      ratio = (rmsL / rmsR) if rmsR > 1e-9 else float("inf")
      measured[az] = ratio
      # (a) the ratio itself
      rel = abs(ratio - ideal) / ideal
      check("ratio%s" % tag, rel <= RATIO_TOL)
      detail_bits.append("az%s: L=%.5f R=%.5f ratio=%.4f ideal=%.4f rel=%.3f"
                         % (tag, rmsL, rmsR, ratio, ideal, rel))

    # (b) strict left-to-right ordering across the five angles
    order_ok = all(az in measured for az, _ in CASES)
    if order_ok:
      seq = [measured[az] for az, _ in CASES]
      order_ok = all(seq[i] < seq[i + 1] for i in range(len(seq) - 1))
    check("strict_ordering", order_ok)

    # (c) lateral symmetry: the +/-90 ratios are reciprocal
    sym_ok = (-90.0 in measured and 90.0 in measured
              and abs(measured[-90.0] * measured[90.0] - 1.0) < 0.05)
    check("lateral_symmetry", sym_ok)

    failed = [name for name, ok in checks if not ok]
    detail = ("checks=%d failed=%s | %s"
              % (len(checks), ",".join(failed) if failed else "none",
                 " | ".join(detail_bits)))
    sys.exit(verdict(len(failed) == 0, detail))
  finally:
    shutil.rmtree(tmp, ignore_errors=True)


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  ap.add_argument("--fixture", default=None)
  ap.add_argument("--az", type=float, default=0.0)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.wav, args.fixture, args.az)
  else:
    _main_driver()
