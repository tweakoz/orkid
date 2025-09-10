#!/usr/bin/env ork.python
"""
Test for multi-buffer (deep pixel) picking with mixed formats.
Tests simultaneous capture from RGBA32F, RGBA16F, RGBA32UI, and RGBA16UI buffers.
"""

import sys
from orkengine import core
from orkengine import lev2
from orkengine.core import vec4
from _pixel_pick_boilerplate import PixelPickTest

tokens = core.CrcStringProxy()

# Shader text for multi-buffer output
MULTI_BUFFER_SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublk_vtx {
  mat4 MatMVP;
}
////////////////////////////////////////
uniform_set ublk_frg {
  // No uniforms needed for simple output
}
////////////////////////////////////////
vertex_interface vif_multi : ublk_vtx {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    vec4 frg_color;
    flat uvec4 frg_pickid16;
    flat uvec4 frg_pickid32;
  }
}
////////////////////////////////////////
fragment_interface fif_multi : vif_multi : ublk_frg {
  outputs { 
    vec4 out_color;      // RGBA32F (layout location 0)
    vec4 out_color16f;   // RGBA16F (layout location 1)
    uvec4 out_pickid32;  // RGBA32UI (layout location 2)
    uvec4 out_pickid16;  // RGBA16UI (layout location 3)
  }
}
////////////////////////////////////////
vertex_shader vs_multi : vif_multi {
  frg_color = vtxcolor;
  
  // Convert vertex color to 16-bit pick IDs (0-65535 range)
  uint r16 = uint(vtxcolor.r * 65535.0);
  uint g16 = uint(vtxcolor.g * 65535.0);
  uint b16 = uint(vtxcolor.b * 65535.0);
  uint a16 = uint(vtxcolor.w * 65535.0);
  frg_pickid16 = uvec4(r16, g16, b16, a16);
  
  // Convert vertex color to 32-bit pick IDs (using lower values for testing)
  // Use 0-255 range to match RGBA8 test expectations
  uint r32 = uint(vtxcolor.r * 255.0);
  uint g32 = uint(vtxcolor.g * 255.0);
  uint b32 = uint(vtxcolor.b * 255.0);
  uint a32 = uint(vtxcolor.w * 255.0);
  frg_pickid32 = uvec4(r32, g32, b32, a32);
  
  gl_Position = MatMVP * vec4(position.xyz, 1.0);
}
////////////////////////////////////////
fragment_shader ps_multi : fif_multi {
  // Output to all 4 render targets
  out_color = frg_color;           // RGBA32F - direct color
  out_color16f = frg_color;         // RGBA16F - direct color (will be quantized by format)
  out_pickid32 = frg_pickid32;      // RGBA32UI - 32-bit IDs
  out_pickid16 = frg_pickid16;      // RGBA16UI - 16-bit IDs
}
////////////////////////////////////////
state_block sb_multi : default {
  // Default state block
}
////////////////////////////////////////
technique tek_multi_buffer {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_multi;
    fragment_shader = ps_multi;
    state_block     = sb_multi;
  }
}
"""

def createMultiPipeline(ctx, shadertext, shadername="multi_buffer"):
    """Create a pipeline from shader text for multi-buffer rendering."""
    material = lev2.FreestyleMaterial()
    material.gpuInitFromShaderText(ctx, shadername, shadertext)
    
    # Configure for multi-target rendering
    material.rasterstate.setBlendingMacro(tokens.OFF)
    material.rasterstate.culltest = tokens.PASS_FRONT
    material.rasterstate.depthtest = tokens.LEQUALS
    
    # Create pipeline permutation
    permu = lev2.FxPipelinePermutation()
    permu.rendering_model = "CUSTOM"
    permu.technique = material.shader.technique("tek_multi_buffer")
    
    # Find and configure pipeline
    pipeline = material.fxcache.findPipeline(permu)
    pipeline.bindParam(material.param("MatMVP"), tokens.RCFD_Camera_MVP_Mono)
    
    # Store material reference
    pipeline.sharedMaterial = material
    return pipeline, material

class MultiBufferPickTest(PixelPickTest):
    """Extended test class for multi-buffer deep pixel picking."""
    
    def __init__(self, format_names):
        super().__init__(format_names)
        self.custom_pipeline = None
        self.custom_material = None
        
    def setup_material(self):
        """Override to use custom multi-buffer pipeline."""
        print("\nSetting up multi-buffer rendering pipeline...")
        self.custom_pipeline, self.custom_material = createMultiPipeline(
            self.ctx, 
            MULTI_BUFFER_SHADERTEXT
        )
        # Store these instead of the default material setup
        self.pipeline = self.custom_pipeline
        self.mtl = self.custom_material
        # We don't need permu for custom pipeline
        self.permu = None
        
    def render_frame(self):
        """Override render to use custom pipeline."""
        self.ctx.beginFrame()
        self.FBI.rtGroupPush(self.rtg)
        self.FBI.rtGroupClear(self.rtg)
        
        RCFD = lev2.RenderContextFrameData(self.ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        
        # For custom pipeline, we don't force technique since it's already set
        if self.permu:
            RCID.forceTechnique(self.permu.technique)
        
        from orkengine.core import mtx4
        RCID.genMatrix(lambda: mtx4())
        
        self.pipeline.wrappedDrawCall(RCID, lambda: self.GBI.drawTriangles(self.vw))
        
        # Pop RTG
        self.FBI.rtGroupPop()
        self.ctx.endFrame()
        
    def capture_test_pixels(self):
        """Override to capture and verify all 4 buffers."""
        # Test points - we'll verify all 4 buffers at each point
        test_points = [
            (64, 64, "Blue"),       # Top-left quadrant
            (192, 64, "Yellow"),    # Top-right quadrant
            (64, 192, "Red"),       # Bottom-left quadrant
            (192, 192, "Green"),    # Bottom-right quadrant
            (127, 127, "Boundary"), # Center boundary
        ]
        
        # Expected values for each buffer at each test point
        # Format: [RGBA32F, RGBA16F, RGBA32UI, RGBA16UI]
        expected_values = {
            "Blue": [
                vec4(0.0, 0.0, 1.0, 1.0),     # RGBA32F
                vec4(0.0, 0.0, 1.0, 1.0),     # RGBA16F (same as 32F)
                vec4(0.0, 0.0, 255.0, 255.0), # RGBA32UI (0-255 range)
                vec4(0.0, 0.0, 65535.0, 65535.0), # RGBA16UI (0-65535 range)
            ],
            "Yellow": [
                vec4(1.0, 1.0, 0.0, 1.0),
                vec4(1.0, 1.0, 0.0, 1.0),
                vec4(255.0, 255.0, 0.0, 255.0),
                vec4(65535.0, 65535.0, 0.0, 65535.0),
            ],
            "Red": [
                vec4(1.0, 0.0, 0.0, 1.0),
                vec4(1.0, 0.0, 0.0, 1.0),
                vec4(255.0, 0.0, 0.0, 255.0),
                vec4(65535.0, 0.0, 0.0, 65535.0),
            ],
            "Green": [
                vec4(0.0, 1.0, 0.0, 1.0),
                vec4(0.0, 1.0, 0.0, 1.0),
                vec4(0.0, 255.0, 0.0, 255.0),
                vec4(0.0, 65535.0, 0.0, 65535.0),
            ],
            "Boundary": [None, None, None, None],  # Any value acceptable
        }
        
        # Begin new frame for captures
        self.ctx.beginFrame()
        
        print("\nCapturing pixels at test points...")
        self.captures = []
        
        for x, y, color_name in test_points:
            # Create PixelFetchContext for 4-buffer capture
            pfc = lev2.PixelFetchContext(self.rtg, 4)
            
            # Set usage modes for each buffer
            pfc.setUsage(0, tokens.FVEC4)    # RGBA32F - as float vector
            pfc.setUsage(1, tokens.FVEC4)    # RGBA16F - as float vector
            pfc.setUsage(2, tokens.FVEC4)    # RGBA32UI - convert to float for comparison
            pfc.setUsage(3, tokens.FVEC4)    # RGBA16UI - convert to float for comparison
            
            # Capture pixel asynchronously
            capture_future = self.FBI.capturePixel(pfc, x, y)
            self.captures.append((x, y, color_name, expected_values[color_name], capture_future))
            print(f"  Capturing pixel at ({x:3}, {y:3}) - expecting {color_name}")
        
        # End frame (submits command buffer with capture commands)
        print("\nEnding frame...")
        self.ctx.endFrame()
        
    def verify_results(self):
        """Override to verify all 4 buffers per pixel."""
        print("\n" + "="*60)
        print(f"Multi-Buffer Pixel Capture Results:")
        print("="*60)
        
        passed_tests = 0
        total_tests = 0
        
        for x, y, expected_name, expected_values, future in self.captures:
            if future.is_ready:
                pfc = future.pixelFetchContext
                if pfc:
                    print(f"\nPixel at ({x:3}, {y:3}) - {expected_name}:")
                    
                    # Check each buffer
                    for buf_idx, (fmt_name, expected_val) in enumerate(zip(self.format_names, expected_values)):
                        total_tests += 1
                        
                        if buf_idx < pfc.numValues:
                            pixel_value = pfc.value(buf_idx)
                            print(f"  {fmt_name:8} : {pixel_value}", end="")
                            
                            if expected_val is not None:
                                # Tolerance for comparison
                                tolerance = 0.01 if "F" in fmt_name else 0.5  # Float formats need tighter tolerance
                                
                                r_match = abs(pixel_value.x - expected_val.x) < tolerance
                                g_match = abs(pixel_value.y - expected_val.y) < tolerance
                                b_match = abs(pixel_value.z - expected_val.z) < tolerance
                                a_match = abs(pixel_value.w - expected_val.w) < tolerance
                                
                                if r_match and g_match and b_match and a_match:
                                    print(f" ✓ (expected {expected_val})")
                                    passed_tests += 1
                                else:
                                    print(f" ✗ (expected {expected_val})")
                            else:
                                # Boundary pixel
                                print(" (boundary)")
                                passed_tests += 1
                        else:
                            print(f"  {fmt_name:8} : MISSING BUFFER")
                else:
                    print(f"Pixel at ({x:3}, {y:3}): FAILED - PixelFetchContext not available")
                    total_tests += len(self.format_names)
            else:
                print(f"Pixel at ({x:3}, {y:3}): FAILED - Capture not completed")
                total_tests += len(self.format_names)
        
        print("\n" + "-"*60)
        print(f"Test Summary: {passed_tests}/{total_tests} tests passed")
        if passed_tests == total_tests:
            print(f"✓ All multi-buffer captures completed successfully!")
        else:
            print(f"✗ Some multi-buffer captures failed")
        
        return 0 if passed_tests == total_tests else 1

def main():
    # Create test with 4 different buffer formats
    # This tests deep pixel rendering with mixed formats
    test = MultiBufferPickTest(["RGBA32F", "RGBA16F", "RGBA32UI", "RGBA16UI"])
    
    # Run the test
    return test.run()

if __name__ == "__main__":
    sys.exit(main())