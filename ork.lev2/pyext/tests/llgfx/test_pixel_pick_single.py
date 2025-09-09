#!/usr/bin/env ork.python
"""
Test for single channel pixel picking with async capture.
Renders a color-coded ID buffer and picks a single pixel asynchronously.
"""

import sys
import os
import time
from orkengine.core import vec2, vec3, vec4
from orkengine.core import mtx4
from orkengine import core
from orkengine import lev2
from obt import path as obt_path

tokens = core.CrcStringProxy()

def main():
    print("="*60)
    print("Starting single channel pixel pick test")
    print("="*60)
    
    # Initialize lev2 app with offscreen graphics context
    print("Initializing lev2 app...")
    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()

    print(f"graphics context: {ctx}")
    FBI = ctx.FBI
    GBI = ctx.GBI
    
    print("Starting main thread...")
    ezapp.mainThreadBegin()
    
    # Create render target group for offscreen rendering
    print("Creating render target group...")
    width = 256
    height = 256
    rtg = lev2.RtGroup(ctx, width, height)
    
    # Add a color buffer (RGBA8 for ID encoding)
    rtb_color = rtg.createBuffer(tokens.RGBA8, tokens.color)
    
    # Set clear color to black (ID=0)
    rtb_color.clearColor = core.vec4(0.0, 0.0, 0.0, 1.0)
    
    print(f"Created RTG: {width}x{height}")
    
    # Begin frame
    print("\nBeginning frame...")

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx,"orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendering_model = "FORWARD_UNLIT"
    permu.technique = mtl.shader.technique("vtxcolor")
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.name = "XXX"
    permu.instanced = False
    permu.skinned = False
    permu.is_picking = False
    permu.stereo = False
    permu.has_vtxcolors = False
    pipeline.bindParam(mtl.param("MatMVP"), mtx4())
    
    vtx_t = lev2.VtxV12N12B12T8C4
    vbuf = vtx_t.staticBuffer(3)
    vw = GBI.lock(vbuf,3)
    vtx = vtx_t(vec3(),vec3(),vec3(),vec2(),0xffffffff)
    vw.add(vtx)
    vw.add(vtx)
    vw.add(vtx)
    GBI.unlock(vw)

    ctx.beginFrame()
    FBI.rtGroupPush(rtg)
    FBI.rtGroupClear(rtg)
    
    RCFD = lev2.RenderContextFrameData(ctx)
    mtl.begin(permu.technique,RCFD)
    GBI.drawTriangles(vw)
    mtl.end(RCFD)
    
    # Pop RTG
    FBI.rtGroupPop()
    
    # Test pixel picking at several locations
    test_points = [
        (0, 0),       # Top-left corner
        (127, 127),   # Center
        (255, 255),   # Bottom-right corner
        (64, 192),    # Random point
    ]
    
    print("\nCapturing pixels at test points...")
    captures = []
    
    for x, y in test_points:
        
        # Capture single pixel asynchronously (must be done before endFrame)
        capture_future = FBI.capturePixel(rtg, x, y)
        captures.append((x, y, capture_future))
        print(f"  Capturing pixel at ({x}, {y}) : {capture_future}")
    
    # End frame (submits command buffer with capture commands)
    print("\nEnding frame...")
    ctx.endFrame()
    
    # Wait for all captures to complete
    print("\nWaiting for pixel captures to complete...")
    all_done = False
    #max_iterations = 100
    iteration = 0
    
    while not all_done:# and iteration < max_iterations:

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
    print("Single Pixel Capture Results:")
    print("="*60)
    
    for x, y, future in captures:
        if future and future.is_ready:
            # TODO: Extract pixel value from future when PixelFetchContext is populated
            print(f"  Pixel at ({x:3}, {y:3}): SUCCESS - Capture completed")
            
            # Once we have PixelFetchContext populated, we can decode the ID:
            # pixel_ctx = future.pixelFetchContext
            # if pixel_ctx:
            #     # Get the first channel (color buffer)
            #     value = pixel_ctx.getPixel(0)  # Channel 0
            #     # Decode ID from RGBA
            #     decoded_id = decode_pick_id(value)
            #     print(f"                    Decoded ID: {decoded_id}")
        else:
            print(f"  Pixel at ({x:3}, {y:3}): FAILED - Capture not completed")
    
    print("\nEnding main thread...")
    ezapp.mainThreadEnd()
        
    print("="*60)
    print("Single channel pixel pick test completed!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())