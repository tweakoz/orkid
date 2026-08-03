#!/usr/bin/env ork.python
################################################################################
# test_soundfield_liveencode — the SF2 LIVE ENCODE canary.
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display, NO assets):
#   A LIVE singularity voice with an authored soundfieldSend{level,spread} is
#   first-order-ambisonic encoded into the engine's one B-format mix point and
#   comes out of the Gerzon stereo decoder pointing where the voice's PANNER2D
#   ANGLE says it is — including while that angle MOVES. Before this test the
#   only thing that could reach the field was a baked 4-channel probe
#   (test_soundfield_direction), and NO gate anywhere moved a source: the 8
#   spatial-audio harness gates all walk the LISTENER past static emitters.
#
#   The encode contract (soundfield.h, spatializer.h) for a plane wave of
#   amplitude s at azimuth az, elevation el, in ACN/SN3D order:
#       W = s/sqrt(2)   Y = s*sin(az)*cos(el)   Z = s*sin(el)   X = s*cos(az)*cos(el)
#   AMBISONIC AZIMUTH IS LEFT-POSITIVE (+Y is the listener's LEFT) while the
#   panner's ANGLE is a = -atan2(x,z) in engine listener space, so
#       az = pi - a
#   and the ideal decoded low-band ratio is L/R = (1+cos(az-45))/(1+cos(az+45)).
#
#   SETTLED SEMANTICS THIS TEST PINS (see spatializer.h — these are law):
#     * FALLOFF is the PANNER's OpenAL inverse-distance-clamped model, NOT the
#       probe smoothstep: the encode reads the voice's POST-panner buffer, so
#       the dry and encoded copies of one voice cannot disagree about distance.
#       Check 3 therefore reuses G8_probe_bed's ORDERED near/mid/far assertion
#       STYLE (near/mid > 1.4, mid/far > 50) against gains predicted by
#       distGain = ref/(ref + rolloff*(clamp(d,ref,max)-ref)).
#     * SPREAD is directivity interpolation (W whole, directional x (1-spread)).
#     * ELEVATION is absent from the live path (PANNER2D has ANGLE/DISTANCE
#       only), so everything here is azimuth-plane; the encode keeps the el term
#       but is fed el=0. No check below expects elevation to be audible.
#     * _level is dB.
#
#   Per check:
#     1 STATIC: five held azimuths decode to the ideal ratio within tol, are
#       strictly ORDERED, are +/-90 reciprocal, AND match the ratio a baked
#       AmbiX probe at the same azimuth produces (the same encode arithmetic
#       arriving by the other road) — the ordering/reciprocity pair is what
#       catches a mirrored or dropped-Y encode that a loose ratio check passes.
#     2 MOVING: one voice swept at constant radius from az=-90 up to az=+90 and
#       back down, ANGLE rewritten every pump (the A8 data-param path the
#       showcase's wind layer uses). Per-window ratios track the ideal, are
#       monotone up then monotone down (a frozen encode passes a single-angle
#       check but never this), and the decoded signal's slew stays under a fixed
#       ceiling (no per-pass gain step).
#       AN ARC, NOT A REVOLUTION: the Gerzon decode's R channel NULLS at
#       az=+135 (and L at -135), so the ratio law has a pole in the rear half —
#       asserting a rear-half ratio would be asserting on a division by zero.
#       The arc covers both sweep directions, which is what (b) needs.
#     3 FALLOFF: azimuth held, three radii (at ref / mid-band / beyond max).
#     4 DETERMINISM: two independent renders of the moving case are
#       BYTE-IDENTICAL (sha256).
#     5 STREAM HEALTH: zero field underruns, render non-silent.
#     6 ABSENT FIELD IS LOUD: a soundfieldSend authored with no SoundField
#       created must fail at the keyOn resolution site, never play dry.
#
#   DETERMINISM SEAM: there is no ECS and no gpu here, so the clock is the
#   pump loop itself — dev.advanceTime(DT) is the fixed step (the same shape
#   the sf1 canaries use); the encode's gain slew is driven by control-pass
#   count, never by wall time.
#
#   ISOLATION: the voice's DRY copy goes to its own output bus which is MUTED,
#   so the captured WAV is the soundfield bus alone. Muting the bus cannot
#   affect the encode: the encode reads the voice's dsp buffer, upstream of any
#   bus.
#
# HEADLESS (consent law): audio-only subsystems (no gpu), ORKID_AUDIO_IOCLASS
# pinned to STREAM in BOTH the driver spawn env and the child BEFORE the engine
# import (genviron snapshots `environ` at library load), so no host audio device
# is ever opened and nothing is audible.
#
# SHAPE (mirrors test_soundfield_direction.py): the driver (no --role) is pure
# stdlib + numpy and spawns each engine-booting render as its own ork.python
# subprocess with ORKID_DRM_MODE stripped.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
import struct
import argparse
# NOTE: subprocess / tempfile / numpy / hashlib are imported lazily in the
# driver-only paths — the child must not pre-load hashlib/ssl-adjacent stdlib
# before orkengine.core (see the note in test_audio_wav_render.py).

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT          = 1.0 / 100.0    # 480 frames @48k == 15 whole control passes
NOTE        = 43             # ~98Hz, below the 700Hz Gerzon shelf
VEL         = 100
LEVEL_DB    = 0.0            # unity: the send's level is authored in dB
SPREAD      = 0.0            # point source: full directivity
DRY_BUS     = "sf2_dry"      # muted, so the capture is the field alone

N_STATIC    = 140            # pumps per static render (1.4s)
SKIP_STATIC = 70             # discard the note's attack + the panner's tail rise
TONE_HZ     = 100.0          # probe fixture tone (the baked cross-check)
FIXTURE_S   = 1.0
RATIO_TOL   = 0.10           # relative, as in test_soundfield_direction

# moving sweep: az -90 -> +90 -> -90 at constant radius.
MOVE_LEG    = 240            # pumps per leg (2.4s per 180deg)
MOVE_SETTLE = 40             # pumps held at the start azimuth before moving
N_WINDOWS   = 6              # per leg -> 30deg per window, 12 windows total
MOVING_TOL  = 0.20           # looser: a window spans 30deg and the gains slew
# no-zipper ceilings, both as ratios so they carry across platforms. as built
# the swept render measures 1.37x the tone's own per-sample slope and 1.05 on
# the envelope; a per-pass gain step (no slew) or an unprimed first pass shows
# up as an outlier here.
ZIPPER_FACTOR = 2.0
ENV_JUMP_MAX  = 1.20

# falloff: the PANNER2D defaults (ref=1, max=100, rolloff=1, minGain=-60dB),
# which no python binding can author today, so the radii are chosen to land on
# G8's thresholds: distGain = 1/d over [1,100] gives 1.000 / 0.556 / 0.010.
FALLOFF_RADII = (1.0, 1.8, 150.0)
FALLOFF_TOL = 0.25           # the panner's allpass/ITD blend colours the RMS
RMS_FLOOR   = 1.0e-3

# azimuth (deg) -> ideal low-band L/R ratio (1+cos(az-45))/(1+cos(az+45))
CASES = (
    (-90.0, 0.171573),
    (-45.0, 0.500000),
    (0.0, 1.000000),
    (45.0, 2.000000),
    (90.0, 5.828427),
)


def ideal_ratio(az_deg):
  az = math.radians(az_deg)
  q  = math.radians(45.0)
  return (1.0 + math.cos(az - q)) / (1.0 + math.cos(az + q))


def panner_angle_for_az(az_deg):
  """az = pi - a  =>  a = pi - az, wrapped into (-pi,pi]."""
  a = math.pi - math.radians(az_deg)
  while a > math.pi:
    a -= 2.0 * math.pi
  while a <= -math.pi:
    a += 2.0 * math.pi
  return a


def dist_gain(d, ref=1.0, mx=100.0, rolloff=1.0, mingain=0.001):
  """PANNER2D's OpenAL inverse-distance-clamped model (panner.cpp)."""
  c = min(max(d, ref), mx)
  return max(ref / (ref + rolloff * (c - ref)), mingain)


################################################################################
# fixture synthesis — a 4ch float32 AmbiX WAV for the baked cross-check, written
# by hand (python's `wave` module cannot write float). Same encode arithmetic as
# test_soundfield_direction.write_ambix, by design: check 1's whole point is
# that the LIVE road and the BAKED road agree.
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
# CHILD — one engine boot: audio-only headless, one live voice with an authored
# soundfieldSend, pump advanceTime teeing to --wav, then exit.
################################################################################

def _make_tone_program(name="sf2_tone"):
  """A sustained ~98Hz sine with a PANNER2D stage appended — the same shape the
  ECS sound emitters build (sampler|osc -> AMP -> PAN), and the same host-side
  recipe the showcase's wind layer uses."""
  from ork.hypergraph.sound.dsl import S, SoundPatch
  from ork.hypergraph.sound.emitter import materialize_sound_instance

  class Tone(SoundPatch):
    def __init__(self):
      sig = S.sine(label="sf2tone")
      env = S.env([("atk", 0.02, 1.0, 0.5), ("sus", 1.0, 1.0, 0.5)],
                  sustain=1, label="sf2env")
      self.output(sig, amp=env)

  mat   = materialize_sound_instance(Tone(), name=name)
  layer = mat.layer
  stage = layer.appendStage("PAN")
  stage.ioconfig.inputs  = [0, 1]
  stage.ioconfig.outputs = [0, 1]
  panner = stage.appendDspBlock("AmpPanner2D", "PANNER")
  return mat, layer, panner


def _role_render(role, wav_path, fixture_path, az_deg, dist):
  # genviron (ork.core) snapshots `environ` when the shared lib LOADS, so the
  # ioclass pin MUST precede the orkengine import.
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  if role == "probe":
    write_ambix(fixture_path, az_deg)

  import orkengine.core as core                # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  npumps = N_STATIC
  if role == "moving":
    npumps = MOVE_SETTLE + 2 * MOVE_LEG

  class App(object):
    def __init__(self):
      self.phase     = 0
      self.pumped    = 0
      self.underruns = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="SoundFieldLiveEncodeTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                     # -> StrAudioDevice SYNC
          wav_output_path=wav_path,
          freerun=True)

    def setup(self, syn):
      syn.masterGain = 1.0
      # identity listener: the field's rotation is identity, so the decode sees
      # the encode unrotated (a live send is encoded in LISTENER space).
      syn.listener_matrix = core.mtx4()
      # the field must ALREADY exist at keyOn: the encode path never creates
      # one (that spawns the feeder thread), which is what check 6 pins.
      self.field = None
      if role != "nofield":
        self.field = S.SoundField.instance()

      if role == "probe":
        self.probe = self.field.createProbe(fixture_path, True)
        self.field.setProbeParams(self.probe, core.vec3(0, 0, 0),
                                  refDistance=10.0, maxDistance=50.0,
                                  rolloff=1.0, gain=1.0)
        for _ in range(200):    # settle the weight slew before audio time
          self.field.update(DT)
        return

      mat, layer, panner = _make_tone_program()
      self.panner = panner
      self.angle  = panner.paramByName("ANGLE")
      self.dist   = panner.paramByName("DISTANCE")

      spat = S.PannerSpatializerData()
      send = S.SoundFieldSendData()
      send.level  = LEVEL_DB
      send.spread = SPREAD
      spat.soundfieldSend = send
      layer.configureSoundFieldSend(spat, panner)

      start_az = -90.0 if role == "moving" else az_deg
      self.angle.coarse = panner_angle_for_az(start_az)
      self.dist.coarse  = dist

      bus = self.ezapp.audio_synth.outputBus(DRY_BUS)
      if bus is None:
        bus = self.ezapp.audio_synth.createOutputBus(DRY_BUS)
      bus.mute = True                     # capture the field alone
      kmod = S.KeyOnModifiers()
      kmod.outputbus = bus
      self.voice = syn.keyOn(NOTE, VEL, mat.program, kmod)
      print("CHILD_VOICE=%d" % (1 if self.voice is not None else 0), flush=True)

    def step(self):
      if role != "moving":
        return
      i = self.pumped - MOVE_SETTLE
      if i < 0:
        return
      if i < MOVE_LEG:
        az = -90.0 + 180.0 * float(i) / float(MOVE_LEG)          # up
      else:
        j  = min(i - MOVE_LEG, MOVE_LEG)
        az = 90.0 - 180.0 * float(j) / float(MOVE_LEG)           # back down
      self.angle.coarse = panner_angle_for_az(az)

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      if self.phase == 0:
        self.setup(syn)
        self.phase = 1
        return
      self.step()
      if self.field is not None:
        self.field.update(DT)
      dev.advanceTime(DT)
      self.pumped += 1
      if self.pumped >= npumps:
        if self.field is not None:
          self.underruns = self.field.underrunCount
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
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


def _lr(path):
  wa = _parse_wav(path)
  fl = wa["floats"]
  return fl[0::2], fl[1::2], wa["sr"]


def _rms(x):
  import numpy as np
  return float(np.sqrt(np.mean(x.astype("f8") * x.astype("f8")))) if len(x) else 0.0


def _spawn(role, wav_path, fixture_path=None, az_deg=0.0, dist=1.0, timeout=300):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"       # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", role, "--wav", wav_path or "",
         "--fixture", fixture_path or "", "--az", repr(az_deg), "--dist", repr(dist)]
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
  import hashlib
  import numpy as np
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="sf2live_")
  checks = []
  bits   = []

  def check(name, ok):
    checks.append((name, bool(ok)))
    return bool(ok)

  def render(role, tag, az=0.0, dist=1.0):
    wav = os.path.join(tmp, "r_%s.wav" % tag)
    fix = os.path.join(tmp, "f_%s.wav" % tag)
    try:
      rc, out = _spawn(role, wav, fix, az, dist)
    except subprocess.TimeoutExpired as e:
      print((e.stdout or "") + (e.stderr or ""))
      sys.exit(verdict(False, "child render timed out (role=%s tag=%s)" % (role, tag)))
    print(out)
    if rc != 0 or not os.path.isfile(wav):
      check("render_%s" % tag, False)
      bits.append("%s=RENDERFAIL(rc=%d)" % (tag, rc))
      return None, out
    check("stream_clean_%s" % tag, _grep(out, "CHILD_UNDERRUNS") == "0")
    return wav, out

  try:
    ############################################################
    # 1 STATIC DIRECTION — live vs analytic ideal vs baked probe
    ############################################################
    live_ratios  = {}
    probe_ratios = {}
    for az, ideal in CASES:
      tag = "%+03d" % int(az)
      wav, _ = render("static", "live" + tag, az=az, dist=1.0)
      if wav is None:
        continue
      L, R, sr = _lr(wav)
      k = SKIP_STATIC * int(round(DT * sr))
      rl, rr = _rms(L[k:]), _rms(R[k:])
      check("nonsilent_live%s" % tag, max(rl, rr) > RMS_FLOOR)
      ratio = (rl / rr) if rr > 1e-12 else float("inf")
      live_ratios[az] = ratio
      rel = abs(ratio - ideal) / ideal
      check("live_ratio%s" % tag, rel <= RATIO_TOL)
      bits.append("live%s: L=%.5f R=%.5f ratio=%.4f ideal=%.4f rel=%.3f"
                  % (tag, rl, rr, ratio, ideal, rel))

      wav, _ = render("probe", "probe" + tag, az=az)
      if wav is None:
        continue
      L, R, sr = _lr(wav)
      k = SKIP_STATIC * int(round(DT * sr))
      rl, rr = _rms(L[k:]), _rms(R[k:])
      pratio = (rl / rr) if rr > 1e-12 else float("inf")
      probe_ratios[az] = pratio
      prel = abs(ratio - pratio) / pratio if pratio > 1e-12 else 1.0
      check("live_vs_baked%s" % tag, prel <= RATIO_TOL)
      bits.append("baked%s: ratio=%.4f vs live rel=%.3f" % (tag, pratio, prel))

    seq = [live_ratios.get(az) for az, _ in CASES]
    check("strict_ordering",
          all(v is not None for v in seq)
          and all(seq[i] < seq[i + 1] for i in range(len(seq) - 1)))
    check("lateral_symmetry",
          -90.0 in live_ratios and 90.0 in live_ratios
          and abs(live_ratios[-90.0] * live_ratios[90.0] - 1.0) < 0.05)

    ############################################################
    # 2 MOVING EMITTER + 4 DETERMINISM
    ############################################################
    mv1, _ = render("moving", "move1")
    mv2, _ = render("moving", "move2")
    if mv1 and mv2:
      h1 = hashlib.sha256(open(mv1, "rb").read()).hexdigest()
      h2 = hashlib.sha256(open(mv2, "rb").read()).hexdigest()
      check("moving_deterministic", h1 == h2)
      bits.append("sha256=%s/%s" % (h1[:12], h2[:12]))

      L, R, sr = _lr(mv1)
      spp   = int(round(DT * sr))
      start = MOVE_SETTLE * spp
      legs  = []
      for leg in range(2):
        base = start + leg * MOVE_LEG * spp
        wlen = (MOVE_LEG * spp) // N_WINDOWS
        rats = []
        for w in range(N_WINDOWS):
          a, b = base + w * wlen, base + (w + 1) * wlen
          rl, rr = _rms(L[a:b]), _rms(R[a:b])
          rat = (rl / rr) if rr > 1e-12 else float("inf")
          rats.append(rat)
          # the window's MEAN azimuth (the sweep is linear in az)
          f  = (w + 0.5) / float(N_WINDOWS)
          az = (-90.0 + 180.0 * f) if leg == 0 else (90.0 - 180.0 * f)
          idl = ideal_ratio(az)
          rel = abs(rat - idl) / idl
          check("move_ratio_leg%d_w%d" % (leg, w), rel <= MOVING_TOL)
          bits.append("mv%d.%d az=%+.1f ratio=%.4f ideal=%.4f rel=%.3f"
                      % (leg, w, az, rat, idl, rel))
        legs.append(rats)
      # (b) monotone up, then monotone back down
      check("move_monotone_up",
            all(legs[0][i] < legs[0][i + 1] for i in range(N_WINDOWS - 1)))
      check("move_monotone_down",
            all(legs[1][i] > legs[1][i + 1] for i in range(N_WINDOWS - 1)))
      # (c) no zipper: a per-pass gain applied as a step shows up as a slew
      #     outlier. the ceiling is a multiple of the tone's own natural
      #     per-sample slope (a 98Hz sine at this amplitude).
      mv = np.concatenate((L[start:], R[start:])).astype("f8")
      seg = slice(start, len(L))
      dmax = max(float(np.max(np.abs(np.diff(L[seg].astype("f8"))))),
                 float(np.max(np.abs(np.diff(R[seg].astype("f8"))))))
      peak = float(np.max(np.abs(mv)))
      natural = peak * 2.0 * math.pi * 98.0 / float(sr)
      check("move_no_zipper", dmax <= ZIPPER_FACTOR * natural)
      bits.append("slew: dmax=%.6g natural=%.6g ratio=%.2f"
                  % (dmax, natural, (dmax / natural) if natural > 0 else -1))
      # envelope continuity: no window-to-window RMS jump beyond the factor a
      #  30deg sweep of the decode can produce.
      env = []
      nw  = (len(L) - start) // (spp * 4)
      for w in range(nw):
        a, b = start + w * spp * 4, start + (w + 1) * spp * 4
        env.append(_rms(L[a:b]) + _rms(R[a:b]))
      jump = max((env[i + 1] / env[i]) if env[i] > 1e-12 else 1.0
                 for i in range(len(env) - 1)) if len(env) > 1 else 1.0
      check("move_envelope_continuous", jump <= ENV_JUMP_MAX)
      bits.append("env_jump=%.3f" % jump)

    ############################################################
    # 3 FALLOFF — the panner's model, G8's ordering style
    ############################################################
    fall = {}
    for d in FALLOFF_RADII:
      tag = "d%g" % d
      wav, _ = render("static", tag, az=0.0, dist=d)
      if wav is None:
        continue
      L, R, sr = _lr(wav)
      k = SKIP_STATIC * int(round(DT * sr))
      fall[d] = _rms(L[k:]) + _rms(R[k:])
      bits.append("fall d=%g rms=%.6g predicted_gain=%.5f"
                  % (d, fall[d], dist_gain(d)))
    if len(fall) == 3:
      near, mid, far = (fall[FALLOFF_RADII[0]], fall[FALLOFF_RADII[1]],
                        fall[FALLOFF_RADII[2]])
      check("fall_near_audible", near > RMS_FLOOR)
      check("fall_near_over_mid", (near / mid) > 1.4 if mid > 1e-12 else False)
      check("fall_mid_over_far", (mid / far) > 50.0 if far > 1e-12 else False)
      pn = dist_gain(FALLOFF_RADII[0]) / dist_gain(FALLOFF_RADII[1])
      check("fall_near_over_mid_analytic",
            abs((near / mid) - pn) / pn <= FALLOFF_TOL if mid > 1e-12 else False)
      bits.append("fall near/mid=%.3f (predicted %.3f) mid/far=%.1f"
                  % (near / mid if mid > 1e-12 else -1, pn,
                     mid / far if far > 1e-12 else -1))

    ############################################################
    # 6 ABSENT FIELD IS LOUD
    ############################################################
    rc, out = _spawn("nofield", os.path.join(tmp, "r_nofield.wav"))
    loud = (rc != 0) and ("soundfieldSend" in out) and ("SoundField" in out)
    check("absent_field_is_loud", loud)
    bits.append("nofield: rc=%d loud=%s" % (rc, loud))
    if not loud:
      print(out)

    failed = [name for name, ok in checks if not ok]
    detail = ("checks=%d failed=%s | %s"
              % (len(checks), ",".join(failed) if failed else "none",
                 " | ".join(bits)))
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
  ap.add_argument("--dist", type=float, default=1.0)
  args, _ = ap.parse_known_args()

  if args.role in ("static", "probe", "moving", "nofield"):
    _role_render(args.role, args.wav, args.fixture, args.az, args.dist)
  else:
    _main_driver()
