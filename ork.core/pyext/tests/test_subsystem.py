#!/usr/bin/env python3
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Unit tests for Subsystem and Application dependency resolution
"""

import unittest
import sys
from orkengine import core
from orkengine.core import CrcStringProxy

tokens = CrcStringProxy()

# Module-level initialization - called once for entire test module
# Create single Application instance (singleton per process)
_app = None

def setUpModule():
    """Called once before any tests in this module"""
    global _app
    _app = core.Application.create()

def tearDownModule():
    """Called once after all tests in this module"""
    global _app
    _app = None  # Release reference, destructor will be called

class TestApplication(unittest.TestCase):
    """Test Application subsystem management"""

    def setUp(self):
        """Set up test fixtures - reference global Application"""
        self.app = _app

    def test_application_creation(self):
        """Test Application was created successfully"""
        self.assertIsNotNone(self.app)
        self.assertIsNotNone(self.app.mainq)
        self.assertIsNotNone(self.app.updq)
        self.assertIsNotNone(self.app.conq)

    def test_opq_subsystem_registered(self):
        """Test OPQ subsystem was auto-registered"""
        opq_sub = self.app.getSubsystem("opq")
        self.assertIsNotNone(opq_sub)
        self.assertEqual(opq_sub.name, "opq")
        # Should be in READY state after initialization
        current_state = opq_sub.currentState()
        self.assertEqual(current_state, opq_sub.state_ready)

    def test_register_single_subsystem(self):
        """Test registering a subsystem"""
        sub = core.Subsystem("test_network")
        self.app.registerSubsystem(sub)

        # Retrieve it
        retrieved = self.app.getSubsystem("test_network")
        self.assertIsNotNone(retrieved)
        self.assertEqual(retrieved.name, "test_network")

    def test_register_multiple_subsystems(self):
        """Test registering multiple subsystems"""
        sub1 = core.Subsystem("test_db")
        sub2 = core.Subsystem("test_cache")

        self.app.registerSubsystem(sub1)
        self.app.registerSubsystem(sub2)

        # Verify both can be retrieved
        self.assertIsNotNone(self.app.getSubsystem("test_db"))
        self.assertIsNotNone(self.app.getSubsystem("test_cache"))

    def test_unregister_subsystem(self):
        """Test unregistering a subsystem"""
        sub = core.Subsystem("test_temp")
        self.app.registerSubsystem(sub)

        # Verify it's registered
        self.assertIsNotNone(self.app.getSubsystem("test_temp"))

        # Unregister
        self.app.unregisterSubsystem("test_temp")

        # Should be None after unregister
        self.assertIsNone(self.app.getSubsystem("test_temp"))


class TestSubsystem(unittest.TestCase):
    """Test Subsystem basic functionality"""

    def test_subsystem_creation(self):
        """Test creating a Subsystem"""
        sub = core.Subsystem("test_subsystem")
        self.assertIsNotNone(sub)
        self.assertEqual(sub.name, "test_subsystem")
        self.assertGreater(sub.nameHash, 0)

    def test_subsystem_states(self):
        """Test that subsystem has all required FSM states"""
        sub = core.Subsystem("test_states")

        # Check all standard states exist
        self.assertIsNotNone(sub.state_uninitialized)
        self.assertIsNotNone(sub.state_initializing)
        self.assertIsNotNone(sub.state_ready)
        self.assertIsNotNone(sub.state_error)
        self.assertIsNotNone(sub.state_shutting_down)
        self.assertIsNotNone(sub.state_terminated)

        # Should start in UNINITIALIZED state
        current = sub.currentState()
        self.assertEqual(current, sub.state_uninitialized)

    def test_subsystem_dependencies(self):
        """Test subsystem dependency management"""
        sub1 = core.Subsystem("dep_test_a")
        sub2 = core.Subsystem("dep_test_b")

        # sub2 depends on sub1
        sub2.addDependency(sub1)

        # Should have the dependency (look up by token)
        self.assertTrue(sub2.hasDependency(tokens.dep_test_a))

        # Remove dependency
        sub2.removeDependency(tokens.dep_test_a)
        self.assertFalse(sub2.hasDependency(tokens.dep_test_a))

    def test_subsystem_initialization(self):
        """Test subsystem initialization transition"""
        sub = core.Subsystem("init_test")

        # Should start UNINITIALIZED
        self.assertEqual(sub.currentState(), sub.state_uninitialized)

        # Initialize (sends START event)
        sub.initialize()
        sub.update()  # Process state transitions

        # Should transition through INITIALIZING to READY
        # (May need multiple updates depending on FSM processing)
        for _ in range(10):
            sub.update()
            current = sub.currentState()
            if current == sub.state_ready:
                break

        # Should reach READY state
        self.assertEqual(sub.currentState(), sub.state_ready)


if __name__ == '__main__':
    unittest.main()
