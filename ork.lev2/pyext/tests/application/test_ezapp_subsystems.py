#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for EzApp Application inheritance and subsystem integration.

This test verifies that OrkEzApp correctly inherits from Application
and has access to the subsystem infrastructure.

Usage:
  ./test_ezapp_subsystems.py
"""

import sys
from orkengine import core
from orkengine import lev2
from orkengine.core import CrcStringProxy

tokens = CrcStringProxy()
results = []

def test(name, condition):
    results.append(condition)
    print(f"  {'PASS' if condition else 'FAIL'}: {name}")

################################################################
# Minimal app class for EzApp.create
################################################################
class TestApp:
    def __init__(self):
        self.gpu_init_called = False
        self.update_init_called = False

    def onGpuInit(self, ctx):
        self.gpu_init_called = True
        # Signal exit immediately after GPU init for this test
        self.ezapp.signalExit()

    def onUpdateInit(self):
        self.update_init_called = True

################################################################

def main():
    print("Testing EzApp Application Inheritance with Subsystems")
    print("=" * 50)

    # Create EzApp with offscreen mode, audio, and subsystems enabled
    print("\n[Creating lev2.OrkEzApp with use_subsystems=True, enable_audio=True]")
    app_obj = TestApp()
    ezapp = lev2.OrkEzApp.create(app_obj, width=64, height=64, offscreen=True, use_subsystems=True, enable_audio=True)
    app_obj.ezapp = ezapp

    print("\n[EzApp Properties]")
    test("EzApp created", ezapp is not None)
    test("mainwin available", ezapp.mainwin is not None)
    test("topWidget available", ezapp.topWidget is not None)
    test("vars available", ezapp.vars is not None)

    print("\n[EzApp AppInitData]")
    appinit = ezapp._appinit
    test("_appinit available", appinit is not None)

    print("\n[Core Subsystems (from Application base)]")
    opq = ezapp.getSubsystem("opq")
    test("OPQ subsystem registered", opq is not None)
    test("OPQ in READY state", opq and opq.currentState() == opq.state_ready)

    catalog = ezapp.getSubsystem("catalog")
    test("CATALOG subsystem registered", catalog is not None)
    test("CATALOG in READY state", catalog and catalog.currentState() == catalog.state_ready)

    core_sub = ezapp.getSubsystem("core")
    test("CORE subsystem registered", core_sub is not None)
    test("CORE in READY state", core_sub and core_sub.currentState() == core_sub.state_ready)

    print("\n[Lev2 Subsystems (GPU/Audio)]")
    gpu = ezapp.getSubsystem("gpu")
    test("GPU subsystem registered", gpu is not None)
    test("GPU in READY state", gpu and gpu.currentState() == gpu.state_ready)

    # Audio is disabled by default in this test
    audio = ezapp.getSubsystem("audio")
    test("AUDIO subsystem not registered (audio disabled)", audio is None)

    # Run the main loop briefly (will exit after onGpuInit signals exit)
    print("\n[Running brief main loop]")
    ezapp.mainThreadLoop()

    print("\n[Callback Verification]")
    test("onGpuInit was called", app_obj.gpu_init_called)
    test("onUpdateInit was called", app_obj.update_init_called)

    # Summary
    passed, failed = results.count(True), results.count(False)
    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
