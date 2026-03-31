#!/usr/bin/env ork.python

"""
ECS Current FSM Visualization
===============================

Visualizes the ACTUAL current ECS FSM (flat, not HFSM).

Update Thread FSM:
    ROOT (flat — all siblings)
    ├── READY     (onEnter: conditional init/compose/link OR unstage OR deactivate+unstage)
    ├── EDIT      (onEnter: conditional stage OR deactivate)
    ├── ACTIVE    (onEnter: activate)
    ├── PAUSED
    └── TERMINATED (onEnter: deactivate, unstage, unlink, decompose, uninitialize)

Transitions:
    NEW → READY → EDIT                (stageSimulation)
    NEW → READY → EDIT → ACTIVE       (startSimulation)
    ACTIVE → EDIT                      (stopSimulation)
    ACTIVE ↔ PAUSED
    any → TERMINATED
"""

from orkengine.core import vec4

from orkengine import core
core.appinit()

from ork.fsmviz import FsmVizBuilder, FsmVisualizer, COL_TEXT_SUB

###############################################################################
# Colors
###############################################################################

COL_READY  = vec4(0.25, 0.30, 0.40, 1.0)
COL_EDIT   = vec4(0.15, 0.45, 0.25, 1.0)
COL_ACTIVE = vec4(0.18, 0.35, 0.60, 1.0)
COL_PAUSED = vec4(0.50, 0.40, 0.15, 1.0)
COL_TERM   = vec4(0.45, 0.15, 0.15, 1.0)

A_STAGE  = vec4(0.35, 0.75, 0.45, 0.95)
A_START  = vec4(0.40, 0.55, 0.85, 0.95)
A_STOP   = vec4(0.85, 0.45, 0.30, 0.95)
A_TERM   = vec4(0.65, 0.20, 0.20, 0.80)
A_PAUSE  = vec4(0.75, 0.60, 0.20, 0.95)
A_RESUME = vec4(0.40, 0.65, 0.85, 0.95)
A_INIT   = vec4(0.55, 0.55, 0.70, 0.95)

###############################################################################
# Update Thread FSM
###############################################################################

def build_current_update_fsm():
    b = FsmVizBuilder()

    root = b.state(None, "ROOT", hidden=True)

    ready  = b.state(root, "READY",
                     pos=(0.50, 0.20), color=COL_READY)
    edit   = b.state(root, "EDIT",
                     pos=(0.25, 0.50), color=COL_EDIT)
    active = b.state(root, "ACTIVE",
                     pos=(0.65, 0.50), color=COL_ACTIVE)
    paused = b.state(root, "PAUSED",
                     pos=(0.85, 0.50), color=COL_PAUSED)
    term   = b.state(root, "TERMINATED",
                     pos=(0.50, 0.80), color=COL_TERM)

    # NEW → READY (first entry: initialize + compose + link)
    # Implicit: FSM starts with currentState=nullptr, first changeState goes to READY

    # READY → EDIT (stage)
    b.transition(ready, "stage", edit,
                 color=A_STAGE, src_side="left", dst_side="top",
                 label_offset=(-30, -14))

    # READY → EDIT → ACTIVE (stage then activate) — for startSimulation from NEW
    # (goes through READY → EDIT → ACTIVE as 3 queued state changes)

    # EDIT → ACTIVE (activate)
    b.transition(edit, "start", active,
                 color=A_START, src_side="right", dst_side="left",
                 label_offset=(-25, -14))

    # ACTIVE → EDIT (deactivate) — stopSimulation
    b.transition(active, "stop", edit,
                 color=A_STOP, src_side="left", dst_side="right",
                 label_offset=(-25, 8))

    # EDIT → READY (unstage) — for re-staging
    b.transition(edit, "unstage", ready,
                 color=A_STOP, src_side="top", dst_side="left",
                 label_offset=(-35, 8))

    # ACTIVE → READY (deactivate + unstage) — direct
    b.transition(active, "reset", ready,
                 color=A_STOP, src_side="top", dst_side="right",
                 label_offset=(6, 8))

    # Pause / resume
    b.transition(active, "pause", paused,
                 color=A_PAUSE, src_side="right", dst_side="left",
                 label_offset=(-30, -14))
    b.transition(paused, "resume", active,
                 color=A_RESUME, src_side="left", dst_side="right",
                 label_offset=(-34, 8))

    # Terminate
    b.transition(ready, "terminate", term,
                 color=A_TERM, src_side="bottom", dst_side="top",
                 label_offset=(6, -4))
    b.transition(edit, "terminate", term,
                 color=A_TERM, src_side="bottom", dst_side="left",
                 label_offset=(-10, 8))
    b.transition(active, "terminate", term,
                 color=A_TERM, src_side="bottom", dst_side="right",
                 label_offset=(-10, 8))

    return b

###############################################################################
# GPU Thread FSM
###############################################################################

COL_GPU_INIT   = vec4(0.30, 0.30, 0.40, 1.0)
COL_GPU_READY  = vec4(0.20, 0.38, 0.55, 1.0)
COL_GPU_TERM   = vec4(0.45, 0.15, 0.15, 1.0)

A_GPU_INIT    = vec4(0.40, 0.65, 0.80, 0.95)
A_GPU_REINIT  = vec4(0.80, 0.50, 0.35, 0.95)
A_GPU_TERM    = vec4(0.65, 0.20, 0.20, 0.80)


def build_current_gpu_fsm():
    b = FsmVizBuilder()

    root = b.state(None, "ROOT", hidden=True)

    gpu_init = b.state(root, "GPU_INIT",
                       pos=(0.25, 0.35), color=COL_GPU_INIT)
    gpu_ready = b.state(root, "GPU_READY",
                        pos=(0.65, 0.35), color=COL_GPU_READY)
    gpu_term = b.state(root, "GPU_TERMINATED",
                       pos=(0.45, 0.75), color=COL_GPU_TERM)

    # GPU_INIT → GPU_READY (gpuInit + gpuLink on all systems)
    b.transition(gpu_init, "needsGpuInit", gpu_ready,
                 color=A_GPU_INIT)

    # GPU_READY → GPU_INIT (re-init triggered by _needsGpuInit flag)
    b.transition(gpu_ready, "re-init", gpu_init,
                 color=A_GPU_REINIT)

    # Terminate
    b.transition(gpu_init, "shutdown", gpu_term,
                 color=A_GPU_TERM)
    b.transition(gpu_ready, "shutdown", gpu_term,
                 color=A_GPU_TERM)

    return b


###############################################################################
# Main
###############################################################################

if __name__ == "__main__":
    app = FsmVisualizer(
        tabs=[
            {
                'name': "ECS Update Thread (current)",
                'builder': build_current_update_fsm(),
                'subtitle': "Flat FSM — conditional logic in onEnter handlers",
                'footer_lines': [
                    ("READY.onEnter: init+compose+link (first) OR unstage (from EDIT) OR deact+unstage (from ACTIVE)", COL_TEXT_SUB),
                    ("EDIT.onEnter: stage (from READY) OR deactivate (from ACTIVE)", COL_TEXT_SUB),
                    ("ACTIVE.onEnter: activate", COL_TEXT_SUB),
                    ("TERMINATED.onEnter: deactivate, unstage, unlink, decompose, uninitialize", COL_TEXT_SUB),
                ],
            },
            {
                'name': "ECS GPU Thread (current)",
                'builder': build_current_gpu_fsm(),
                'subtitle': "Flat FSM — _needsGpuInit flag drives transitions",
                'footer_lines': [
                    ("GPU_INIT: polls _needsGpuInit, runs gpuInit+gpuLink on all systems", COL_TEXT_SUB),
                    ("GPU_READY: polls _needsGpuInit for re-init, else gpuUpdate all systems", COL_TEXT_SUB),
                    ("No GPU barrier — no synchronization with update thread teardown", COL_TEXT_SUB),
                ],
            },
        ],
    )
    app.createEzApp(
        name="ECS FSM (Current) Visualizer",
        width=1440,
        height=820,
        ssaa=2
    )
    app.ezapp.mainThreadLoop(on_iter=lambda: {})
