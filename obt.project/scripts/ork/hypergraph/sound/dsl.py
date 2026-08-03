###############################################################################
# hypersound (A1a) — the patch DSL: a pure-python SoundNode IR + the `S.*` op
# namespace. Author a singularity voice as a python expression DAG; the emitter
# (sound/emitter.py) lowers it onto the EXISTING pyext data model — no new
# authoring C++. Example:
#
#   class Pad(SoundPatch):
#     def __init__(self, cutoff=900.0):
#       env  = S.env([("atk", 0.02, 1.0, 0.5),
#                     ("sus", 1.00, 1.0, 0.5),
#                     ("rel", 0.30, 0.0, 0.5)], sustain=1)
#       tone = S.saw() + S.saw(pitch=7.0)         # 7 cents detune
#       self.output(S.lowpass2(tone, cutoff=S.p("cutoff", cutoff, unit="hz")),
#                   amp=env)
#
#   snd = materialize_sound_patch(Pad, cutoff=1200.0)   # -> .program/.bank/.manifest
#   voice = synth.keyOn(60, 127, snd.program, None)
#
# SHAPE: mirrors ork.hypergraph.ptex3d.dsl — a plain object DAG built by the
# authoring expressions themselves. There is NO thread-local trace context and no
# implicit "current graph": a node IS the graph, and everything the emitter needs
# is reachable from the node handed to self.output(). Ops that this module does
# not define natively fall through to the shared ork.hypergraph.registry under the
# family name "sound", so external packages can add verbs without editing orkid.
#
# WHAT A NODE MEANS: singularity is NOT a plug graph. Blocks execute in strict
# append order inside a DspStageData, reading and writing FIXED channel indices of
# the layer's shared DspBuffer (Sum2, say, always reads its stage's ioconfig input
# 0 and 1). So `a + b` does not wire anything — it tells the EMITTER to allocate
# channel indices and append blocks in evaluation order. Authors never see
# channels; see emitter.py for the allocator.
###############################################################################

import math


class SoundDslError(RuntimeError):
  """A hypersound patch is malformed — a bad verb, an unknown block param, an
  unsatisfiable in-place constraint. Always raised at AUTHORING/PLAN time, never
  swallowed into a silent nullptr/zero signal."""


###############################################################################
# block spec table — verb -> the reflected singularity DspBlockData class plus
# the SIGNAL ARITY the C++ compute() actually touches.
#
# `classname` is what DspStageData.appendDspBlock() takes; the pyext prepends
# "Dsp" and looks the result up in the RTTI class registry, e.g.
# "OscilSine" -> DspOscilSine -> SINE_DATA (oscil.cpp:19).
#
# The arity fields are NOT cosmetic. A block reads getInpBuf(i)/getOutBuf(i),
# which index its stage's ioconfig `_inputs`/`_outputs` VECTORS
# (dspblock.cpp:127-141). Under-sizing those lists is an out-of-bounds
# std::vector read, not an error — so the emitter sizes each stage's ioconfig
# from these numbers:
#
#   nargs      how many operand SIGNALS the verb takes.
#   nslots_in  length of the stage's ioconfig input list. Usually nargs, but a
#              source uses 1 (a self-referencing dummy entry) so that it can
#              share a stage with the unary chain that follows it. AMP_ADAPTIVE
#              is the ONE block that branches on numInputs() (amp.cpp:59), so
#              this number is behavioural there, not just a bound.
#   nout       channels the block writes == length of the output list. NOISE,
#              SAMPLER and STREAMING_OSCILLATOR all take getOutBuf(1) even when
#              they only write channel 0, hence nout=2.
#   inplace    the block reads AND writes its OUTPUT channel and never calls
#              getInpBuf at all (PWM): its result channel must BE its operand's.
###############################################################################

class BlockSpec:
  __slots__ = ("verb", "classname", "nargs", "nslots_in", "nout", "inplace")

  def __init__(self, verb, classname, nargs, nslots_in, nout, inplace=False):
    self.verb = verb
    self.classname = classname
    self.nargs = nargs
    self.nslots_in = nslots_in
    self.nout = nout
    self.inplace = inplace


_SPECS = {
  # --- sources -------------------------------------------------------------
  "sine":     BlockSpec("sine",     "OscilSine",    0, 1, 1),
  "saw":      BlockSpec("saw",      "OscilSaw",     0, 1, 1),
  "square":   BlockSpec("square",   "OscilSquare",  0, 1, 1),
  "noise":    BlockSpec("noise",    "OscilNoise",   0, 1, 2),
  "sample":   BlockSpec("sample",   "Sampler",      0, 1, 2),
  "stream":   BlockSpec("stream",   "StreamingOscillator", 0, 1, 2),
  # --- unary ---------------------------------------------------------------
  "pwm":      BlockSpec("pwm",      "OscilPWM",     1, 1, 1, inplace=True),
  "lowpass2": BlockSpec("lowpass2", "FilterLowPass2", 1, 1, 1),
  "lowpass4": BlockSpec("lowpass4", "Filter4PoleLowPassWithSep", 1, 1, 1),
  "gain":     BlockSpec("gain",     "AmpMonoGain",  1, 1, 1),
  # --- binary --------------------------------------------------------------
  "sum2":     BlockSpec("sum2",     "FxMixSum2",    2, 2, 1),
  # --- emitter-owned (not authorable) --------------------------------------
  "_pitch":   BlockSpec("_pitch",   "Pitch",        0, 0, 0),
  "_amp":     BlockSpec("_amp",     "AmpAdaptive",  1, 1, 2),
}


###############################################################################
# the RTTI vocabulary — every reflected singularity DSP block / controller class
# that S.block() / S.controller() can reach.
#
# These MIRROR the C++ registrations (the same law as caps.py mirroring
# krztypes.h): scraped from ImplementReflectionX(...) in
# ork.lev2/src/aud/singularity/. They exist because rtti::Class::FindClass has NO
# python binding, so a bad class name would otherwise only be caught inside
# DspStageData::appendDspBlock — as an OrkAssert, i.e. a process abort with no
# traceback. Validating here makes it a normal python error at trace time.
#
# REGENERATE with:
#   grep -rho 'ImplementReflectionX([^,]*, *"Dsp[A-Za-z0-9_]*"' \
#     ork.lev2/src/aud/singularity/ | sed 's/.*"Dsp\([A-Za-z0-9_]*\)"/\1/' | sort -u
# (controllers: same, with "Syn" and ork.lev2/src/aud/singularity/controllers/,
#  minus the ControllerData base itself)
###############################################################################

KNOWN_DSP_BLOCK_CLASSES = frozenset("""
AmpAdaptive AmpBalance AmpBang AmpModOsc AmpMono AmpMonoGain AmpNoiseGate AmpPanner
AmpPanner2D AmpPanner2DU AmpPlus AmpRingMod AmpStereoGain AmpUL AmpX AmpXFade AmpXGain
EqParaBass EqParaMid EqParaTreble EqParametricEq EqSteepResonantBass
Filter2PoleAllPass Filter2PoleLowPass Filter4PoleLowPassWithSep Filter4PoleWithSep
FilterAllPass FilterBandPass FilterBandpass2 FilterDoubleNotchWithSep
FilterHighFreqStimulator FilterHighPass FilterLowPass FilterLowPass2 FilterLowPass2Res
FilterLowPassClip FilterLowPassGate FilterNotch FilterNotch2
FxDelayStereoDynamicEcho FxMixStereoEnhancer FxMixSum2 FxPitchShifter
FxPitchShifterRecursive FxReverbFDN4 FxReverbFDN4X FxReverbFDN8 FxReverbTest
FxTimeDomainConvolve HwInput MonoInStereoOut NonlinDistortion NonlinShaper NonlinShaper2
NonlinShaper2Param NonlinWrap OscPMX OscilNoise OscilPWM OscilSaw OscilSawAndShaper
OscilSawPlus OscilShapeMod OscilShapeModPlus OscilSine OscilSinePlus OscilSquare
OscilSyncCarrier OscilSyncModulator Pitch Sampler SpectralConvolve SpectralConvolveTD
SpectralScale SpectralShift SpectralTest StereoDelay StreamingOscillator
ToFrequencyDomain ToTimeDomain
""".split())

KNOWN_CONTROLLER_CLASSES = frozenset("""
Asr ConstControllerData CustomControllerData Fun Gradient Lfo NatEnvWrapperData
RateLevelEnv Tx81ZEnv YmEnv
""".split())


def _validate_class(classname, known, what):
  if not isinstance(classname, str) or not classname:
    raise SoundDslError("S.%s(): class name must be a non-empty string, got %r"
                        % (what, classname))
  if classname in known:
    return classname
  import difflib
  near = difflib.get_close_matches(classname, sorted(known), n=5, cutoff=0.5)
  raise SoundDslError(
      "hypersound: '%s' is not a reflected singularity %s class. %s\n"
      "(the vocabulary mirrors ImplementReflectionX in ork.lev2/src/aud/singularity — if the "
      "class is new, add it to dsl.py's KNOWN_%s_CLASSES.)"
      % (classname, what,
         ("did you mean: %s?" % ", ".join(near)) if near else "no close match.",
         "DSP_BLOCK" if what == "block" else "CONTROLLER"))


###############################################################################
# IR — values (scalars usable as a block param) and nodes (audio-rate signals)
###############################################################################

class SoundValue:
  """Base of things that can be the VALUE of a dsp block parameter: a declared
  param (S.p) or a modulation attachment (S.mod / a bare controller)."""
  __slots__ = ()

  def _reject_arith(self, other):
    raise SoundDslError(
        "hypersound: arithmetic on a declared param / modulation value is not available. "
        "Folding it into a constant would defeat the whole point of declaring it (the value "
        "must stay reachable and tweakable, never baked). Scale it at the attach site instead: "
        "S.mod(ctrl, scale=..., bias=...).")

  def __add__(self, o):  self._reject_arith(o)
  def __radd__(self, o): self._reject_arith(o)
  def __sub__(self, o):  self._reject_arith(o)
  def __rsub__(self, o): self._reject_arith(o)
  def __mul__(self, o):  self._reject_arith(o)
  def __rmul__(self, o): self._reject_arith(o)
  def __truediv__(self, o):  self._reject_arith(o)
  def __rtruediv__(self, o): self._reject_arith(o)


class ParamDecl(SoundValue):
  """A DECLARED patch parameter (S.p). A1a scope is declaration-only: the emitter
  records {name, default, min, max, unit} in the patch's manifest and lowers
  `default` as the dsp param's initial (coarse) value. RUNTIME mutation belongs to
  A2 — it needs CustomControllerData, which has zero python bindings today — so
  .set() raises rather than pretending."""
  __slots__ = ("name", "default", "min", "max", "unit")

  def __init__(self, name, default, minval=None, maxval=None, unit=None):
    if not isinstance(name, str) or not name:
      raise SoundDslError("S.p(): name must be a non-empty string, got %r" % (name,))
    self.name = name
    self.default = float(default)
    self.min = None if minval is None else float(minval)
    self.max = None if maxval is None else float(maxval)
    self.unit = unit

  def set(self, value):
    raise NotImplementedError(
        "hypersound S.p('%s').set() is A2, not A1a. Runtime param mutation lowers onto "
        "CustomControllerData + the _CCIVALS-style queue, and CustomControllerData has NO "
        "python bindings yet. A1a declares the manifest and lowers the DEFAULT only."
        % self.name)

  def spec(self):
    return dict(name=self.name, default=self.default, min=self.min,
                max=self.max, unit=self.unit)

  def __repr__(self):
    return "S.p(%r, %g, unit=%r)" % (self.name, self.default, self.unit)


class ControllerNode:
  """A modulation SOURCE (an envelope or an LFO) — lowered by the emitter with
  LayerData.appendController(<classname>, <name>). Controllers are per-LAYER, not
  per-block; one node = one controller no matter how many params reference it."""
  __slots__ = ("verb", "classname", "cfg", "label")

  def __init__(self, verb, classname, cfg, label=None):
    self.verb = verb
    self.classname = classname
    self.cfg = cfg
    self.label = label

  def __repr__(self):
    return "S.%s(%s)" % (self.verb, self.label or "")


class ModRef(SoundValue):
  """A controller attached to a dsp param: `param.mods.src1 = ctrl` plus
  src1scale / src1bias, with `base` seeding the param's own coarse value. This is
  the frqdom_loshelf.py attach idiom, made declarative."""
  __slots__ = ("controller", "scale", "bias", "base")

  def __init__(self, controller, scale=1.0, bias=0.0, base=None):
    if not isinstance(controller, ControllerNode):
      raise SoundDslError("S.mod(): first argument must be a controller (S.adsr/S.env/S.lfo), "
                          "got %r" % (controller,))
    self.controller = controller
    self.scale = float(scale)
    self.bias = float(bias)
    self.base = None if base is None else float(base)


class SoundNode:
  """One audio-rate MONO signal == one dsp block. `_args` are the operand signals
  in the order the block's compute() reads them (getInpBuf(0), getInpBuf(1), ...).
  Node IDENTITY is the sharing rule: referencing the same node twice references
  the same block and the same dsp channel — it is not duplicated. That matters for
  stateful blocks (an oscillator cloned would be a DIFFERENT oscillator)."""
  __slots__ = ("_spec", "_args", "_props", "_label")

  def __init__(self, spec, args=(), props=None, label=None):
    self._spec = spec
    self._args = tuple(args)
    self._props = dict(props or {})
    self._label = label

  # ── arithmetic ──
  def __add__(self, o):
    return _sum2(self, o)

  def __radd__(self, o):
    return _sum2(o, self)

  def __mul__(self, o):
    return _scale(self, o)

  def __rmul__(self, o):
    return _scale(self, o)

  def __repr__(self):
    return "SoundNode<%s%s>" % (self._spec.verb, "/" + self._label if self._label else "")


def _require_node(x, what):
  if isinstance(x, SoundNode):
    return x
  if isinstance(x, (int, float)):
    raise SoundDslError(
        "hypersound: %s needs an audio signal, got the number %r. A bare constant is not a "
        "signal in singularity (there is no DC-source block); use an oscillator, or pass the "
        "number as a block param (e.g. S.saw(pitch=%r))." % (what, x, x))
  raise SoundDslError("hypersound: %s needs an audio signal (SoundNode), got %r" % (what, x))


def _sum2(a, b):
  a = _require_node(a, "'+'")
  b = _require_node(b, "'+'")
  return SoundNode(_SPECS["sum2"], args=(a, b))


def _scale(node, k):
  if isinstance(k, SoundNode):
    raise SoundDslError(
        "hypersound: signal * signal (ring modulation) is not in the A1a verb set. Only "
        "signal * <positive number> (a gain) is. Ring mod maps to DspRingMod and is deferred.")
  if isinstance(k, SoundValue):
    raise SoundDslError(
        "hypersound: signal * <declared param / modulation> is not in the A1a verb set — a "
        "modulated gain is S.gain(sig, gain=S.mod(ctrl, ...)).")
  return S.gain(node, scale=k)


###############################################################################
# S — the op namespace
###############################################################################

class _Ops:
  # ---- external extension hook (ork.hypergraph.registry) ----
  # Any op name NOT defined natively below falls back to the shared DSL op registry under the
  # family "sound", so external packages can add hypersound verbs via `@op("sound","name")` and
  # call them as S.<name>(...) with NO edit to this class. Native methods always win (this runs
  # only when normal attribute lookup misses). Dunders are never intercepted.
  def __getattr__(self, name):
    if name.startswith("__"):
      raise AttributeError(name)
    from ork.hypergraph.registry import get_op
    fn = get_op("sound", name)
    if fn is None:
      raise AttributeError(
          "hypersound DSL has no op '%s' — define it natively, or register it from any package: "
          "`from ork.hypergraph.registry import op; @op('sound','%s')`" % (name, name))
    return fn

  def register(self, name, fn=None):
    """Register a hypersound op -> callable as S.<name>(...). Decorator or direct:
       S.register('pluck')(fn)   |   S.register('pluck', fn)."""
    from ork.hypergraph.registry import register_op
    if fn is None:
      return lambda f: register_op("sound", name, f)
    return register_op("sound", name, fn)

  # ── generic escape hatches: ANY reflected block / controller ───────────────

  def block(self, classname, *signals, label=None, nin=None, nout=2, inplace=False, **param_defaults):
    """Reach ANY reflected singularity DSP block by its RTTI class name — the whole
    existing vocabulary is authorable on day one, curated verb or not:

        ring  = S.block("AmpRingMod", a, b)
        shape = S.block("NonlinShaper", tone, nin=1, nout=1, adjust=0.6)

    `signals` are the operand signals in getInpBuf order. `param_defaults` are
    lowered through paramByName() exactly as for a curated verb (numbers, S.p(),
    S.mod(), controllers), falling back to a python property on the block's derived
    pybind type (e.g. dataset= on SpectralConvolve).

    ARITY DEFAULTS ARE THE SAFE ONES. Every block in the tree touches at most io
    index 0 and 1 (the engine-wide 2-in/2-out shape), so `nin` defaults to
    max(len(signals), 2) and `nout` to 2: the ioconfig lists are then always long
    enough for any getInpBuf/getOutBuf the block performs, which an under-sized
    list would answer with an out-of-bounds vector read. Narrow them when you have
    READ the block's compute(): nin=1/nout=1 makes the block share a stage with a
    unary chain instead of opening its own. The DSL value of a multi-output block
    is its FIRST output channel — a stereo block's right channel is dropped, since
    A1a's value model is mono."""
    _validate_class(classname, KNOWN_DSP_BLOCK_CLASSES, "block")
    args = tuple(_require_node(s, "S.block(%r)" % classname) for s in signals)
    nslots_in = max(len(args), 2) if nin is None else int(nin)
    if nslots_in < len(args):
      raise SoundDslError("S.block(%r): nin=%d is smaller than the %d signals passed"
                          % (classname, nslots_in, len(args)))
    if int(nout) < 1:
      raise SoundDslError("S.block(%r): nout must be >= 1" % classname)
    if inplace and len(args) != 1:
      raise SoundDslError("S.block(%r): inplace=True needs exactly one signal" % classname)
    spec = BlockSpec(classname, classname, len(args), max(nslots_in, 1), int(nout), bool(inplace))
    return SoundNode(spec, args=args, props=param_defaults, label=label)

  def controller(self, classname, label=None, **props):
    """Reach ANY reflected singularity ControllerData class by RTTI class name.
    `props` are set as python properties on the created controller (loud error if
    the binding does not expose one). Envelopes whose shape needs addSegment() are
    NOT expressible this way — use S.env / S.adsr."""
    _validate_class(classname, KNOWN_CONTROLLER_CLASSES, "controller")
    return ControllerNode("controller", classname, dict(_props=dict(props)), label=label)

  # ── declared params + modulation ───────────────────────────────────────────

  def p(self, name, default, min=None, max=None, unit=None):
    """Declare a patch parameter. A1a lowers `default` as the dsp param's initial
    value and records the whole spec in the patch manifest; runtime mutation is A2."""
    return ParamDecl(name, default, min, max, unit)

  def mod(self, controller, scale=1.0, bias=0.0, base=None):
    """Attach `controller` to a dsp param as modulation source 1, with src1scale /
    src1bias, optionally seeding the param's own value with `base`."""
    return ModRef(controller, scale=scale, bias=bias, base=base)

  # ── controllers ────────────────────────────────────────────────────────────

  def env(self, segments, sustain=None, release=None, bipolar=False, label=None):
    """A rate/level envelope (SynRateLevelEnv). `segments` is an ordered list of
    (name, time, level, power) exactly as RateLevelEnvData::addSegment takes them;
    `sustain`/`release` are segment INDICES. The `ampenv` flag is NOT set here —
    the emitter sets it on whichever envelope a patch passes as output(amp=...),
    because ampenv means "this env drives Layer::_ampenvgain", which is a role,
    not a property of the curve."""
    segs = []
    for seg in segments:
      if len(seg) != 4:
        raise SoundDslError("S.env(): each segment is (name, time, level, power), got %r" % (seg,))
      nm, t, lvl, pwr = seg
      segs.append((str(nm), float(t), float(lvl), float(pwr)))
    if not segs:
      raise SoundDslError("S.env(): needs at least one segment")
    return ControllerNode("env", "RateLevelEnv",
                          dict(segments=segs, sustain=sustain, release=release,
                               bipolar=bool(bipolar)),
                          label=label)

  def adsr(self, attack=0.01, decay=0.1, sustain=1.0, release=0.2, power=0.5, label=None):
    """The friendly 4-segment ADSR over the same SynRateLevelEnv. Times are in
    seconds, `sustain` is a LEVEL in 0..1. For an exact hand-authored curve use
    S.env(...) — this is sugar over it."""
    segs = [("atk", float(attack), 1.0, float(power)),
            ("dec", float(decay), float(sustain), float(power)),
            ("sus", 1.0, float(sustain), float(power)),
            ("rel", float(release), 0.0, float(power))]
    return ControllerNode("adsr", "RateLevelEnv",
                          dict(segments=segs, sustain=2, release=None, bipolar=False),
                          label=label)

  def lfo(self, rate=1.0, phase=0.0, min_rate=None, max_rate=None, label=None):
    """A low frequency oscillator (SynLfo). `rate` sets both minRate and maxRate
    (the pair is the modulatable range); pass min_rate/max_rate to spread them."""
    lo = float(rate if min_rate is None else min_rate)
    hi = float(rate if max_rate is None else max_rate)
    return ControllerNode("lfo", "Lfo",
                          dict(initialPhase=float(phase), minRate=lo, maxRate=hi),
                          label=label)

  # ── sources ────────────────────────────────────────────────────────────────

  def sine(self, label=None, **props):
    """PolyBLEP sine oscillator (DspOscilSine). Param: pitch (cents offset)."""
    return SoundNode(_SPECS["sine"], props=props, label=label)

  def saw(self, label=None, **props):
    """PolyBLEP saw/ramp oscillator (DspOscilSaw). Param: pitch (cents offset)."""
    return SoundNode(_SPECS["saw"], props=props, label=label)

  def square(self, label=None, **props):
    """PolyBLEP square oscillator (DspOscilSquare). Param: pitch (cents offset)."""
    return SoundNode(_SPECS["square"], props=props, label=label)

  def noise(self, label=None, **props):
    """White noise (DspOscilNoise).

    NOT DETERMINISTIC AND NOT SEEDABLE. NOISE::compute draws from the process-global
    C `rand()` (oscil.cpp) — the block has no seed, no phase and no state of its own,
    so two renders of the same patch agree only if every other rand() consumer in the
    process ran identically. Byte-identity gates must not contain S.noise until the
    block grows a per-instance seeded generator."""
    return SoundNode(_SPECS["noise"], props=props, label=label)

  def sample(self, label=None, **props):
    """Multisample playback oscillator (DspSampler). The keymap/multisample it plays
    is LAYER state (LayerData.keymap), not block state — pass keymap= to
    SoundPatch.output(). UNVERIFIED in A1a: no canary keys a sampled patch."""
    return SoundNode(_SPECS["sample"], props=props, label=label)

  def stream(self, label=None, **props):
    """Streaming oscillator (DspStreamingOscillator). Set its chunk source with
    source=<AudioStreamingInputChunkSource>. UNVERIFIED in A1a: no canary streams."""
    return SoundNode(_SPECS["stream"], props=props, label=label)

  # ── unary ──────────────────────────────────────────────────────────────────

  def pwm(self, signal, label=None, **props):
    """Pulse-width offset (DspOscilPWM). NOT a source: PWM::compute reads and writes
    its OUTPUT channel in place and never calls getInpBuf — it adds `offset`*0.01 to
    whatever the upstream block left in that channel (the K2000 saw->PWM idiom). The
    emitter therefore gives it its operand's channel, which requires the operand to
    have exactly ONE consumer; sharing a signal into a pwm is a loud error."""
    return SoundNode(_SPECS["pwm"], args=(_require_node(signal, "S.pwm()"),),
                     props=props, label=label)

  def lowpass2(self, signal, label=None, **props):
    """Two-pole lowpass at fixed -6dB resonance (DspFilterLowPass2). Param: cutoff (Hz)."""
    return SoundNode(_SPECS["lowpass2"], args=(_require_node(signal, "S.lowpass2()"),),
                     props=props, label=label)

  def lowpass4(self, signal, label=None, **props):
    """Four-pole lowpass — PROVISIONAL MAPPING, AWAITING OWNER RATIFICATION.

    There is no plain 4-pole lowpass block in singularity. This maps to
    DspFilter4PoleLowPassWithSep (two cascaded 2-poles whose cutoffs are separated
    by `separation` cents), with separation defaulted to 0 so the two stages track
    exactly and the block behaves as a straight 4-pole. Params: cutoff (Hz),
    resonance, separation (cents). If the owner ratifies a different target block,
    THIS mapping is what changes — patches keep saying S.lowpass4."""
    props.setdefault("separation", 0.0)
    return SoundNode(_SPECS["lowpass4"], args=(_require_node(signal, "S.lowpass4()"),),
                     props=props, label=label)

  def gain(self, signal, scale=None, db=None, label=None, **props):
    """Static gain (DspAmpMonoGain). Give EITHER `scale` (a linear multiplier, >0) or
    `db`; `scale` is converted with 20*log10 because the block's param is in dB.
    GAIN::compute soft-saturates its result at unity (softsat(x,1), amp.cpp), which
    is EXACTLY linear for |output| <= 1 and compresses above it — so `scale` values
    that push a signal past full scale are not a clean multiply."""
    if (scale is None) == (db is None):
      raise SoundDslError("S.gain(): pass exactly one of scale= (linear) or db=")
    if scale is not None:
      if scale <= 0.0:
        raise SoundDslError(
            "S.gain(scale=%r): the block's parameter is in dB, so the linear scale must be "
            "positive. Zero means 'do not connect this branch' and negative (inversion) has "
            "no block in the A1a verb set." % (scale,))
      db = 20.0 * math.log10(float(scale))
    props["gain"] = float(db)
    return SoundNode(_SPECS["gain"], args=(_require_node(signal, "S.gain()"),),
                     props=props, label=label)


S = _Ops()


###############################################################################
# SoundPatch — the authoring base class
###############################################################################

class SoundPatch:
  """Author base. Subclass and, in __init__(self, **params), build S.* expressions
  and finish with exactly one self.output(...) call."""

  def output(self, signal, amp=None, pan=7, panmode=0, gain_db=None,
             keymap=None, monophonic=None, portamento=None):
    """Declare the patch's output signal and its amplitude envelope.

    `amp` is REQUIRED and must be an envelope controller. Without one,
    Layer::_ampenvgain stays at its 1.0 default: the voice plays at full level and
    NEVER releases — a silent-failure mode (a stuck voice, not an error), so the
    DSL refuses instead. The emitter sets ampenv=True on whatever is passed here.

    `pan`/`panmode` default to the fixed-centre pair (panmode 0 = Fixed, pan 7 =
    centre, since Layer::currentPan uses (pan-7)/7); LayerData's own defaults
    (-1/0) are NOT centre. `gain_db`, `keymap`, `monophonic`, `portamento` are
    left untouched when None."""
    if not isinstance(signal, SoundNode):
      raise SoundDslError("SoundPatch.output(): first argument must be a signal (SoundNode), "
                          "got %r" % (signal,))
    if amp is None:
      raise SoundDslError(
          "SoundPatch.output(): amp= is required. Pass an envelope (e.g. "
          "amp=S.adsr(0.01, 0.1, 0.8, 0.2)); with no amp envelope the layer's _ampenvgain "
          "stays 1.0 forever and the voice never releases.")
    if not isinstance(amp, ControllerNode):
      raise SoundDslError("SoundPatch.output(): amp= must be an envelope controller "
                          "(S.adsr/S.env), got %r" % (amp,))
    self._out_signal = signal
    self._out_amp = amp
    self._out_layer = dict(pan=pan, panmode=panmode, gain_db=gain_db, keymap=keymap,
                           monophonic=monophonic, portamento=portamento)

  # -- accessors the emitter uses; they fail loud rather than returning None --

  def signal(self):
    sig = getattr(self, "_out_signal", None)
    if sig is None:
      raise SoundDslError("%s never called self.output(...) — a patch with no output signal "
                          "has nothing to lower." % type(self).__name__)
    return sig

  def ampenv(self):
    self.signal()
    return self._out_amp

  def layer_settings(self):
    self.signal()
    return dict(self._out_layer)


###############################################################################
# graph utilities (shared with the emitter)
###############################################################################

def postorder(root):
  """Evaluation order: every node appears after all of its operands, exactly once.
  Node identity (not structural equality) is the sharing key."""
  order = []
  seen = set()
  stack = [(root, False)]
  while stack:
    node, expanded = stack.pop()
    if expanded:
      order.append(node)
      continue
    if id(node) in seen:
      continue
    seen.add(id(node))
    stack.append((node, True))
    for arg in reversed(node._args):
      if id(arg) not in seen:
        stack.append((arg, False))
  return order


def use_counts(order):
  """node id -> how many operand slots reference it (duplicates count twice, so
  `x + x` reports 2 uses of x)."""
  counts = {id(n): 0 for n in order}
  for node in order:
    for arg in node._args:
      counts[id(arg)] += 1
  return counts
