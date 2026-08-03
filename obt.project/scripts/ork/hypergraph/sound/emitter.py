###############################################################################
# hypersound (A1a) — the emitter: a one-shot walk that lowers a SoundNode DAG
# onto BankData / ProgramData / LayerData / DspStageData / DspBlockData /
# ControllerData through the EXISTING pyext surface (the ork.singularity.sampler
# construction idiom), with NO new authoring C++.
#
# ─── WHY THE EMITTER OWNS CHANNEL ALLOCATION ────────────────────────────────
# Singularity is not a plug graph. Every block in a DspStageData runs in append
# order and addresses the layer's ONE shared DspBuffer through its stage's
# ioconfig: getInpBuf(i) resolves to `ioconfig._inputs[i]` and getOutBuf(i) to
# `ioconfig._outputs[i]` (dspblock.cpp:127-141), because DspBlockData::
# _dspchannels — the per-block remap — has NO python binding, so every block a
# python author appends uses the identity mapping. Consequences:
#
#   * a block's wiring IS its stage's ioconfig, and all blocks in a stage share it;
#   * therefore blocks may share a stage exactly when they want the SAME
#     (inputs, outputs) channel lists — a chain of unary ops on one channel does,
#     which is why `sine -> lowpass2 -> lowpass4` costs ONE stage, while every
#     fork/sum mints new ones;
#   * channels 0 and 1 are the layer output (the only two Layer::beginCompute
#     zeroes and the only two the mix path reads), so scratch starts at 2.
#
# The allocator is a linear-scan register allocator over the post-order walk: a
# node's result takes over its first operand's channel when that operand dies at
# this node (which is what makes chains collapse into one stage), otherwise it
# takes the lowest free channel; dead operands' channels return to the free list.
# Since post-order guarantees a producer runs before its consumers in the same
# control pass, a scratch channel is always written before it is read — which
# matters because only channels 0/1 are cleared per block.
#
# ─── TWO PHASES ─────────────────────────────────────────────────────────────
# plan_sound_patch() is PURE PYTHON: it walks the DAG, allocates channels, packs
# stages, builds the param manifest and runs every caps check. Nothing touches
# the engine until execute_plan(). A patch that busts a ceiling therefore raises
# SoundCapsError while leaving NO half-built ProgramData behind.
###############################################################################

from ork.hypergraph.sound import caps
from ork.hypergraph.sound.dsl import (
    ControllerNode,
    ModRef,
    ParamDecl,
    SoundDslError,
    SoundPatch,
    postorder,
    use_counts,
)


###############################################################################
# plan structures (pure data — no engine types)
###############################################################################

class PlannedBlock:
  __slots__ = ("verb", "classname", "name", "props", "in_channels", "out_channels")

  def __init__(self, verb, classname, name, props, in_channels, out_channels):
    self.verb = verb
    self.classname = classname
    self.name = name
    self.props = props
    self.in_channels = tuple(in_channels)
    self.out_channels = tuple(out_channels)

  def __repr__(self):
    return "%s<%s> in%s out%s" % (self.name, self.classname,
                                  list(self.in_channels), list(self.out_channels))


class PlannedStage:
  __slots__ = ("name", "inputs", "outputs", "blocks")

  def __init__(self, name, inputs, outputs):
    self.name = name
    self.inputs = list(inputs)
    self.outputs = list(outputs)
    self.blocks = []

  def __repr__(self):
    return "stage<%s> in%s out%s blocks%s" % (self.name, self.inputs, self.outputs,
                                              [b.name for b in self.blocks])


class PlannedController:
  __slots__ = ("verb", "classname", "name", "cfg", "ampenv")

  def __init__(self, verb, classname, name, cfg, ampenv):
    self.verb = verb
    self.classname = classname
    self.name = name
    self.cfg = cfg
    self.ampenv = ampenv


class SoundPlan:
  """The complete lowering recipe for one patch — everything execute_plan() needs
  and everything a caller wants to assert on, with no engine objects in it."""
  __slots__ = ("patch_name", "stages", "controllers", "manifest", "layer_settings",
               "out_channel", "peak_channels")

  def __init__(self, patch_name):
    self.patch_name = patch_name
    self.stages = []
    self.controllers = []
    self.manifest = {}
    self.layer_settings = {}
    self.out_channel = -1
    self.peak_channels = 0

  def block_names(self):
    return [b.name for st in self.stages for b in st.blocks]

  def stage_shape(self):
    """(stage name, #blocks, inputs, outputs) per stage — the compact form a test
    asserts idempotency against."""
    return [(st.name, len(st.blocks), tuple(st.inputs), tuple(st.outputs))
            for st in self.stages]


###############################################################################
# planning
###############################################################################

class _ChannelPool:
  """Lowest-index-first free list over the scratch channels [2, 32)."""

  def __init__(self, patch_name):
    self._patch_name = patch_name
    self._free = list(range(caps.NUM_RESERVED_OUTPUT_CHANNELS, caps.KMAX_DSP_CHANNELS))
    self._live = 0
    self.peak = 0

  def alloc(self):
    if not self._free:
      caps.check_channels_available(self._live + 1, self._patch_name)
      # unreachable unless the two constants disagree; never return a bogus channel.
      raise caps.SoundCapsError("hypersound '%s': dsp channel pool exhausted" % self._patch_name)
    chan = self._free.pop(0)
    caps.check_channel_index(chan, self._patch_name)
    self._live += 1
    self.peak = max(self.peak, self._live)
    return chan

  def release(self, channels):
    for chan in channels:
      self._free.append(chan)
      self._live -= 1
    self._free.sort()


def _param_value_and_manifest(value, plan, block_name, param_name):
  """Reduce one authored prop value to what execute_plan() applies, recording any
  declared param in the manifest. Returns (coarse_or_None, ModRef_or_None)."""
  if isinstance(value, ParamDecl):
    spec = value.spec()
    prior = plan.manifest.get(value.name)
    if prior is None:
      entry = dict(spec)
      entry["sites"] = []
      plan.manifest[value.name] = entry
      prior = entry
    else:
      for key in ("default", "min", "max", "unit"):
        if prior[key] != spec[key]:
          raise SoundDslError(
              "hypersound: patch param '%s' is declared twice with different %s (%r vs %r). "
              "Declare it once and reuse the returned S.p handle."
              % (value.name, key, prior[key], spec[key]))
    prior["sites"].append((block_name, param_name))
    return value.default, None
  if isinstance(value, ModRef):
    return value.base, value
  if isinstance(value, ControllerNode):
    return None, ModRef(value)
  if isinstance(value, (int, float)):
    return float(value), None
  raise SoundDslError(
      "hypersound: block '%s' param '%s' got %r — expected a number, an S.p() declaration, "
      "a controller, or S.mod(...)." % (block_name, param_name, value))


def _plan_controller(node, plan, seen, counters, ampenv_node):
  key = id(node)
  if key in seen:
    return seen[key]
  counters[node.verb] = counters.get(node.verb, 0) + 1
  name = node.label or ("%s_%d" % (node.verb, counters[node.verb] - 1))
  planned = PlannedController(node.verb, node.classname, name, dict(node.cfg),
                              ampenv=(node is ampenv_node))
  plan.controllers.append(planned)
  caps.check_controller_count(len(plan.controllers), plan.patch_name)
  seen[key] = planned
  return planned


def plan_sound_patch(patch, patch_name=None):
  """Walk a traced SoundPatch and produce its SoundPlan. Pure python: no engine
  import, no engine mutation, every ceiling checked before anything is built."""
  if not isinstance(patch, SoundPatch):
    raise SoundDslError("plan_sound_patch(): expected a SoundPatch instance, got %r" % (patch,))
  root = patch.signal()
  ampenv_node = patch.ampenv()
  name = patch_name or type(patch).__name__

  plan = SoundPlan(name)
  plan.layer_settings = patch.layer_settings()

  order = postorder(root)
  remaining = use_counts(order)
  remaining[id(root)] += 1            # the final amp block consumes the root

  pool = _ChannelPool(name)
  channels = {}                        # id(node) -> [channel, ...]
  ctrl_seen = {}
  ctrl_counters = {}
  block_counters = {}
  used_block_names = set()

  for node in order:
    spec = node._spec
    arg_channels = [channels[id(a)][0] for a in node._args]
    for arg in node._args:
      remaining[id(arg)] -= 1

    # result channels: take over the first operand's channels when it dies here
    # (that is what lets a unary chain keep one channel and so one stage).
    reuse = None
    if node._args:
      first = node._args[0]
      first_chans = channels[id(first)]
      if remaining[id(first)] == 0 and len(first_chans) >= spec.nout:
        reuse = first_chans
    if spec.inplace and reuse is None:
      raise SoundDslError(
          "hypersound: S.%s reads and writes its own channel in place, so its input signal "
          "must have exactly one consumer — this one is shared (or is not the block's first "
          "operand). Give the shared signal its own downstream copy." % spec.verb)
    if reuse is not None:
      out_channels = list(reuse[:spec.nout])
      pool.release(reuse[spec.nout:])
    else:
      out_channels = [pool.alloc() for _ in range(spec.nout)]
    channels[id(node)] = out_channels

    # dead operands whose channels were not taken over go back to the pool
    freed = set()
    for arg in node._args:
      if id(arg) in freed:
        continue
      freed.add(id(arg))
      if remaining[id(arg)] == 0 and channels[id(arg)] is not reuse:
        pool.release(channels[id(arg)])

    # ioconfig input list: the operand channels, grown to the block's declared
    # slot count. A source has no operands but still needs an index-addressable
    # entry — its own channel, which is also what lets it share a stage with the
    # unary chain that follows it. Padding repeats the last real channel so an
    # unexpected getInpBuf reads a live signal rather than an out-of-range slot.
    in_list = list(arg_channels)
    while len(in_list) < spec.nslots_in:
      in_list.append(in_list[-1] if in_list else out_channels[0])
    out_list = list(out_channels)

    block_counters[spec.verb] = block_counters.get(spec.verb, 0) + 1
    bname = node._label or ("%s_%d" % (spec.verb, block_counters[spec.verb] - 1))
    if bname in used_block_names:
      raise SoundDslError(
          "hypersound: two blocks named '%s'. Block names must be unique within a layer "
          "(DspStageData::appendDspBlock asserts on a duplicate); drop the label= or "
          "make it unique." % bname)
    used_block_names.add(bname)

    props = {}
    mods = {}
    for pname, pvalue in node._props.items():
      coarse, mod = _param_value_and_manifest(pvalue, plan, bname, pname)
      if coarse is not None:
        props[pname] = coarse
      if mod is not None:
        pctrl = _plan_controller(mod.controller, plan, ctrl_seen, ctrl_counters, ampenv_node)
        mods[pname] = (pctrl, mod.scale, mod.bias)
    caps.check_param_count(len(set(props) | set(mods)), bname, name)
    packed = dict(props)
    if mods:
      packed["_mods"] = mods

    pblock = PlannedBlock(spec.verb, spec.classname, bname, packed, in_list, out_list)
    _append_block(plan, pblock, in_list, out_list)

  # the pitch block is layer infrastructure (PITCH::compute touches no buffer, it
  # only publishes the layer's pitch state the oscillators read), so it costs no
  # stage: it rides at the head of the first stage, ahead of every source.
  pitch = PlannedBlock("_pitch", "Pitch", "pitch", {}, (), ())
  plan.stages[0].blocks.insert(0, pitch)
  caps.check_block_count(len(plan.stages[0].blocks), plan.stages[0].name, name)

  # the amp env is a controller even if no param references it
  amp_ctrl = _plan_controller(ampenv_node, plan, ctrl_seen, ctrl_counters, ampenv_node)

  # final AMP stage: mono in from the root's channel, stereo out to the layer's
  # 0/1. AMP_ADAPTIVE branches on numInputs(), and 1 input is the mono->L+R path.
  plan.out_channel = channels[id(root)][0]
  amp_stage = PlannedStage("AMP", [plan.out_channel], list(range(caps.NUM_RESERVED_OUTPUT_CHANNELS)))
  amp_block = PlannedBlock("_amp", "AmpAdaptive", "amp",
                           dict(_mods={"gain": (amp_ctrl, 1.0, 0.0)}),
                           amp_stage.inputs, amp_stage.outputs)
  amp_stage.blocks.append(amp_block)
  plan.stages.append(amp_stage)
  caps.check_stage_count(len(plan.stages), name)

  plan.peak_channels = pool.peak
  return plan


def _append_block(plan, pblock, in_list, out_list):
  """Put a block in the trailing stage when it wants exactly that stage's channel
  mapping, else open a new stage. Both ceilings are checked before the append."""
  stage = plan.stages[-1] if plan.stages else None
  fits = (stage is not None
          and stage.name != "AMP"
          and stage.inputs == in_list
          and stage.outputs == out_list
          and len(stage.blocks) < caps.KMAX_DSP_BLOCKS_PER_STAGE)
  if not fits:
    caps.check_stage_count(len(plan.stages) + 1, plan.patch_name)
    stage = PlannedStage("S%d" % len(plan.stages), in_list, out_list)
    plan.stages.append(stage)
  stage.blocks.append(pblock)
  caps.check_block_count(len(stage.blocks), stage.name, plan.patch_name)


###############################################################################
# execution — the only part that touches the engine
###############################################################################

class MaterializedSound:
  """A lowered patch: the engine objects plus the plan that produced them. Hold on
  to `.bank` — the ProgramData alone does not keep it alive."""
  __slots__ = ("name", "patch", "plan", "bank", "program", "layer", "manifest")

  def __init__(self, name, patch, plan, bank, program, layer):
    self.name = name
    self.patch = patch
    self.plan = plan
    self.bank = bank
    self.program = program
    self.layer = layer
    self.manifest = plan.manifest

  def setParam(self, name, value):
    raise NotImplementedError(
        "hypersound runtime param mutation ('%s') is A2, not A1a. It lowers onto "
        "CustomControllerData plus the _CCIVALS-style queue, and CustomControllerData has NO "
        "python bindings today. A1a declares the manifest and lowers each param's DEFAULT as "
        "its initial value." % name)

  def __repr__(self):
    return "MaterializedSound<%s> stages=%d blocks=%d params=%d" % (
        self.name, len(self.plan.stages), len(self.plan.block_names()), len(self.manifest))


def _apply_block_props(blk, pblock, controllers, patch_name):
  mods = pblock.props.get("_mods", {})
  for pname, value in pblock.props.items():
    if pname == "_mods":
      continue
    param = blk.paramByName(pname)
    if param is not None:
      param.coarse = float(value)
      continue
    if hasattr(blk, pname):
      setattr(blk, pname, value)
      continue
    available = sorted(blk.params.keys())
    raise SoundDslError(
        "hypersound '%s': block '%s' (%s) has no dsp param or property named '%s'. Its params "
        "are %s." % (patch_name, pblock.name, pblock.classname, pname, available))
  for pname, (pctrl, scale, bias) in mods.items():
    param = blk.paramByName(pname)
    if param is None:
      available = sorted(blk.params.keys())
      raise SoundDslError(
          "hypersound '%s': cannot modulate '%s' on block '%s' (%s) — no such dsp param. Its "
          "params are %s." % (patch_name, pname, pblock.name, pblock.classname, available))
    param.mods.src1 = controllers[pctrl.name]
    param.mods.src1scale = scale
    param.mods.src1bias = bias


def execute_plan(plan, bank=None, program_name=None, patch=None):
  """Build the engine objects a SoundPlan describes. Engine imports happen HERE
  (core before lev2, the import-order law) so planning stays engine-free."""
  import orkengine.core                                    # noqa: F401  (core FIRST)
  from orkengine.lev2 import singularity as _sing

  progname = program_name or plan.patch_name
  bank = bank if bank is not None else _sing.BankData()
  program = bank.newProgram(progname)
  layer = program.newLayer()

  controllers = {}
  for pctrl in plan.controllers:
    ctrl = layer.appendController(pctrl.classname, pctrl.name)
    if pctrl.verb in ("env", "adsr"):
      ctrl.ampenv = bool(pctrl.ampenv)
      ctrl.bipolar = bool(pctrl.cfg.get("bipolar", False))
      if pctrl.cfg.get("sustain") is not None:
        ctrl.sustainSegment = int(pctrl.cfg["sustain"])
      if pctrl.cfg.get("release") is not None:
        ctrl.releaseSegment = int(pctrl.cfg["release"])
      for segname, segtime, seglevel, segpower in pctrl.cfg["segments"]:
        ctrl.addSegment(segname, segtime, seglevel, segpower)
    elif pctrl.verb == "lfo":
      ctrl.initialPhase = float(pctrl.cfg["initialPhase"])
      ctrl.minRate = float(pctrl.cfg["minRate"])
      ctrl.maxRate = float(pctrl.cfg["maxRate"])
    else:                                   # S.controller() escape hatch
      for propname, propvalue in pctrl.cfg.get("_props", {}).items():
        if not hasattr(ctrl, propname):
          raise SoundDslError(
              "hypersound '%s': controller '%s' (Syn%s) has no python property '%s'."
              % (plan.patch_name, pctrl.name, pctrl.classname, propname))
        setattr(ctrl, propname, propvalue)
    controllers[pctrl.name] = ctrl

  settings = plan.layer_settings
  layer.panmode = int(settings["panmode"])
  layer.pan = int(settings["pan"])
  if settings.get("gain_db") is not None:
    layer.gain = float(settings["gain_db"])
  if settings.get("keymap") is not None:
    layer.keymap = settings["keymap"]
  if settings.get("monophonic") is not None:
    program.monophonic = bool(settings["monophonic"])
  if settings.get("portamento") is not None:
    program.portamentoRate = float(settings["portamento"])

  for pstage in plan.stages:
    stage = layer.appendStage(pstage.name)
    stage.ioconfig.inputs = list(pstage.inputs)
    stage.ioconfig.outputs = list(pstage.outputs)
    for pblock in pstage.blocks:
      blk = stage.appendDspBlock(pblock.classname, pblock.name)
      if pblock.verb == "_pitch":
        layer.pitchBlock = blk
      _apply_block_props(blk, pblock, controllers, plan.patch_name)

  return MaterializedSound(progname, patch, plan, bank, program, layer)


###############################################################################
# the one-call front doors
###############################################################################

_CACHE = {}


def clear_sound_cache():
  """Drop the materialize cache. Engine objects are per-synth-session, so a test
  that boots a second engine in one process must clear it."""
  _CACHE.clear()


def materialize_sound_instance(patch, name=None, bank=None):
  """Lower an ALREADY-CONSTRUCTED SoundPatch. Uncached — the caller owns the
  instance and therefore owns the trace-once question."""
  plan = plan_sound_patch(patch, name)
  return execute_plan(plan, bank=bank, program_name=name, patch=patch)


def materialize_sound_patch(patch_class, name=None, bank=None, cache=True, **params):
  """Instantiate a SoundPatch subclass with **params, plan it, and build the engine
  objects. Returns a MaterializedSound.

  TRACE-ONCE: the result is memoized on (class, name, params, bank), so calling
  this twice with the same arguments returns the SAME MaterializedSound and appends
  NOTHING a second time. Pass cache=False (or call clear_sound_cache()) when a
  second, independent ProgramData is genuinely wanted."""
  if not (isinstance(patch_class, type) and issubclass(patch_class, SoundPatch)):
    raise SoundDslError("materialize_sound_patch(): expected a SoundPatch SUBCLASS, got %r. "
                        "For an existing instance use materialize_sound_instance()."
                        % (patch_class,))
  progname = name or patch_class.__name__
  key = None
  if cache:
    try:
      key = (patch_class, progname, tuple(sorted(params.items())),
             None if bank is None else id(bank))
      hash(key)
    except TypeError:
      key = None                      # unhashable param -> uncached, never wrong
    if key is not None and key in _CACHE:
      return _CACHE[key]
  patch = patch_class(**params)
  result = materialize_sound_instance(patch, name=progname, bank=bank)
  if key is not None:
    _CACHE[key] = result
  return result
