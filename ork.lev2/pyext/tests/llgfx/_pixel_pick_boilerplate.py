#!/usr/bin/env ork.python
"""
Common boilerplate code for pixel picking tests.
Provides shared rendering and capture functionality.
"""

import sys
import os
import time
from orkengine.core import vec2, vec3, vec4
from orkengine.core import mtx4
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

class PixelPickTest:
    """Encapsulates all test state to ensure proper object lifetime."""
    
    def __init__(self, buffer_format, format_name):
        self.buffer_format = buffer_format
        self.format_name = format_name
        self.ezapp = None
        self.ctx = None
        self.FBI = None
        self.GBI = None
        self.rtg = None
        self.rtb_color = None
        self.mtl = None
        self.permu = None
        self.pipeline = None
        self.vtx_t = None
        self.vbuf = None  # CRITICAL: Keep vbuf alive!
        self.vw = None
        self.captures = []
        
    def initialize(self):
        """Initialize lev2 app and graphics context."""
        print("="*60)
        print(f"Starting single channel pixel pick test ({self.format_name})")
        print("="*60)
        
        print("Initializing lev2 app...")
        self.ezapp = lev2.lev2appinit()
        gfxenv = lev2.GfxEnv.ref
        self.ctx = gfxenv.loadingContext()
        
        print(f"graphics context: {self.ctx}")
        self.FBI = self.ctx.FBI
        self.GBI = self.ctx.GBI
        
        print("Starting main thread...")
        self.ezapp.mainThreadBegin()
        
    def setup_render_target(self, width=256, height=256):
        """Create render target group for offscreen rendering."""
        print(f"Creating render target group with format {self.format_name}...")
        self.rtg = lev2.RtGroup(self.ctx, width, height)
        
        # Add a color buffer with specified format
        self.rtb_color = self.rtg.createBuffer(self.buffer_format, tokens.color)
        
        # Set clear color to black (ID=0)
        self.rtb_color.clearColor = core.vec4(0.0, 0.0, 0.0, 1.0)
        
        print(f"Created RTG: {width}x{height} with format {self.format_name}")
        
    def setup_material(self):
        """Set up material and pipeline for rendering."""
        print("\nSetting up rendering...")
        self.mtl = lev2.FreestyleMaterial()
        self.mtl.gpuInit(self.ctx, "orkshader://solid.fxv2")
        self.permu = lev2.FxPipelinePermutation()
        self.permu.rendering_model = "CUSTOM"
        self.permu.technique = self.mtl.shader.technique("vtxcolor")
        self.pipeline = self.mtl.fxcache.findPipeline(self.permu)
        self.pipeline.name = "test_pattern"
        self.permu.instanced = False
        self.permu.skinned = False
        self.permu.is_picking = False
        self.permu.stereo = False
        self.permu.has_vtxcolors = True  # Enable vertex colors
        
        # Use identity matrix (NDC coordinates already in -1 to 1 range)
        proj_mtx = mtx4()
        self.pipeline.bindParam(self.mtl.param("MatMVP"), proj_mtx)
        
    def create_test_pattern_vb(self):
        """Create a vertex buffer with a 2x2 colored quad test pattern."""
        self.vtx_t = lev2.VtxV12N12B12T8C4
        
        # 4 quads * 2 triangles * 3 vertices = 24 vertices
        self.vbuf = self.vtx_t.staticBuffer(24)  # Keep vbuf as member!
        self.vw = self.GBI.lock(self.vbuf, 24)
        
        # Helper to add a colored quad
        def add_quad(x0, y0, x1, y1, color_rgba):
            normal = vec3(0, 0, 1)
            binormal = vec3(1, 0, 0)
            uv = vec2(0, 0)
            
            # Triangle 1: bottom-left, bottom-right, top-right
            self.vw.add(self.vtx_t(vec3(x0, y0, 0), normal, binormal, uv, color_rgba))
            self.vw.add(self.vtx_t(vec3(x1, y1, 0), normal, binormal, uv, color_rgba))
            self.vw.add(self.vtx_t(vec3(x1, y0, 0), normal, binormal, uv, color_rgba))
            
            # Triangle 2: bottom-left, top-right, top-left
            self.vw.add(self.vtx_t(vec3(x0, y0, 0), normal, binormal, uv, color_rgba))
            self.vw.add(self.vtx_t(vec3(x0, y1, 0), normal, binormal, uv, color_rgba))
            self.vw.add(self.vtx_t(vec3(x1, y1, 0), normal, binormal, uv, color_rgba))
        
        # Create test pattern with 4 colored quadrants
        # In Vulkan, NDC Y goes from -1 (top) to 1 (bottom)
        # Top-left: Blue (0xFFFF0000 in RGBA8)
        add_quad(-1.0, -1.0, 0.0, 0.0, 0xFFFF0000)
        
        # Top-right: Yellow (0xFF00FFFF in RGBA8)
        add_quad(0.0, -1.0, 1.0, 0.0, 0xFF00FFFF)
        
        # Bottom-left: Red (0xFF0000FF in RGBA8)
        add_quad(-1.0, 0.0, 0.0, 1.0, 0xFF0000FF)
        
        # Bottom-right: Green (0xFF00FF00 in RGBA8)
        add_quad(0.0, 0.0, 1.0, 1.0, 0xFF00FF00)
        
        self.GBI.unlock(self.vw)
        
    def render_frame(self):
        """Render a frame."""
        self.ctx.beginFrame()
        self.FBI.rtGroupPush(self.rtg)
        self.FBI.rtGroupClear(self.rtg)
        
        RCFD = lev2.RenderContextFrameData(self.ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        RCID.forceTechnique(self.permu.technique)
        RCID.genMatrix(lambda: mtx4())
        
        self.pipeline.wrappedDrawCall(RCID, lambda: self.GBI.drawTriangles(self.vw))
        
        # Pop RTG
        self.FBI.rtGroupPop()
        self.ctx.endFrame()
        
    def capture_test_pixels(self):
        """Capture pixels at test points."""
        # Test points and expected colors for verification
        # In Vulkan, NDC Y goes from -1 (top) to 1 (bottom)
        # The framebuffer is 256x256, with 4 quadrants:
        # Top-left (0,0 to 127,127): Blue
        # Top-right (128,0 to 255,127): Yellow  
        # Bottom-left (0,128 to 127,255): Red
        # Bottom-right (128,128 to 255,255): Green
        test_points = [
            (64, 64, "Blue", vec4(0.0, 0.0, 1.0, 1.0)),       # Top-left quadrant
            (192, 64, "Yellow", vec4(1.0, 1.0, 0.0, 1.0)),    # Top-right quadrant
            (64, 192, "Red", vec4(1.0, 0.0, 0.0, 1.0)),       # Bottom-left quadrant
            (192, 192, "Green", vec4(0.0, 1.0, 0.0, 1.0)),    # Bottom-right quadrant
            (127, 127, "Blue/Yellow/Red/Green boundary", None),  # Center boundary
        ]
        
        # Begin new frame for captures
        self.ctx.beginFrame()
        
        print("\nCapturing pixels at test points...")
        self.captures = []
        
        for x, y, expected_name, expected_color in test_points:
            # Capture single pixel asynchronously (must be done before endFrame)
            capture_future = self.FBI.capturePixel(self.rtg, x, y)
            self.captures.append((x, y, expected_name, expected_color, capture_future))
            print(f"  Capturing pixel at ({x:3}, {y:3}) - expecting {expected_name}")
        
        # End frame (submits command buffer with capture commands)
        print("\nEnding frame...")
        self.ctx.endFrame()
        
    def wait_for_captures(self):
        """Wait for all captures to complete."""
        print("\nWaiting for pixel captures to complete...")
        all_done = False
        iteration = 0
        
        while not all_done:
            self.ezapp.mainThreadIter()
            
            # Check if all captures are ready
            all_done = True
            for x, y, expected_name, expected_color, future in self.captures:
                if future and not future.is_ready:
                    all_done = False
                    break
            
            iteration += 1
            
    def verify_results(self):
        """Verify captured pixels match expected values."""
        print("\n" + "="*60)
        print(f"Single Pixel Capture Results ({self.format_name}):")
        print("="*60)
        
        passed_tests = 0
        total_tests = 0
        
        for x, y, expected_name, expected_color, future in self.captures:
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
            print(f"✓ All pixel captures completed successfully for {self.format_name}!")
        else:
            print(f"✗ Some pixel captures failed for {self.format_name}")
        
        return 0 if passed_tests == total_tests else 1
        
    def cleanup(self):
        """Clean up resources."""
        print("\nEnding main thread...")
        self.ezapp.mainThreadEnd()
        
        print("="*60)
        print(f"Single channel pixel pick test ({self.format_name}) completed!")
        print("="*60)
        
    def run(self, enable_render_loop=False):
        """Run the complete test."""
        self.initialize()
        self.setup_render_target()
        self.setup_material()
        self.create_test_pattern_vb()
        
        # Render the test pattern
        if enable_render_loop:
            counter = 0
            while counter < 100000:
                self.render_frame()
                counter += 1
                time.sleep(1)
        else:
            self.render_frame()
        
        self.capture_test_pixels()
        self.wait_for_captures()
        result = self.verify_results()
        self.cleanup()
        
        return result

def run_pixel_pick_test(buffer_format, format_name, enable_render_loop=False):
    """
    Run a pixel picking test with the specified buffer format.
    
    Args:
        buffer_format: The buffer format token (e.g., tokens.RGBA32F)
        format_name: Human-readable format name for display
        enable_render_loop: Whether to run continuous rendering loop
    """
    test = PixelPickTest(buffer_format, format_name)
    return test.run(enable_render_loop)