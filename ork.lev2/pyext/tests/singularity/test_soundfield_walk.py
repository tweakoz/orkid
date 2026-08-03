#!/usr/bin/env ork.python
################################################################################
# test_soundfield_walk — the SoundField distance-weighting + determinism canary
# (SF1).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   (1) CROSSFADE. Two probes 20m apart, each looping its own distinctive tone,
#       are weighted by the radial falloff solve (smoothstep between refDistance
#       and maxDistance, ~250ms hysteresis slew) as the listener is stepped
#       along the line between them in five segments. Per segment, the decoded
#       stereo is band-analysed at each probe's tone frequency: the near probe's
#       amplitude must fall MONOTONICALLY and the far probe's must rise
#       monotonically, by a real margin at every step. A weight solve that
#       ignored distance, or that normalized the wrong way, holds them flat.
#       Both fixtures are authored at azimuth 0 so the decode contributes no
#       lateral asymmetry and the only thing under test is the weighting.
#   (2) SLOT SCHEDULING under motion: probe B carries weight 0 at the first
#       station, so it holds NO streaming slot there and must acquire, prime and
#       fade in mid-run. That path is exercised on every run of this test.
#   (3) BYTE DETERMINISM (WAV determinism law). Two independent child renders of
#       the identical script must produce BYTE-IDENTICAL WAVs. This is the check
#       that fences the feeder thread: the probe rings are filled by a
#       background thread, and if the audio thread could ever observe a
#       different fill state between two runs the renders would diverge.
#
# HEADLESS (consent law): audio-only subsystems (no gpu), ORKID_AUDIO_IOCLASS
# pinned to STREAM in BOTH the driver spawn env and the child BEFORE the engine
# import, so no host audio device is ever opened and nothing is audible.
#
# BOUNDED SELF-EXIT: the child runs a fixed pump schedule and exits; the driver
# additionally bounds each spawn with a subprocess timeout.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
import struct
import argparse
# NOTE: subprocess / hashlib / tempfile / numpy are imported lazily in the
# driver-only paths — the child must not pre-load hashlib/ssl-adjacent stdlib
# before orkengine.core (see the note in test_audio_wav_render.py).

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT             = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
PUMPS_PER_SEG  = 150           # 1.5s per station == 6 slew time-constants
MEASURE_PUMPS  = 60            # the last 0.6s of each station is the window
PREROLL        = 200           # update() calls before any audio time elapses

# 480 samples/period at 100Hz and 160 at 300Hz: a 60-pump (28800 sample) window
# is a whole number of periods of BOTH, so a rectangular-window DFT at either
# frequency isolates that tone exactly, whatever phase the loop restarted on.
TONE_A_HZ      = 100.0
TONE_B_HZ      = 300.0
FIXTURE_S      = 1.0           # whole periods of both tones -> seamless loops

PROBE_A_POS    = (0.0, 0.0, 0.0)
PROBE_B_POS    = (20.0, 0.0, 0.0)
REF_DIST       = 2.0
MAX_DIST       = 18.0
# stations along the A->B line. the smoothstep is symmetric about the midpoint,
# so the two weights sum to exactly 1 at every station and the tier
# normalization (sum>1) never engages — this is a pure crossfade.
STATIONS       = (0.0, 5.0, 10.0, 15.0, 20.0)

MIN_STEP_REL   = 0.05          # each crossfade step must move by >=5% relative
RMS_FLOOR      = 0.02


################################################################################
# fixture synthesis — a 4ch float32 AmbiX WAV (ACN/SN3D), written by hand.
################################################################################

def write_ambix(path, az_deg, freq, seconds=FIXTURE_S, sr=48000, amp=0.5):
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
# CHILD — one engine boot: two probes, a scripted five-station walk, one WAV.
################################################################################

def _role_render(wav_path, fixture_a, fixture_b):
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  write_ambix(fixture_a, 0.0, TONE_A_HZ)
  write_ambix(fixture_b, 0.0, TONE_B_HZ)

  import orkengine.core as core                # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  def head_pose(x):
    # identity orientation (facing world -Z), translated along world +X.
    m = core.mtx4()
    m.setColumn(3, core.vec4(x, 0.0, 0.0, 1.0))
    return m

  class App(object):
    def __init__(self):
      self.phase     = 0
      self.pumped    = 0
      self.station   = -1
      self.underruns = -1
      self.weights   = []      # (wA, wB) sampled at the end of every station
      self.ezapp = OrkEzApp.create(
          self,
          name="SoundFieldWalkTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                     # -> StrAudioDevice SYNC
          wav_output_path=wav_path,
          freerun=True)

    def _place(self, station_index):
      syn = self.ezapp.audio_synth
      syn.listener_matrix = head_pose(STATIONS[station_index])
      self.station = station_index

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      if self.phase == 0:
        syn.masterGain = 1.0
        self.field  = S.SoundField.instance()
        self.probeA = self.field.createProbe(fixture_a, True)
        self.probeB = self.field.createProbe(fixture_b, True)
        self.field.setProbeParams(self.probeA, core.vec3(*PROBE_A_POS),
                                  refDistance=REF_DIST, maxDistance=MAX_DIST,
                                  rolloff=1.0, gain=1.0)
        self.field.setProbeParams(self.probeB, core.vec3(*PROBE_B_POS),
                                  refDistance=REF_DIST, maxDistance=MAX_DIST,
                                  rolloff=1.0, gain=1.0)
        self._place(0)
        # settle station 0 before any audio time elapses, so segment 0 is a
        # steady state rather than a fade-in.
        for _ in range(PREROLL):
          self.field.update(DT)
        self.phase = 1
        return

      seg = self.pumped // PUMPS_PER_SEG
      if seg != self.station:
        self.weights.append((self.field.probeWeight(self.probeA),
                             self.field.probeWeight(self.probeB)))
        self._place(seg)

      self.field.update(DT)
      dev.advanceTime(DT)
      self.pumped += 1

      if self.pumped >= PUMPS_PER_SEG * len(STATIONS):
        self.weights.append((self.field.probeWeight(self.probeA),
                             self.field.probeWeight(self.probeB)))
        self.underruns = self.field.underrunCount
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_UNDERRUNS=%d" % app.underruns, flush=True)
  print("CHILD_WEIGHTS=%s"
        % ";".join("%.6f,%.6f" % (a, b) for a, b in app.weights), flush=True)
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


def _tone_amplitude(x, freq, sr):
  # single-bin DFT. the window is a whole number of periods of every tone in
  # play, so this isolates one probe's contribution from the other's exactly.
  import numpy as np
  n = x.size
  t = np.arange(n, dtype=np.float64)
  ph = -2.0j * np.pi * freq * t / float(sr)
  return float(2.0 * abs(np.dot(x.astype(np.float64), np.exp(ph))) / n)


def _spawn_render(wav_path, fixture_a, fixture_b, timeout=300):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"       # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path,
         "--fixtureA", fixture_a, "--fixtureB", fixture_b]
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
# DRIVER
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="sfwalk_")
  checks = []
  bits = []

  def check(name, ok):
    checks.append((name, bool(ok)))

  try:
    wavA = os.path.join(tmp, "walkA.wav")
    wavB = os.path.join(tmp, "walkB.wav")
    fixA = os.path.join(tmp, "fixture_a.wav")
    fixB = os.path.join(tmp, "fixture_b.wav")

    try:
      rc1, out1 = _spawn_render(wavA, fixA, fixB)
      rc2, out2 = _spawn_render(wavB, fixA, fixB)
    except subprocess.TimeoutExpired as e:
      print((e.stdout or "") + (e.stderr or ""))
      sys.exit(verdict(False, "child render timed out (possible wedge)"))

    print(out1)
    print(out2)

    check("render1_rc", rc1 == 0 and os.path.isfile(wavA))
    check("render2_rc", rc2 == 0 and os.path.isfile(wavB))
    if not (rc1 == 0 and rc2 == 0 and os.path.isfile(wavA) and os.path.isfile(wavB)):
      sys.exit(verdict(False, "render failed rc1=%d rc2=%d" % (rc1, rc2)))

    check("stream_clean", _grep(out1, "CHILD_UNDERRUNS") == "0"
                          and _grep(out2, "CHILD_UNDERRUNS") == "0")

    # (3) byte determinism
    shA, shB = _sha256(wavA), _sha256(wavB)
    check("byte_determinism", shA == shB)
    bits.append("sha=%s/%s" % (shA[:12], shB[:12]))

    wa  = _parse_wav(wavA)
    sr  = wa["sr"]
    fpp = int(round(DT * sr))
    fl  = wa["floats"]
    L   = fl[0::2]
    check("frames_exact", wa["frames"] == fpp * PUMPS_PER_SEG * len(STATIONS))
    check("nonsilent", float(np.sqrt(np.mean(L * L))) > RMS_FLOOR)

    ampA = []
    ampB = []
    for s in range(len(STATIONS)):
      end   = (s + 1) * PUMPS_PER_SEG * fpp
      start = end - MEASURE_PUMPS * fpp
      win   = L[start:end]
      ampA.append(_tone_amplitude(win, TONE_A_HZ, sr))
      ampB.append(_tone_amplitude(win, TONE_B_HZ, sr))
    bits.append("ampA=" + ",".join("%.5f" % a for a in ampA))
    bits.append("ampB=" + ",".join("%.5f" % a for a in ampB))
    bits.append("weights=" + (_grep(out1, "CHILD_WEIGHTS") or "?"))

    # (1) monotone crossfade, with a real margin at every step
    downA = all(ampA[i + 1] < ampA[i] * (1.0 - MIN_STEP_REL) for i in range(len(ampA) - 1))
    upB   = all(ampB[i + 1] > ampB[i] + MIN_STEP_REL * max(ampB[i], 1e-4)
                for i in range(len(ampB) - 1))
    check("probeA_monotone_down", downA)
    check("probeB_monotone_up", upB)
    # the crossfade must actually cross: A dominates at the first station and B
    # at the last.
    check("crossfade_endpoints", ampA[0] > ampB[0] * 4.0 and ampB[-1] > ampA[-1] * 4.0)
    # (2) probe B holds no slot at station 0 (weight exactly zero at 20m with
    # maxDistance 18) and must acquire one mid-run.
    check("probeB_starts_parked", ampB[0] < 0.01 * ampB[-1])

    failed = [n for n, ok in checks if not ok]
    detail = ("checks=%d failed=%s | %s"
              % (len(checks), ",".join(failed) if failed else "none", " | ".join(bits)))
    sys.exit(verdict(len(failed) == 0, detail))
  finally:
    shutil.rmtree(tmp, ignore_errors=True)


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  ap.add_argument("--fixtureA", default=None)
  ap.add_argument("--fixtureB", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.wav, args.fixtureA, args.fixtureB)
  else:
    _main_driver()
