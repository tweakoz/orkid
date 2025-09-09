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

def render_and_capture(ctx,rtg,mtl,tek,pipeline,vw):
    FBI = ctx.FBI
    GBI = ctx.GBI
    ctx.beginFrame()
    FBI.rtGroupPush(rtg)
    FBI.rtGroupClear(rtg)
    
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(tek)
    RCID.genMatrix(lambda: mtx4())


    pipeline.wrappedDrawCall(RCID, lambda: GBI.drawTriangles(vw) )
    
    # Pop RTG
    FBI.rtGroupPop()
    ctx.endFrame()

    
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

    # Setup material and rendering
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx,"orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendering_model = "CUSTOM"
    permu.technique = mtl.shader.technique("vtxcolor")
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.name = "test_pattern"
    permu.instanced = False
    permu.skinned = False
    permu.is_picking = False
    permu.stereo = False
    permu.has_vtxcolors = True  # Enable vertex colors
    
    # Use identity matrix (NDC coordinates already in -1 to 1 range)
    proj_mtx = mtx4()
    pipeline.bindParam(mtl.param("MatMVP"), proj_mtx)
    
    # Create vertex buffer for test pattern - 4 colored quads in different corners
    vtx_t = lev2.VtxV12N12B12T8C4
    # 4 quads * 2 triangles * 3 vertices = 24 vertices
    vbuf = vtx_t.staticBuffer(24)
    vw = GBI.lock(vbuf, 24)
    
    # Helper to add a colored quad
    def add_quad(vw, x0, y0, x1, y1, color_rgba):
        """Add a quad with specified bounds and color"""
        normal = vec3(0, 0, 1)
        binormal = vec3(1, 0, 0)
        uv = vec2(0, 0)
        
        # Triangle 1: bottom-left, bottom-right, top-right
        vw.add(vtx_t(vec3(x0, y0, 0), normal, binormal, uv, color_rgba))
        vw.add(vtx_t(vec3(x1, y1, 0), normal, binormal, uv, color_rgba))
        vw.add(vtx_t(vec3(x1, y0, 0), normal, binormal, uv, color_rgba))
        
        # Triangle 2: bottom-left, top-right, top-left
        vw.add(vtx_t(vec3(x0, y0, 0), normal, binormal, uv, color_rgba))
        vw.add(vtx_t(vec3(x0, y1, 0), normal, binormal, uv, color_rgba))
        vw.add(vtx_t(vec3(x1, y1, 0), normal, binormal, uv, color_rgba))
    
    # Create test pattern with 4 colored quadrants
    # Top-left: Red (0xFF0000FF in RGBA8)
    add_quad(vw, -1.0, 0.0, 0.0, 1.0, 0xFF0000FF)
    
    # Top-right: Green (0x00FF00FF in RGBA8)
    add_quad(vw, 0.0, 0.0, 1.0, 1.0, 0xFF00FF00)
    
    # Bottom-left: Blue (0x0000FFFF in RGBA8)
    add_quad(vw, -1.0, -1.0, 0.0, 0.0, 0xFFFF0000)
    
    # Bottom-right: Yellow (0xFFFF00FF in RGBA8)
    add_quad(vw, 0.0, -1.0, 1.0, 0.0, 0xFF00FFFF)
    
    GBI.unlock(vw)

    #counter = 0
    #while counter<100000:
    #  render_and_capture(ctx,rtg,mtl,permu.technique,pipeline,vw)
    #  counter += 1
    #  time.sleep(1)
    render_and_capture(ctx,rtg,mtl,permu.technique,pipeline,vw)
  
    ctx.beginFrame()

    # Test pixel picking at known locations in our test pattern
    # The framebuffer is 256x256, with 4 quadrants:
    # Top-left (0,0 to 127,127): Red
    # Top-right (128,0 to 255,127): Green  
    # Bottom-left (0,128 to 127,255): Blue
    # Bottom-right (128,128 to 255,255): Yellow
    test_points = [
        (64, 64, "Red", vec4(1.0, 0.0, 0.0, 1.0)),       # Top-left quadrant
        (192, 64, "Green", vec4(0.0, 1.0, 0.0, 1.0)),    # Top-right quadrant
        (64, 192, "Blue", vec4(0.0, 0.0, 1.0, 1.0)),     # Bottom-left quadrant
        (192, 192, "Yellow", vec4(1.0, 1.0, 0.0, 1.0)),  # Bottom-right quadrant
        (127, 127, "Red/Green/Blue/Yellow boundary", None),  # Center boundary (could be any)
    ]
    
    print("\nCapturing pixels at test points...")
    captures = []
    
    for x, y, expected_name, expected_color in test_points:
        # Capture single pixel asynchronously (must be done before endFrame)
        capture_future = FBI.capturePixel(rtg, x, y)
        captures.append((x, y, expected_name, expected_color, capture_future))
        print(f"  Capturing pixel at ({x:3}, {y:3}) - expecting {expected_name}")
    
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
        for x, y, expected_name, expected_color, future in captures:
            if future and not future.is_ready:
                all_done = False
                break
        
        iteration += 1
    
    # Report results
    print("\n" + "="*60)
    print("Single Pixel Capture Results:")
    print("="*60)
    
    passed_tests = 0
    total_tests = 0
    
    for x, y, expected_name, expected_color, future in captures:
        total_tests += 1
        if future and future.is_ready:
            # Get the actual pixel data
            pixel_ctx = future.pixelFetchContext
            if pixel_ctx:
                # Get pixel value from the first (color) buffer
                pixel_value = pixel_ctx.value(0)  # Buffer index 0
                print(f"  Pixel at ({x:3}, {y:3}): Captured {pixel_value} - Expected: {expected_name}")
                
                if expected_color:
                    # Compare with expected color (with tolerance for float precision)
                    tolerance = 0.01
                    r_match = abs(pixel_value.x - expected_color.x) < tolerance
                    g_match = abs(pixel_value.y - expected_color.y) < tolerance
                    b_match = abs(pixel_value.z - expected_color.z) < tolerance
                    a_match = abs(pixel_value.w - expected_color.w) < tolerance
                    
                    if r_match and g_match and b_match and a_match:
                        print(f"                    ✓ Color matches expected {expected_color}")
                        passed_tests += 1
                    else:
                        print(f"                    ✗ Color mismatch! Expected {expected_color}")
                else:
                    # Boundary pixel - any color is acceptable
                    print(f"                    (Boundary pixel - color {pixel_value})")
                    passed_tests += 1
            else:
                print(f"  Pixel at ({x:3}, {y:3}): FAILED - PixelFetchContext not available")
        else:
            print(f"  Pixel at ({x:3}, {y:3}): FAILED - Capture not completed")
    
    print("\n" + "-"*60)
    print(f"Test Summary: {passed_tests}/{total_tests} tests passed")
    if passed_tests == total_tests:
        print("✓ All pixel captures completed successfully!")
    else:
        print("✗ Some pixel captures failed")
    
    print("\nEnding main thread...")
    ezapp.mainThreadEnd()
        
    print("="*60)
    print("Single channel pixel pick test completed!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())