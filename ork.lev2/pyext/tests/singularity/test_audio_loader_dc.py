#!/usr/bin/env ork.python
################################################################################
# test_audio_loader_dc — the SampleData loaders must not INVENT a DC offset.
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display, NO synth boot):
#   SampleData's normalize step (sampler.cpp debias_and_normalize, shared by
#   loadFromAudioFile and loadFromFloatWaveformBuffer) removes the TRUE MEAN and
#   then peak-normalizes. It used to subtract the PEAK MIDPOINT (_max+_min)*0.5,
#   which equals the mean only for symmetric content — so on anything natural it
#   FROZE a DC offset of (mean - midpoint)/halfrange into the int16 block the
#   sampler plays (measured to -0.125 full scale on repo assets), which the amp
#   and panner gains then scaled into an audible one-sided LF pedestal.
#
#   The probe waveform is synthesized here, sin(x) + 0.5*cos(2x) over whole
#   periods: mean exactly 0, but the even harmonic makes the peaks strongly
#   ASYMMETRIC (max ~ +0.35, min ~ -0.94 after scaling). That asymmetry is the
#   whole point — the old code injects ~ +0.46 FS of DC into THIS signal, i.e.
#   the teeth are 2.5 orders of magnitude outside the tolerance below.
#
#   Three cases, all three loader entry points that normalize:
#     C1 audiofile      S.SampleData(audiofile=...)  -> loadFromAudioFile,
#                       normalize defaulted true (pyext_aud_singul_datas.cpp)
#     C2 float buffer   loadFromFloatWaveformBuffer(..., normalize=True) — the
#                       twin that carried the identical bug
#     C3 zero guard     a CONSTANT buffer (peak-to-peak 0). Old code divided by
#                       zero here and cast inf to int16; the guard must yield
#                       silence instead.
#
#   Asserted per case: |dc| <= 1e-3 FS, peak == 1.0 within 1e-3 (normalization
#   actually happened), and no int16 wrap — a wrapped sample reads as the
#   opposite sign at near-full scale, so wrap is caught by requiring the block's
#   extremes to sit on the sides the input's extremes were on.
#
# SHAPE (mirrors test_hypersound_arith.py): the driver (no --role) is pure
# stdlib + numpy, synthesizes the probe, writes the float32 WAV by hand, and
# spawns the loading work as its OWN ork.python subprocess with ORKID_DRM_MODE
# stripped. The child prints one machine-parsable stats line per case.
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

SR = 48000
CYCLES = 100                  # whole periods -> the probe's mean is exactly 0
N = 48000                     # 1 s; N/CYCLES = 480 samples per period, integral
DC_TOL = 1.0e-3               # ork.vet.audio.py's dc_offset ceiling, FS units
PEAK_TOL = 1.0e-3             # peak-normalized means max|x| == 1
S16_MAX = 32767.0
CONST_LEVEL = 0.5             # C3's constant buffer value


################################################################################
# PROBE — synthesized identically in driver and child (driver writes the WAV,
# child re-derives the same array for the float-buffer case).
################################################################################

def _probe():
  import numpy as np
  x = 2.0 * math.pi * CYCLES * np.arange(N, dtype=np.float64) / float(N)
  sig = np.sin(x) + 0.5 * np.cos(2.0 * x)
  return (sig / 1.6).astype(np.float32)      # keep it inside +-1 for the WAV


################################################################################
# CHILD — loads the probe through each entry point and reports block stats.
################################################################################

def _stats_line(tag, block):
  import numpy as np
  f = block.astype(np.float64) / S16_MAX
  print("CASE_%s n=%d dc=%.9f peak=%.9f min=%.9f max=%.9f"
        % (tag, f.size, float(np.mean(f)), float(np.max(np.abs(f))),
           float(np.min(f)), float(np.max(f))), flush=True)


def _role_load(wav_path):
  import numpy as np
  import orkengine.core                        # core FIRST (import-order law)
  from orkengine.lev2 import singularity as S

  # C1 — loadFromAudioFile, normalize defaulted true.
  smp1 = S.SampleData(name="probe_file", originalPitch=440.0, rootKey=48,
                      audiofile=wav_path)
  _stats_line("audiofile", smp1.sample_block)

  # C2 — the float-buffer twin, normalize explicitly true.
  smp2 = S.SampleData(name="probe_buffer")
  smp2.loadFromFloatWaveformBuffer(_probe(), float(SR), 1, 440.0, True)
  _stats_line("floatbuf", smp2.sample_block)

  # C3 — zero guard: constant buffer, zero peak-to-peak.
  smp3 = S.SampleData(name="probe_const")
  flat = np.full(1024, CONST_LEVEL, dtype=np.float32)
  smp3.loadFromFloatWaveformBuffer(flat, float(SR), 1, 440.0, True)
  _stats_line("constant", smp3.sample_block)

  sys.exit(0)


################################################################################
# DRIVER
################################################################################

def _write_float_wav(path, samples):
  """mono 32-bit float RIFF/WAVE — libsndfile reads it back bit-exact, so the
  probe's mean survives into the loader unquantized."""
  import struct
  raw = samples.astype("<f4").tobytes()
  fmt = struct.pack("<HHIIHH", 3, 1, SR, SR * 4, 4, 32)
  with open(path, "wb") as f:
    f.write(b"RIFF" + struct.pack("<I", 4 + 8 + len(fmt) + 8 + len(raw)) + b"WAVE")
    f.write(b"fmt " + struct.pack("<I", len(fmt)) + fmt)
    f.write(b"data" + struct.pack("<I", len(raw)) + raw)


def _spawn_load(wav_path, timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"         # no device, ever
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "load", "--wav", wav_path]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _parse_cases(out):
  cases = {}
  for line in out.splitlines():
    if not line.startswith("CASE_"):
      continue
    head, rest = line.split(" ", 1)
    tag = head[len("CASE_"):]
    kv = {}
    for tok in rest.split():
      k, v = tok.split("=", 1)
      kv[k] = float(v)
    cases[tag] = kv
  return cases


def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  probe = _probe()
  p_mean = float(np.mean(probe.astype(np.float64)))
  p_min = float(np.min(probe))
  p_max = float(np.max(probe))
  midpoint = 0.5 * (p_max + p_min)
  halfrange = 0.5 * (p_max - p_min)
  # what the OLD (peak-midpoint) bias would have injected — the teeth's scale.
  old_dc = (p_mean - midpoint) / halfrange

  print("PROBE mean=%+0.9f min=%+0.6f max=%+0.6f midpoint=%+0.6f "
        "old_law_injected_dc=%+0.6f" % (p_mean, p_min, p_max, midpoint, old_dc))

  tmp = tempfile.mkdtemp(prefix="loader_dc_")
  wav = os.path.join(tmp, "probe.wav")
  try:
    _write_float_wav(wav, probe)
    rc, out = _spawn_load(wav)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    shutil.rmtree(tmp, ignore_errors=True)
    sys.exit(verdict(False, "child loader run timed out (possible wedge)"))
  finally:
    shutil.rmtree(tmp, ignore_errors=True)

  print("---- child (rc=%d) ----" % rc)
  print(out)

  cases = _parse_cases(out)
  checks = []

  # A0 — the probe itself is DC-free and asymmetric enough to have teeth.
  a0 = abs(p_mean) < 1.0e-6 and abs(old_dc) > 0.1
  checks.append(("A0_probe dc=%+0.2e old_law_dc=%+0.3f (|dc|<1e-6, teeth>0.1)"
                 % (p_mean, old_dc), a0))

  for tag, expect_n in (("audiofile", N), ("floatbuf", N)):
    c = cases.get(tag)
    if c is None:
      checks.append(("%s MISSING from child output" % tag, False))
      continue
    dc_ok = abs(c["dc"]) <= DC_TOL
    pk_ok = abs(c["peak"] - 1.0) <= PEAK_TOL
    # no wrap: the block's extremes stay on the input's sides. A wrapped
    # positive peak would surface as a near-full-scale NEGATIVE sample.
    wrap_ok = c["max"] > 0.0 and c["min"] < 0.0 and c["peak"] <= 1.0 + PEAK_TOL
    n_ok = int(c["n"]) == expect_n
    checks.append(("%s n=%d dc=%+0.6f peak=%.6f min=%+0.4f max=%+0.4f "
                   "(|dc|<=%.4f, peak==1, no wrap)"
                   % (tag, int(c["n"]), c["dc"], c["peak"], c["min"], c["max"],
                      DC_TOL),
                   dc_ok and pk_ok and wrap_ok and n_ok))

  c3 = cases.get("constant")
  if c3 is None:
    checks.append(("constant MISSING from child output", False))
  else:
    # zero guard: constant in -> silence out, not inf/NaN garbage.
    g = c3["peak"] == 0.0 and c3["dc"] == 0.0
    checks.append(("constant dc=%+0.6f peak=%.6f (zero guard -> silence)"
                   % (c3["dc"], c3["peak"]), g))

  print("")
  for desc, ok in checks:
    print("  [%s] %s" % ("PASS" if ok else "FAIL", desc))

  passed = rc == 0 and all(ok for _, ok in checks)
  detail = ("rc=%d checks=%d/%d" %
            (rc, sum(1 for _, ok in checks if ok), len(checks)))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  args = ap.parse_args()

  if args.role == "load":
    _role_load(args.wav)
  else:
    _main_driver()
