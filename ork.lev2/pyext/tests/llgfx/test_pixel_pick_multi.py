#!/usr/bin/env ork.python
"""
Test for multi-channel pixel picking with async capture (deep pixels).
Renders multiple buffers (color, normal, depth) and picks from all channels.
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
    print("Starting multi-channel pixel pick test (deep pixels)")
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
    
    # Create render target group for offscreen rendering
    print("Creating render target group with multiple buffers...")
    width = 256
    height = 256
    rtg = lev2.RtGroup(ctx, width, height)
    
    # Add multiple buffers for deep pixel support
    # All buffers must use "color" as usage in Vulkan
    # Buffer 0: Color/Pick ID (RGBA8)
    rtb_color = rtg.createBuffer(tokens.RGBA8, tokens.color)
    rtb_color.clearColor = core.vec4(1.0, 0.0, 0.0, 1.0)  # Red
    
    # Buffer 1: Normal (RGBA32F for precision) - also use 'color' as usage
    rtb_normal = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb_normal.clearColor = core.vec4(0.0, 0.0, 1.0, 0.0)  # Pointing up (Z)
    
    # Buffer 2: World position (RGBA32F) - also use 'color' as usage
    rtb_position = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb_position.clearColor = core.vec4(0.0, 0.0, 0.0, 1.0)  # Origin
    
    print(f"Created RTG: {width}x{height} with 3 buffers")
    print(f"  Buffer 0: Color/ID (RGBA8)")
    print(f"  Buffer 1: Normal (RGBA32F)")
    print(f"  Buffer 2: Position (RGBA32F)")
    
    # Begin frame
    print("\nBeginning frame...")
    ctx.beginFrame()
    
    # Push RTG
    fbi.rtGroupPush(rtg)
    
    # Clear all buffers
    fbi.rtGroupClear(rtg)
    
    # TODO: Here we would normally render geometry with multiple outputs
    # For now, the buffers just contain the clear colors
    
    # Pop RTG
    fbi.rtGroupPop()
    
    # Test pixel picking at several locations
    test_points = [
        (0, 0),       # Top-left corner
        (127, 127),   # Center
        (255, 255),   # Bottom-right corner
        (100, 150),   # Random point
    ]
    
    print("\nCapturing deep pixels at test points...")
    captures = []
    
    for x, y in test_points:
        print(f"  Capturing deep pixel at ({x}, {y})...")
        
        # Capture single pixel asynchronously (all channels) - must be done before endFrame
        capture_future = fbi.capturePixel(rtg, x, y)
        captures.append((x, y, capture_future))
    
    # End frame (submits command buffer with capture commands)
    print("\nEnding frame...")
    ctx.endFrame()
    
    # Wait for all captures to complete
    print("\nWaiting for deep pixel captures to complete...")
    all_done = False
    max_iterations = 100
    iteration = 0
    
    while not all_done and iteration < max_iterations:
        ezapp.mainThreadIter()
        
        # Check if all captures are ready
        all_done = True
        for x, y, future in captures:
            if future and not future.is_ready:
                all_done = False
                break
        
        iteration += 1
    
    # Report results
    print("\n" + "="*60)
    print("Multi-Channel Pixel Capture Results:")
    print("="*60)
    
    for x, y, future in captures:
        if future and future.is_ready:
            print(f"  Pixel at ({x:3}, {y:3}): SUCCESS - Deep pixel capture completed")
            
            # TODO: Once PixelFetchContext is populated with multi-channel data:
            # pixel_ctx = future.pixelFetchContext
            # if pixel_ctx:
            #     # Get values from each channel
            #     color_value = pixel_ctx.getPixel(0)     # Color/ID buffer
            #     normal_value = pixel_ctx.getPixel(1)    # Normal buffer
            #     position_value = pixel_ctx.getPixel(2)  # Position buffer
            #     
            #     print(f"                    Color:    {color_value}")
            #     print(f"                    Normal:   {normal_value}")
            #     print(f"                    Position: {position_value}")
            #     
            #     # Decode pick ID from color channel
            #     decoded_id = decode_pick_id(color_value)
            #     print(f"                    Decoded ID: {decoded_id}")
        else:
            print(f"  Pixel at ({x:3}, {y:3}): FAILED - Capture not completed")
    
    print("\nEnding main thread...")
    ezapp.mainThreadEnd()
        
    print("="*60)
    print("Multi-channel pixel pick test completed!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())