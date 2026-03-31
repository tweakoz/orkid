#!/usr/bin/env ork.python

"""
ECS Update-Thread HFSM Visualization
======================================

Builds the proposed ECS lifecycle FSM and visualizes it using ork.fsmviz.

    ROOT
    ├── INITIALIZED
    ├── EDIT_COMPOSED           (onEnter: compose,  onExit: decompose)
    │   └── EDIT_LINKED     (onEnter: link,     onExit: unlink)
    │       └── EDIT_STAGED (onEnter: stage,    onExit: unstage)
    │           └── EDIT
    ├── RUN_COMPOSED            (onEnter: compose,  onExit: decompose)
    │   └── RUN_LINKED      (onEnter: link,     onExit: unlink)
    │       └── RUN_STAGED  (onEnter: stage,    onExit: unstage)
    │           └── ACTIVATED (onEnter: activate, onExit: deactivate)
    │               ├── ACTIVE
    │               └── PAUSED
    └── TERMINATED

All transitions route through INITIALIZED — no direct EDIT<->ACTIVE.
"""

from orkengine.core import vec4

from orkengine import core
core.appinit()

from ork.fsmviz import FsmVizBuilder, FsmVisualizer, COL_TEXT_SUB

###############################################################################
# Colors
###############################################################################

# Edit branch — green-tinted, progressively lighter per nesting depth
EDIT_GROUP = [
    vec4(0.08, 0.14, 0.12, 0.95),
    vec4(0.10, 0.17, 0.14, 0.90),
    vec4(0.12, 0.20, 0.16, 0.85),
]
EDIT_BORDER = vec4(0.20, 0.40, 0.28, 0.5)
EDIT_HOOK   = vec4(0.55, 0.70, 0.60, 0.9)

# Run branch — purple-tinted, progressively lighter per nesting depth
RUN_GROUP = [
    vec4(0.10, 0.08, 0.16, 0.95),
    vec4(0.12, 0.10, 0.19, 0.90),
    vec4(0.14, 0.12, 0.22, 0.85),
    vec4(0.16, 0.14, 0.25, 0.80),
]
RUN_BORDER = vec4(0.28, 0.20, 0.42, 0.5)
RUN_HOOK   = vec4(0.60, 0.55, 0.75, 0.9)

# Leaf node colors
COL_INIT   = vec4(0.30, 0.35, 0.45, 1.0)
COL_TERM   = vec4(0.45, 0.15, 0.15, 1.0)
COL_EDIT   = vec4(0.15, 0.45, 0.25, 1.0)
COL_ACTIVE = vec4(0.18, 0.35, 0.60, 1.0)
COL_PAUSED = vec4(0.50, 0.40, 0.15, 1.0)

# Arrow colors
A_EDIT   = vec4(0.35, 0.75, 0.45, 0.95)
A_RUN    = vec4(0.40, 0.55, 0.85, 0.95)
A_STOP   = vec4(0.85, 0.45, 0.30, 0.95)
A_TERM   = vec4(0.65, 0.20, 0.20, 0.80)
A_PAUSE  = vec4(0.75, 0.60, 0.20, 0.95)
A_RESUME = vec4(0.40, 0.65, 0.85, 0.95)
A_INIT   = vec4(0.55, 0.55, 0.70, 0.95)

###############################################################################
# FSM Definition
###############################################################################

def build_ecs_update_fsm():
    b = FsmVizBuilder()

    # Root (hidden from diagram)
    root = b.state(None, "ROOT", hidden=True)

    # Center column
    new  = b.state(root, "NEW",
                   pos=(0.50, 0.18), color=vec4(0.35, 0.35, 0.50, 1.0))
    init = b.state(root, "INITIALIZED",
                   pos=(0.50, 0.38), color=COL_INIT)
    term = b.state(root, "TERMINATED",
                   pos=(0.50, 0.62), color=COL_TERM)

    # ── Edit branch (left column) ──
    edit_composed = b.state(root, "EDIT_COMPOSED",
                        group_color=EDIT_GROUP[0], border_color=EDIT_BORDER,
                        hooks="onEnter: compose    onExit: decompose",
                        hook_color=EDIT_HOOK)
    edit_linked = b.state(edit_composed, "EDIT_LINKED",
                          group_color=EDIT_GROUP[1], border_color=EDIT_BORDER,
                          hooks="onEnter: link    onExit: unlink",
                          hook_color=EDIT_HOOK)
    edit_staged = b.state(edit_linked, "EDIT_STAGED",
                          group_color=EDIT_GROUP[2], border_color=EDIT_BORDER,
                          hooks="onEnter: stage    onExit: unstage",
                          hook_color=EDIT_HOOK)
    edit = b.state(edit_staged, "EDIT",
                   pos=(0.20, 0.50), color=COL_EDIT)

    # ── Run branch (right column) ──
    run_composed = b.state(root, "RUN_COMPOSED",
                       group_color=RUN_GROUP[0], border_color=RUN_BORDER,
                       hooks="onEnter: compose    onExit: decompose",
                       hook_color=RUN_HOOK)
    run_linked = b.state(run_composed, "RUN_LINKED",
                         group_color=RUN_GROUP[1], border_color=RUN_BORDER,
                         hooks="onEnter: link    onExit: unlink",
                         hook_color=RUN_HOOK)
    run_staged = b.state(run_linked, "RUN_STAGED",
                         group_color=RUN_GROUP[2], border_color=RUN_BORDER,
                         hooks="onEnter: stage    onExit: unstage",
                         hook_color=RUN_HOOK)
    activated = b.state(run_staged, "ACTIVATED",
                        group_color=RUN_GROUP[3], border_color=RUN_BORDER,
                        hooks="onEnter: activate    onExit: deactivate",
                        hook_color=RUN_HOOK)
    active = b.state(activated, "ACTIVE",
                     pos=(0.75, 0.55), color=COL_ACTIVE)
    paused = b.state(activated, "PAUSED",
                     pos=(0.88, 0.55), color=COL_PAUSED)

    # ── Transitions ──

    # NEW -> INITIALIZED
    b.transition(new, "initialize", init,
                 color=A_INIT, src_side="bottom", dst_side="top",
                 label_offset=(6, -4))

    # Bringup (all route through INITIALIZED)
    b.transition(init, "edit", edit,
                 color=A_EDIT, src_side="left", dst_side="right",
                 label_offset=(-35, -14))
    b.transition(init, "run", active,
                 color=A_RUN, src_side="right", dst_side="left",
                 label_offset=(-22, -14))

    # Teardown to INITIALIZED
    b.transition(edit, "stop", init,
                 color=A_STOP, src_side="right", dst_side="left",
                 label_offset=(-28, 8))
    b.transition(active, "stop", init,
                 color=A_STOP, src_side="left", dst_side="right",
                 label_offset=(-28, 8))
    b.transition(paused, "stop", init,
                 color=A_STOP, src_side="left", dst_side="bottom",
                 label_offset=(-28, 8))

    # Pause / resume (within ACTIVATED group)
    b.transition(active, "pause", paused,
                 color=A_PAUSE, src_side="right", dst_side="left",
                 label_offset=(-30, -14))
    b.transition(paused, "resume", active,
                 color=A_RESUME, src_side="left", dst_side="right",
                 label_offset=(-34, 8))

    # Terminate
    b.transition(init, "terminate", term,
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

COL_GPU_IDLE   = vec4(0.30, 0.30, 0.40, 1.0)
COL_GPU_ACTIVE = vec4(0.20, 0.38, 0.55, 1.0)
COL_GPU_TERM   = vec4(0.45, 0.15, 0.15, 1.0)

GPU_GROUP = [
    vec4(0.10, 0.10, 0.18, 0.95),
    vec4(0.12, 0.12, 0.21, 0.90),
]
GPU_BORDER = vec4(0.25, 0.25, 0.45, 0.5)
GPU_HOOK   = vec4(0.55, 0.60, 0.80, 0.9)

A_GPU_INIT    = vec4(0.40, 0.65, 0.80, 0.95)
A_GPU_EXIT    = vec4(0.80, 0.50, 0.35, 0.95)
A_GPU_TERM    = vec4(0.65, 0.20, 0.20, 0.80)


def build_ecs_gpu_fsm():
    b = FsmVizBuilder()

    root = b.state(None, "ROOT", hidden=True)

    gpu_idle = b.state(root, "GPU_IDLE",
                       pos=(0.25, 0.35), color=COL_GPU_IDLE)

    gpu_active_group = b.state(root, "GPU_ACTIVE",
                               group_color=GPU_GROUP[0], border_color=GPU_BORDER,
                               hooks="onEnter: gpuInit + gpuLink    onExit: gpuExit",
                               hook_color=GPU_HOOK)
    gpu_running = b.state(gpu_active_group, "GPU_RUNNING",
                          pos=(0.65, 0.35), color=COL_GPU_ACTIVE)

    gpu_term = b.state(root, "GPU_TERMINATED",
                       pos=(0.45, 0.75), color=COL_GPU_TERM)

    b.transition(gpu_idle, "systems_ready", gpu_running,
                 color=A_GPU_INIT)
    b.transition(gpu_running, "systems_teardown", gpu_idle,
                 color=A_GPU_EXIT)
    b.transition(gpu_idle, "shutdown", gpu_term,
                 color=A_GPU_TERM)
    b.transition(gpu_running, "shutdown", gpu_term,
                 color=A_GPU_TERM)

    return b


###############################################################################
# Main
###############################################################################

if __name__ == "__main__":
    app = FsmVisualizer(
        tabs=[
            {
                'name': "ECS Update-Thread HFSM",
                'builder': build_ecs_update_fsm(),
                'subtitle': "All transitions route through INITIALIZED",
                'footer_lines': [
                    ("Lifecycle: compose -> link -> stage -> [activate]", COL_TEXT_SUB),
                    ("Teardown:  [deactivate] -> unstage -> unlink -> decompose", COL_TEXT_SUB),
                ],
            },
            {
                'name': "ECS GPU-Thread FSM",
                'builder': build_ecs_gpu_fsm(),
                'subtitle': "Follows update thread — synchronous exit barrier before decompose",
                'footer_lines': [
                    ("GPU_IDLE: waiting for update thread to compose+link", COL_TEXT_SUB),
                    ("GPU_ACTIVE: gpuInit+gpuLink on enter, gpuExit on exit", COL_TEXT_SUB),
                    ("systems_teardown: synchronous — update waits for gpuExit", COL_TEXT_SUB),
                ],
            },
        ],
    )
    app.createEzApp(
        name="ECS FSM Visualizer",
        width=1440,
        height=820,
        ssaa=2
    )
    app.ezapp.mainThreadLoop(on_iter=lambda: {})
