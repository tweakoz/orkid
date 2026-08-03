#!/usr/bin/env ork.python
###############################################################################
# ork.spatialaudio.bakewavs.py — deterministic procedural-foley bake for the
# spatial-audio showcase scene (scn_spatial_audio_showcase).
#
# TECHNIQUE: "procedural foley baking" (the SoundSeed-Air / classic-synthesis
# idiom): every sampled asset the scene plays is SYNTHESIZED HERE, offline,
# from seeded DSP recipes — so the corpus is reproducible from this script
# alone and each asset owns a DELIBERATE spectral band. The bands are the
# measurement contract the harness (ork.spatialaudio.harness.py) gates on:
#
#   band  asset                     recipe                          core Hz
#   ----  ------------------------  ------------------------------  --------
#   S     water_grain_[abc].wav     modulated bandpass-noise        500-1500
#         stream_loop.wav           burble (water = modulated
#                                   noise, the Farnell recipe)
#   B     bird_[abc].wav            FM chirp syllables              2500-5000
#   C     chime.wav                 inharmonic-partial strike       1600-2300
#         (runtime-spawned)         (additive bell recipe)
#   K     clank_[ab].wav            damped modal metal impact       700-1400
#   M     hum_loop.wav              mains-hum additive loop         100-260
#   -     traffic_wash.wav          brown-noise swell (flavor bed,  80-600
#                                   no gate)
#   P     treeline_ambix.wav        4ch FOA AmbiX shimmer bed      6500-7500
#         (SoundFieldProbe)         (see bake_ambix_bed: NOT
#                                   octave-compensated, NOT mono)
#
# (wind is NOT baked — wind is the runtime-SYNTHESIZED source, a hypersound
#  filtered-noise patch built live in the scene module; band W < 320 Hz.)
#
# All output: 48 kHz mono 16-bit PCM WAV under <ork.data>/sounds/showcase/.
# Deterministic: fixed numpy seed; re-running overwrites byte-identically.
#
#   ork.spatialaudio.bakewavs.py            # bake into ork.data/sounds/showcase
#   ork.spatialaudio.bakewavs.py -o /tmp/x  # bake elsewhere (inspection)
###############################################################################

import argparse
import os
import struct
import sys

import numpy as np

SR = 48000
SEED = 0xA0D10  # "AUDIO" — every asset derives its own stream from this


###############################################################################
# WAV writer — plain 16-bit PCM, no deps beyond stdlib.
###############################################################################

###############################################################################
# OCTAVE COMPENSATION (measured, spectrum-verified): the ECS sound emitters
# preload SampleData with _rootKey=60 but _originalPitch=C3, so a note-60
# trigger plays every wav at exactly 2x rate (+1 octave). The per-component
# pitchOffsetCents=-1200 counter is applied RACILY (deferred per-voice pitch —
# some voices ring at 2x anyway), so instead every asset is FFT-resampled one
# octave DOWN at bake time: played back at the engine's 2x, it lands exactly
# on the designed spectrum and duration. Remove when the sampler roots wavs
# at their true rate (then also drop the resample).
###############################################################################

def _octave_down(x):
  n = len(x)
  X = np.fft.rfft(x)
  X2 = np.zeros(n + 1, dtype=complex)     # rfft size for 2n samples
  X2[: len(X)] = X
  return np.fft.irfft(X2, 2 * n) * 2.0    # x2 length, amplitude preserved


def write_wav16(path, data, sr=SR):
  data = _octave_down(data)
  data = np.clip(data, -1.0, 1.0)
  pcm = (data * 32767.0).astype("<i2").tobytes()
  hdr = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE"
  hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, sr, sr * 2, 2, 16)
  hdr += b"data" + struct.pack("<I", len(pcm))
  with open(path, "wb") as f:
    f.write(hdr + pcm)
  peak = float(np.abs(data).max()) if data.size else 0.0
  rms = float(np.sqrt(np.mean(data * data))) if data.size else 0.0
  print("  %-18s %6.2fs  peak %.3f  rms %.3f" %
        (os.path.basename(path), len(data) / sr, peak, rms))


def _rng(tag):
  return np.random.default_rng(SEED + hash(tag) % (1 << 16))


def _norm(x, peak=0.7):
  m = np.abs(x).max()
  return x * (peak / m) if m > 0 else x


def _bandpass_noise(rng, n, lo, hi):
  """FFT-brickwall bandpass white noise — deterministic, exact band."""
  x = rng.standard_normal(n)
  X = np.fft.rfft(x)
  f = np.fft.rfftfreq(n, 1.0 / SR)
  X[(f < lo) | (f > hi)] = 0.0
  return np.fft.irfft(X, n)


###############################################################################
# S — water grains + seamless stream loop (Farnell water: bandpass noise with
# multi-rate amplitude "burble" modulation + droplet chirplets on top).
###############################################################################

def _water_bed(rng, n, band=(500.0, 1500.0)):
  bed = _bandpass_noise(rng, n, band[0], band[1])
  t = np.arange(n) / SR
  # burble: product of slow incommensurate LFOs -> non-repeating gurgle
  m = (1.0
       + 0.45 * np.sin(2 * np.pi * 2.9 * t + rng.uniform(0, 6.28))
       + 0.30 * np.sin(2 * np.pi * 6.7 * t + rng.uniform(0, 6.28))
       + 0.20 * np.sin(2 * np.pi * 11.3 * t + rng.uniform(0, 6.28)))
  return bed * (0.55 + 0.45 * np.clip(m / 1.95, -1, 1))


def _droplets(rng, n, count, f_lo=600.0, f_hi=800.0):
  # droplet chirplets stay INSIDE band S: sweep tops out at f_hi*1.4 = 1120 Hz
  # (a wider earlier sweep reached 2.5 kHz and bled into the chime + bird
  # measurement bands — the G5 false-floor found by the harness round 1).
  out = np.zeros(n)
  for _ in range(count):
    at = rng.integers(0, n - 2400)
    dur = int(rng.uniform(0.02, 0.05) * SR)
    t = np.arange(dur) / SR
    f0 = rng.uniform(f_lo, f_hi)
    sweep = f0 * (1.0 + 0.4 * t / t[-1])          # rising "bloip", <= 1120 Hz
    ph = 2 * np.pi * np.cumsum(sweep) / SR
    env = np.exp(-t * 60.0)
    out[at:at + dur] += np.sin(ph) * env * rng.uniform(0.2, 0.5)
  return out


def bake_water_grain(tag, dur):
  rng = _rng("water" + tag)
  n = int(dur * SR)
  x = _water_bed(rng, n) + _droplets(rng, n, int(dur * 6))
  # grain envelope: raised-cosine edges (0.15s) — StochWav crossfades on top
  edge = int(0.15 * SR)
  env = np.ones(n)
  env[:edge] = 0.5 - 0.5 * np.cos(np.pi * np.arange(edge) / edge)
  env[-edge:] = env[:edge][::-1]
  return _norm(x * env, 0.6)


def bake_stream_loop(dur=6.0):
  rng = _rng("streamloop")
  n = int(dur * SR)
  x = _water_bed(rng, n) + _droplets(rng, n, int(dur * 5))
  # make seamless: crossfade the tail into the head (loop lap 0.25s)
  lap = int(0.25 * SR)
  w = np.linspace(0.0, 1.0, lap)
  x[:lap] = x[:lap] * w + x[-lap:] * (1.0 - w)
  return _norm(x[: n - lap], 0.6)


###############################################################################
# B — bird chirps: 2-4 FM syllables, band 2500-5000 Hz.
###############################################################################

def bake_bird(tag):
  rng = _rng("bird" + tag)
  syllables = rng.integers(2, 5)
  parts = []
  for s in range(syllables):
    dur = rng.uniform(0.09, 0.22)
    n = int(dur * SR)
    t = np.arange(n) / SR
    f0 = rng.uniform(2800.0, 4200.0)
    # chirp contour: down-up warble, keeps 2.5-5k
    f = f0 * (1.0 + 0.18 * np.sin(2 * np.pi * rng.uniform(8, 22) * t)
              + 0.10 * (t / dur) * rng.choice([-1.0, 1.0]))
    ph = 2 * np.pi * np.cumsum(f) / SR
    env = np.sin(np.pi * t / dur) ** 1.5
    parts.append(np.sin(ph) * env)
    gap = int(rng.uniform(0.04, 0.12) * SR)
    parts.append(np.zeros(gap))
  return _norm(np.concatenate(parts), 0.65)


###############################################################################
# C — chime: additive inharmonic-partial strike, band 1600-2300 Hz.
###############################################################################

def bake_chime(dur=2.5):
  rng = _rng("chime")
  n = int(dur * SR)
  t = np.arange(n) / SR
  x = np.zeros(n)
  for f, a, d in ((1710.0, 1.00, 1.1), (1940.0, 0.65, 0.9),
                  (2130.0, 0.45, 0.7), (2255.0, 0.30, 0.55)):
    x += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) * np.exp(-t / d)
  # strike transient (short in-band noise burst)
  x[: int(0.012 * SR)] += _bandpass_noise(rng, int(0.012 * SR), 1600, 2300) * 0.8
  return _norm(x, 0.7)


###############################################################################
# K — clank: damped modal metal impact, modes 700-1400 Hz.
###############################################################################

def bake_clank(tag):
  # modal cluster CONCENTRATED in the measurement band (1150-1450) with
  # longer ring — a struck-pipe "tink" that meters cleanly above the water
  # floor (round-4 G4 fix: the old 700-1400 spread put 2/3 of the energy
  # outside the gate band).
  rng = _rng("clank" + tag)
  dur = rng.uniform(0.45, 0.6)
  n = int(dur * SR)
  t = np.arange(n) / SR
  x = np.zeros(n)
  jitter = rng.uniform(0.97, 1.03)
  for f, a, d in ((1180.0, 1.0, 0.22), (1300.0, 0.75, 0.16), (1420.0, 0.55, 0.12)):
    x += a * np.sin(2 * np.pi * f * jitter * t + rng.uniform(0, 6.28)) * np.exp(-t / d)
  x[: int(0.008 * SR)] += _bandpass_noise(rng, int(0.008 * SR), 1100, 1500) * 1.2
  return _norm(x, 0.7)


###############################################################################
# M — machine hum loop: 120 Hz mains + harmonics, seamless (exact cycles).
###############################################################################

def bake_hum(dur=2.0):
  # frequencies chosen so an integer number of cycles fits dur exactly ->
  # a perfectly seamless loop with no crossfade needed.
  n = int(dur * SR)
  t = np.arange(n) / SR
  x = (1.00 * np.sin(2 * np.pi * 120.0 * t)
       + 0.45 * np.sin(2 * np.pi * 240.0 * t)
       + 0.18 * np.sin(2 * np.pi * 180.0 * t))
  rng = _rng("hum")
  x += _bandpass_noise(rng, n, 100, 260) * 0.12
  return _norm(x, 0.5)


###############################################################################
# traffic wash — brown-noise swell bed (flavor, ungated).
###############################################################################

def bake_traffic(dur=4.0):
  rng = _rng("traffic")
  n = int(dur * SR)
  x = np.cumsum(rng.standard_normal(n))          # brown noise
  x -= np.linspace(x[0], x[-1], n)               # detrend -> loopable-ish
  X = np.fft.rfft(x)
  f = np.fft.rfftfreq(n, 1.0 / SR)
  X[(f < 80) | (f > 600)] = 0.0
  x = np.fft.irfft(X, n)
  t = np.arange(n) / SR
  swell = 0.6 + 0.4 * np.sin(2 * np.pi * t / dur * 2.0)
  edge = int(0.3 * SR)
  env = np.ones(n)
  env[:edge] = np.linspace(0, 1, edge)
  env[-edge:] = np.linspace(1, 0, edge)
  return _norm(x * swell * env, 0.5)


###############################################################################
# P — the AmbiX bed the scene's SoundFieldProbe streams (band 6.5-7.5 kHz, an
# airy shimmer well clear of every other asset's band).
#
# THE ONE ASSET HERE THAT IS NOT SAMPLER-PLAYED: SoundField's feeder reads it
# with libsndfile at native rate straight into the B-format mix point, so the
# +1-octave sampler quirk that every other asset compensates for does NOT
# apply — this one must NOT be octave-shifted (hence its own writer).
#
# 4 channels, FOA AmbiX = ACN order (W Y Z X) + SN3D, which createProbe
# requires exactly. Encoded as a plane wave at azimuth `az_deg`:
#   W = s/sqrt(2)   Y = s*sin(az)   Z = 0   X = s*cos(az)
# (the same encode the SF1 unit-test fixtures use). FFT-brickwall noise is
# circularly continuous, so the file loops seamlessly.
###############################################################################

def bake_ambix_bed(dur=2.0, band=(6500.0, 7500.0), az_deg=0.0):
  rng = _rng("ambix_bed")
  n   = int(dur * SR)
  s   = _norm(_bandpass_noise(rng, n, band[0], band[1]), 0.5)
  az  = np.deg2rad(az_deg)
  return np.stack(
      [s / np.sqrt(2.0), s * np.sin(az), np.zeros(n), s * np.cos(az)], axis=1)


def write_wavN16(path, chans, sr=SR):
  """N-channel 16-bit PCM, NO octave compensation (see bake_ambix_bed)."""
  data = np.clip(chans, -1.0, 1.0)
  nch  = data.shape[1]
  ba   = nch * 2
  pcm  = (data.reshape(-1) * 32767.0).astype("<i2").tobytes()
  hdr = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE"
  hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, nch, sr, sr * ba, ba, 16)
  hdr += b"data" + struct.pack("<I", len(pcm))
  with open(path, "wb") as f:
    f.write(hdr + pcm)
  peak = float(np.abs(data).max())
  rms = float(np.sqrt(np.mean(data * data)))
  print("  %-18s %6.2fs  %dch  peak %.3f  rms %.3f" %
        (os.path.basename(path), len(data) / sr, nch, peak, rms))


###############################################################################

def main():
  ap = argparse.ArgumentParser()
  default_out = os.path.join(
      os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
      "ork.data", "sounds", "showcase")
  ap.add_argument("-o", "--outdir", default=default_out)
  args = ap.parse_args()
  os.makedirs(args.outdir, exist_ok=True)
  print("baking spatial-audio showcase corpus -> %s" % args.outdir)

  def put(name, data):
    write_wav16(os.path.join(args.outdir, name), data)

  put("water_grain_a.wav", bake_water_grain("a", 1.8))
  put("water_grain_b.wav", bake_water_grain("b", 2.1))
  put("water_grain_c.wav", bake_water_grain("c", 1.6))
  put("stream_loop.wav", bake_stream_loop())
  put("bird_a.wav", bake_bird("a"))
  put("bird_b.wav", bake_bird("b"))
  put("bird_c.wav", bake_bird("c"))
  put("chime.wav", bake_chime())
  put("clank_a.wav", bake_clank("a"))
  put("clank_b.wav", bake_clank("b"))
  put("hum_loop.wav", bake_hum())
  put("traffic_wash.wav", bake_traffic())
  write_wavN16(os.path.join(args.outdir, "treeline_ambix.wav"), bake_ambix_bed())
  print("done.")
  return 0


if __name__ == "__main__":
  sys.exit(main())
