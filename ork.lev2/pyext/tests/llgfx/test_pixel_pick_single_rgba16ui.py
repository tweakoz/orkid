#!/usr/bin/env ork.python
"""
Test for single channel pixel picking with RGBA16UI format.
Tests 16-bit unsigned integer buffer format used for pick IDs.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
from orkengine import core
from orkengine import lev2
from orkengine.core import vec4
from _pixel_pick_boilerplate import PixelPickTest

tokens = core.CrcStringProxy()

# Shader text for RGBA16UI output
RGBA16UI_PICK_SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublk_VTX16UI {
  mat4 MatMVP;
}
////////////////////////////////////////
uniform_set ublk_FRG16UI {
  // No uniforms needed for simple ID output
}
////////////////////////////////////////
vertex_interface vif_PICK16UI : ublk_VTX16UI {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    flat uvec4 frg_pickid;
  }
}
////////////////////////////////////////
fragment_interface fif_PICK16UI : vif_PICK16UI : ublk_FRG16UI {
  // inputs inherited from vertex interface
  outputs { 
    uvec4 out_pickid;
  }
}
////////////////////////////////////////
vertex_shader vs_pick16ui : vif_PICK16UI {
  // Convert vertex color to 16-bit pick IDs
  // Scale 0-1 range to 0-65535 range for 16-bit IDs
  uint r = uint(vtxcolor.r * 65535.0);
  uint g = uint(vtxcolor.g * 65535.0);
  uint b = uint(vtxcolor.b * 65535.0);
  uint a = uint(vtxcolor.w * 65535.0);
  frg_pickid = uvec4(r, g, b, a);
  gl_Position = MatMVP * vec4(position.xyz, 1.0);
}
////////////////////////////////////////
fragment_shader ps_pick16ui : fif_PICK16UI {
  out_pickid = frg_pickid;
}
////////////////////////////////////////
state_block sb_pick16ui : default {
  // Default state block
}
////////////////////////////////////////
technique tek_pick_rgba16ui {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_pick16ui;
    fragment_shader = ps_pick16ui;
    state_block     = sb_pick16ui;
  }
}
"""

def createPipeline(ctx, shadertext, shadername="rgba16ui_pick"):
    """Create a pipeline from shader text for RGBA16UI picking."""
    material = lev2.FreestyleMaterial()
    material.gpuInitFromShaderText(ctx, shadername, shadertext)
    
    # Configure for picking (no blending, depth test enabled)
    material.rasterstate.setBlendingMacro(tokens.OFF)
    material.rasterstate.culltest = tokens.PASS_FRONT
    material.rasterstate.depthtest = tokens.LEQUALS
    
    # Create pipeline permutation
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = material.shader.technique("tek_pick_rgba16ui")
    
    # Find and configure pipeline
    pipeline = material.fxcache.findPipeline(permu)
    pipeline.bindParam(material.param("MatMVP"), tokens.RCFD_Camera_MVP_Mono)
    
    # Store material reference
    pipeline.sharedMaterial = material
    return pipeline, material

class RGBA16UIPickTest(PixelPickTest):
    """Extended test class that uses custom pipeline for RGBA16UI."""
    
    def __init__(self, format_names):
        super().__init__(format_names)
        self.custom_pipeline = None
        self.custom_material = None
        
    def setup_material(self):
        """Override to use custom pipeline for RGBA16UI."""
        print("\nSetting up RGBA16UI pick rendering pipeline...")
        self.custom_pipeline, self.custom_material = createPipeline(
            self.ctx, 
            RGBA16UI_PICK_SHADERTEXT
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
        """Override to use RGBA16UI-specific test points."""
        # Test points and expected ID values for RGBA16UI
        # The shader converts vertex colors to 16-bit unsigned int IDs
        # Colors are scaled from 0-1 to 0-65535 range
        test_points = [
            # For Blue (0xFFFF0000): r=0, g=0, b=1, a=1 -> R=0, G=0, B=65535, A=65535
            (64, 64, "Blue ID", vec4(0.0, 0.0, 65535.0, 65535.0)),
            # For Yellow (0xFF00FFFF): r=1, g=1, b=0, a=1 -> R=65535, G=65535, B=0, A=65535  
            (192, 64, "Yellow ID", vec4(65535.0, 65535.0, 0.0, 65535.0)),
            # For Red (0xFF0000FF): r=1, g=0, b=0, a=1 -> R=65535, G=0, B=0, A=65535
            (64, 192, "Red ID", vec4(65535.0, 0.0, 0.0, 65535.0)),
            # For Green (0xFF00FF00): r=0, g=1, b=0, a=1 -> R=0, G=65535, B=0, A=65535
            (192, 192, "Green ID", vec4(0.0, 65535.0, 0.0, 65535.0)),
            (127, 127, "Boundary", None),
        ]
        
        # Begin new frame for captures
        self.ctx.beginFrame()
        
        print("\nCapturing pixels at test points...")
        self.captures = []
        
        for x, y, expected_name, expected_value in test_points:
            # Create PixelFetchContext for RGBA16UI capture
            pfc = lev2.PixelFetchContext(self.rtg, 1)
            # Set usage to FVEC4 (converts uint16 values to float)
            pfc.setUsage(0, tokens.FVEC4)
            
            # Capture single pixel asynchronously
            capture_future = self.FBI.capturePixel(pfc, x, y)
            self.captures.append((x, y, expected_name, expected_value, capture_future))
            print(f"  Capturing pixel at ({x:3}, {y:3}) - expecting {expected_name}")
        
        # End frame (submits command buffer with capture commands)
        print("\nEnding frame...")
        self.ctx.endFrame()

def main():
    # RGBA16UI uses 16-bit unsigned integers per channel
    # When used for picking, these can encode up to 65535 unique IDs per channel
    # For this test, vertex colors are scaled to 16-bit unsigned int IDs
    
    # Create and run the RGBA16UI-specific test
    test = RGBA16UIPickTest(["RGBA16UI"])
    
    # The test expects these ID values based on the vertex colors:
    # Blue (r=0, g=0, b=1, a=1) -> R=0, G=0, B=65535, A=65535
    # Yellow (r=1, g=1, b=0, a=1) -> R=65535, G=65535, B=0, A=65535
    # Red (r=1, g=0, b=0, a=1) -> R=65535, G=0, B=0, A=65535
    # Green (r=0, g=1, b=0, a=1) -> R=0, G=65535, B=0, A=65535
    
    # Note: PixelFetchContext converts these back to floats for comparison
    
    return test.run()

if __name__ == "__main__":
    sys.exit(main())