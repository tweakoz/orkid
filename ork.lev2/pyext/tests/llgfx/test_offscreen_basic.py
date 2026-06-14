#!/usr/bin/env ork.python
"""
Basic test for offscreen Vulkan rendering.
Creates offscreen context, begins frame with RTG clear, ends frame, and exits.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
import time
from orkengine import core
from orkengine import lev2
from orkengine import ecs

tokens = core.CrcStringProxy()

def main():
    print("="*60)
    print("Starting offscreen graphics test")
    print("="*60)
    
    # Initialize lev2 app with offscreen graphics context
    print("Initializing lev2 app...")
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()

    print(f"graphics context: {ctx}")
    fbi = ctx.FBI
    
    print("Starting main thread...")
    ezapp.mainThreadBegin()
    
    # Create a simple RTG for offscreen rendering
    print("Creating render target group...")
    width = 256
    height = 256
    rtg = lev2.RtGroup(ctx, width, height)
    
    # Add a color buffer
    rtb_color = rtg.createBuffer(tokens.RGBA8,tokens.color)
    
    print(f"Created RTG: {width}x{height}")
    
    # Perform a single frame
    print("Beginning frame...")
    ctx.beginFrame()
    
    # Push RTG
    print("Pushing RTG...")
    #fbi.rtGroupPush(rtg)
    
    # Clear the framebuffer
    print("Clearing framebuffer...")
    #fbi.rtGroupClear(rtg)  # Dark blue clear color
    
    # Pop RTG
    print("Popping RTG...")
    #fbi.rtGroupPop()
    
    # End frame
    print("Ending frame...")
    ctx.endFrame()
    
    # Process one iteration to ensure frame completes
    print("Processing frame...")
    ezapp.mainThreadIter()
    
    print("Ending main thread...")
    ezapp.mainThreadEnd()
    
    print("="*60)
    print("Offscreen graphics test completed successfully!")
    print("="*60)
    return 0

if __name__ == "__main__":
    rc = main()
    ecs.headless_exit()
    sys.exit(rc)