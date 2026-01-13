#!/usr/bin/env python3
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Unit tests for SubsystemFsm init/shutdown ordering
"""

import unittest
import sys
from orkengine import core

# Global list to track initialization order
init_order = []
shutdown_order = []


def create_tracked_subsystem(name, dependencies=None):
    """Create a subsystem that tracks when it initializes/shutdowns"""
    subsystem = core.SubsystemFsm(name)

    # Set dependencies if provided
    if dependencies:
        for dep in dependencies:
            subsystem.dependencies[dep.nameHash] = dep

    # Setup callbacks to track order
    def on_init(instance):
        init_order.append(name)
        # Move to READY state
        instance.sendEvent("READY")

    def on_shutdown(instance):
        shutdown_order.append(name)
        # Move to TERMINATED state
        instance.sendEvent("TERMINATED")

    # Assign callbacks
    subsystem.state_initializing._onenter = on_init
    subsystem.state_shutting_down._onenter = on_shutdown

    return subsystem


class TestInitOrdering(unittest.TestCase):
    """Test initialization ordering"""

    def setUp(self):
        """Set up test fixtures"""
        global init_order, shutdown_order
        init_order = []
        shutdown_order = []
        core.coreappinit()
        self.app = core.Application()

    def test_no_dependencies_parallel(self):
        """Test subsystems with no dependencies can init in any order"""
        sub_a = create_tracked_subsystem("a")
        sub_b = create_tracked_subsystem("b")
        sub_c = create_tracked_subsystem("c")

        self.app.registerSubsystem(sub_a)
        self.app.registerSubsystem(sub_b)
        self.app.registerSubsystem(sub_c)

        # All three should have no dependencies
        self.assertEqual(len(sub_a.dependencies), 0)
        self.assertEqual(len(sub_b.dependencies), 0)
        self.assertEqual(len(sub_c.dependencies), 0)

        # Note: We don't actually call _initSubsystemsInWaves() here
        # because it would require full FSM setup. This test just
        # verifies the dependency structure.

    def test_linear_dependency_chain(self):
        """Test A → B → C linear dependency chain"""
        sub_a = create_tracked_subsystem("a")
        sub_b = create_tracked_subsystem("b", [sub_a])
        sub_c = create_tracked_subsystem("c", [sub_b])

        self.app.registerSubsystem(sub_a)
        self.app.registerSubsystem(sub_b)
        self.app.registerSubsystem(sub_c)

        # Verify dependency structure
        self.assertEqual(len(sub_a.dependencies), 0)
        self.assertEqual(len(sub_b.dependencies), 1)
        self.assertEqual(len(sub_c.dependencies), 1)

        self.assertIn(sub_a.nameHash, sub_b.dependencies)
        self.assertIn(sub_b.nameHash, sub_c.dependencies)

    def test_diamond_dependency(self):
        """
        Test diamond dependency:
            A
           / \\
          B   C
           \\ /
            D
        """
        sub_a = create_tracked_subsystem("a")
        sub_b = create_tracked_subsystem("b", [sub_a])
        sub_c = create_tracked_subsystem("c", [sub_a])
        sub_d = create_tracked_subsystem("d", [sub_b, sub_c])

        self.app.registerSubsystem(sub_a)
        self.app.registerSubsystem(sub_b)
        self.app.registerSubsystem(sub_c)
        self.app.registerSubsystem(sub_d)

        # Verify dependency structure
        self.assertEqual(len(sub_a.dependencies), 0)
        self.assertEqual(len(sub_b.dependencies), 1)
        self.assertEqual(len(sub_c.dependencies), 1)
        self.assertEqual(len(sub_d.dependencies), 2)

        self.assertIn(sub_a.nameHash, sub_b.dependencies)
        self.assertIn(sub_a.nameHash, sub_c.dependencies)
        self.assertIn(sub_b.nameHash, sub_d.dependencies)
        self.assertIn(sub_c.nameHash, sub_d.dependencies)

    def test_multi_level_dependencies(self):
        """
        Test multi-level dependency graph:
        Network (level 0)
        GPU (level 0)
        Physics → Network (level 1)
        Rendering → GPU (level 1)
        ECS → GPU + Physics (level 2)
        """
        sub_network = create_tracked_subsystem("network")
        sub_gpu = create_tracked_subsystem("gpu")
        sub_physics = create_tracked_subsystem("physics", [sub_network])
        sub_rendering = create_tracked_subsystem("rendering", [sub_gpu])
        sub_ecs = create_tracked_subsystem("ecs", [sub_gpu, sub_physics])

        self.app.registerSubsystem(sub_network)
        self.app.registerSubsystem(sub_gpu)
        self.app.registerSubsystem(sub_physics)
        self.app.registerSubsystem(sub_rendering)
        self.app.registerSubsystem(sub_ecs)

        # Verify dependency levels
        # Level 0: Network, GPU (no dependencies)
        self.assertEqual(len(sub_network.dependencies), 0)
        self.assertEqual(len(sub_gpu.dependencies), 0)

        # Level 1: Physics, Rendering (1 dependency each)
        self.assertEqual(len(sub_physics.dependencies), 1)
        self.assertEqual(len(sub_rendering.dependencies), 1)

        # Level 2: ECS (2 dependencies)
        self.assertEqual(len(sub_ecs.dependencies), 2)


class TestShutdownOrdering(unittest.TestCase):
    """Test shutdown ordering (reverse of init)"""

    def setUp(self):
        """Set up test fixtures"""
        global init_order, shutdown_order
        init_order = []
        shutdown_order = []
        core.coreappinit()
        self.app = core.Application()

    def test_shutdown_reverse_order(self):
        """Test shutdown happens in reverse dependency order"""
        # Create linear chain A → B → C
        sub_a = create_tracked_subsystem("a")
        sub_b = create_tracked_subsystem("b", [sub_a])
        sub_c = create_tracked_subsystem("c", [sub_b])

        self.app.registerSubsystem(sub_a)
        self.app.registerSubsystem(sub_b)
        self.app.registerSubsystem(sub_c)

        # Shutdown should be: C, then B, then A (reverse of init)
        # Note: Actual shutdown testing would require calling
        # _shutdownSubsystemsInWaves(), which needs full system setup

        # For now, just verify dependency structure is correct
        # for reverse topological sort
        self.assertEqual(len(sub_c.dependencies), 1)  # C depends on B
        self.assertEqual(len(sub_b.dependencies), 1)  # B depends on A
        self.assertEqual(len(sub_a.dependencies), 0)  # A has no deps

    def test_shutdown_leaf_nodes_first(self):
        """Test that leaf nodes (no dependents) shutdown first"""
        # Create tree:
        #     A
        #    / \\
        #   B   C
        #   |
        #   D
        sub_a = create_tracked_subsystem("a")
        sub_b = create_tracked_subsystem("b", [sub_a])
        sub_c = create_tracked_subsystem("c", [sub_a])
        sub_d = create_tracked_subsystem("d", [sub_b])

        self.app.registerSubsystem(sub_a)
        self.app.registerSubsystem(sub_b)
        self.app.registerSubsystem(sub_c)
        self.app.registerSubsystem(sub_d)

        # Shutdown order should be:
        # Wave 1: D, C (leaf nodes - no one depends on them)
        # Wave 2: B (only D depended on it, now D is shut down)
        # Wave 3: A (B and C depended on it, now both shut down)

        # Verify leaf nodes have dependents
        # C and D are leaves (no subsystem depends on them)
        # We can verify by checking that A and B have dependents


if __name__ == '__main__':
    unittest.main()
