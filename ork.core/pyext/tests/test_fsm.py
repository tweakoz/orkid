#!/usr/bin/env ork.python

"""
Test for Hierarchical Finite State Machine (HFSM)
FsmData: Shared state machine definition
FsmInstance: Per-entity runtime state
"""

import sys
import os
import argparse
import subprocess
import tempfile

from orkengine import core
core.appinit()

from orkengine.core import fsm

def test_basic_fsm():
    """Test basic state machine operations"""
    print("\n=== test_basic_fsm ===")

    # Create shared data (definition)
    data = fsm.FsmData()

    # Create states
    idle = data.createState(None, "idle")
    running = data.createState(None, "running")

    assert idle is not None
    assert running is not None
    assert idle.name == "idle"
    assert running.name == "running"

    # Add transitions
    data.addTransition(idle, "start", running)
    data.addTransition(running, "stop", idle)

    # Create instance
    inst = fsm.FsmInstance(data)

    # Start in idle
    inst.changeState(idle)
    inst.update()
    assert inst.currentState == idle
    print(f"  Current state: {inst.currentState.name}")

    # Transition to running
    inst.sendEvent("start")
    inst.update()
    assert inst.currentState == running
    print(f"  After 'start': {inst.currentState.name}")

    # Transition back to idle
    inst.sendEvent("stop")
    inst.update()
    assert inst.currentState == idle
    print(f"  After 'stop': {inst.currentState.name}")

    print("  PASSED")

def test_hierarchical_fsm():
    """Test hierarchical state machine with nested states"""
    print("\n=== test_hierarchical_fsm ===")

    data = fsm.FsmData()

    # Create hierarchy: root -> (sa -> (s1, s2), sb -> s3)
    root = data.createState(None, "root")
    sa = data.createState(root, "sa")
    sb = data.createState(root, "sb")
    s1 = data.createState(sa, "s1")
    s2 = data.createState(sa, "s2")
    s3 = data.createState(sb, "s3")

    # Check hierarchy
    assert s1.parent == sa
    assert s2.parent == sa
    assert s3.parent == sb
    assert sa.parent == root
    assert sb.parent == root
    assert root.parent is None

    # Add transitions
    data.addTransition(s1, "e1to2", s2)
    data.addTransition(s2, "e2to3", s3)

    # Track callbacks - callbacks receive instance
    log = []

    s1.onEnter = lambda inst: log.append("s1.enter")
    s1.onExit = lambda inst: log.append("s1.exit")
    s2.onEnter = lambda inst: log.append("s2.enter")
    s2.onExit = lambda inst: log.append("s2.exit")
    s3.onEnter = lambda inst: log.append("s3.enter")
    sa.onEnter = lambda inst: log.append("sa.enter")
    sa.onExit = lambda inst: log.append("sa.exit")
    sb.onEnter = lambda inst: log.append("sb.enter")

    # Create instance
    inst = fsm.FsmInstance(data)

    # Start at s1
    inst.changeState(s1)
    inst.update()
    print(f"  Log after entering s1: {log}")
    assert "sa.enter" in log
    assert "s1.enter" in log

    # Transition within same parent (s1 -> s2)
    log.clear()
    inst.sendEvent("e1to2")
    inst.update()
    print(f"  Log after s1->s2: {log}")
    assert "s1.exit" in log
    assert "s2.enter" in log
    assert "sa.exit" not in log  # Parent should not exit

    # Transition across hierarchy (s2 -> s3)
    log.clear()
    inst.sendEvent("e2to3")
    inst.update()
    print(f"  Log after s2->s3: {log}")
    assert "s2.exit" in log
    assert "sa.exit" in log
    assert "sb.enter" in log
    assert "s3.enter" in log

    print("  PASSED")

def test_token_events():
    """Test using explicit event tokens"""
    print("\n=== test_token_events ===")

    data = fsm.FsmData()

    idle = data.createState(None, "idle")
    active = data.createState(None, "active")

    # Create event tokens
    activate = fsm.event("activate")
    deactivate = fsm.event("deactivate")

    print(f"  activate token: 0x{activate:x}")
    print(f"  deactivate token: 0x{deactivate:x}")

    # Use tokens for transitions
    data.addTransition(idle, activate, active)
    data.addTransition(active, deactivate, idle)

    inst = fsm.FsmInstance(data)
    inst.changeState(idle)
    inst.update()
    assert inst.currentState == idle

    # Use token to send event
    inst.sendEvent(activate)
    inst.update()
    assert inst.currentState == active
    print(f"  After activate: {inst.currentState.name}")

    inst.sendEvent(deactivate)
    inst.update()
    assert inst.currentState == idle
    print(f"  After deactivate: {inst.currentState.name}")

    print("  PASSED")

def test_predicated_transition():
    """Test predicated transitions with guard conditions"""
    print("\n=== test_predicated_transition ===")

    data = fsm.FsmData()

    idle = data.createState(None, "idle")
    active = data.createState(None, "active")

    # Counter for predicate - predicate receives instance
    counter = [0]

    def allow_after_3(inst):
        counter[0] += 1
        result = counter[0] >= 3
        print(f"    predicate called (count={counter[0]}): {result}")
        return result

    data.addPredicatedTransition(idle, "try_activate", active, allow_after_3)

    inst = fsm.FsmInstance(data)
    inst.changeState(idle)
    inst.update()

    # First two attempts should fail
    inst.sendEvent("try_activate")
    inst.update()
    assert inst.currentState == idle

    inst.sendEvent("try_activate")
    inst.update()
    assert inst.currentState == idle

    # Third attempt should succeed
    inst.sendEvent("try_activate")
    inst.update()
    assert inst.currentState == active
    print(f"  Final state: {inst.currentState.name}")

    print("  PASSED")

def test_update_callback():
    """Test onUpdate callback"""
    print("\n=== test_update_callback ===")

    data = fsm.FsmData()

    counter = [0]

    state = data.createState(None, "counting")
    state.onUpdate = lambda inst: counter.__setitem__(0, counter[0] + 1)

    inst = fsm.FsmInstance(data)
    inst.changeState(state)

    for i in range(5):
        inst.update()

    assert counter[0] == 5
    print(f"  Update count: {counter[0]}")

    print("  PASSED")

def test_dot_generation():
    """Test DOT graph generation"""
    print("\n=== test_dot_generation ===")

    data = fsm.FsmData()

    # Create a simple hierarchy
    root = data.createState(None, "root")
    idle = data.createState(root, "idle")
    running = data.createState(root, "running")

    data.addTransition(idle, "start", running)
    data.addTransition(running, "stop", idle)

    inst = fsm.FsmInstance(data)
    inst.changeState(idle)
    inst.update()

    # Generate DOT from instance (shows current state)
    dot = inst.generateDot("TestFSM")

    assert "digraph TestFSM" in dot
    assert "idle" in dot
    assert "running" in dot
    assert "root" in dot
    assert "->" in dot  # Has transitions

    print("  DOT output:")
    for line in dot.split('\n')[:10]:
        print(f"    {line}")
    print("    ...")

    print("  PASSED")

def test_states_property():
    """Test states enumeration"""
    print("\n=== test_states_property ===")

    data = fsm.FsmData()

    s1 = data.createState(None, "s1")
    s2 = data.createState(None, "s2")
    s3 = data.createState(None, "s3")

    states = data.states
    assert len(states) == 3

    names = {s.name for s in states}
    assert names == {"s1", "s2", "s3"}

    print(f"  States: {[s.name for s in states]}")

    print("  PASSED")

def test_shared_data():
    """Test multiple instances sharing same FsmData"""
    print("\n=== test_shared_data ===")

    # Create shared data
    data = fsm.FsmData()

    idle = data.createState(None, "idle")
    active = data.createState(None, "active")

    data.addTransition(idle, "activate", active)
    data.addTransition(active, "deactivate", idle)

    # Create multiple instances sharing same data
    inst1 = fsm.FsmInstance(data)
    inst2 = fsm.FsmInstance(data)

    # Both start in idle
    inst1.changeState(idle)
    inst2.changeState(idle)
    inst1.update()
    inst2.update()

    assert inst1.currentState.name == "idle"
    assert inst2.currentState.name == "idle"
    print(f"  inst1: {inst1.currentState.name}, inst2: {inst2.currentState.name}")

    # Activate only inst1
    inst1.sendEvent("activate")
    inst1.update()

    assert inst1.currentState.name == "active"
    assert inst2.currentState.name == "idle"  # inst2 unchanged
    print(f"  After inst1 activate: inst1={inst1.currentState.name}, inst2={inst2.currentState.name}")

    # Both share same data
    assert inst1.data is inst2.data
    print("  Both instances share same FsmData: OK")

    print("  PASSED")

def test_instance_variables():
    """Test instance-specific variables via vars (VarMap)"""
    print("\n=== test_instance_variables ===")

    data = fsm.FsmData()
    state_a = data.createState(None, "state_a")
    state_b = data.createState(None, "state_b")

    # Callback that uses instance vars (VarMap attribute access)
    def on_enter_a(inst):
        if "enter_count" in inst.vars:
            count = inst.vars.enter_count
        else:
            count = 0
        inst.vars.enter_count = count + 1

    state_a.onEnter = on_enter_a

    inst1 = fsm.FsmInstance(data)
    inst2 = fsm.FsmInstance(data)

    # Toggle between states to trigger onEnter 3 times on inst1
    for _ in range(3):
        inst1.changeState(state_a)
        inst1.update()
        inst1.changeState(state_b)
        inst1.update()

    # Enter once on inst2
    inst2.changeState(state_a)
    inst2.update()

    assert inst1.vars.enter_count == 3
    assert inst2.vars.enter_count == 1
    print(f"  inst1 enter_count: {inst1.vars.enter_count}")
    print(f"  inst2 enter_count: {inst2.vars.enter_count}")

    print("  PASSED")

def test_fsm_group():
    """Test FsmGroup for coordinating multiple instances"""
    print("\n=== test_fsm_group ===")

    # Create shared data with states for graceful shutdown
    data = fsm.FsmData()
    running = data.createState(None, "running")
    shutting_down = data.createState(None, "shutting_down")
    ok2exit = data.createState(None, "ok2exit")

    # Add transitions
    data.addTransition(running, "shutdown", shutting_down)
    data.addTransition(shutting_down, "cleanup_done", ok2exit)

    # Create a group
    group = fsm.FsmGroup()
    print(f"  Created group: {group}")

    # Create multiple instances in the group
    instances = []
    for i in range(5):
        inst = group.createInstance(data)
        inst.changeState(running)
        inst.update()
        instances.append(inst)

    print(f"  Group count: {group.count}")
    assert group.count == 5

    # All should be in "running" state
    assert group.allInState("running")
    assert group.allInState(running)
    print("  All instances in 'running' state: OK")

    # Broadcast shutdown event
    group.broadcastEvent("shutdown")
    for inst in instances:
        inst.update()

    # All should now be in "shutting_down"
    assert group.allInState("shutting_down")
    print("  After broadcast 'shutdown': all in 'shutting_down'")

    # Simulate cleanup completing at different times
    instances[0].sendEvent("cleanup_done")
    instances[0].update()
    instances[1].sendEvent("cleanup_done")
    instances[1].update()

    # Not all in ok2exit yet
    assert not group.allInState("ok2exit")
    print("  After 2 instances cleanup: not all in 'ok2exit' yet")

    # Complete the rest
    for inst in instances[2:]:
        inst.sendEvent("cleanup_done")
        inst.update()

    # Now all should be ok2exit
    assert group.allInState("ok2exit")
    print("  After all cleanup: all in 'ok2exit'")

    # Test auto-unregister on destruction
    del instances[0]
    assert group.count == 4
    print("  After deleting one instance: count = 4")

    print("  PASSED")

def visualize_fsm():
    """Generate DOT, convert to PNG, and open it"""
    print("\n=== Visualizing FSM ===")

    data = fsm.FsmData()

    # Game Character AI - hierarchical state machine
    ai = data.createState(None, "AI")

    # PATROL states
    patrol = data.createState(ai, "PATROL")
    walk = data.createState(patrol, "WALK")
    idle = data.createState(patrol, "IDLE")
    investigate = data.createState(patrol, "INVESTIGATE")

    # COMBAT states
    combat = data.createState(ai, "COMBAT")
    chase = data.createState(combat, "CHASE")
    attack = data.createState(combat, "ATTACK")
    take_cover = data.createState(combat, "TAKE_COVER")

    # FLEE state
    flee = data.createState(ai, "FLEE")

    # PATROL transitions
    data.addTransition(walk, "timeout", idle)
    data.addTransition(idle, "timeout", walk)
    data.addTransition(walk, "noise", investigate)
    data.addTransition(idle, "noise", investigate)
    data.addTransition(investigate, "clear", walk)

    # PATROL -> COMBAT transitions
    data.addTransition(walk, "enemy_spotted", chase)
    data.addTransition(idle, "enemy_spotted", chase)
    data.addTransition(investigate, "enemy_spotted", chase)

    # COMBAT transitions
    data.addTransition(chase, "in_range", attack)
    data.addTransition(attack, "out_of_range", chase)
    data.addTransition(chase, "low_health", take_cover)
    data.addTransition(attack, "low_health", take_cover)
    data.addTransition(take_cover, "recovered", attack)

    # COMBAT -> FLEE transitions
    data.addTransition(chase, "critical", flee)
    data.addTransition(attack, "critical", flee)
    data.addTransition(take_cover, "critical", flee)

    # FLEE -> PATROL transition
    data.addTransition(flee, "safe", idle)

    # Create instance and start in WALK state
    inst = fsm.FsmInstance(data)
    inst.changeState(walk)
    inst.update()

    # Generate DOT from instance (shows current state highlight)
    dot = inst.generateDot("GameCharacterAI", K=6.0, sep=60, size_w=32, size_h=24)

    # Write to temp file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.dot', delete=False) as f:
        dot_path = f.name
        f.write(dot)

    png_path = dot_path.replace('.dot', '.png')

    print(f"  DOT file: {dot_path}")
    print(f"  PNG file: {png_path}")

    # Convert to PNG using graphviz dot
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
    parser = argparse.ArgumentParser(description="FSM Python Binding Tests")
    parser.add_argument("-v", "--visualize", action="store_true",
                        help="Generate and open FSM visualization as PNG")
    args = parser.parse_args()

    print("FSM Python Binding Tests (FsmData/FsmInstance)")
    print("=" * 50)

    test_basic_fsm()
    test_hierarchical_fsm()
    test_token_events()
    test_predicated_transition()
    test_update_callback()
    test_dot_generation()
    test_states_property()
    test_shared_data()
    test_instance_variables()

    test_fsm_group()

    print("\n" + "=" * 50)
    print("All tests PASSED!")

    if args.visualize:
        visualize_fsm()
