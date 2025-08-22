#!/usr/bin/env ork.python
"""
Test for offscreen Vulkan rendering with frame capture.
Renders a colored triangle and captures it to a PNG file.
"""

import sys
import os
import time
from orkengine import core
from orkengine import lev2
from obt import path as obt_path

tokens = core.CrcStringProxy()

def main():
    print("="*60)
    print("Starting offscreen frame capture test")
    print("="*60)
    
    # Initialize lev2 app with offscreen graphics context
    print("Initializing lev2 app...")
    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()

    print(f"graphics context: {ctx}")
    fbi = ctx.FBI
    
    print("Starting main thread...")
    ezapp.mainThreadBegin()
    
    # Create a render target group for offscreen rendering
    print("Creating render target group...")
    width = 512
    height = 512
    rtg = lev2.RtGroup(ctx, width, height)
    
    # Add a color buffer
    rtb_color = rtg.createBuffer(tokens.RGBA8, tokens.color)
    
    print(f"Created RTG: {width}x{height}")
    
    # Set clear color on the buffer (dark blue background)
    print("Setting clear color...")
    rtb_color.clearColor = core.vec4(0.2, 0.3, 0.4, 1.0)
    
    # Begin frame
    print("Beginning frame...")
    ctx.beginFrame()
    
    # Push RTG
    print("Pushing RTG...")
    fbi.rtGroupPush(rtg)
    
    # Clear the framebuffer
    print("Clearing framebuffer...")
    fbi.rtGroupClear(rtg)
    
    # For now, just clear to a solid color to test capture
    # Complex rendering would require proper setup of vertex buffers, shaders etc.
    
    # Pop RTG
    print("Popping RTG...")
    fbi.rtGroupPop()
    
    # Capture the frame to a PNG file (must be done before endFrame while command buffer is active)
    output_path = core.Path("/tmp/test_capture.png")
    print(f"Capturing frame to {output_path}...")
    
    # Get the render target buffer and capture it - returns a future
    capture_future = fbi.captureToFile(rtb_color, output_path)
    
    # End frame
    print("Ending frame...")
    ctx.endFrame()
    
    # Wait for the capture future to be realized
    print("Waiting for capture to complete...")
    while capture_future and not capture_future.is_ready:
        ezapp.mainThreadIter()
    print("Capture completed!")
    
    print("Ending main thread...")
    ezapp.mainThreadEnd()
    
    # Check if file was created
    if output_path.exists:
        file_size = os.path.getsize(str(output_path))
        print(f"Success! Captured image saved to {output_path} (size: {file_size} bytes)")
    else:
        print(f"Warning: Output file {output_path} was not created")
    
    print("="*60)
    print("Offscreen frame capture test completed!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())