#!/usr/bin/env ork.python

"""
Complex Game Character HFSM - Deep Hierarchy Visualization
Uses FsmVizBuilder/FsmVisualizer for interactive PrimCanvas rendering
"""

from orkengine.core import vec4

from orkengine import core
core.appinit()

from ork.fsmviz import FsmVizBuilder, FsmVisualizer, COL_TEXT_SUB

###############################################################################
# Colors
###############################################################################

# Group nesting colors (darker = deeper)
GRP_ALIVE   = vec4(0.08, 0.12, 0.10, 0.95)
GRP_IDLE    = vec4(0.10, 0.16, 0.12, 0.90)
GRP_PATROL  = vec4(0.10, 0.14, 0.16, 0.90)
GRP_COMBAT  = vec4(0.14, 0.10, 0.10, 0.90)
GRP_ENGAGE  = vec4(0.16, 0.12, 0.12, 0.85)
GRP_DEFENSE = vec4(0.18, 0.12, 0.14, 0.85)
GRP_SUPPORT = vec4(0.14, 0.14, 0.10, 0.85)
GRP_INTERACT = vec4(0.10, 0.12, 0.16, 0.90)
GRP_DEAD    = vec4(0.15, 0.08, 0.08, 0.95)

GRP_BORDER  = vec4(0.25, 0.25, 0.30, 0.5)
GRP_HOOK    = vec4(0.50, 0.60, 0.70, 0.9)

# Leaf state colors
COL_STAND      = vec4(0.15, 0.40, 0.20, 1.0)
COL_LOOK       = vec4(0.20, 0.45, 0.25, 1.0)
COL_FIDGET     = vec4(0.25, 0.38, 0.22, 1.0)
COL_WALK       = vec4(0.20, 0.35, 0.50, 1.0)
COL_WAIT       = vec4(0.25, 0.40, 0.45, 1.0)
COL_INVESTIGATE = vec4(0.30, 0.45, 0.50, 1.0)
COL_CHASE      = vec4(0.55, 0.25, 0.20, 1.0)
COL_ATTACK     = vec4(0.65, 0.20, 0.15, 1.0)
COL_FLANK      = vec4(0.60, 0.30, 0.20, 1.0)
COL_BLOCK      = vec4(0.40, 0.25, 0.50, 1.0)
COL_DODGE      = vec4(0.45, 0.30, 0.55, 1.0)
COL_RETREAT    = vec4(0.50, 0.25, 0.45, 1.0)
COL_HEAL       = vec4(0.25, 0.50, 0.35, 1.0)
COL_BACKUP     = vec4(0.35, 0.45, 0.30, 1.0)
COL_TALK       = vec4(0.20, 0.35, 0.55, 1.0)
COL_TRADE      = vec4(0.30, 0.40, 0.50, 1.0)
COL_USE_OBJ    = vec4(0.25, 0.45, 0.45, 1.0)
COL_DYING      = vec4(0.55, 0.15, 0.15, 1.0)
COL_CORPSE     = vec4(0.35, 0.15, 0.15, 1.0)

# Arrow colors by category
A_IDLE     = vec4(0.40, 0.60, 0.40, 0.90)
A_PATROL   = vec4(0.40, 0.55, 0.70, 0.90)
A_COMBAT   = vec4(0.70, 0.35, 0.30, 0.90)
A_DEFENSE  = vec4(0.55, 0.40, 0.65, 0.90)
A_SUPPORT  = vec4(0.45, 0.60, 0.40, 0.90)
A_INTERACT = vec4(0.40, 0.50, 0.70, 0.90)
A_DEATH    = vec4(0.75, 0.20, 0.20, 0.95)
A_CROSS    = vec4(0.60, 0.55, 0.45, 0.90)

###############################################################################
# FSM Definition
###############################################################################

def build_game_character_fsm():
    b = FsmVizBuilder()

    root = b.state(None, "AI", hidden=True)

    # === ALIVE branch ===
    alive = b.state(root, "ALIVE",
                    group_color=GRP_ALIVE, border_color=GRP_BORDER)

    # -- IDLE group --
    idle = b.state(alive, "IDLE",
                   group_color=GRP_IDLE, border_color=GRP_BORDER)
    stand      = b.state(idle, "STAND",
                         pos=(0.08, 0.12), color=COL_STAND)
    look_around = b.state(idle, "LOOK_AROUND",
                          pos=(0.18, 0.12), color=COL_LOOK)
    fidget     = b.state(idle, "FIDGET",
                         pos=(0.28, 0.12), color=COL_FIDGET)

    # -- PATROL group --
    patrol = b.state(alive, "PATROL",
                     group_color=GRP_PATROL, border_color=GRP_BORDER)
    walk        = b.state(patrol, "WALK",
                          pos=(0.40, 0.12), color=COL_WALK)
    wait        = b.state(patrol, "WAIT",
                          pos=(0.50, 0.12), color=COL_WAIT)
    investigate = b.state(patrol, "INVESTIGATE",
                          pos=(0.60, 0.12), color=COL_INVESTIGATE)

    # -- COMBAT group --
    combat = b.state(alive, "COMBAT",
                     group_color=GRP_COMBAT, border_color=GRP_BORDER)

    # ENGAGE subgroup
    engage = b.state(combat, "ENGAGE",
                     group_color=GRP_ENGAGE, border_color=GRP_BORDER)
    chase  = b.state(engage, "CHASE",
                     pos=(0.15, 0.45), color=COL_CHASE)
    attack = b.state(engage, "ATTACK",
                     pos=(0.28, 0.45), color=COL_ATTACK)
    flank  = b.state(engage, "FLANK",
                     pos=(0.41, 0.45), color=COL_FLANK)

    # DEFENSIVE subgroup
    defensive = b.state(combat, "DEFENSIVE",
                        group_color=GRP_DEFENSE, border_color=GRP_BORDER)
    block   = b.state(defensive, "BLOCK",
                      pos=(0.15, 0.70), color=COL_BLOCK)
    dodge   = b.state(defensive, "DODGE",
                      pos=(0.28, 0.70), color=COL_DODGE)
    retreat = b.state(defensive, "RETREAT",
                      pos=(0.41, 0.70), color=COL_RETREAT)

    # SUPPORT subgroup
    support = b.state(combat, "SUPPORT",
                      group_color=GRP_SUPPORT, border_color=GRP_BORDER)
    heal_ally   = b.state(support, "HEAL_ALLY",
                          pos=(0.55, 0.58), color=COL_HEAL)
    call_backup = b.state(support, "CALL_BACKUP",
                          pos=(0.55, 0.72), color=COL_BACKUP)

    # -- INTERACT group --
    interact = b.state(alive, "INTERACT",
                       group_color=GRP_INTERACT, border_color=GRP_BORDER)
    talk       = b.state(interact, "TALK",
                         pos=(0.72, 0.12), color=COL_TALK)
    trade      = b.state(interact, "TRADE",
                         pos=(0.82, 0.12), color=COL_TRADE)
    use_object = b.state(interact, "USE_OBJECT",
                         pos=(0.92, 0.12), color=COL_USE_OBJ)

    # === DEAD branch ===
    dead = b.state(root, "DEAD",
                   group_color=GRP_DEAD, border_color=GRP_BORDER)
    dying  = b.state(dead, "DYING",
                     pos=(0.78, 0.70), color=COL_DYING)
    corpse = b.state(dead, "CORPSE",
                     pos=(0.90, 0.70), color=COL_CORPSE)

    # =================== TRANSITIONS ===================

    # IDLE internal
    b.transition(stand, "bored", look_around, color=A_IDLE)
    b.transition(look_around, "bored", fidget, color=A_IDLE)
    b.transition(fidget, "calm", stand, color=A_IDLE)
    b.transition(look_around, "calm", stand, color=A_IDLE)

    # IDLE -> PATROL
    b.transition(stand, "patrol_order", walk, color=A_CROSS)
    b.transition(look_around, "patrol_order", walk, color=A_CROSS)
    b.transition(fidget, "patrol_order", walk, color=A_CROSS)

    # PATROL internal
    b.transition(walk, "reached_point", wait, color=A_PATROL)
    b.transition(wait, "timeout", walk, color=A_PATROL)
    b.transition(walk, "heard_noise", investigate, color=A_PATROL)
    b.transition(wait, "heard_noise", investigate, color=A_PATROL)
    b.transition(investigate, "nothing_found", walk, color=A_PATROL)

    # PATROL -> IDLE
    b.transition(walk, "stand_down", stand, color=A_CROSS)
    b.transition(wait, "stand_down", stand, color=A_CROSS)

    # PATROL -> COMBAT (enemy spotted)
    b.transition(walk, "enemy_spotted", chase, color=A_COMBAT)
    b.transition(wait, "enemy_spotted", chase, color=A_COMBAT)
    b.transition(investigate, "enemy_spotted", chase, color=A_COMBAT)

    # ENGAGE internal
    b.transition(chase, "in_range", attack, color=A_COMBAT)
    b.transition(attack, "target_moved", chase, color=A_COMBAT)
    b.transition(chase, "flank_opportunity", flank, color=A_COMBAT)
    b.transition(flank, "in_range", attack, color=A_COMBAT)
    b.transition(attack, "need_position", flank, color=A_COMBAT)

    # ENGAGE -> DEFENSIVE
    b.transition(attack, "taking_damage", block, color=A_DEFENSE)
    b.transition(chase, "ambushed", dodge, color=A_DEFENSE)
    b.transition(flank, "detected", dodge, color=A_DEFENSE)

    # DEFENSIVE internal
    b.transition(block, "opening", dodge, color=A_DEFENSE)
    b.transition(dodge, "safe", block, color=A_DEFENSE)
    b.transition(block, "overwhelmed", retreat, color=A_DEFENSE)
    b.transition(dodge, "overwhelmed", retreat, color=A_DEFENSE)

    # DEFENSIVE -> ENGAGE
    b.transition(block, "counter_attack", attack, color=A_COMBAT)
    b.transition(dodge, "counter_attack", attack, color=A_COMBAT)
    b.transition(retreat, "regrouped", chase, color=A_COMBAT)

    # ENGAGE -> SUPPORT
    b.transition(attack, "ally_hurt", heal_ally, color=A_SUPPORT)
    b.transition(chase, "need_help", call_backup, color=A_SUPPORT)

    # DEFENSIVE -> SUPPORT
    b.transition(retreat, "ally_hurt", heal_ally, color=A_SUPPORT)
    b.transition(retreat, "need_help", call_backup, color=A_SUPPORT)

    # SUPPORT internal
    b.transition(heal_ally, "healed", call_backup, color=A_SUPPORT)
    b.transition(call_backup, "backup_arrived", heal_ally, color=A_SUPPORT)

    # SUPPORT -> ENGAGE
    b.transition(heal_ally, "threat_close", attack, color=A_COMBAT)
    b.transition(call_backup, "threat_close", chase, color=A_COMBAT)

    # COMBAT -> IDLE (combat over)
    b.transition(attack, "target_dead", stand, color=A_CROSS)
    b.transition(chase, "target_lost", look_around, color=A_CROSS)
    b.transition(retreat, "escaped", stand, color=A_CROSS)

    # IDLE -> INTERACT
    b.transition(stand, "npc_nearby", talk, color=A_INTERACT)
    b.transition(look_around, "item_found", use_object, color=A_INTERACT)

    # INTERACT internal
    b.transition(talk, "offer_trade", trade, color=A_INTERACT)
    b.transition(trade, "trade_done", talk, color=A_INTERACT)
    b.transition(talk, "point_at_object", use_object, color=A_INTERACT)
    b.transition(use_object, "done", talk, color=A_INTERACT)

    # INTERACT -> IDLE
    b.transition(talk, "goodbye", stand, color=A_CROSS)
    b.transition(trade, "goodbye", stand, color=A_CROSS)
    b.transition(use_object, "interrupted", stand, color=A_CROSS)

    # INTERACT -> COMBAT (attacked during interaction)
    b.transition(talk, "attacked", dodge, color=A_COMBAT)
    b.transition(trade, "attacked", dodge, color=A_COMBAT)
    b.transition(use_object, "attacked", dodge, color=A_COMBAT)

    # ANY ALIVE -> DEAD
    b.transition(stand, "fatal_damage", dying, color=A_DEATH)
    b.transition(attack, "fatal_damage", dying, color=A_DEATH)
    b.transition(block, "fatal_damage", dying, color=A_DEATH)
    b.transition(retreat, "fatal_damage", dying, color=A_DEATH)
    b.transition(chase, "fatal_damage", dying, color=A_DEATH)

    # DEAD internal
    b.transition(dying, "death_anim_done", corpse, color=A_DEATH)

    return b

###############################################################################
# Main
###############################################################################

if __name__ == "__main__":
    builder = build_game_character_fsm()

    app = FsmVisualizer(
        builder,
        title="Game Character AI — HFSM",
        subtitle="Idle / Patrol / Combat (Engage+Defensive+Support) / Interact / Dead",
        footer_lines=[
            ("22 leaf states, 5 hierarchy levels, 60+ transitions", COL_TEXT_SUB),
            ("Drag nodes to rearrange. Layout persists across runs.", COL_TEXT_SUB),
        ],
    )
    app.createEzApp(
        name="Game Character HFSM",
        width=1600,
        height=900,
    )
    app.ezapp.mainThreadLoop(on_iter=lambda: {})
