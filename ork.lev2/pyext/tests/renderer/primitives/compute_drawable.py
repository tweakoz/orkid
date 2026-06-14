#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# Minimal ComputeDrawableData sanity test: a compute shader generates a cube's 36 triangle
# vertices (+ the VkDrawIndirectCommand) into one SSBO; the drawable then DrawIndirectEML's it.
# No camera/cull — the simplest possible exercise of compute-generated geometry + indirect draw.
#
# Uses ComponentizedApplication + StandardSceneGraphComponent (the SGVP path) so the drawable's
# onPreRender hook actually fires — OrkEzApp.create's direct paint callback never runs preRender.
################################################################################

import sys
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

NVERTS  = 36                       # 6 faces * 2 tris * 3 verts
ARGS_OFF = 0                       # uint vc, ic, fv, fi
POS_OFF  = 16                      # vec4 positions[36]
COL_OFF  = POS_OFF + NVERTS * 16   # vec4 colors[36]
TOTAL    = COL_OFF + NVERTS * 16

SHADER = """
fxconfig fxcfg_default {}
uniform_block ublock_vtx (descriptor_set 0) { mat4 mvp; }
////////////////////////////////////////
// one SSBO shared by the VS + the compute pass -> single merged resource @ binding 0.
storage_interface sif_box (descriptor_set 0) {
  buffer layout(std430) box_data {
    uint a_vc; uint a_ic; uint a_fv; uint a_fi;   // VkDrawIndirectCommand @0
    vec4 positions[36];
    vec4 colors[36];
  };
}
////////////////////////////////////////
vertex_interface iface_vtx : ublock_vtx : sif_box { outputs { vec4 frg_clr; } }
fragment_interface iface_frg { inputs { vec4 frg_clr; } outputs { layout(location = 0) vec4 out_clr; } }
vertex_shader vs_box : iface_vtx {
  gl_Position = mvp * positions[gl_VertexID];
  frg_clr = colors[gl_VertexID];
}
fragment_shader ps_box : iface_frg { out_clr = frg_clr; }
technique tek_box {
  fxconfig = fxcfg_default;
  pass p0 { vertex_shader = vs_box; fragment_shader = ps_box; state_block = default; }
}
////////////////////////////////////////
libblock lib_box {
  // write one quad (a,b,c,d -> tris a,b,c & a,c,d) of constant color into positions/colors @base.
  void writeQuad(uint base, vec3 a, vec3 b, vec3 c, vec3 d, vec4 col) {
    positions[base + 0u] = vec4(a, 1.0); positions[base + 1u] = vec4(b, 1.0); positions[base + 2u] = vec4(c, 1.0);
    positions[base + 3u] = vec4(a, 1.0); positions[base + 4u] = vec4(c, 1.0); positions[base + 5u] = vec4(d, 1.0);
    colors[base + 0u] = col; colors[base + 1u] = col; colors[base + 2u] = col;
    colors[base + 3u] = col; colors[base + 4u] = col; colors[base + 5u] = col;
  }
}
////////////////////////////////////////
compute_interface iface_box { storage { sif_box } inputs { layout(local_size_x = 1); } }
compute_shader cs_box : iface_box : lib_box {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  float s = 1.0;
  vec3 c0 = vec3(-s,-s,-s); vec3 c1 = vec3(s,-s,-s); vec3 c2 = vec3(s,s,-s); vec3 c3 = vec3(-s,s,-s);
  vec3 c4 = vec3(-s,-s,s);  vec3 c5 = vec3(s,-s,s);  vec3 c6 = vec3(s,s,s);  vec3 c7 = vec3(-s,s,s);
  writeQuad(0u,  c0, c1, c2, c3, vec4(1.0, 0.2, 0.2, 1.0));  // -Z
  writeQuad(6u,  c4, c5, c6, c7, vec4(0.2, 1.0, 0.2, 1.0));  // +Z
  writeQuad(12u, c0, c4, c7, c3, vec4(0.2, 0.2, 1.0, 1.0));  // -X
  writeQuad(18u, c1, c5, c6, c2, vec4(1.0, 1.0, 0.2, 1.0));  // +X
  writeQuad(24u, c0, c1, c5, c4, vec4(1.0, 0.2, 1.0, 1.0));  // -Y
  writeQuad(30u, c3, c2, c6, c7, vec4(0.2, 1.0, 1.0, 1.0));  // +Y
  a_vc = 36u; a_ic = 1u; a_fv = 0u; a_fi = 0u;               // VkDrawIndirectCommand
}
"""

################################################################################

class ComputeBoxApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
        "std_scenegraph",
        StandardSceneGraphComponent,
        eye=vec3(4, 3, 5),
        tgt=vec3(0, 0, 0),
        up=vec3(0, 1, 0),
        grid_variant=None)
    self.createEzApp()

  def _onGpuInit(self, ctx):
    self.scene     = self.SGC.scenegraph
    self.layer_fwd = self.SGC.layer_fwd
    FXI = ctx.FXI
    self.ssbo = FXI.createShaderStorageBufferWithLength(TOTAL)

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "compute_box", SHADER)
    mtl.rasterstate.culltest  = tokens.OFF
    mtl.rasterstate.depthtest = tokens.LEQUALS
    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_box")
    pipe = mtl.fxcache.findPipeline(permu)
    pipe.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipe.bindStorage(mtl.storage("sif_box"), self.ssbo)
    pipe.sharedMaterial = mtl
    cs_box = mtl.computeShader("cs_box")

    cdd = lev2.ComputeDrawableData()
    cdd.pipeline = pipe
    cdd.addComputePass(cs_box, [(mtl.storage("sif_box"), self.ssbo)], 1, 1, 1)
    cdd.setIndirect(args=self.ssbo, args_offset=ARGS_OFF, primtype=tokens.TRIANGLES)
    self.node = self.layer_fwd.createDrawableNodeFromData("box", cdd)

    self.scene.lightingmanager.gpuInit(ctx)
    print("compute_drawable: cube generated by compute -> DrawIndirectEML", flush=True)

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _onUiEvent(self, uievent):
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()

################################################################################

if __name__ == "__main__":
  app = ComputeBoxApp()
  app.ezapp.mainThreadLoop()
