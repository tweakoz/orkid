#!/usr/bin/env ork.python

"""
Complex Game Character HFSM - Deep Hierarchy Test
Uses FsmData/FsmInstance pattern
"""

import sys
import os
import argparse
import subprocess
import tempfile

from orkengine import core
core.appinit()

from orkengine.core import fsm

def create_complex_game_fsm():
    """Create a deep, complex game character FSM"""

    data = fsm.FsmData()

    # AI (root)
    # ├── ALIVE
    # │   ├── IDLE
    # │   │   ├── STAND
    # │   │   ├── LOOK_AROUND
    # │   │   └── FIDGET
    # │   ├── PATROL
    # │   │   ├── WALK
    # │   │   ├── WAIT
    # │   │   └── INVESTIGATE
    # │   ├── COMBAT
    # │   │   ├── ENGAGE
    # │   │   │   ├── CHASE
    # │   │   │   ├── ATTACK
    # │   │   │   └── FLANK
    # │   │   ├── DEFENSIVE
    # │   │   │   ├── BLOCK
    # │   │   │   ├── DODGE
    # │   │   │   └── RETREAT
    # │   │   └── SUPPORT
    # │   │       ├── HEAL_ALLY
    # │   │       └── CALL_BACKUP
    # │   └── INTERACT
    # │       ├── TALK
    # │       ├── TRADE
    # │       └── USE_OBJECT
    # └── DEAD
    #     ├── DYING
    #     └── CORPSE

    ai = data.createState(None, "AI")

    # === ALIVE branch ===
    alive = data.createState(ai, "ALIVE")

    # IDLE states (level 2)
    idle = data.createState(alive, "IDLE")
    stand = data.createState(idle, "STAND")
    look_around = data.createState(idle, "LOOK_AROUND")
    fidget = data.createState(idle, "FIDGET")

    # PATROL states (level 2)
    patrol = data.createState(alive, "PATROL")
    walk = data.createState(patrol, "WALK")
    wait = data.createState(patrol, "WAIT")
    investigate = data.createState(patrol, "INVESTIGATE")

    # COMBAT states (level 2)
    combat = data.createState(alive, "COMBAT")

    # ENGAGE substates (level 3)
    engage = data.createState(combat, "ENGAGE")
    chase = data.createState(engage, "CHASE")
    attack = data.createState(engage, "ATTACK")
    flank = data.createState(engage, "FLANK")

    # DEFENSIVE substates (level 3)
    defensive = data.createState(combat, "DEFENSIVE")
    block = data.createState(defensive, "BLOCK")
    dodge = data.createState(defensive, "DODGE")
    retreat = data.createState(defensive, "RETREAT")

    # SUPPORT substates (level 3)
    support = data.createState(combat, "SUPPORT")
    heal_ally = data.createState(support, "HEAL_ALLY")
    call_backup = data.createState(support, "CALL_BACKUP")

    # INTERACT states (level 2)
    interact = data.createState(alive, "INTERACT")
    talk = data.createState(interact, "TALK")
    trade = data.createState(interact, "TRADE")
    use_object = data.createState(interact, "USE_OBJECT")

    # === DEAD branch ===
    dead = data.createState(ai, "DEAD")
    dying = data.createState(dead, "DYING")
    corpse = data.createState(dead, "CORPSE")

    # =================== TRANSITIONS ===================

    # IDLE internal transitions
    data.addTransition(stand, "bored", look_around)
    data.addTransition(look_around, "bored", fidget)
    data.addTransition(fidget, "calm", stand)
    data.addTransition(look_around, "calm", stand)

    # IDLE -> PATROL
    data.addTransition(stand, "patrol_order", walk)
    data.addTransition(look_around, "patrol_order", walk)
    data.addTransition(fidget, "patrol_order", walk)

    # PATROL internal transitions
    data.addTransition(walk, "reached_point", wait)
    data.addTransition(wait, "timeout", walk)
    data.addTransition(walk, "heard_noise", investigate)
    data.addTransition(wait, "heard_noise", investigate)
    data.addTransition(investigate, "nothing_found", walk)

    # PATROL -> IDLE
    data.addTransition(walk, "stand_down", stand)
    data.addTransition(wait, "stand_down", stand)

    # PATROL -> COMBAT (enemy spotted)
    data.addTransition(walk, "enemy_spotted", chase)
    data.addTransition(wait, "enemy_spotted", chase)
    data.addTransition(investigate, "enemy_spotted", chase)

    # ENGAGE internal transitions
    data.addTransition(chase, "in_range", attack)
    data.addTransition(attack, "target_moved", chase)
    data.addTransition(chase, "flank_opportunity", flank)
    data.addTransition(flank, "in_range", attack)
    data.addTransition(attack, "need_position", flank)

    # ENGAGE -> DEFENSIVE
    data.addTransition(attack, "taking_damage", block)
    data.addTransition(chase, "ambushed", dodge)
    data.addTransition(flank, "detected", dodge)

    # DEFENSIVE internal transitions
    data.addTransition(block, "opening", dodge)
    data.addTransition(dodge, "safe", block)
    data.addTransition(block, "overwhelmed", retreat)
    data.addTransition(dodge, "overwhelmed", retreat)

    # DEFENSIVE -> ENGAGE
    data.addTransition(block, "counter_attack", attack)
    data.addTransition(dodge, "counter_attack", attack)
    data.addTransition(retreat, "regrouped", chase)

    # ENGAGE -> SUPPORT
    data.addTransition(attack, "ally_hurt", heal_ally)
    data.addTransition(chase, "need_help", call_backup)

    # DEFENSIVE -> SUPPORT
    data.addTransition(retreat, "ally_hurt", heal_ally)
    data.addTransition(retreat, "need_help", call_backup)

    # SUPPORT internal transitions
    data.addTransition(heal_ally, "healed", call_backup)
    data.addTransition(call_backup, "backup_arrived", heal_ally)

    # SUPPORT -> ENGAGE
    data.addTransition(heal_ally, "threat_close", attack)
    data.addTransition(call_backup, "threat_close", chase)

    # COMBAT -> IDLE (combat over)
    data.addTransition(attack, "target_dead", stand)
    data.addTransition(chase, "target_lost", look_around)
    data.addTransition(retreat, "escaped", stand)

    # IDLE -> INTERACT
    data.addTransition(stand, "npc_nearby", talk)
    data.addTransition(look_around, "item_found", use_object)

    # INTERACT internal transitions
    data.addTransition(talk, "offer_trade", trade)
    data.addTransition(trade, "trade_done", talk)
    data.addTransition(talk, "point_at_object", use_object)
    data.addTransition(use_object, "done", talk)

    # INTERACT -> IDLE
    data.addTransition(talk, "goodbye", stand)
    data.addTransition(trade, "goodbye", stand)
    data.addTransition(use_object, "interrupted", stand)

    # INTERACT -> COMBAT (attacked during interaction)
    data.addTransition(talk, "attacked", dodge)
    data.addTransition(trade, "attacked", dodge)
    data.addTransition(use_object, "attacked", dodge)

    # ANY ALIVE -> DEAD
    data.addTransition(stand, "fatal_damage", dying)
    data.addTransition(attack, "fatal_damage", dying)
    data.addTransition(block, "fatal_damage", dying)
    data.addTransition(retreat, "fatal_damage", dying)
    data.addTransition(chase, "fatal_damage", dying)

    # DEAD internal
    data.addTransition(dying, "death_anim_done", corpse)

    return data, stand  # Return FsmData and initial state


def visualize_fsm():
    """Generate DOT, convert to PNG, and open it"""
    print("\n=== Complex Game Character HFSM ===")

    data, initial_state = create_complex_game_fsm()

    # Create instance and start in STAND state
    inst = fsm.FsmInstance(data)
    inst.changeState(initial_state)
    inst.update()

    # Generate DOT with layout params (from instance to show current state)
    dot = inst.generateDot("GameCharacterAI", K=2.0, sep=30, size_w=24, size_h=18)

    # Write to temp file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.dot', delete=False) as f:
        dot_path = f.name
        f.write(dot)

    png_path = dot_path.replace('.dot', '.png')

    print(f"  DOT file: {dot_path}")
    print(f"  PNG file: {png_path}")
    print(f"  States: {len(data.states)}")

    # Convert to PNG using graphviz dot (hierarchical)
    result = subprocess.run(
        ["dot", "-Tpng", "-o", png_path, dot_path],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"  ERROR: dot command failed: {result.stderr}")
        return

    print("  PNG generated successfully")

    # Open the PNG (macOS)
    subprocess.run(["open", png_path])
    print("  Opened PNG viewer")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Complex Game HFSM Test")
    parser.add_argument("-v", "--visualize", action="store_true",
                        help="Generate and open FSM visualization as PNG")
    args = parser.parse_args()

    if args.visualize:
        visualize_fsm()
    else:
        print("Run with -v to visualize the complex game FSM")
