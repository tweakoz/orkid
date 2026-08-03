#!/usr/bin/env ork.python
###############################################################################
# ork.synth.showcase.py — LIVE ALL-SYNTHESIZED spatial audio tour.
#
# A terminal program: boots singularity on the REAL audio device (the
# sequencer.py idiom — audio-only subsystems, no window) and plays a narrated
# ~120 s tour of PURE SYNTHESIS — zero wav/sample assets. The listener sits
# still; the SOURCES move: every sustained voice carries a PANNER2D stage
# whose ANGLE/DISTANCE data-params are animated live from the main thread
# (params-are-data — the same per-voice spatialization path the ECS emitters
# use), and one-shot grains are scattered across a round-robin program pool so
# each strike owns its own panner.
#
# MOVEMENTS (synthesis techniques):
#   I    aurora wind      subtractive: noise -> gust-LFO-modulated 2-pole
#                         lowpass; source ORBITS the listener
#   II   rain shimmer     scheduler granular: two grain flavors — sine
#                         "droplet" with fast falling pitch envelope, and
#                         bandpass-noise "spray"; density ramps, angles
#                         scattered per grain
#   III  FM bells         two-operator phase modulation (OscPMX mod->carrier,
#                         3:1 ratio, decaying index envelope); pentatonic
#                         strikes on alternating L<->R FLY-BYS
#   IV   evening pad      detuned saw -> resonant 4-pole lowpass with slow
#                         filter LFO; a held chord, voices fanned across the
#                         field, slowly rotating
#   V    dawn chorus      sine chirps: bipolar pitch envelopes + vibrato LFO,
#                         stochastic clusters, darting pans
#   VI   finale           everything at once + an arpeggio storm that pushes
#                         voice count into the steal ladder, then a clean
#                         master-gain release to silence
#
#   ork.synth.showcase.py                       # the live tour (~120 s, rc=0)
#   ork.synth.showcase.py --loop                # repeat until Ctrl-C
#   ork.synth.showcase.py --duration 60         # compressed tour
#   ork.synth.showcase.py --master-gain-db -9   # quieter (default 0)
#   ork.synth.showcase.py --diag                # + xrun counter per movement
#
# Ctrl-C at ANY point = clean teardown (voices keyed off, master faded, exit
# rc=0). --verify runs the IDENTICAL tour code path headless (STREAM device,
# WAV tee) and scores per-movement RMS + a pan-flip — no real device touched.
###############################################################################

import argparse
import math
import os
import random
import signal
import sys
import time

###############################################################################
# CLI parse FIRST — the headless render role must pin the STREAM ioclass into
# the environment BEFORE any engine import (the genviron trap).
###############################################################################

ap = argparse.ArgumentParser(description="live all-synthesized spatial audio tour")
ap.add_argument("--duration", type=float, default=120.0,
                help="tour length in seconds (default 120)")
ap.add_argument("--loop", action="store_true",
                help="repeat the tour until Ctrl-C")
ap.add_argument("--master-gain-db", type=float, default=0.0,
                help="master output gain in dB (default 0; peak stays ~ -13 dBFS)")
ap.add_argument("--diag", action="store_true",
                help="print the device xrun (underflow) counter at every movement "
                     "boundary and at exit; also enables the in-callback "
                     "compute-headroom windows (ORKID_PA_DIAG)")
ap.add_argument("--verify", action="store_true",
                help="INTERNAL: headless gate — render the identical tour to a "
                     "WAV via the STREAM device and score it (no real device)")
ap.add_argument("--headless-wav", default=None,
                help="INTERNAL: child role — render the tour to this wav path")
args = ap.parse_args()

HEADLESS = args.headless_wav is not None
if HEADLESS:
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ["ORKID_AUDIO_STREAM_SYNC"] = "1"

# the in-callback compute-headroom windows are env-gated in the device layer,
# and the gate is read on the first callback — so it must be pinned here, before
# the engine is imported (the genviron trap).
if args.diag:
  os.environ["ORKID_PA_DIAG"] = "1"

SELF = os.path.abspath(__file__)
DT = 0.01                      # headless pump quantum: 480 frames @48k exactly
NOTE_A4 = 69


def note_hz(n):
  return 440.0 * 2.0 ** ((n - NOTE_A4) / 12.0)


###############################################################################
# VERIFY DRIVER — pure stdlib+numpy; spawns the tour as its own child with the
# STREAM env pinned, then scores the WAV. Lives up here so the engine is never
# imported in this role.
###############################################################################

def run_verify():
  import struct
  import subprocess
  import numpy as np

  wav = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(SELF))),
                     ".tmp", "spatial_audio", "synth_showcase_verify.wav")
  os.makedirs(os.path.dirname(wav), exist_ok=True)
  if os.path.exists(wav):
    os.remove(wav)

  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"
  env["ORKID_AUDIO_STREAM_SYNC"] = "1"
  cmd = ["ork.python", SELF, "--headless-wav", wav,
         "--duration", str(args.duration),
         "--master-gain-db", str(args.master_gain_db)]
  t0 = time.time()
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=1800, env=env)
  print(p.stdout)
  if p.returncode != 0:
    print(p.stderr[-4000:])
    print("TESTVERDICT=FAIL detail=child rc=%d" % p.returncode)
    return 1

  # parse float WAV
  data = open(wav, "rb").read()
  pos = 12
  fmt = audio = None
  while pos + 8 <= len(data):
    cid = data[pos:pos + 4]
    csz = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    body = data[pos + 8:pos + 8 + csz]
    if cid == b"fmt ":
      fmt = body
    elif cid == b"data":
      audio = body
    pos += 8 + csz + (csz & 1)
  tag, ch, sr, _br, ba, bits = struct.unpack("<HHIIHH", fmt[:16])
  fl = np.frombuffer(audio[: len(audio) // ba * ba], dtype="<f4")
  L = fl[0::2].astype(np.float64)
  R = fl[1::2].astype(np.float64)
  mono = 0.5 * (L + R)
  dur = len(mono) / sr
  peak = float(np.abs(fl).max())

  scale = args.duration / 120.0

  def rms(x, t0s, t1s):
    a, b = int(t0s * scale * sr), min(int(t1s * scale * sr), len(x))
    seg = x[a:b]
    return float(np.sqrt(np.mean(seg * seg))) if seg.size else 0.0

  def band(x, lo, hi):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    return np.fft.irfft(X, len(x))

  print("")
  print("VERIFY: wav=%s ch=%d sr=%d dur=%.1fs peak=%.3f" % (wav, ch, sr, dur, peak))
  checks = []

  # per-movement non-silence windows (times in 120s-normalized tour seconds)
  windows = [("I_wind", 6, 18), ("II_shimmer", 26, 42), ("III_bells", 46, 64),
             ("IV_pad", 70, 90), ("V_chorus", 84, 100), ("VI_finale", 104, 112)]
  for name, a, b in windows:
    r = rms(mono, a, b)
    checks.append(("%s rms=%.5f (>1e-4)" % (name, r), r > 1e-4))

  # pan flip: movement I orbits the wind source. Compare wind-band stereo
  # asymmetry between opposite orbit phases.
  wl = band(L, 55, 400)
  wr = band(R, 55, 400)

  def asym(a, b):
    l, r = rms(wl, a, b), rms(wr, a, b)
    return (l - r) / (l + r + 1e-12)

  # orbit period is 40 s: opposite sides of the circle are 20 s apart
  a_1 = asym(7, 11)
  a_2 = asym(27, 31)
  checks.append(("I_orbit_flip asym %+.2f -> %+.2f (sign flip, |a|>0.05)"
                 % (a_1, a_2),
                 (a_1 * a_2 < 0.0) and abs(a_1) > 0.05 and abs(a_2) > 0.05))

  # headroom + tail silence (clean release)
  checks.append(("headroom peak=%.3f (<0.9)" % peak, peak < 0.9))
  tail = rms(mono, 118.5, 120.0)
  checks.append(("tail_release rms=%.6f (<3e-3)" % tail, tail < 3e-3))

  for desc, ok in checks:
    print("  [%s] %s" % ("PASS" if ok else "FAIL", desc))
  passed = all(ok for _, ok in checks)
  print("TESTVERDICT=%s detail=checks=%d/%d wall=%.0fs"
        % ("PASS" if passed else "FAIL",
           sum(1 for _, ok in checks if ok), len(checks), time.time() - t0))
  return 0 if passed else 1


if args.verify:
  sys.exit(run_verify())

###############################################################################
# engine boot (import-order law: core first)
###############################################################################

import orkengine.core  # noqa: E402,F401
from orkengine import lev2  # noqa: E402
from orkengine.lev2 import OrkEzApp  # noqa: E402
from orkengine.lev2 import singularity as SY  # noqa: E402

TAU = 2.0 * math.pi


###############################################################################
# --diag surface — the device xrun counter, sampled from the MAIN thread (the
# audio callback only ever bumps a relaxed atomic). Counters read zero on any
# device that runs no host callback (STREAM/NULL), so this is safe in every
# role.
###############################################################################

_diag_last_underflows = 0


def diag_mark(label):
  global _diag_last_underflows
  if not args.diag:
    return
  ctrs = lev2.audioDiagCounters()
  total = ctrs["underflows"]
  delta = total - _diag_last_underflows
  _diag_last_underflows = total
  print("[PA_DIAG] mark<%s> underflows_total<%d> delta<%+d> callbacks<%d> "
        "framesPerBuffer<%d> SR<%g>"
        % (label, total, delta, ctrs["callbacks"],
           ctrs["frames_per_buffer"], ctrs["sample_rate"]), flush=True)


###############################################################################
# patch builders — raw pyext program construction (the pmx.py / T1 idiom).
# Every builder returns (program, handles-dict). All programs end in a PAN
# stage (AmpPanner2D) whose ANGLE/DISTANCE params the tour animates live.
###############################################################################

class Patch:
  """One built program + its live-animatable parameter handles."""

  def __init__(self, bank, name):
    self.name = name
    self.prog = bank.newProgram(name)
    self.lyr = self.prog.newLayer()
    self.handles = {}

  def std_stages(self):
    lyr = self.lyr
    dsp = lyr.appendStage("DSP")
    amp = lyr.appendStage("AMP")
    pan = lyr.appendStage("PAN")
    dsp.ioconfig.inputs = [0, 1]
    dsp.ioconfig.outputs = [0, 1]
    amp.ioconfig.inputs = [0]          # AmpAdaptive: mono in -> stereo out
    amp.ioconfig.outputs = [0, 1]
    pan.ioconfig.inputs = [0, 1]
    pan.ioconfig.outputs = [0, 1]
    lyr.panmode = 0
    lyr.pan = 7                        # centered pre-panner
    pch = dsp.appendDspBlock("Pitch", "pitch")
    lyr.pitchBlock = pch
    return dsp, amp, pan

  def add_panner(self, pan_stage, angle=0.0, dist=5.0):
    pblk = pan_stage.appendDspBlock("AmpPanner2D", "panner")
    a = pblk.paramByName("ANGLE")
    d = pblk.paramByName("DISTANCE")
    a.coarse = angle
    d.coarse = dist
    self.handles["angle"] = a
    self.handles["dist"] = d
    return pblk

  def amp_env(self, amp_stage, segments, sustain=None, release=None):
    env = self.lyr.appendController("RateLevelEnv", "AMPENV")
    env.ampenv = True
    env.bipolar = False
    if sustain is not None:
      env.sustainSegment = sustain
    if release is not None:
      env.releaseSegment = release
    for nm, tm, lv, pw in segments:
      env.addSegment(nm, tm, lv, pw)
    blk = amp_stage.appendDspBlock("AmpAdaptive", "amp")
    blk.paramByName("gain").mods.src1 = env
    blk.paramByName("gain").mods.src1scale = 1.0
    return env


def build_wind(bank, tag):
  """Movement I — subtractive wind: noise -> gust-LFO lowpass -> lowpass."""
  p = Patch(bank, "wind_" + tag)
  dsp, amp, pan = p.std_stages()
  dsp.appendDspBlock("OscilNoise", "noise")
  lp1 = dsp.appendDspBlock("FilterLowPass2", "gustlp")
  gust = p.lyr.appendController("Lfo", "GUSTLFO")
  gust.properties.minRate = 0.11
  gust.properties.maxRate = 0.11
  cut = lp1.paramByName("cutoff")
  cut.coarse = 210.0
  cut.mods.src1 = gust
  cut.mods.src1scale = 120.0
  lp2 = dsp.appendDspBlock("FilterLowPass2", "toplp")
  lp2.paramByName("cutoff").coarse = 380.0
  p.amp_env(amp, [("atk", 2.5, 1.0, 0.5), ("sus", 1.0, 1.0, 0.5),
                  ("rel", 2.0, 0.0, 0.5)], sustain=1, release=2)
  p.add_panner(pan, angle=0.0, dist=3.5)
  return p


def build_droplet(bank, tag):
  """Movement II grain (a) — sine 'droplet': fast falling pitch envelope."""
  p = Patch(bank, "droplet_" + tag)
  dsp, amp, pan = p.std_stages()
  osc = dsp.appendDspBlock("OscilSine", "osc")
  penv = p.lyr.appendController("RateLevelEnv", "DROPENV")
  penv.ampenv = False
  penv.bipolar = True
  penv.addSegment("s0", 0.0, 1.0, 1.0)
  penv.addSegment("s1", 0.10, 0.0, 2.0)
  pp = osc.paramByName("pitch")
  pp.mods.src1 = penv
  pp.mods.src1scale = 900.0            # falls ~9 semitones over the grain
  p.amp_env(amp, [("atk", 0.004, 1.0, 0.5), ("dec", 0.16, 0.0, 2.0)])
  p.add_panner(pan, dist=3.0)
  return p


def build_spray(bank, tag):
  """Movement II grain (b) — bandpass-noise 'spray' burst."""
  p = Patch(bank, "spray_" + tag)
  dsp, amp, pan = p.std_stages()
  dsp.appendDspBlock("OscilNoise", "noise")
  bp = dsp.appendDspBlock("FilterBandpass2", "bp")
  bp.paramByName("cutoff").coarse = 1100.0
  p.handles["cutoff"] = bp.paramByName("cutoff")
  p.amp_env(amp, [("atk", 0.01, 0.8, 0.5), ("dec", 0.25, 0.0, 1.5)])
  p.add_panner(pan, dist=3.0)
  return p


def build_bell(bank, tag):
  """Movement III — 2-op FM (phase modulation) bell: OscPMX mod -> carrier,
  ~3:1 ratio, decaying modulation index (the classic FM bell recipe)."""
  p = Patch(bank, "bell_" + tag)
  dsp, amp, pan = p.std_stages()

  modenv = p.lyr.appendController("RateLevelEnv", "MODENV")
  modenv.ampenv = False
  modenv.bipolar = False
  modenv.addSegment("hit", 0.002, 2.2, 1.0)
  modenv.addSegment("dec", 1.1, 0.0, 2.0)

  mod = dsp.appendDspBlock("OscPMX", "fm_mod")
  mod.properties.InputChannel = 0
  mod.properties.PmInputChannels = [0, 1, 2, 3]
  mod.paramByName("amp").coarse = 0.0
  mod.paramByName("amp").mods.src1 = modenv
  mod.paramByName("amp").mods.src1scale = 1.0
  mod.paramByName("pitch").coarse = 19.02        # +19.02 semis ~= 3.0x ratio

  car = dsp.appendDspBlock("OscPMX", "fm_car")
  car.properties.InputChannel = 0
  car.properties.PmInputChannels = [0, 1, 2, 3]
  car.paramByName("amp").coarse = 1.0
  car.paramByName("pitch").coarse = 0.0

  p.amp_env(amp, [("atk", 0.002, 1.0, 0.4), ("dec", 2.8, 0.0, 2.2)])
  p.add_panner(pan, dist=3.0)
  return p


def build_pad(bank, tag):
  """Movement IV — detuned saw through a slow resonant 4-pole lowpass."""
  p = Patch(bank, "pad_" + tag)
  dsp, amp, pan = p.std_stages()
  dsp.appendDspBlock("OscilSaw", "saw")
  lp = dsp.appendDspBlock("Filter4PoleLowPassWithSep", "lp4")
  flfo = p.lyr.appendController("Lfo", "FILTLFO")
  flfo.properties.minRate = 0.07
  flfo.properties.maxRate = 0.07
  cut = lp.paramByName("cutoff")
  cut.coarse = 900.0
  cut.mods.src1 = flfo
  cut.mods.src1scale = 500.0
  lp.paramByName("resonance").coarse = 0.25
  lp.paramByName("separation").coarse = 12.0     # slight cascade detune
  p.amp_env(amp, [("atk", 3.0, 1.0, 0.5), ("sus", 1.0, 1.0, 0.5),
                  ("rel", 4.0, 0.0, 0.6)], sustain=1, release=2)
  p.add_panner(pan, dist=4.5)
  return p


def build_bird(bank, tag):
  """Movement V — chirp gesture: sine + bipolar pitch sweep + vibrato LFO."""
  p = Patch(bank, "bird_" + tag)
  dsp, amp, pan = p.std_stages()
  osc = dsp.appendDspBlock("OscilSine", "osc")
  sweep = p.lyr.appendController("RateLevelEnv", "CHIRPENV")
  sweep.ampenv = False
  sweep.bipolar = True
  sweep.addSegment("up", 0.06, 1.0, 0.6)
  sweep.addSegment("dn", 0.12, -0.4, 1.4)
  vib = p.lyr.appendController("Lfo", "VIB")
  vib.properties.minRate = 11.0
  vib.properties.maxRate = 11.0
  pp = osc.paramByName("pitch")
  pp.mods.src1 = sweep
  pp.mods.src1scale = 700.0
  pp.mods.src2 = vib
  pp.mods.src2mindepth = 45.0
  pp.mods.src2maxdepth = 45.0
  p.amp_env(amp, [("atk", 0.006, 0.9, 0.5), ("dec", 0.22, 0.0, 1.6)])
  p.add_panner(pan, dist=3.0)
  return p


###############################################################################
# the TOUR — one timeline, identical in live and headless modes.
###############################################################################

class Tour:

  def __init__(self, synth, duration):
    self.syn = synth
    self.T = duration
    self.k = duration / 120.0          # timeline scale
    self.rng = random.Random(0xA0D10)  # deterministic gesture schedule
    self.live_voices = []              # (prginst, patch, note, off_deadline)
    self.total_keyons = 0
    self.announced = set()
    self.done = False
    self._next = {}                    # per-generator next-fire times

    bank = SY.BankData()
    self.bank = bank                   # keep alive
    self.wind = build_wind(bank, "a")
    self.droplets = [build_droplet(bank, str(i)) for i in range(5)]
    self.sprays = [build_spray(bank, str(i)) for i in range(4)]
    self.bells = [build_bell(bank, str(i)) for i in range(4)]
    self.pads = [build_pad(bank, str(i)) for i in range(4)]
    self.birds = [build_bird(bank, str(i)) for i in range(4)]
    self.pool_idx = {}
    self.wind_voice = None
    self.pad_voices = []
    self.bell_count = 0

  # ---- helpers -------------------------------------------------------------

  def say(self, key, msg):
    if key not in self.announced:
      self.announced.add(key)
      print("  %s" % msg, flush=True)

  def pick(self, pool):
    i = self.pool_idx.get(id(pool), 0)
    self.pool_idx[id(pool)] = (i + 1) % len(pool)
    return pool[i]

  def key_on(self, patch, note, vel, hold=None, t=0.0):
    inst = self.syn.keyOn(note, vel, patch.prog, None)
    if inst is not None:
      deadline = (t + hold) if hold is not None else None
      self.live_voices.append([inst, patch, note, deadline])
      self.total_keyons += 1
    return inst

  def key_off(self, entry):
    inst, patch, note, _ = entry
    try:
      self.syn.keyOff(inst, note, 0)
    except Exception:
      pass

  def reap(self, t):
    keep = []
    for entry in self.live_voices:
      if entry[3] is not None and t >= entry[3]:
        self.key_off(entry)
      else:
        keep.append(entry)
    self.live_voices = keep

  def every(self, key, t, period):
    """True when generator `key` should fire at time t (period seconds)."""
    nxt = self._next.get(key, None)
    if nxt is None:
      self._next[key] = t + period
      return True
    if t >= nxt:
      self._next[key] = t + period
      return True
    return False

  # ---- the timeline --------------------------------------------------------

  def update(self, t):
    k = self.k
    u = t / k                          # normalized tour seconds (0..120)

    # ============ I: WIND (0..24, sustains under everything to 114) =========
    if u >= 0.0 and self.wind_voice is None and "m1" not in self.announced:
      self.announced.add("m1")
      print("\n[%6.1fs] I. AURORA WIND — filtered-noise gusts (subtractive)"
            % t, flush=True)
      diag_mark("I.wind")
      self.say("wind1", "wind rises ahead, then ORBITS the listener "
                        "counter-clockwise (full circle / 40s)")
      self.wind_voice = self.key_on(self.wind, 60, 110)
    if self.wind_voice is not None:
      # orbit: angle sweeps a full circle; distance swells in and out
      ang = TAU * (u / 40.0)
      dist = 3.5 + 1.2 * math.sin(TAU * u / 31.0)
      self.wind.handles["angle"].coarse = ang
      self.wind.handles["dist"].coarse = max(1.8, dist)
      if u > 10 and u < 30:
        self.say("wind2", "        ...wind passing your LEFT ear now, "
                          "sliding behind you...")

    # ============ II: RAIN SHIMMER (20..46) =================================
    if 20.0 <= u < 46.0:
      if "m2" not in self.announced:
        print("\n[%6.1fs] II. RAIN SHIMMER — scheduler granular: sine "
              "droplets + bandpass spray" % t, flush=True)
        self.announced.add("m2")
        diag_mark("II.rain")
        self.say("m2b", "grains scattered around the horizon, density "
                        "ramping 2 -> 14 grains/s")
      dens = 2.0 + 12.0 * min(1.0, (u - 20.0) / 20.0)
      if self.every("grain", t, k / dens):
        if self.rng.random() < 0.6:
          g = self.pick(self.droplets)
          note = self.rng.randint(84, 100)
        else:
          g = self.pick(self.sprays)
          g.handles["cutoff"].coarse = self.rng.uniform(700.0, 2200.0)
          note = 72
        g.handles["angle"].coarse = self.rng.uniform(-math.pi, math.pi)
        g.handles["dist"].coarse = self.rng.uniform(1.5, 5.0)
        self.key_on(g, note, self.rng.randint(45, 80), hold=0.5 * k, t=t)

    # ============ III: FM BELLS (44..70) ====================================
    if 44.0 <= u < 70.0:
      if "m3" not in self.announced:
        print("\n[%6.1fs] III. FM BELLS — 2-op phase-mod (3:1, decaying "
              "index), pentatonic strikes" % t, flush=True)
        self.announced.add("m3")
        diag_mark("III.bells")
      if self.every("bell", t, 3.2 * k):
        penta = [60, 62, 65, 67, 70, 72, 74, 77]
        note = penta[self.rng.randint(0, len(penta) - 1)]
        b = self.pick(self.bells)
        self.bell_count += 1
        ltr = (self.bell_count % 2) == 0
        b._flyby = (t, +1.0 if ltr else -1.0)
        print("        bell %d (%s fly-by, note %d)"
              % (self.bell_count, "left->right" if ltr else "right->left",
                 note), flush=True)
        self.key_on(b, note, 96, hold=3.2 * k, t=t)
      for b in self.bells:
        fb = getattr(b, "_flyby", None)
        if fb is not None:
          age = (t - fb[0]) / (3.0 * k)
          if age <= 1.0:
            x = (age * 2.0 - 1.0) * fb[1]          # -1 .. +1 across the front
            b.handles["angle"].coarse = -math.atan2(x * 5.0, 2.0)
            b.handles["dist"].coarse = max(1.5, math.hypot(x * 5.0, 2.0))

    # ============ IV: EVENING PAD (62..112) =================================
    if u >= 62.0 and not self.pad_voices:
      print("\n[%6.1fs] IV. EVENING PAD — detuned saws, resonant 4-pole "
            "sweep; chord fanned across the field, slowly rotating"
            % t, flush=True)
      diag_mark("IV.pad")
      chord = [(38, -1.0), (45, -0.35), (53, +0.35), (60, +1.0)]  # D min add9
      for note, fan in chord:
        pv = self.pick(self.pads)
        pv._fan = fan
        self.pad_voices.append((pv, self.key_on(pv, note, 48)))
    if self.pad_voices and u < 112.0:
      rot = 0.35 * math.sin(TAU * u / 47.0)
      for pv, _ in self.pad_voices:
        pv.handles["angle"].coarse = pv._fan * 1.05 + rot
        pv.handles["dist"].coarse = 6.0

    # ============ V: DAWN CHORUS (80..104) ==================================
    if 80.0 <= u < 104.0:
      if "m5" not in self.announced:
        print("\n[%6.1fs] V. DAWN CHORUS — chirp gestures (bipolar pitch "
              "sweeps + 11 Hz vibrato), darting pans" % t, flush=True)
        self.announced.add("m5")
        diag_mark("V.chorus")
      if self.every("birdburst", t, 1.9 * k):
        n_chirp = self.rng.randint(2, 5)
        base_ang = self.rng.uniform(-math.pi, math.pi)
        for i in range(n_chirp):
          bd = self.pick(self.birds)
          bd.handles["angle"].coarse = base_ang + self.rng.uniform(-0.5, 0.5)
          bd.handles["dist"].coarse = self.rng.uniform(2.0, 5.0)
          note = self.rng.randint(93, 103)
          self.key_on(bd, note, self.rng.randint(50, 72),
                      hold=(0.3 + 0.12 * i) * k, t=t)

    # ============ VI: FINALE (100..114) =====================================
    if 100.0 <= u < 114.0:
      if "m6" not in self.announced:
        print("\n[%6.1fs] VI. FINALE — arpeggio storm over everything; "
              "pushing voice count into the steal ladder" % t, flush=True)
        self.announced.add("m6")
        diag_mark("VI.finale")
      if self.every("arp", t, 0.14 * k):
        scale_notes = [50, 53, 57, 60, 62, 65, 69, 72, 74, 77, 81, 84]
        note = scale_notes[self.total_keyons % len(scale_notes)]
        b = self.pick(self.bells)
        b.handles["angle"].coarse = TAU * ((self.total_keyons % 12) / 12.0)
        b.handles["dist"].coarse = 3.0
        self.key_on(b, note, 60, hold=2.5 * k, t=t)
      if self.every("voicecount", t, 2.0 * k):
        print("        live voices tracked: %d (total keyons %d)"
              % (len(self.live_voices), self.total_keyons), flush=True)

    # ============ RELEASE (114..) ===========================================
    if u >= 114.0 and "rel" not in self.announced:
      self.announced.add("rel")
      print("\n[%6.1fs] ...release: keying everything off, wind fades last"
            % t, flush=True)
      diag_mark("release")
      for entry in self.live_voices:
        self.key_off(entry)
      self.live_voices = []
      self.wind_voice = None
      self.pad_voices = []
    if u >= 114.0:
      # master fade to silence across the last 5 normalized seconds
      fade = max(0.0, 1.0 - (u - 114.0) / 5.0)
      self.syn.masterGain = self.master_lin * fade

    self.reap(t)
    if u >= 120.0 and not self.done:
      self.done = True


###############################################################################
# the APP — one shape, two devices: live PortAudio (default) or STREAM+WAV
# (--headless-wav). The tour code path is byte-identical.
###############################################################################

class ShowcaseApp:

  def __init__(self):
    self.tour = None
    self.t0 = None
    self.pumped = 0
    self.exiting = False
    self.stop_requested = False
    kwargs = dict(
        name="SynthShowcase",
        use_subsystems=["opq", "core", "audioO"],
        enable_audio_synth=True,
        freerun=True)
    if HEADLESS:
      kwargs["audio_stream_sync"] = True
      kwargs["wav_output_path"] = args.headless_wav
    self.ezapp = OrkEzApp.create(self, **kwargs)

  def request_stop(self, *_):
    if self.stop_requested:
      print("\n(second interrupt — forcing exit)", flush=True)
      os._exit(1)
    self.stop_requested = True
    print("\n[interrupt] clean teardown...", flush=True)

  def iterate(self):
    syn = self.ezapp.audio_synth
    if syn is None:
      return
    dev = self.ezapp.audio_device if HEADLESS else None

    if self.tour is None:
      master_lin = 10.0 ** (args.master_gain_db / 20.0)
      syn.masterGain = master_lin
      self.tour = Tour(syn, args.duration)
      self.tour.master_lin = master_lin
      print("=" * 66, flush=True)
      print(" ORKID SINGULARITY — live synthesis tour (%ds, master %+0.1f dB)"
            % (int(args.duration), args.master_gain_db), flush=True)
      print(" 100%% synthesized - no samples. Sources move; you sit still."
            , flush=True)
      print("=" * 66, flush=True)
      diag_mark("boot")
      self.t0 = time.monotonic()

    # timebase: wall-clock live; pumped-frames headless
    if HEADLESS:
      t = self.pumped * DT
      self.pumped += 1
      self.tour.update(t)
      dev.advanceTime(DT)
    else:
      t = time.monotonic() - self.t0
      self.tour.update(t)
      time.sleep(0.004)

    if self.stop_requested and not self.exiting:
      self.exiting = True
      for entry in self.tour.live_voices:
        self.tour.key_off(entry)
      self.tour.live_voices = []
      syn.masterGain = 0.0
      self.ezapp.signalExit()
      return

    if self.tour.done and not self.exiting:
      if args.loop and not HEADLESS:
        print("\n(--loop: tour restarting)\n", flush=True)
        syn.masterGain = self.tour.master_lin
        announced_master = self.tour.master_lin
        self.tour = Tour(syn, args.duration)
        self.tour.master_lin = announced_master
        self.t0 = time.monotonic()
      else:
        self.exiting = True
        print("\ntour complete — clean exit.", flush=True)
        self.ezapp.signalExit()


def main():
  app = ShowcaseApp()
  signal.signal(signal.SIGINT, app.request_stop)
  signal.signal(signal.SIGTERM, app.request_stop)
  app.ezapp.mainThreadLoop(on_iter=app.iterate)
  diag_mark("exit")
  print("TOUR_EXIT keyons=%d" % (app.tour.total_keyons if app.tour else 0),
        flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
