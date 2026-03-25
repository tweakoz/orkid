#!/usr/bin/env ork.python
"""
Test for single channel pixel picking with RGBA32F format.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
from orkengine import core
from orkengine.core import vec4
from _pixel_pick_boilerplate import run_pixel_pick_test

tokens = core.CrcStringProxy()

def main():
    # RGBA32F uses floating point values directly (0.0 to 1.0)
    # Expected colors for RGBA32F format
    def get_expected_colors():
        return [
            (64, 64, "Blue", vec4(0.0, 0.0, 1.0, 1.0)),       # Top-left quadrant
            (192, 64, "Yellow", vec4(1.0, 1.0, 0.0, 1.0)),    # Top-right quadrant
            (64, 192, "Red", vec4(1.0, 0.0, 0.0, 1.0)),       # Bottom-left quadrant
            (192, 192, "Green", vec4(0.0, 1.0, 0.0, 1.0)),    # Bottom-right quadrant
            (127, 127, "Blue/Yellow/Red/Green boundary", None),  # Center boundary
        ]
    
    # Run test with RGBA32F format
    return run_pixel_pick_test("RGBA32F")

if __name__ == "__main__":
    sys.exit(main())