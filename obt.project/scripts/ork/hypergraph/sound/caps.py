###############################################################################
# ork.hypergraph.sound.caps — the singularity fixed-array ceilings, enforced in
# PYTHON, BEFORE anything is appended to a LayerData.
#
# WHY THIS FILE EXISTS: the ceilings below are C++ `static constexpr int`s in
# ork/lev2/aud/singularity/krztypes.h — they are NOT exposed to python, and the
# pyext append entry points do not all guard against them:
#
#   LayerData.appendStage      -> AlgData::appendStage OrkAsserts at 17 (a HARD
#                                 abort of the whole process, no python traceback)
#   DspStageData.appendDspBlock-> NO check at all (defect #16): python can push
#                                 past kmaxdspblocksperstage and silently overrun
#                                 the fixed `DspBlock::_dspchannel[32]` arrays.
#   LayerData.appendController -> NO check: writes ControlBlockData::
#                                 _controller_datas[kmaxctrlperblock] unguarded.
#
# So the emitter validates a fully-planned patch against these numbers and raises
# SoundCapsError with a named, actionable message BEFORE it touches the engine.
# A capped patch therefore leaves NO half-built ProgramData behind.
#
# These literals MIRROR C++ constants. If krztypes.h changes, change them here.
###############################################################################


# krztypes.h:33  kmaxdspblocksperstage = 32   (vertical dim of a layer's dsp grid)
KMAX_DSP_BLOCKS_PER_STAGE = 32
# krztypes.h:34  kmaxdspstagesperlayer = 16   (horizontal dim of a layer's dsp grid)
KMAX_DSP_STAGES_PER_LAYER = 16
# krztypes.h:35  kmaxctrlperblock = 32        (ControlBlockData::_controller_datas[])
KMAX_CONTROLLERS_PER_LAYER = 32
# krztypes.h:36  kmaxparmperblock = 32        (DspBlockData::addParam ceiling)
KMAX_PARAMS_PER_BLOCK = 32
# DspBuffer::_channels[] is sized kmaxdspblocksperstage (dspbuffer.cpp:30) and
# DspBuffer::channel() wraps modulo that — an out-of-range channel index would
# ALIAS another signal instead of failing, so the emitter must never mint one.
KMAX_DSP_CHANNELS = 32
# channels 0 and 1 are the layer's stereo OUTPUT: Layer::beginCompute zeroes only
# those two and Layer's mix path reads only those two (layer.cpp:281-282, 339-340).
# Everything above them is scratch the emitter is free to allocate.
NUM_RESERVED_OUTPUT_CHANNELS = 2


class SoundCapsError(RuntimeError):
  """A hypersound patch would overrun a singularity fixed-array ceiling. Raised
  BEFORE any engine object is mutated — the ceilings are hard C++ array bounds,
  not soft budgets."""


def check_stage_count(nstages, patch_name):
  if nstages > KMAX_DSP_STAGES_PER_LAYER:
    raise SoundCapsError(
        "hypersound patch '%s' plans %d dsp stages but a singularity LayerData holds at most "
        "%d (krztypes.h kmaxdspstagesperlayer). Each parallel branch costs stages: a chain of "
        "unary ops on one channel shares ONE stage, but every sum/fork mints new ones. Reduce "
        "the branch count, or split the patch across layers."
        % (patch_name, nstages, KMAX_DSP_STAGES_PER_LAYER))


def check_block_count(nblocks, stage_name, patch_name):
  if nblocks > KMAX_DSP_BLOCKS_PER_STAGE:
    raise SoundCapsError(
        "hypersound patch '%s' plans %d dsp blocks in stage '%s' but a DspStageData holds at "
        "most %d (krztypes.h kmaxdspblocksperstage). NOTE: the pyext appendDspBlock does NOT "
        "check this (defect #16) — exceeding it overruns DspBlock::_dspchannel[32]."
        % (patch_name, nblocks, stage_name, KMAX_DSP_BLOCKS_PER_STAGE))


def check_controller_count(ncontrollers, patch_name):
  if ncontrollers > KMAX_CONTROLLERS_PER_LAYER:
    raise SoundCapsError(
        "hypersound patch '%s' plans %d controllers but a ControlBlockData holds at most %d "
        "(krztypes.h kmaxctrlperblock). appendController does not bounds-check."
        % (patch_name, ncontrollers, KMAX_CONTROLLERS_PER_LAYER))


def check_param_count(nparams, block_name, patch_name):
  if nparams > KMAX_PARAMS_PER_BLOCK:
    raise SoundCapsError(
        "hypersound patch '%s' sets %d params on block '%s' but a DspBlockData holds at most %d "
        "(krztypes.h kmaxparmperblock)."
        % (patch_name, nparams, block_name, KMAX_PARAMS_PER_BLOCK))


def check_channel_index(channel, patch_name):
  if channel < 0 or channel >= KMAX_DSP_CHANNELS:
    raise SoundCapsError(
        "hypersound patch '%s' allocated dsp channel %d, outside [0,%d). DspBuffer::channel() "
        "wraps modulo %d, so this would silently ALIAS another signal."
        % (patch_name, channel, KMAX_DSP_CHANNELS, KMAX_DSP_CHANNELS))


def check_channels_available(nwanted, patch_name):
  navail = KMAX_DSP_CHANNELS - NUM_RESERVED_OUTPUT_CHANNELS
  if nwanted > navail:
    raise SoundCapsError(
        "hypersound patch '%s' needs %d simultaneously-live dsp channels but only %d are "
        "available (%d total minus the %d reserved for the layer's stereo output). Too many "
        "signals alive at once — narrow the patch."
        % (patch_name, nwanted, navail, KMAX_DSP_CHANNELS, NUM_RESERVED_OUTPUT_CHANNELS))
