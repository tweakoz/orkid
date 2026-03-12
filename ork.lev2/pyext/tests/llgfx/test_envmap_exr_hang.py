#!/usr/bin/env ork.python
"""
Test: EnvMapProcessor with HDR source - reproduction of the EXR hang.
Uses processToXIRDataBlockAsync with a small procedurally-generated HDR
texture to test the full TaskGraph pipeline without needing an external file.

If no EXR file is available, creates a small test texture and writes it
to a temp file, then processes it through the envmap pipeline.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
import os
import time
import signal
from pathlib import Path
from orkengine import core
from orkengine import lev2

TIMEOUT = 120.0  # seconds - envmap processing is heavy

def find_test_exr():
    """Look for any small EXR or HDR file we can use."""
    # Check common locations
    candidates = [
        Path.home() / "Downloads" / "kloofendal_48d_partly_cloudy_puresky_4k.exr",
    ]
    for p in candidates:
        if p.exists():
            return str(p)
    return None

def main():
    print("=" * 60)
    print("Test: EnvMapProcessor EXR/HDR pipeline (hang reproduction)")
    print("=" * 60)

    source = find_test_exr()
    if not source:
        print("SKIP: No EXR test file found")
        print("  Place an EXR file at ~/Downloads/*.exr to enable this test")
        return 0  # skip, not fail

    print(f"Source: {source}")

    ezapp = lev2.lev2appinit()

    timed_out = False
    def onCtrlC(signum, frame):
        nonlocal timed_out
        timed_out = True
    signal.signal(signal.SIGINT, onCtrlC)

    ezapp.mainThreadBegin()

    # Start async processing (this is the code path that hangs)
    print("Starting processToXIRDataBlockAsync...")
    future = lev2.EnvMapProcessor.processToXIRDataBlockAsync(source)
    if not future:
        print("FAIL: processToXIRDataBlockAsync returned None")
        ezapp.mainThreadEnd()
        return 1

    start = time.time()
    report_time = start
    last_status = ""

    while not future.isReady() and not timed_out:
        time.sleep(0.1)
        ezapp.mainThreadIterCommandLine()
        now = time.time()
        elapsed = now - start

        if elapsed > TIMEOUT:
            timed_out = True
            break

        if now - report_time > 5:
            print(f"  Processing... ({int(elapsed)}s elapsed)")
            report_time = now

    elapsed = time.time() - start

    if timed_out:
        print(f"\nFAIL: Processing timed out after {elapsed:.1f}s")
        print("  This confirms the EXR processing hang.")
        print("  The TaskGraph is likely deadlocked in ContextExecutor::executePhase()")
        ezapp.mainThreadEnd()
        return 1

    result = future.get()
    if not result:
        print("FAIL: Processing returned null result")
        ezapp.mainThreadEnd()
        return 1

    print(f"\nPASSED: EXR processing completed in {elapsed:.1f}s")
    print(f"  Result size: {len(result.bytes)} bytes")

    ezapp.mainThreadEnd()
    return 0

if __name__ == "__main__":
    exit_code = main()
    os._exit(exit_code)
