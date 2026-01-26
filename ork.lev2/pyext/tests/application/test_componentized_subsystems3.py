#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for Python-implemented subsystems using the declarative API.

This test creates a custom Python subsystem with:
- A background worker thread with dummy workload
- Structured startup/shutdown coordinated by the HFSM
- Dependency on 'core' declared in Subsystem constructor

The subsystem demonstrates:
1. Creating a Subsystem from Python with dependencies=['core']
2. Setting FSM state callbacks (onEnter)
3. Coordinating thread lifecycle with FSM events
4. Passing subsystem objects to use_subsystems

Usage:
  ./test_componentized_subsystems3.py
"""

import sys
import time
import threading
from orkengine.core import *
from orkengine.core import logger
from orkengine.lev2 import *
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()

# Global log channel for pyworker subsystem
logchan = logger().configureChannel("PYWORKER", vec3(1.0, 0.41, 0.71), True)

################################################################
# Python-implemented Subsystem with Worker Thread
################################################################

class WorkerSubsystem:
    """
    A Python-implemented subsystem with a background worker thread.

    The worker thread does dummy work (counting) and is coordinated
    with the HFSM lifecycle:
    - Thread starts in state_initializing callback
    - Thread runs during state_ready
    - Thread stops in state_shutting_down callback
    """

    def __init__(self, name="pyworker"):
        self.name = name
        # Declare dependency on 'core' - pyworker inits after core, shuts down before core
        self._subsystem = Subsystem(name, dependencies=['core'])

        # Worker thread state
        self._thread = None
        self._keep_running = threading.Event()
        self._thread_started = threading.Event()
        self._thread_stopped = threading.Event()
        self._work_counter = 0
        self._work_interval = 0.1  # seconds between work iterations

        # Configure FSM callbacks
        self._setup_fsm_callbacks()

    def _setup_fsm_callbacks(self):
        """Configure the FSM state callbacks."""

        # INITIALIZING state -> start the worker thread
        def on_initializing(fsm_instance):
            logchan.log("INITIALIZING: Starting worker thread...")

            # Reset events
            self._keep_running.set()
            self._thread_started.clear()
            self._thread_stopped.clear()

            # Start worker thread
            self._thread = threading.Thread(target=self._worker_loop, daemon=True)
            self._thread.start()

            # Wait for thread to confirm it started
            if self._thread_started.wait(timeout=2.0):
                logchan.log("Worker thread started successfully")
                fsm_instance.sendEvent("READY")
            else:
                logchan.log("ERROR: Worker thread failed to start")
                fsm_instance.sendEvent("ERROR")

        self._subsystem.state_initializing.onEnter = on_initializing

        # READY state -> thread is running
        def on_ready(fsm_instance):
            logchan.log("READY: Worker thread is running")

        self._subsystem.state_ready.onEnter = on_ready

        # SHUTTING_DOWN state -> stop the worker thread
        def on_shutting_down(fsm_instance):
            logchan.log("SHUTTING_DOWN: Stopping worker thread...")

            # Signal thread to stop
            self._keep_running.clear()

            # Wait for thread to stop (with timeout)
            if self._thread and self._thread.is_alive():
                if self._thread_stopped.wait(timeout=2.0):
                    logchan.log("Worker thread stopped gracefully")
                else:
                    logchan.log("WARNING: Worker thread did not stop in time")

            # Join the thread
            if self._thread:
                self._thread.join(timeout=1.0)
                self._thread = None

            logchan.log(f"Total work iterations: {self._work_counter}")
            fsm_instance.sendEvent("TERMINATED")

        self._subsystem.state_shutting_down.onEnter = on_shutting_down

        # TERMINATED state -> cleanup complete
        def on_terminated(fsm_instance):
            logchan.log("TERMINATED: Subsystem fully shutdown")

        self._subsystem.state_terminated.onEnter = on_terminated

    def _worker_loop(self):
        """Background worker thread loop."""
        logchan.log("Worker thread: loop starting")
        self._thread_started.set()

        while self._keep_running.is_set():
            # Do dummy work
            self._work_counter += 1

            # Log every 10 iterations
            if self._work_counter % 10 == 0:
                logchan.log(f"Worker: iteration {self._work_counter}")

            # Sleep between iterations
            time.sleep(self._work_interval)

        logchan.log("Worker thread: loop exiting")
        self._thread_stopped.set()

    @property
    def subsystem(self):
        """Get the underlying Subsystem object."""
        return self._subsystem

    @property
    def work_counter(self):
        """Get the current work counter value."""
        return self._work_counter


################################################################
# Test Application
################################################################

class WorkerSubsystemTestApp(ComponentizedApplication):
    """Test app demonstrating a Python-implemented subsystem."""

    def __init__(self, worker_subsystem):
        super().__init__()
        self.frame_count = 0
        self.max_frames = 100  # Run for ~100 frames then exit
        self.worker = worker_subsystem

        # Pass the worker subsystem object directly in use_subsystems
        # Dependencies are declared in the Subsystem constructor
        self.ezapp_args = {
            'width': 640,
            'height': 480,
            'offscreen': False,
            'use_subsystems': ['gpu', 'lev2', worker_subsystem.subsystem],
        }

    ##############################################

    def _onUiInit(self):
        """Set up minimal UI with a TextBox."""
        lg_group = self.ezapp.topLayoutGroup
        self.text_box_item = lg_group.makeChild(
            uiclass=ui.TextBox,
            args=["worker_test", vec4(0.1, 0.2, 0.3, 1), "Worker Test"]
        )
        self.text_box_item.layout.fill(lg_group.layout)
        self.text_box = self.text_box_item.widget
        self.text_box.setText("Python Subsystem Test\n\nInitializing...")

    ##############################################

    def _onUpdate(self, updinfo):
        """Update display and check for exit."""
        self.frame_count += 1

        # Update display
        worker_count = self.worker.work_counter if self.worker else 0
        worker_state = "N/A"
        if self.worker:
            state = self.worker.subsystem.currentState()
            if state:
                worker_state = state.name

        display_text = "Python Subsystem Test\n"
        display_text += "=" * 35 + "\n\n"
        display_text += f"Frame: {self.frame_count}\n"
        display_text += f"Worker iterations: {worker_count}\n"
        display_text += f"Worker state: {worker_state}\n"
        display_text += f"\nWill exit at frame {self.max_frames}"
        self.text_box.setText(display_text)

        # Exit after max_frames
        if self.frame_count >= self.max_frames:
            logchan.log(f"Reached {self.max_frames} frames, signaling exit")
            self.text_box.setText("Python Subsystem Test\n\nShutting down...")
            self.ezapp.signalExit()


################################################################
# Main
################################################################

def main():
    logchan.log("=" * 50)
    logchan.log("Testing Python-implemented Subsystem")
    logchan.log("=" * 50)
    logchan.log("This test demonstrates:")
    logchan.log("  - Creating a Subsystem in Python with dependencies=['core']")
    logchan.log("  - Passing subsystem objects to use_subsystems")
    logchan.log("  - Automatic dependency resolution and wave-based init")

    # Create the worker subsystem BEFORE creating the app
    # Dependencies are declared in the constructor
    worker = WorkerSubsystem("pyworker")
    logchan.log(f"Created pyworker subsystem with dependencies: {worker.subsystem._pending_dependencies}")

    # Create app with worker subsystem
    app = WorkerSubsystemTestApp(worker)
    ezapp = app.createEzApp()

    logchan.log("Subsystem Status After Init:")
    for name in ['opq', 'core', 'gpu', 'lev2', 'pyworker']:
        sub = ezapp.getSubsystem(name)
        if sub:
            state = sub.currentState()
            state_name = state.name if state else "unknown"
            logchan.log(f"  {name}: {state_name}")
        else:
            logchan.log(f"  {name}: NOT REGISTERED")

    logchan.log("Running main loop")
    ezapp.mainThreadLoop()

    logchan.log("Shutdown")
    ezapp.shutdown()

    if app.worker:
        logchan.log(f"Final worker count: {app.worker.work_counter}")

    logchan.log("Test complete!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
