#!/usr/bin/env ork.python
################################################################################
# test_hypersound_arith — golden arithmetic of the hypersound DSL (slice A1a).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   The `a + b` and `k * a` operators of the S.* DSL lower to dsp topology whose
#   OUTPUT SATISFIES THE ARITHMETIC THEY SPELL — checked as EXACT float
#   relationships, not eyeballed RMS, in the style of test_audio_insert_forkjoin:
#
#     single  S.saw()                            -> x
#     double  S.saw() + S.saw()                  -> x+x  == 2*single EXACTLY
#     shared  x = S.saw(); x + x                 -> x+x  == 2*single EXACTLY
#     half    x = S.saw(); 0.5*(x + x)           -> x     BYTEWISE == single
#
#   (x+x and 0.5*(x+x) are exact in binary floating point, which is what lets
#   these assert bit equality instead of a tolerance.)
#
#   `double` and `shared` also probe the DAG's SHARING RULE from two sides:
#   two separate S.saw() calls must lower to TWO oscillator blocks that happen to
#   agree sample for sample, while one node referenced twice must lower to ONE
#   block whose channel is read twice — so the block lists are asserted too, not
#   just the audio. If node identity ever stopped being the sharing key, `shared`
#   would grow a second oscillator and the counts would catch it even though the
#   arithmetic still passed.
#
# PRECONDITION, ASSERTED: |single| <= 1.0 everywhere. DspFxMixSum2 clips its sum
#   to +/-2 and DspAmpMonoGain soft-saturates at unity, so an oscillator that
#   overshot full scale would break the exactness of BOTH claims. A0 measures the
#   peak so a failure reads as "the oscillator got louder", not "the DSL broke".
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure stdlib
# + numpy and spawns each engine-booting render as its OWN ork.python subprocess,
# with ORKID_DRM_MODE stripped from the child env.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
# NOTE: subprocess / struct / hashlib / tempfile are imported lazily inside the
# driver-only functions. hashlib pre-loads the SYSTEM libcrypto, which then makes
# the staged libssl fail its OPENSSL_3.3.0 version check when the CHILD later
# imports orkengine.core — so the child path must touch none of them first.

# prepend THIS checkout's scripts dir so ork.testing / ork.hypergraph resolve
# from the same tree.
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

ENV_SEGMENTS = [("atk", 0.01, 1.0, 0.5),
                ("sus", 1.0, 1.0, 0.5),
                ("rel", 0.2, 0.0, 0.5)]
ENV_SUSTAIN = 1


################################################################################
# CHILD — one engine boot per scenario.
################################################################################

def _patch_classes():
  from ork.hypergraph.sound import S, SoundPatch

  def env():
    return S.env(ENV_SEGMENTS, sustain=ENV_SUSTAIN)

  class Single(SoundPatch):
    def __init__(self):
      self.output(S.saw(), amp=env())

  class Double(SoundPatch):
    """Two independent oscillator NODES -> two blocks summed."""
    def __init__(self):
      self.output(S.saw() + S.saw(), amp=env())

  class Shared(SoundPatch):
    """ONE oscillator node referenced twice -> one block, its channel read twice."""
    def __init__(self):
      sig = S.saw()
      self.output(sig + sig, amp=env())

  class Half(SoundPatch):
    def __init__(self):
      sig = S.saw()
      self.output(0.5 * (sig + sig), amp=env())

  return dict(single=Single, double=Double, shared=Shared, half=Half)


def _role_render(scenario, wav_path):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from ork.hypergraph.sound import materialize_sound_patch

  patch_class = _patch_classes()[scenario]
  notes = []

  class App(object):
    def __init__(self):
      self.phase = 0            # 0=await synth, 1=pumping
      self.pumped = 0
      self.total_frames = 0
      self.sr = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="HyperSoundArithTest",
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
        self.snd = materialize_sound_patch(patch_class)
        notes.append("CHILD_BLOCKS=%s" % ",".join(self.snd.plan.block_names()))
        notes.append("CHILD_STAGES=%d" % len(self.snd.plan.stages))
        syn.programbus.uiprogram = self.snd.program
        self.voice = syn.keyOn(60, 127, self.snd.program, None)
        self.phase = 1
        return
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
  for line in notes:
    print(line, flush=True)
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


def _spawn_render(scenario, wav_path, timeout=300):
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
# DRIVER
################################################################################

def _main_driver():
  import subprocess
  import tempfile
  import shutil
  import numpy as np
  from ork.testing import verdict

  renders = [("single", "single"), ("single_dup", "single"),
             ("double", "double"), ("shared", "shared"), ("half", "half")]

  tmp = tempfile.mkdtemp(prefix="hypersound_arith_")
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

  a0 = a1 = a2 = a3 = a4 = a5 = False
  detail_extra = ""
  if all_rc_ok and all_exist:
    W = {label: _parse_wav(wavs[label]) for label, _ in renders}
    ch = {label: W[label]["floats"] for label, _ in renders}
    expected = N_ITERS * int(round(DT * W["single"]["sr"])) if W["single"]["sr"] else -1
    child_frames = int(_grep(outs["single"], "CHILD_TOTAL_FRAMES") or -1)
    rms_single = float(np.sqrt(np.mean(ch["single"] * ch["single"])))
    peak_single = float(np.max(np.abs(ch["single"])))

    # A0: real, equally-sized, non-silent renders AND the <=1.0 headroom the
    #     exactness of A1/A2/A3 depends on
    same_len = len(set(len(ch[label]) for label, _ in renders)) == 1
    a0 = (same_len
          and all(W[label]["channels"] == 2 and W[label]["is_float"] for label, _ in renders)
          and W["single"]["frames"] == expected and W["single"]["frames"] == child_frames
          and expected > 0 and rms_single > RMS_FLOOR and peak_single <= 1.0)

    # A1: two oscillator nodes summed == exactly twice one of them (and NOT 1x,
    #     which is what a dropped operand would look like)
    d_double = _maxreldiff(ch["double"], ch["single"], 2.0)
    d_double_null = _maxreldiff(ch["double"], ch["single"], 1.0)
    a1 = (d_double == 0.0) and (d_double_null > 0.0)

    # A2: one node referenced twice sums to the same exact 2x
    d_shared = _maxreldiff(ch["shared"], ch["single"], 2.0)
    a2 = (d_shared == 0.0)

    # A3: 0.5*(x+x) restores x bit for bit
    a3 = (_sha256(wavs["half"]) == _sha256(wavs["single"]))

    # A4: double-run byte determinism of the DSL render path
    a4 = (_sha256(wavs["single"]) == _sha256(wavs["single_dup"]))

    # A5: the SHARING RULE, structurally — two S.saw() calls lower to two saw
    #     blocks, one node referenced twice lowers to exactly one
    blocks_double = (_grep(outs["double"], "CHILD_BLOCKS") or "").split(",")
    blocks_shared = (_grep(outs["shared"], "CHILD_BLOCKS") or "").split(",")
    a5 = (sum(1 for b in blocks_double if b.startswith("saw_")) == 2
          and sum(1 for b in blocks_shared if b.startswith("saw_")) == 1
          and sum(1 for b in blocks_shared if b.startswith("sum2_")) == 1)

    detail_extra = ("frames=%d expected=%d rms=%.4f peak=%.6f d_double=%.3e "
                    "d_double_null=%.3e d_shared=%.3e blocks_double=%s blocks_shared=%s"
                    % (W["single"]["frames"], expected, rms_single, peak_single,
                       d_double, d_double_null, d_shared,
                       "|".join(blocks_double), "|".join(blocks_shared)))

  passed = all_rc_ok and all_exist and a0 and a1 and a2 and a3 and a4 and a5
  detail = ("rc_ok=%s exists=%s A0_render_headroom=%s A1_sum_two_nodes=%s "
            "A2_sum_shared_node=%s A3_half_of_double=%s A4_determinism=%s "
            "A5_sharing_rule=%s %s"
            % (all_rc_ok, all_exist, a0, a1, a2, a3, a4, a5, detail_extra))
  shutil.rmtree(tmp, ignore_errors=True)
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--scenario", default=None)
  ap.add_argument("--wav", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.scenario, args.wav)
  else:
    _main_driver()
