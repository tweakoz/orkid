#!/usr/bin/env ork.python
################################################################################
# test_soundfield_rotation — the SoundField listener-rotation canary (SF1).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   Turning the listener turns the ambisonic field the OTHER way, by exactly the
#   right amount and in the right direction. This is the one piece of the SF1
#   chain that a direction-only canary cannot see: SoundField applies the
#   listener's inverse rotation as a 3x3 on ACN 1..3 (W passthrough) before the
#   decode, and a stubbed-out (identity) rotator produces a perfectly plausible
#   render that is simply wrong.
#
#   THE SIGN CONVENTION, derived from the engine (not assumed):
#     * synth::_listener_matrix is the HEAD->WORLD pose: StochWavSoundEmitter.cpp
#       sets `_listener_matrix = viewMtx.inverse()` where viewMtx comes from
#       CameraData::computeViewMatrix -> Matrix44::lookAt -> glm::lookAtRH
#       (cmatrix4.hpp:1131). The inverse of a lookAtRH view matrix has columns
#       [right, up, -forward], so an identity pose faces world -Z with world +X
#       to the listener's RIGHT.
#     * +X being the listener's right is the engine's existing audio convention:
#       PANNER2D turns angle=-atan2(relx,relz) into a pan whose x component is
#       +relx/|r|, and x=+1 decodes hard right (panner.cpp:146..175, and the
#       caller at StochWavSoundEmitter.cpp:588..592).
#     * ambisonic azimuth is LEFT-positive (+Y is left), so az=-90 is the
#       listener's right.
#   Therefore: rotating the head pose about world +Y by +90deg (right-hand rule)
#   swings the listener's facing from world -Z to world -X, i.e. the listener
#   turns to their LEFT, and a source that was dead ahead ends up on their
#   RIGHT. A fixture at az 0 heard through a +90deg-yawed listener must produce
#   the SAME stereo signature as a fixture at az -90 heard through an
#   unrotated one.
#
#   Per check:
#     (a) the yawed render's L/R ratio matches the az=-90 reference render;
#     (b) the FULL signature matches — per-channel RMS, not just the ratio;
#     (c) the control render (az 0, no yaw) is centered and is far from the
#         yawed one, so (a)+(b) cannot be satisfied by a no-op rotator;
#     (d) every render streamed clean (zero ring underruns) and non-silent.
#
# HEADLESS (consent law): audio-only subsystems (no gpu), ORKID_AUDIO_IOCLASS
# pinned to STREAM in BOTH the driver spawn env and the child BEFORE the engine
# import, so no host audio device is ever opened and nothing is audible.
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

DT         = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
PREROLL    = 200           # update() calls in phase 0 (no audio generated)
SKIP_PUMPS = 20            # pumps discarded from the analysis window
N_ITERS    = 160           # total pumps per render (1.6s of audio)
TONE_HZ    = 100.0
FIXTURE_S  = 1.0           # 100 whole periods -> a seamless loop
MATCH_TOL  = 0.03          # relative, rotated-vs-reference
RMS_FLOOR  = 0.02

# name -> (fixture azimuth deg, listener yaw deg about world +Y)
CASES = (
    ("rot",  0.0, 90.0),   # dead-ahead source, listener turned to their left
    ("ref", -90.0, 0.0),   # the same thing said statically: source on the right
    ("ctrl", 0.0, 0.0),    # unrotated control: dead centre
)


################################################################################
# fixture synthesis — a 4ch float32 AmbiX WAV (ACN/SN3D), written by hand.
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
# CHILD
################################################################################

def _role_render(wav_path, fixture_path, az_deg, yaw_deg):
  os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"
  os.environ.pop("ORKID_DRM_MODE", None)

  write_ambix(fixture_path, az_deg)

  import orkengine.core as core                # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  def head_pose(yaw_rad):
    # HEAD->WORLD, columns [right, up, -forward, position]: a rotation about
    # world +Y by yaw (right-hand rule). yaw=0 faces world -Z with +X to the
    # right, which is the identity pose.
    m = core.mtx4()
    m.setColumn(0, core.vec4(math.cos(yaw_rad), 0.0, -math.sin(yaw_rad), 0.0))
    m.setColumn(1, core.vec4(0.0, 1.0, 0.0, 0.0))
    m.setColumn(2, core.vec4(math.sin(yaw_rad), 0.0, math.cos(yaw_rad), 0.0))
    m.setColumn(3, core.vec4(0.0, 0.0, 0.0, 1.0))
    return m

  class App(object):
    def __init__(self):
      self.phase     = 0
      self.pumped    = 0
      self.weight    = -1.0
      self.underruns = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="SoundFieldRotationTest",
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
        syn.listener_matrix = head_pose(math.radians(yaw_deg))
        self.field = S.SoundField.instance()
        self.probe = self.field.createProbe(fixture_path, True)
        # the listener sits AT the probe, well inside refDistance -> weight 1,
        # so distance can never contaminate the rotation measurement.
        self.field.setProbeParams(self.probe, core.vec3(0, 0, 0),
                                  refDistance=10.0, maxDistance=50.0,
                                  rolloff=1.0, gain=1.0)
        for _ in range(PREROLL):
          self.field.update(DT)
        self.weight = self.field.probeWeight(self.probe)
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
  print("CHILD_PUMPED=%d" % app.pumped, flush=True)
  print("CHILD_WEIGHT=%.6f" % app.weight, flush=True)
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
  return dict(channels=channels, sr=sr, floats=floats)


def _spawn_render(wav_path, fixture_path, az_deg, yaw_deg, timeout=240):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)             # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"       # no host audio device, ever
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path,
         "--fixture", fixture_path, "--az", repr(az_deg), "--yaw", repr(yaw_deg)]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


def _relerr(a, b):
  denom = max(abs(a), abs(b), 1e-9)
  return abs(a - b) / denom


################################################################################
# DRIVER
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  tmp = tempfile.mkdtemp(prefix="sfrotation_")
  checks = []
  meas = {}
  bits = []

  def check(name, ok):
    checks.append((name, bool(ok)))

  try:
    for name, az, yaw in CASES:
      wav = os.path.join(tmp, "render_%s.wav" % name)
      fix = os.path.join(tmp, "fixture_%s.wav" % name)
      try:
        rc, out = _spawn_render(wav, fix, az, yaw)
      except subprocess.TimeoutExpired as e:
        print((e.stdout or "") + (e.stderr or ""))
        sys.exit(verdict(False, "child render '%s' timed out (possible wedge)" % name))
      print(out)

      if rc != 0 or not os.path.isfile(wav):
        check("render_%s" % name, False)
        bits.append("%s=RENDERFAIL(rc=%d)" % (name, rc))
        continue

      check("stream_clean_%s" % name, _grep(out, "CHILD_UNDERRUNS") == "0")

      wa = _parse_wav(wav)
      fl = wa["floats"]
      L  = fl[0::2]
      R  = fl[1::2]
      k  = SKIP_PUMPS * int(round(DT * wa["sr"]))
      rmsL = float(np.sqrt(np.mean(L[k:] * L[k:])))
      rmsR = float(np.sqrt(np.mean(R[k:] * R[k:])))
      check("nonsilent_%s" % name, max(rmsL, rmsR) > RMS_FLOOR)
      ratio = (rmsL / rmsR) if rmsR > 1e-9 else float("inf")
      meas[name] = (rmsL, rmsR, ratio)
      bits.append("%s(az=%g yaw=%g): L=%.5f R=%.5f ratio=%.4f"
                  % (name, az, yaw, rmsL, rmsR, ratio))

    have = all(n in meas for n, _, _ in CASES)
    check("all_renders", have)

    if have:
      rL, rR, rRatio = meas["rot"]
      fL, fR, fRatio = meas["ref"]
      cL, cR, cRatio = meas["ctrl"]
      # (a) the yawed field lands on the reference azimuth
      check("rotated_matches_reference_ratio", _relerr(rRatio, fRatio) <= MATCH_TOL)
      # (b) the whole signature, not just its ratio
      check("rotated_matches_reference_L", _relerr(rL, fL) <= MATCH_TOL)
      check("rotated_matches_reference_R", _relerr(rR, fR) <= MATCH_TOL)
      # (c) an identity rotator would land on the control instead
      check("control_is_centered", abs(cRatio - 1.0) <= MATCH_TOL)
      check("rotation_actually_moved_it", (rRatio < 0.5) and (cRatio / max(rRatio, 1e-9) > 2.0))
      bits.append("relerr(ratio)=%.4f relerr(L)=%.4f relerr(R)=%.4f"
                  % (_relerr(rRatio, fRatio), _relerr(rL, fL), _relerr(rR, fR)))

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
  ap.add_argument("--fixture", default=None)
  ap.add_argument("--az", type=float, default=0.0)
  ap.add_argument("--yaw", type=float, default=0.0)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.wav, args.fixture, args.az, args.yaw)
  else:
    _main_driver()
