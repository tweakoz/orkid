#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Unit tests for Application.create() kwargs support
Tests that Application can be configured via Python kwargs.

NOTE: This test must be run in isolation (not with other Application tests)
because Application is a singleton per process.
"""

import unittest
from orkengine import core
from orkengine.core import CrcStringProxy

tokens = CrcStringProxy()

# Module-level initialization - called once for entire test module
_app = None

def setUpModule():
    """Called once before any tests in this module"""
    global _app
    # Create Application with catalog DISABLED
    _app = core.Application.create(std_asset_catalog=False)

def tearDownModule():
    """Called once after all tests in this module"""
    global _app
    _app = None  # Release reference, destructor will be called


class TestApplicationKwargs(unittest.TestCase):
    """Test Application.create() kwargs support"""

    def setUp(self):
        """Set up test fixtures"""
        self.app = _app

    def test_application_created(self):
        """Test Application was created successfully"""
        self.assertIsNotNone(self.app)

    def test_opq_subsystem_ready(self):
        """Test OPQ subsystem is still present (always required)"""
        opq_sub = self.app.getSubsystem("opq")
        self.assertIsNotNone(opq_sub)
        self.assertEqual(opq_sub.currentState(), opq_sub.state_ready)

    def test_core_subsystem_ready(self):
        """Test CORE subsystem is still present (always required)"""
        core_sub = self.app.getSubsystem("core")
        self.assertIsNotNone(core_sub)
        self.assertEqual(core_sub.currentState(), core_sub.state_ready)

    def test_catalog_subsystem_disabled(self):
        """Test CATALOG subsystem is NOT present when std_asset_catalog=False"""
        catalog_sub = self.app.getSubsystem("catalog")
        self.assertIsNone(catalog_sub)

    def test_core_only_depends_on_opq(self):
        """Test CORE only depends on OPQ when catalog is disabled"""
        core_sub = self.app.getSubsystem("core")
        self.assertIsNotNone(core_sub)
        # CORE should have OPQ dependency
        self.assertTrue(core_sub.hasDependency(tokens.opq))
        # CORE should NOT have CATALOG dependency (since catalog is disabled)
        self.assertFalse(core_sub.hasDependency(tokens.catalog))


if __name__ == '__main__':
    unittest.main()
