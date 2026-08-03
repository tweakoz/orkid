#!/usr/bin/env ork.python
################################################################################
# test_hypersound_basic — the hypersound DSL lowers EXACTLY (slice A1a).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   G1 A DSL-authored patch renders BYTE-IDENTICALLY twice.
#   G2 THE STRONG GATE — that same DSL patch renders BYTE-IDENTICALLY to the
#      RAW-PYEXT program of test_audio_wav_render._build_sine_program (copied in
#      verbatim below). The DSL allocates different dsp CHANNELS and packs its
#      stages differently, so byte equality is a claim about the LOWERING: the
#      emitter produced an arithmetically identical voice, not merely a similar
#      one. If a future emitter change perturbs pitch/env/amp wiring at all, the
#      hashes diverge.
#   G3 TRACE-ONCE — materialize_sound_patch() called twice returns the SAME
#      MaterializedSound, the plan's stage shape is unchanged, the engine-side
#      LayerData reports the same stage_count (i.e. nothing was appended a second
#      time), and the twice-materialized program renders byte-identically to the
#      once-materialized one.
#   G4 S.p() DECLARATION — a declared param lands in the patch manifest with its
#      {default, unit} intact AND its default is lowered as the dsp param's
#      initial coarse value, read back off the engine object. Runtime mutation
#      raises NotImplementedError naming A2 rather than silently doing nothing.
#   G5 THE S.block() ESCAPE HATCH — a block with no curated verb (NonlinShaper)
#      appends through the same channel machinery, renders deterministically, and
#      audibly CHANGES the signal (so the block really ran, not a no-op append).
#   G6 CAPS — a 33-block stage and a 17-stage layer raise SoundCapsError from
#      PYTHON, before any engine object exists, instead of tripping the C++
#      OrkAssert in AlgData::appendStage or silently overrunning the unchecked
#      DspStageData::appendDspBlock (defect #16). The boundary cases (32 blocks /
#      16 stages) must still plan cleanly, so this is a threshold, not a floor.
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure stdlib
# + numpy and spawns each engine-booting render as its OWN ork.python subprocess,
# with ORKID_DRM_MODE stripped from the child env. The G6 caps child boots no
# app at all — hypersound PLANNING is pure python by construction.
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

# the amp envelope both the DSL patch and the raw-pyext program use, segment for
# segment — G2 is only meaningful if the two voices differ ONLY in how they were
# authored.
ENV_SEGMENTS = [("atk", 0.01, 1.0, 0.5),
                ("sus", 1.0, 1.0, 0.5),
                ("rel", 0.2, 0.0, 0.5)]
ENV_SUSTAIN = 1

PARAM_CUTOFF = 900.0
SHAPER_AMOUNT = 0.25


################################################################################
# CHILD — one engine boot per render variant.
################################################################################

def _build_sine_program(S):
  # VERBATIM from test_audio_wav_render.py::_build_sine_program — the raw-pyext
  # reference topology G2 compares against. Do not "improve" it: its value here
  # is that it is the hand-authored program the DSL must reproduce exactly.
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


def _patch_classes():
  """The DSL patches under test. Imported lazily so the module-level import of
  this file stays engine-free for the driver."""
  from ork.hypergraph.sound import S, SoundPatch

  def env():
    return S.env(ENV_SEGMENTS, sustain=ENV_SUSTAIN)

  class SineVoice(SoundPatch):
    """The DSL spelling of _build_sine_program: one sine through the amp env."""
    def __init__(self):
      self.output(S.sine(), amp=env())

  class ParamVoice(SoundPatch):
    def __init__(self, cutoff=PARAM_CUTOFF):
      self.output(S.lowpass2(S.sine(), cutoff=S.p("cutoff", cutoff, unit="hz")),
                  amp=env())

  class HatchVoice(SoundPatch):
    """No curated verb exists for DspNonlinShaper or SynAsr — the S.block /
    S.controller escape hatches reach both anyway. The Asr is attached at
    src1scale 0 / src1bias 0, so it contributes EXACTLY zero to `amount`: the
    render stays byte-comparable to a hatch-free one while still proving the
    controller was created, named and bound."""
    def __init__(self):
      asr = S.controller("Asr", label="asr0")
      self.output(S.block("NonlinShaper", S.sine(), nin=1, nout=1,
                          amount=S.mod(asr, scale=0.0, bias=0.0, base=SHAPER_AMOUNT),
                          label="shaper"),
                  amp=env())

  return SineVoice, ParamVoice, HatchVoice


def _role_render(variant, wav_path):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S
  from ork.hypergraph.sound import materialize_sound_patch

  SineVoice, ParamVoice, HatchVoice = _patch_classes()
  notes = []                            # extra CHILD_* lines to emit at exit

  def build():
    if variant == "manual":
      return _build_sine_program(S)
    if variant == "dsl":
      snd = materialize_sound_patch(SineVoice)
      return snd, snd.program
    if variant == "twice":
      first = materialize_sound_patch(SineVoice)
      rep_first = repr(first.layer)
      second = materialize_sound_patch(SineVoice)
      notes.append("CHILD_IDENTITY=%d" % int(first is second))
      notes.append("CHILD_SHAPE_SAME=%d"
                   % int(first.plan.stage_shape() == second.plan.stage_shape()))
      notes.append("CHILD_LAYER_REPR_SAME=%d" % int(rep_first == repr(second.layer)))
      notes.append("CHILD_LAYER_REPR=%s" % repr(second.layer).replace(" ", ""))
      return second, second.program
    if variant == "param":
      snd = materialize_sound_patch(ParamVoice)
      entry = snd.manifest.get("cutoff", {})
      notes.append("CHILD_MANIFEST=%s|%r|%s|%d"
                   % (entry.get("name"), entry.get("default"), entry.get("unit"),
                      len(entry.get("sites", []))))
      # the DECLARED default must have been lowered onto the real dsp param
      block_name, param_name = entry["sites"][0]
      stage_name = next(st.name for st in snd.plan.stages
                        if any(b.name == block_name for b in st.blocks))
      blk = snd.layer.stage(stage_name).dspblockByName(block_name)
      notes.append("CHILD_COARSE=%r" % blk.paramByName(param_name).coarse)
      try:
        snd.setParam("cutoff", 1234.0)
        notes.append("CHILD_SETPARAM=NOT_RAISED")
      except NotImplementedError as err:
        notes.append("CHILD_SETPARAM=%d" % int("A2" in str(err)))
      return snd, snd.program
    if variant == "hatch":
      snd = materialize_sound_patch(HatchVoice)
      notes.append("CHILD_HATCH_BLOCKS=%s" % ",".join(snd.plan.block_names()))
      notes.append("CHILD_HATCH_CTRLS=%s"
                   % ",".join("%s:%s" % (c.name, c.classname) for c in snd.plan.controllers))
      return snd, snd.program
    raise ValueError("unknown variant %r" % variant)

  class App(object):
    def __init__(self):
      self.phase = 0            # 0=await synth, 1=pumping
      self.pumped = 0
      self.total_frames = 0
      self.sr = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="HyperSoundBasicTest",
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
        self.keepalive, self.prog = build()
        syn.programbus.uiprogram = self.prog
        self.voice = syn.keyOn(60, 127, self.prog, None)
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
# CHILD — the caps / fail-loud role. Boots NO app: hypersound planning is pure
# python, so every ceiling is decided before an engine object could exist.
################################################################################

def _role_check():
  import orkengine.core                 # core FIRST (ork.hypergraph pulls lev2)
  from ork.hypergraph.sound import (S, SoundPatch, SoundCapsError, SoundDslError,
                                    plan_sound_patch)

  def env():
    return S.env(ENV_SEGMENTS, sustain=ENV_SUSTAIN)

  def chain_patch(n):
    class Chain(SoundPatch):
      def __init__(self):
        sig = S.saw()
        for i in range(n):
          sig = S.lowpass2(sig, cutoff=1000.0 + i)
        self.output(sig, amp=env())
    return Chain

  def tree_patch(n):
    class Tree(SoundPatch):
      def __init__(self):
        acc = S.saw()
        for i in range(n):
          acc = acc + S.saw(pitch=float(i))
        self.output(acc, amp=env())
    return Tree

  def classify(fn):
    try:
      fn()
      return "ok"
    except Exception as err:               # noqa: BLE001 — the classification IS the check
      return type(err).__name__

  def plan(cls):
    return lambda: plan_sound_patch(cls())

  # a unary chain shares ONE stage, so it is the blocks-per-stage ceiling that
  # bites: pitch + saw + N filters. N=30 -> 32 blocks (legal), N=31 -> 33.
  print("CHECK_BLOCKS_32=%s" % classify(plan(chain_patch(30))), flush=True)
  print("CHECK_BLOCKS_33=%s" % classify(plan(chain_patch(31))), flush=True)
  # every summed branch mints two stages, so N sums -> 2N+2 stages incl. AMP.
  print("CHECK_STAGES_16=%s" % classify(plan(tree_patch(7))), flush=True)
  print("CHECK_STAGES_17=%s" % classify(plan(tree_patch(8))), flush=True)

  # the counts the ceilings were measured against, so a threshold shift is visible
  print("CHECK_BLOCKS_32_N=%d" % len(plan_sound_patch(chain_patch(30)()).stages[0].blocks),
        flush=True)
  print("CHECK_STAGES_16_N=%d" % len(plan_sound_patch(tree_patch(7)()).stages), flush=True)

  # fail-loud surfaces that must never degrade to a silent nullptr / no-op
  print("CHECK_BAD_BLOCK_CLASS=%s"
        % classify(lambda: S.block("NoSuchBlockClass", S.sine())), flush=True)
  print("CHECK_BAD_CTRL_CLASS=%s"
        % classify(lambda: S.controller("NoSuchController")), flush=True)

  class NoAmp(SoundPatch):
    def __init__(self):
      self.output(S.sine())
  print("CHECK_NO_AMP=%s" % classify(lambda: NoAmp()), flush=True)

  class SharedPwm(SoundPatch):
    def __init__(self):
      shared = S.saw()
      self.output(S.pwm(shared) + shared, amp=env())
  print("CHECK_PWM_SHARED=%s" % classify(plan(SharedPwm)), flush=True)

  class BadParam(SoundPatch):
    def __init__(self):
      self.output(S.sine(nosuchparam=1.0), amp=env())
  print("CHECK_UNKNOWN_PARAM_PLANS=%s" % classify(plan(BadParam)), flush=True)

  print("CHECK_PARAM_SET=%s" % classify(lambda: S.p("x", 1.0).set(2.0)), flush=True)
  print("CHECK_PARAM_ARITH=%s" % classify(lambda: S.p("x", 1.0) * 2.0), flush=True)
  sys.exit(0)


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


def _spawn(args, timeout=300):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF] + args
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

  renders = [("dsl", "dsl"), ("dsl_dup", "dsl"), ("manual", "manual"),
             ("twice", "twice"), ("param", "param"),
             ("hatch", "hatch"), ("hatch_dup", "hatch")]

  tmp = tempfile.mkdtemp(prefix="hypersound_")
  wavs = {label: os.path.join(tmp, label + ".wav") for label, _ in renders}
  rcs = {}
  outs = {}

  try:
    for label, variant in renders:
      rcs[label], outs[label] = _spawn(["--role", "render", "--variant", variant,
                                        "--wav", wavs[label]])
    rc_check, out_check = _spawn(["--role", "check"], timeout=180)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child timed out (possible wedge)"))

  for label, _ in renders:
    print("---- %s (rc=%d) ----" % (label, rcs[label]))
    print(outs[label])
  print("---- check (rc=%d) ----" % rc_check)
  print(out_check)

  all_rc_ok = all(rc == 0 for rc in rcs.values()) and rc_check == 0
  all_exist = all(os.path.isfile(p) for p in wavs.values())

  g0 = g1 = g2 = g3 = g4 = g5 = g6 = False
  detail_extra = ""
  if all_rc_ok and all_exist:
    W = {label: _parse_wav(wavs[label]) for label, _ in renders}
    ch = {label: W[label]["floats"] for label, _ in renders}
    expected = N_ITERS * int(round(DT * W["dsl"]["sr"])) if W["dsl"]["sr"] else -1
    child_frames = int(_grep(outs["dsl"], "CHILD_TOTAL_FRAMES") or -1)
    rms_dsl = float(np.sqrt(np.mean(ch["dsl"] * ch["dsl"])))

    # G0: the DSL render is a real, non-silent, exactly-sized stereo float WAV
    g0 = (W["dsl"]["channels"] == 2 and W["dsl"]["is_float"]
          and W["dsl"]["frames"] == expected and W["dsl"]["frames"] == child_frames
          and expected > 0 and rms_dsl > RMS_FLOOR)

    # G1: two independent DSL renders are byte-identical
    g1 = (_sha256(wavs["dsl"]) == _sha256(wavs["dsl_dup"]))

    # G2: THE STRONG GATE — DSL == raw pyext, byte for byte
    g2 = (_sha256(wavs["dsl"]) == _sha256(wavs["manual"]))

    # G3: trace-once — same object, same plan shape, same engine-side stage count,
    #     and the twice-materialized program renders identically
    g3 = (_grep(outs["twice"], "CHILD_IDENTITY") == "1"
          and _grep(outs["twice"], "CHILD_SHAPE_SAME") == "1"
          and _grep(outs["twice"], "CHILD_LAYER_REPR_SAME") == "1"
          and _sha256(wavs["twice"]) == _sha256(wavs["dsl"]))

    # G4: S.p declaration -> manifest + lowered default + A2-named mutation error
    manifest = _grep(outs["param"], "CHILD_MANIFEST")
    coarse = _grep(outs["param"], "CHILD_COARSE")
    g4 = (manifest == "cutoff|%r|hz|1" % PARAM_CUTOFF
          and coarse == repr(PARAM_CUTOFF)
          and _grep(outs["param"], "CHILD_SETPARAM") == "1")

    # G5: the S.block / S.controller hatches appended real, deterministic,
    #     AUDIBLE engine objects with no curated verb behind them
    rms_hatch = float(np.sqrt(np.mean(ch["hatch"] * ch["hatch"])))
    g5 = (_sha256(wavs["hatch"]) == _sha256(wavs["hatch_dup"])
          and _sha256(wavs["hatch"]) != _sha256(wavs["dsl"])
          and rms_hatch > RMS_FLOOR
          and "shaper" in (_grep(outs["hatch"], "CHILD_HATCH_BLOCKS") or "")
          and "asr0:Asr" in (_grep(outs["hatch"], "CHILD_HATCH_CTRLS") or ""))

    # G6: ceilings raise python errors at the threshold, not one block early/late
    g6 = (_grep(out_check, "CHECK_BLOCKS_32") == "ok"
          and _grep(out_check, "CHECK_BLOCKS_33") == "SoundCapsError"
          and _grep(out_check, "CHECK_STAGES_16") == "ok"
          and _grep(out_check, "CHECK_STAGES_17") == "SoundCapsError"
          and _grep(out_check, "CHECK_BLOCKS_32_N") == "32"
          and _grep(out_check, "CHECK_STAGES_16_N") == "16"
          and _grep(out_check, "CHECK_BAD_BLOCK_CLASS") == "SoundDslError"
          and _grep(out_check, "CHECK_BAD_CTRL_CLASS") == "SoundDslError"
          and _grep(out_check, "CHECK_NO_AMP") == "SoundDslError"
          and _grep(out_check, "CHECK_PWM_SHARED") == "SoundDslError"
          and _grep(out_check, "CHECK_UNKNOWN_PARAM_PLANS") == "ok"
          and _grep(out_check, "CHECK_PARAM_SET") == "NotImplementedError"
          and _grep(out_check, "CHECK_PARAM_ARITH") == "SoundDslError")

    detail_extra = ("frames=%d expected=%d rms_dsl=%.4f rms_hatch=%.4f manifest=%s coarse=%s"
                    % (W["dsl"]["frames"], expected, rms_dsl, rms_hatch, manifest, coarse))

  passed = all_rc_ok and all_exist and g0 and g1 and g2 and g3 and g4 and g5 and g6
  detail = ("rc_ok=%s exists=%s G0_render=%s G1_determinism=%s G2_dsl_eq_rawpyext=%s "
            "G3_trace_once=%s G4_param_manifest=%s G5_block_hatch=%s G6_caps=%s %s"
            % (all_rc_ok, all_exist, g0, g1, g2, g3, g4, g5, g6, detail_extra))
  shutil.rmtree(tmp, ignore_errors=True)
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--variant", default=None)
  ap.add_argument("--wav", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "render":
    _role_render(args.variant, args.wav)
  elif args.role == "check":
    _role_check()
  else:
    _main_driver()
