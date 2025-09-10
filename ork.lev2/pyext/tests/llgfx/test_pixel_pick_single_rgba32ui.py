#!/usr/bin/env ork.python
"""
Test for single channel pixel picking with RGBA32UI format.
Tests unsigned integer buffer format used for pick IDs.
"""

import sys
from orkengine import core
from orkengine import lev2
from orkengine.core import vec4
from _pixel_pick_boilerplate import PixelPickTest

tokens = core.CrcStringProxy()

# Shader text for RGBA32UI output (modified from POINTCLOUD_SHADERTEXT)
RGBA32UI_PICK_SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublk_VPICK {
  mat4 MatMVP;
}
////////////////////////////////////////
uniform_set ublk_FPICK {
  // No uniforms needed for simple ID output
}
////////////////////////////////////////
vertex_interface vif_PICK : ublk_VPICK {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    flat uvec4 frg_pickid;
  }
}
////////////////////////////////////////
fragment_interface fif_PICK : vif_PICK : ublk_FPICK {
  // inputs inherited from vertex interface
  outputs { 
    uvec4 out_pickid;
  }
}
////////////////////////////////////////
vertex_shader vs_pick : vif_PICK {
  // Convert vertex color (0-255 range packed in RGBA8) to pick ID
  // For this test, we'll use the color components as ID components
  // In real picking, this would be an object ID
  uint r = uint(vtxcolor.r * 255.0);
  uint g = uint(vtxcolor.g * 255.0);
  uint b = uint(vtxcolor.b * 255.0);
  uint a = uint(vtxcolor.w * 255.0);
  frg_pickid = uvec4(r, g, b, a);
  gl_Position = MatMVP * vec4(position.xyz, 1.0);
}
////////////////////////////////////////
fragment_shader ps_pick : fif_PICK {
  out_pickid = frg_pickid;
}
////////////////////////////////////////
state_block sb_pick : default {
  // Default state block
}
////////////////////////////////////////
technique tek_pick_rgba32ui {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_pick;
    fragment_shader = ps_pick;
    state_block     = sb_pick;
  }
}
"""

def createPipeline(ctx, shadertext, shadername="rgba32ui_pick"):
    """Create a pipeline from shader text for RGBA32UI picking."""
    material = lev2.FreestyleMaterial()
    material.gpuInitFromShaderText(ctx, shadername, shadertext)
    
    # Configure for picking (no blending, depth test enabled)
    material.rasterstate.setBlendingMacro(tokens.OFF)
    material.rasterstate.culltest = tokens.PASS_FRONT
    material.rasterstate.depthtest = tokens.LEQUALS
    
    # Create pipeline permutation
    permu = lev2.FxPipelinePermutation()
    permu.rendering_model = "CUSTOM"
    permu.technique = material.shader.technique("tek_pick_rgba32ui")
    
    # Find and configure pipeline
    pipeline = material.fxcache.findPipeline(permu)
    pipeline.bindParam(material.param("MatMVP"), tokens.RCFD_Camera_MVP_Mono)
    
    # Store material reference
    pipeline.sharedMaterial = material
    return pipeline, material

class RGBA32UIPickTest(PixelPickTest):
    """Extended test class that uses custom pipeline for RGBA32UI."""
    
    def __init__(self, buffer_format, format_name):
        super().__init__(buffer_format, format_name)
        self.custom_pipeline = None
        self.custom_material = None
        
    def setup_material(self):
        """Override to use custom pipeline for RGBA32UI."""
        print("\nSetting up RGBA32UI pick rendering pipeline...")
        self.custom_pipeline, self.custom_material = createPipeline(
            self.ctx, 
            RGBA32UI_PICK_SHADERTEXT
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
        """Override to use RGBA32UI-specific test points."""
        # Test points and expected ID values for RGBA32UI
        # The shader converts vertex colors to unsigned int IDs
        test_points = [
            # For Blue (0xFFFF0000): BGRA layout -> B=255, G=0, R=0, A=255 -> becomes R=0, G=0, B=255, A=255
            (64, 64, "Blue ID", vec4(0.0, 0.0, 255.0, 255.0)),
            # For Yellow (0xFF00FFFF): BGRA layout -> B=0, G=255, R=255, A=255 -> becomes R=255, G=255, B=0, A=255  
            (192, 64, "Yellow ID", vec4(255.0, 255.0, 0.0, 255.0)),
            # For Red (0xFF0000FF): BGRA layout -> B=0, G=0, R=255, A=255 -> becomes R=255, G=0, B=0, A=255
            (64, 192, "Red ID", vec4(255.0, 0.0, 0.0, 255.0)),
            # For Green (0xFF00FF00): BGRA layout -> B=255, G=255, R=0, A=255 -> becomes R=0, G=255, B=0, A=255
            (192, 192, "Green ID", vec4(0.0, 255.0, 0.0, 255.0)),
            (127, 127, "Boundary", None),
        ]
        
        # Begin new frame for captures
        self.ctx.beginFrame()
        
        print("\nCapturing pixels at test points...")
        self.captures = []
        
        for x, y, expected_name, expected_value in test_points:
            # Create PixelFetchContext for RGBA32UI capture
            pfc = lev2.PixelFetchContext(self.rtg, 1)
            # Set usage to FVEC4 (converts uint32 values to float)
            pfc.setUsage(0, tokens.FVEC4)
            
            # Capture single pixel asynchronously
            capture_future = self.FBI.capturePixel(pfc, x, y)
            self.captures.append((x, y, expected_name, expected_value, capture_future))
            print(f"  Capturing pixel at ({x:3}, {y:3}) - expecting {expected_name}")
        
        # End frame (submits command buffer with capture commands)
        print("\nEnding frame...")
        self.ctx.endFrame()

def main():
    # RGBA32UI uses 32-bit unsigned integers per channel
    # When used for picking, these typically encode object IDs
    # For this test, vertex colors are converted to unsigned int IDs
    
    # Create and run the RGBA32UI-specific test
    test = RGBA32UIPickTest(tokens.RGBA32UI, "RGBA32UI")
    
    # The test expects these ID values based on the vertex colors:
    # Blue (0xFFFF0000) -> R=0, G=0, B=255, A=255
    # Yellow (0xFF00FFFF) -> R=255, G=255, B=0, A=255
    # Red (0xFF0000FF) -> R=255, G=0, B=0, A=255
    # Green (0xFF00FF00) -> R=0, G=255, B=0, A=255
    
    # Note: PixelFetchContext converts these back to floats for comparison
    # So we expect to see the uint values as floats
    
    return test.run()

if __name__ == "__main__":
    sys.exit(main())