#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for ComponentizedApplication with HFSM Subsystem integration.

This test verifies that ComponentizedApplication correctly works with
the C++ HFSM subsystem infrastructure (use_subsystems=True).

Key concepts being tested:
- ComponentizedApplication (Python) component lifecycle
- C++ Subsystem FSM lifecycle (via use_subsystems=True)
- Init→Link two-phase pattern across both systems
- Minimal UI with TextBox displaying countdown
- Proper shutdown sequencing

Usage:
  ./test_componentized_subsystems.py
"""

import sys
import time
from orkengine import core
from orkengine import lev2
from orkengine.core import vec3, vec4, CrcStringProxy
from ork.app.application import ComponentizedApplication, ApplicationComponent

tokens = CrcStringProxy()
results = []

def test(name, condition):
    results.append(condition)
    print(f"  {'PASS' if condition else 'FAIL'}: {name}")

################################################################
# Test Component - tracks lifecycle callbacks
################################################################

class LifecycleTracker(ApplicationComponent):
    """Component that tracks which lifecycle callbacks were invoked."""

    def __init__(self):
        super().__init__()
        self.callbacks_invoked = []

    def _onAppInit(self, app, initdata):
        self.callbacks_invoked.append("onAppInit")

    def _onAppLink(self, app, initdata):
        self.callbacks_invoked.append("onAppLink")

    def _onGpuInit(self, ctx):
        self.callbacks_invoked.append("onGpuInit")

    def _onGpuLink(self, ctx):
        self.callbacks_invoked.append("onGpuLink")

    def _onUpdateInit(self):
        self.callbacks_invoked.append("onUpdateInit")

    def _onUpdateLink(self):
        self.callbacks_invoked.append("onUpdateLink")

    def _onUpdateExit(self):
        self.callbacks_invoked.append("onUpdateExit")

    def _onGpuExit(self, ctx):
        self.callbacks_invoked.append("onGpuExit")

    def _onAppExit(self):
        self.callbacks_invoked.append("onAppExit")

################################################################
# Test Application
################################################################

class TestApp(ComponentizedApplication):
    """Test application that combines ComponentizedApplication with subsystems."""

    def __init__(self):
        super().__init__()
        self.tracker = self.addComponent("tracker", LifecycleTracker)
        self.frame_count = 0
        self.countdown_seconds = 15
        self.start_time = None

        self.ezapp_args = {
            'width': 640,
            'height': 480,
            'offscreen': False,
            'use_subsystems': True,
            'enable_audio': True
        }

    ##############################################

    def _onUiInit(self):
        """Set up minimal UI with a single TextBox that fills the window."""
        lg_group = self.ezapp.topLayoutGroup

        # Create a single TextBox that fills the layout
        self.text_box_item = lg_group.makeChild(
            uiclass=lev2.ui.TextBox,
            args=["countdown", vec4(0.3, 0.5, 0.7, 1), "Countdown"]
        )
        self.text_box_item.layout.fill(lg_group.layout)
        self.text_box = self.text_box_item.widget
        self.text_box.setText(f"Starting countdown: {self.countdown_seconds}s")

    ##############################################

    def _onGpuInit(self, ctx):
        pass  # UI already set up in _onUiInit

    ##############################################

    def _onUpdateInit(self):
        """Initialize the start time for countdown."""
        self.start_time = time.time()

    def _onUpdate(self, updinfo):
        """Update countdown and exit after countdown_seconds."""
        self.frame_count += 1
        if self.start_time is None:
            return

        elapsed = time.time() - self.start_time
        remaining = self.countdown_seconds - elapsed

        if remaining <= 0:
            self.text_box.setText("Countdown complete!")
            print(f"  Countdown finished after {self.frame_count} frames, signaling exit...")
            self.ezapp.signalExit()
        else:
            self.text_box.setText(f"Countdown: {int(remaining + 0.5)}s")

################################################################

def main():
    print("Testing ComponentizedApplication with HFSM Subsystems")
    print("=" * 60)

    # Create app and ezapp
    print("\n[Creating TestApp (ComponentizedApplication)]")
    app = TestApp()

    print("\n[Creating EzApp with use_subsystems=True]")
    ezapp = app.createEzApp()

    print("\n[EzApp Properties]")
    test("EzApp created", ezapp is not None)
    test("mainwin available", ezapp.mainwin is not None)
    test("topWidget available", ezapp.topWidget is not None)

    print("\n[C++ Subsystems (from Application base)]")

    # Core subsystems
    opq = ezapp.getSubsystem("opq")
    test("OPQ subsystem registered", opq is not None)
    test("OPQ in READY state", opq and opq.currentState() == opq.state_ready)

    catalog = ezapp.getSubsystem("catalog")
    test("CATALOG subsystem registered", catalog is not None)
    test("CATALOG in READY state", catalog and catalog.currentState() == catalog.state_ready)

    core_sub = ezapp.getSubsystem("core")
    test("CORE subsystem registered", core_sub is not None)
    test("CORE in READY state", core_sub and core_sub.currentState() == core_sub.state_ready)

    # Lev2 subsystems
    gpu = ezapp.getSubsystem("gpu")
    test("GPU subsystem registered", gpu is not None)
    test("GPU in READY state", gpu and gpu.currentState() == gpu.state_ready)

    audio = ezapp.getSubsystem("audio")
    test("AUDIO subsystem registered", audio is not None)
    test("AUDIO in READY state", audio and audio.currentState() == audio.state_ready)

    print("\n[UI Verification]")
    test("TextBox created", app.text_box is not None)

    print("\n[Python Component State (before mainloop)]")
    tracker = app.tracker
    test("Tracker component exists", tracker is not None)
    print(f"  Callbacks so far: {tracker.callbacks_invoked}")

    # Run the main loop (will exit after countdown completes in onUpdate)
    print("\n[Running main loop with TextBox countdown UI]")
    ezapp.mainThreadLoop()

    print("\n[Python Component Lifecycle Verification]")
    callbacks = tracker.callbacks_invoked
    print(f"  All callbacks: {callbacks}")

    # Verify Init→Link ordering
    test("onAppInit called", "onAppInit" in callbacks)
    test("onAppLink called", "onAppLink" in callbacks)
    test("onGpuInit called", "onGpuInit" in callbacks)
    test("onGpuLink called", "onGpuLink" in callbacks)
    test("onUpdateInit called", "onUpdateInit" in callbacks)
    test("onUpdateLink called", "onUpdateLink" in callbacks)

    # Verify Init before Link (two-phase pattern)
    if "onAppInit" in callbacks and "onAppLink" in callbacks:
        test("onAppInit before onAppLink",
             callbacks.index("onAppInit") < callbacks.index("onAppLink"))

    if "onGpuInit" in callbacks and "onGpuLink" in callbacks:
        test("onGpuInit before onGpuLink",
             callbacks.index("onGpuInit") < callbacks.index("onGpuLink"))

    if "onUpdateInit" in callbacks and "onUpdateLink" in callbacks:
        test("onUpdateInit before onUpdateLink",
             callbacks.index("onUpdateInit") < callbacks.index("onUpdateLink"))

    # Verify exit callbacks
    test("onUpdateExit called", "onUpdateExit" in callbacks)
    test("onGpuExit called", "onGpuExit" in callbacks)

    # Verify frames ran
    test("Update loop ran", app.frame_count > 0)

    # Summary
    passed, failed = results.count(True), results.count(False)
    print(f"\n{'='*60}")
    print(f"Results: {passed} passed, {failed} failed")

    # Explicit shutdown
    print("\n[Calling ezapp.shutdown()]")
    ezapp.shutdown()

    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
