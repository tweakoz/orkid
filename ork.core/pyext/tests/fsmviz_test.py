#!/usr/bin/env ork.python

"""
Elevator HFSM Visualization
=============================

Models a building elevator with:
  - IDLE at optimal floor (lobby)
  - Door open/close cycle
  - Moving between floors
  - Emergency stop
  - Maintenance/out-of-service mode

HFSM Hierarchy:
    ROOT
    ├── OFF
    ├── OPERATIONAL          (onEnter: selfTest  onExit: safeStop)
    │   ├── IDLE_GROUP       (onEnter: gotoIdleFloor)
    │   │   └── IDLE
    │   ├── DOOR_GROUP       (onEnter: chime  onExit: clearTimer)
    │   │   ├── OPENING
    │   │   ├── OPEN
    │   │   └── CLOSING
    │   ├── MOVING_GROUP     (onEnter: lockDoors  onExit: unlockDoors)
    │   │   ├── ACCELERATING
    │   │   ├── CRUISING
    │   │   └── DECELERATING
    │   └── E_STOP
    └── MAINTENANCE          (onEnter: lockout  onExit: clearLockout)
"""

from orkengine.core import vec4

from orkengine import core
core.appinit()

from ork.fsmviz import FsmVizBuilder, FsmVisualizer, COL_TEXT_SUB

###############################################################################
# Colors
###############################################################################

# Operational group — blue-tinted nesting
OP_GROUP = [
    vec4(0.08, 0.10, 0.18, 0.95),  # OPERATIONAL
    vec4(0.10, 0.12, 0.21, 0.90),  # inner groups
    vec4(0.12, 0.14, 0.24, 0.85),  # innermost
]
OP_BORDER = vec4(0.20, 0.25, 0.45, 0.5)
OP_HOOK   = vec4(0.50, 0.60, 0.80, 0.9)

# Leaf colors
COL_OFF     = vec4(0.30, 0.30, 0.35, 1.0)
COL_IDLE    = vec4(0.15, 0.40, 0.25, 1.0)
COL_OPENING = vec4(0.20, 0.45, 0.55, 1.0)
COL_OPEN    = vec4(0.25, 0.55, 0.40, 1.0)
COL_CLOSING = vec4(0.35, 0.45, 0.50, 1.0)
COL_ACCEL   = vec4(0.20, 0.30, 0.60, 1.0)
COL_CRUISE  = vec4(0.18, 0.35, 0.65, 1.0)
COL_DECEL   = vec4(0.25, 0.30, 0.55, 1.0)
COL_ESTOP   = vec4(0.60, 0.15, 0.15, 1.0)
COL_MAINT   = vec4(0.55, 0.40, 0.10, 1.0)

# Arrow colors
A_POWER   = vec4(0.50, 0.70, 0.50, 0.95)
A_CALL    = vec4(0.40, 0.55, 0.85, 0.95)
A_DOOR    = vec4(0.35, 0.70, 0.65, 0.95)
A_MOVE    = vec4(0.45, 0.50, 0.80, 0.95)
A_ARRIVE  = vec4(0.55, 0.75, 0.45, 0.95)
A_ESTOP   = vec4(0.85, 0.25, 0.25, 0.95)
A_RESUME  = vec4(0.50, 0.65, 0.50, 0.95)
A_MAINT   = vec4(0.75, 0.60, 0.20, 0.95)
A_TIMEOUT = vec4(0.55, 0.55, 0.65, 0.90)
A_IDLE    = vec4(0.40, 0.60, 0.40, 0.90)

###############################################################################
# FSM Definition
###############################################################################

def build_elevator_fsm():
    b = FsmVizBuilder()

    root = b.state(None, "ROOT", hidden=True)

    # ── Top-level states ──
    off   = b.state(root, "OFF",
                    pos=(0.08, 0.15), color=COL_OFF)
    maint = b.state(root, "MAINTENANCE",
                    pos=(0.08, 0.85), color=COL_MAINT)

    # ── OPERATIONAL group ──
    operational = b.state(root, "OPERATIONAL",
                          group_color=OP_GROUP[0], border_color=OP_BORDER,
                          hooks="onEnter: selfTest    onExit: safeStop",
                          hook_color=OP_HOOK)

    # IDLE group
    idle_group = b.state(operational, "IDLE_GROUP",
                         group_color=OP_GROUP[1], border_color=OP_BORDER,
                         hooks="onEnter: gotoIdleFloor",
                         hook_color=OP_HOOK)
    idle = b.state(idle_group, "IDLE",
                   pos=(0.35, 0.20), color=COL_IDLE)

    # DOOR group
    door_group = b.state(operational, "DOOR_GROUP",
                         group_color=OP_GROUP[1], border_color=OP_BORDER,
                         hooks="onEnter: chime    onExit: clearTimer",
                         hook_color=OP_HOOK)
    opening = b.state(door_group, "OPENING",
                      pos=(0.58, 0.20), color=COL_OPENING)
    door_open = b.state(door_group, "OPEN",
                        pos=(0.72, 0.20), color=COL_OPEN)
    closing = b.state(door_group, "CLOSING",
                      pos=(0.88, 0.20), color=COL_CLOSING)

    # MOVING group
    moving_group = b.state(operational, "MOVING_GROUP",
                           group_color=OP_GROUP[1], border_color=OP_BORDER,
                           hooks="onEnter: lockDoors    onExit: unlockDoors",
                           hook_color=OP_HOOK)
    accel   = b.state(moving_group, "ACCELERATING",
                      pos=(0.50, 0.60), color=COL_ACCEL)
    cruise  = b.state(moving_group, "CRUISING",
                      pos=(0.68, 0.60), color=COL_CRUISE)
    decel   = b.state(moving_group, "DECELERATING",
                      pos=(0.86, 0.60), color=COL_DECEL)

    # E_STOP (direct child of OPERATIONAL, not in a subgroup)
    estop = b.state(operational, "E_STOP",
                    pos=(0.35, 0.60), color=COL_ESTOP)

    # ── Transitions ──

    # Power on/off
    b.transition(off, "powerOn", idle,
                 color=A_POWER, src_side="right", dst_side="top",
                 label_offset=(0, -14))

    # Call arrives while idle -> open doors
    b.transition(idle, "call", opening,
                 color=A_CALL, src_side="right", dst_side="left",
                 label_offset=(-20, -14))

    # Door cycle: opening -> open -> closing
    b.transition(opening, "opened", door_open,
                 color=A_DOOR, src_side="right", dst_side="left",
                 label_offset=(-28, -14))
    b.transition(door_open, "timeout", closing,
                 color=A_TIMEOUT, src_side="right", dst_side="left",
                 label_offset=(-30, -14))

    # Obstruction while closing -> reopen
    b.transition(closing, "obstruction", opening,
                 color=A_DOOR, src_side="top", dst_side="bottom",
                 label_offset=(0, 4))

    # Door closed, have destination -> move
    b.transition(closing, "closed+dest", accel,
                 color=A_MOVE, src_side="bottom", dst_side="right",
                 label_offset=(10, 8))

    # Door closed, no destination -> idle
    b.transition(closing, "closed", idle,
                 color=A_IDLE, src_side="left", dst_side="right",
                 label_offset=(-28, 8))

    # Moving: accel -> cruise -> decel
    b.transition(accel, "atSpeed", cruise,
                 color=A_MOVE, src_side="right", dst_side="left",
                 label_offset=(-30, -14))
    b.transition(cruise, "nearFloor", decel,
                 color=A_MOVE, src_side="right", dst_side="left",
                 label_offset=(-36, -14))

    # Arrived at floor -> open doors
    b.transition(decel, "arrived", opening,
                 color=A_ARRIVE, src_side="top", dst_side="bottom",
                 label_offset=(10, 4))

    # Emergency stop from any moving state
    b.transition(accel, "emergency", estop,
                 color=A_ESTOP, src_side="left", dst_side="right",
                 label_offset=(-38, -14))
    b.transition(cruise, "emergency", estop,
                 color=A_ESTOP, src_side="left", dst_side="top",
                 label_offset=(-38, 8))

    # Resume from e-stop
    b.transition(estop, "resume", idle,
                 color=A_RESUME, src_side="top", dst_side="bottom",
                 label_offset=(6, 4))

    # Maintenance mode (from off)
    b.transition(off, "maintain", maint,
                 color=A_MAINT, src_side="bottom", dst_side="top",
                 label_offset=(6, -4))
    b.transition(maint, "release", off,
                 color=A_POWER, src_side="top", dst_side="bottom",
                 label_offset=(-40, 4))

    # Shutdown from idle
    b.transition(idle, "powerOff", off,
                 color=A_POWER, src_side="left", dst_side="right",
                 label_offset=(-36, 8))

    return b

###############################################################################
# Main
###############################################################################

if __name__ == "__main__":
    builder = build_elevator_fsm()

    app = FsmVisualizer(
        builder,
        title="Elevator HFSM",
        subtitle="Door cycle, floor movement, emergency stop, maintenance",
        footer_lines=[
            ("Idle floor: lobby (ground). Doors auto-close after timeout.", COL_TEXT_SUB),
            ("E_STOP halts car immediately. Resume returns to IDLE.", COL_TEXT_SUB),
        ],
    )
    app.createEzApp(
        name="Elevator FSM",
        width=1440,
        height=820,
    )
    app.ezapp.mainThreadLoop(on_iter=lambda: {})
