#!/usr/bin/env ork.python
"""
Test for offscreen Vulkan rendering with multiple frame captures.
Renders 4 different colored RTGs and captures each to a separate PNG file.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
import os
import time
from orkengine import core
from orkengine import lev2
from obt import path as obt_path

tokens = core.CrcStringProxy()

def main():
    print("="*60)
    print("Starting multi-RTG offscreen frame capture test")
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
    
    # Define colors for each RTG
    colors = [
        core.vec4(1.0, 0.0, 0.0, 1.0),  # Red
        core.vec4(0.0, 1.0, 0.0, 1.0),  # Green
        core.vec4(0.0, 0.0, 1.0, 1.0),  # Blue
        core.vec4(1.0, 1.0, 0.0, 1.0),  # Yellow
    ]
    
    # Create 4 render target groups for offscreen rendering
    print("Creating 4 render target groups...")
    width = 256
    height = 256
    rtgs = []
    
    for i in range(4):
        rtg = lev2.RtGroup(ctx, width, height)
        # Add a color buffer
        rtb_color = rtg.createBuffer(tokens.RGBA8, tokens.color)
        # Set clear color for this RTG
        rtb_color.clearColor = colors[i]
        
        rtgs.append({
            'rtg': rtg,
            'rtb': rtb_color,
            'color': colors[i],
            'index': i
        })
        
        print(f"  RTG {i}: {width}x{height}, color: R={colors[i].x:.1f} G={colors[i].y:.1f} B={colors[i].z:.1f}")
    
    # Begin frame
    print("\nBeginning frame...")
    ctx.beginFrame()
    
    # Render and capture each RTG
    print("Rendering and capturing 4 RTGs...")
    captures = []
    
    for rtg_info in rtgs:
        rtg = rtg_info['rtg']
        rtb = rtg_info['rtb']
        idx = rtg_info['index']
        
        # Push RTG
        fbi.rtGroupPush(rtg)
        
        # Clear the framebuffer (uses the clearColor we set)
        fbi.rtGroupClear(rtg)
        
        # Pop RTG
        fbi.rtGroupPop()
        
        # Capture the frame to a PNG file
        output_path = core.Path(f"/tmp/test_capture_{idx}.png")
        print(f"  Capturing RTG {idx} to {output_path}...")
        
        # Get the render target buffer and capture it - returns a future
        capture_future = fbi.captureToFile(rtb, output_path)
        captures.append((idx, output_path, capture_future))
    
    # End frame
    print("\nEnding frame...")
    ctx.endFrame()
    
    # Wait for all captures to complete
    print("\nWaiting for all captures to complete...")
    all_done = False
    max_iterations = 100
    iteration = 0
    
    while not all_done and iteration < max_iterations:
        ezapp.mainThreadIter()
        
        # Check if all captures are ready
        all_done = True
        for idx, path, future in captures:
            if future and not future.is_ready:
                all_done = False
                break
        
        iteration += 1
    
    # Report results
    print("\n" + "="*60)
    print("Capture Results:")
    print("="*60)
    
    for idx, path, future in captures:
        if future and future.is_ready and path.exists:
            # Get file size
            file_size = os.path.getsize(str(path))
            color = colors[idx]
            print(f"  RTG {idx}: SUCCESS - {path} ({file_size} bytes)")
            print(f"           Color: R={color.x:.1f} G={color.y:.1f} B={color.z:.1f}")
        else:
            print(f"  RTG {idx}: FAILED - Capture not completed or file not created")
    
    print("\nEnding main thread...")
    ezapp.mainThreadEnd()
        
    print("="*60)
    print("Multi-RTG offscreen frame capture test completed!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())