###############################################################################
# ork.hypergraph.sound — hypersound: the singularity patch DSL (A1a).
#
# Layout:
#   dsl.py      — the SoundNode IR, the S.* op namespace (registry-extensible),
#                 and the SoundPatch authoring base. PURE PYTHON: importing it
#                 pulls in no engine module, so a patch can be traced and
#                 validated with no synth, no audio device and no display.
#   emitter.py  — the one-shot lowering: DAG -> channel allocation -> stages ->
#                 ProgramData / LayerData / DspBlockData / ControllerData, via
#                 the existing pyext surface. The ONLY file that imports lev2.
#   caps.py     — the krztypes.h fixed-array ceilings, checked in python before
#                 anything is appended (the pyext append paths either abort the
#                 process or do not check at all).
#
# Typical use:
#   from ork.hypergraph.sound import S, SoundPatch, materialize_sound_patch
#
#   class Blip(SoundPatch):
#     def __init__(self, cutoff=1200.0):
#       self.output(S.lowpass2(S.saw(), cutoff=S.p("cutoff", cutoff, unit="hz")),
#                   amp=S.adsr(0.005, 0.08, 0.6, 0.15))
#
#   snd = materialize_sound_patch(Blip)
#   synth.keyOn(60, 127, snd.program, None)     # keep snd (it owns the bank)
###############################################################################

from ork.hypergraph.sound.dsl import (
    S,
    BlockSpec,
    ControllerNode,
    ModRef,
    ParamDecl,
    SoundDslError,
    SoundNode,
    SoundPatch,
    SoundValue,
    KNOWN_DSP_BLOCK_CLASSES,
    KNOWN_CONTROLLER_CLASSES,
    postorder,
    use_counts,
)
from ork.hypergraph.sound.caps import SoundCapsError
from ork.hypergraph.sound.emitter import (
    MaterializedSound,
    SoundPlan,
    clear_sound_cache,
    execute_plan,
    materialize_sound_instance,
    materialize_sound_patch,
    plan_sound_patch,
)

__all__ = [
    # authoring
    "S", "SoundPatch", "SoundNode", "SoundValue", "ParamDecl", "ModRef",
    "ControllerNode", "BlockSpec",
    "KNOWN_DSP_BLOCK_CLASSES", "KNOWN_CONTROLLER_CLASSES",
    # lowering
    "materialize_sound_patch", "materialize_sound_instance",
    "plan_sound_patch", "execute_plan", "clear_sound_cache",
    "MaterializedSound", "SoundPlan",
    # graph utilities
    "postorder", "use_counts",
    # errors
    "SoundDslError", "SoundCapsError",
]
