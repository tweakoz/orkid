#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
# GPU-generated geometry via INDIRECT draws (the mesh-shader replacement path):
#   * Left  surface: DrawIndirectEML        — non-indexed, vertex count from a compute-written
#                    VkDrawIndirectCommand. Compute expands each triangle vertex.
#   * Right surface: DrawIndexedIndirectEML  — indexed, index count from a compute-written
#                    VkDrawIndexedIndirectCommand. Compute writes shared vertices + an index buffer.
# Both pull vertices from an SSBO in the VS (gl_VertexIndex); the CPU never sets the draw count.
# Clone of primitive_types.py; compute/SSBO/bindStorage pattern from datasources/computeshader.py.
################################################################################

import math, signal, sys
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import *

sys.path.append(lev2exdir().as_string + "/python")
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

GRID = 2048                 # cells per side
SCALE = 4.0               # world size of each surface
NVERTS = GRID * GRID * 6  # non-indexed: 6 verts per cell
NIVERTS = (GRID + 1) * (GRID + 1)   # indexed: shared vertex grid
NINDICES = GRID * GRID * 6          # indexed: 6 indices per cell

# SSBO layout: vec4 positions[N] | vec4 colors[N] | vec4 params(.x = time)
def geo_size(n):
    return n * 16 + n * 16 + 16
PARAMS_OFF = lambda n: n * 16 + n * 16

################################################################################
# Shared vertex-pull surface shader + the two compute generators. The VS is identical for both
# the non-indexed and indexed objects (pull positions[gl_VertexIndex]); only the draw + the
# generator differ.
################################################################################

SHADER = """
fxconfig fxcfg_default {}
////////////////////////////////////////
uniform_block ublock_vtx (descriptor_set 0) { mat4 mvp; }
////////////////////////////////////////
storage_interface sif_geo (descriptor_set 0) {
  buffer layout(std430) geo_data { vec4 positions[$$$N]; vec4 colors[$$$N]; vec4 params; };
}
storage_interface sif_args (descriptor_set 0) {
  buffer layout(std430) args_data { uint a_w; uint a_x; uint a_y; uint a_z; uint a_q; };
}
storage_interface sif_idx (descriptor_set 0) {
  buffer layout(std430) idx_data { uint indices[$$$NIDX]; };
}
////////////////////////////////////////
vertex_interface iface_vtx : ublock_vtx : sif_geo {
  outputs { vec4 frg_clr; }
}
fragment_interface iface_frg {
  inputs { vec4 frg_clr; }
  outputs { layout(location = 0) vec4 out_clr; }
}
vertex_shader vs_gen : iface_vtx {
  gl_Position = mvp * positions[gl_VertexID];
  frg_clr = colors[gl_VertexID];
}
fragment_shader ps_gen : iface_frg {
  out_clr = frg_clr;
}
technique tek_gen {
  fxconfig = fxcfg_default;
  pass p0 { vertex_shader = vs_gen; fragment_shader = ps_gen; state_block = default; }
}
////////////////////////////////////////
// helper: wavy height + color for a unit-square coord (fx,fz) in [-0.5,0.5]
libblock lib_gen {
  vec4 surf_pos(float fx, float fz, float t) {
    float h = sin(fx * 12.0 + t) * cos(fz * 12.0 + t * 1.3) * 0.06;
    return vec4(fx * $$$SCALE, h * $$$SCALE, fz * $$$SCALE, 1.0);
  }
  vec4 surf_col(float fx, float fz, float t) {
    float h = sin(fx * 12.0 + t) * cos(fz * 12.0 + t * 1.3) * 0.06;
    return vec4(0.5 + h * 6.0, 0.35, 0.7 - h * 6.0, 1.0);
  }
}
////////////////////////////////////////
// NON-INDEXED generator: one thread per triangle vertex; expands each of the 6 corners.
compute_interface iface_ni { storage { sif_geo sif_args } inputs { layout(local_size_x = 64); } }
compute_shader cs_gen_nonindexed : iface_ni : lib_gen {
  uint i = gl_GlobalInvocationID.x;
  uint total = uint($$$N);
  if (i >= total) { return; }
  float t = params.x;
  uint G = uint($$$GRID);
  uint quad = i / 6u;
  uint corner = i % 6u;
  uint cx = quad % G;
  uint cy = quad / G;
  float ox; float oz;
  if      (corner == 0u) { ox = 0.0; oz = 0.0; }
  else if (corner == 1u) { ox = 1.0; oz = 0.0; }
  else if (corner == 2u) { ox = 1.0; oz = 1.0; }
  else if (corner == 3u) { ox = 0.0; oz = 0.0; }
  else if (corner == 4u) { ox = 1.0; oz = 1.0; }
  else                   { ox = 0.0; oz = 1.0; }
  float fx = (float(cx) + ox) / float(G) - 0.5;
  float fz = (float(cy) + oz) / float(G) - 0.5;
  positions[i] = surf_pos(fx, fz, t);
  colors[i]    = surf_col(fx, fz, t);
  if (i == 0u) { a_w = total; a_x = 1u; a_y = 0u; a_z = 0u; a_q = 0u; }  // VkDrawIndirectCommand
}
////////////////////////////////////////
// INDEXED generator: ONE shader writes the shared grid vertices (tid < NV) into sif_geo AND a
// cell's 6 indices (tid < C) into sif_idx + the draw command. It touches sif_geo (the resource
// the graphics VS also uses), which anchors the descriptor layout — a compute shader whose
// storages are ALL compute-only fails to create a pipeline (no merged-resource base).
compute_interface iface_idx { storage { sif_geo sif_idx sif_args } inputs { layout(local_size_x = 64); } }
compute_shader cs_gen_indexed : iface_idx : lib_gen {
  uint tid = gl_GlobalInvocationID.x;
  float t = params.x;
  uint G  = uint($$$GRID);
  uint GP = G + 1u;
  uint NV = GP * GP;
  if (tid < NV) {
    uint vx = tid % GP;
    uint vy = tid / GP;
    float fx = float(vx) / float(G) - 0.5;
    float fz = float(vy) / float(G) - 0.5;
    positions[tid] = surf_pos(fx, fz, t);
    colors[tid]    = surf_col(fx, fz, t);
  }
  uint C = G * G;
  if (tid < C) {
    uint cx = tid % G;
    uint cy = tid / G;
    uint v00 = cy * GP + cx;
    uint v10 = v00 + 1u;
    uint v01 = v00 + GP;
    uint v11 = v01 + 1u;
    uint b = tid * 6u;
    indices[b + 0u] = v00; indices[b + 1u] = v10; indices[b + 2u] = v11;
    indices[b + 3u] = v00; indices[b + 4u] = v11; indices[b + 5u] = v01;
    if (tid == 0u) { a_w = C * 6u; a_x = 1u; a_y = 0u; a_z = 0u; a_q = 0u; } // VkDrawIndexedIndirectCommand
  }
}
"""

################################################################################

def make_material(ctx, n, nidx):
    mtl = FreestyleMaterial()
    # NB: substitute the LONGER tokens first — $$$N is a prefix of $$$NIDX/$$$NIV, so replacing
    # it first would mangle them (e.g. "$$$NIDX" -> "13824IDX").
    txt = (SHADER.replace("$$$NIDX", str(nidx)).replace("$$$NIV", str(NIVERTS))
                 .replace("$$$N", str(n)).replace("$$$GRID", str(GRID))
                 .replace("$$$SCALE", str(SCALE)))
    mtl.gpuInitFromShaderText(ctx, "indirect_gen", txt)
    mtl.rasterstate.culltest = tokens.OFF
    mtl.rasterstate.depthtest = tokens.LEQUALS
    permu = FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_gen")
    pipe = mtl.fxcache.findPipeline(permu)
    pipe.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipe.sharedMaterial = mtl
    return mtl, pipe

################################################################################

class IndirectApp:
    def __init__(self):
        self.ezapp = OrkEzApp.create(self, height=720, width=1280, use_subsystems=["opq", "core", "gpu", "lev2"])
        self.ezapp.setRefreshPolicy(RefreshFastest, 0)
        setupUiCamera(app=self, eye=vec3(0, 5, 9), tgt=vec3(0, 0, 0))
        signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
        self.time = 0.0

    def onGpuInit(self, ctx):
        self.context = ctx
        createSceneGraph(app=self)

        FXI = ctx.FXI

        # ---- NON-INDEXED object (DrawIndirectEML) ----
        self.geo_ni  = FXI.createShaderStorageBufferWithLength(geo_size(NVERTS))
        self.args_ni = FXI.createShaderStorageBufferWithLength(32)
        self.mtl_ni, self.pipe_ni = make_material(ctx, NVERTS, 1)
        self.pipe_ni.bindStorage(self.mtl_ni.storage("sif_geo"), self.geo_ni)
        self.cs_ni = self.mtl_ni.computeShader("cs_gen_nonindexed")
        prim_ni = primitives.PointsPrimitiveV12C4.createWithSSBO(0, self.geo_ni)
        prim_ni.setIndirect(args=self.args_ni, primtype=tokens.TRIANGLES)   # non-indexed
        self.node_ni = prim_ni.createNode("indirect_nonindexed", self.layer1, self.pipe_ni)
        self.node_ni.worldTransform.translation = vec3(-2.6, 0, 0)

        # ---- INDEXED object (DrawIndexedIndirectEML) ----
        self.geo_ix  = FXI.createShaderStorageBufferWithLength(geo_size(NIVERTS))
        self.idx_ix  = FXI.createShaderStorageBufferWithLength(NINDICES * 4)
        self.args_ix = FXI.createShaderStorageBufferWithLength(32)
        self.mtl_ix, self.pipe_ix = make_material(ctx, NIVERTS, NINDICES)
        self.pipe_ix.bindStorage(self.mtl_ix.storage("sif_geo"), self.geo_ix)
        self.cs_ix = self.mtl_ix.computeShader("cs_gen_indexed")
        prim_ix = primitives.PointsPrimitiveV12C4.createWithSSBO(0, self.geo_ix)
        prim_ix.setIndirect(args=self.args_ix, index=self.idx_ix, primtype=tokens.TRIANGLES)  # indexed
        self.node_ix = prim_ix.createNode("indirect_indexed", self.layer1, self.pipe_ix)
        self.node_ix.worldTransform.translation = vec3(2.6, 0, 0)

        print("Indirect-draw GPU geometry demo:")
        print("  Left:  DrawIndirectEML        (non-indexed, %d verts)" % NVERTS)
        print("  Right: DrawIndexedIndirectEML (indexed, %d verts / %d indices)" % (NIVERTS, NINDICES))
        self.scene.lightingmanager.gpuInit(ctx)

    def onGpuUpdate(self, ctx):
        CI, FXI = ctx.CI, ctx.FXI
        # write time into each geo SSBO's params, then dispatch the generators (outside a render pass).
        FXI.copyDataIntoShaderStorageBuffer([self.time, 0.0, 0.0, 0.0], self.geo_ni, PARAMS_OFF(NVERTS))
        FXI.copyDataIntoShaderStorageBuffer([self.time, 0.0, 0.0, 0.0], self.geo_ix, PARAMS_OFF(NIVERTS))
        CI.beginDispatchPhase()
        CI.bindStorageBuffer(self.cs_ni, 0, self.geo_ni)
        CI.bindStorageBuffer(self.cs_ni, 1, self.args_ni)
        CI.dispatch(self.cs_ni, (NVERTS + 63) // 64, 1, 1)
        nthreads = max(NIVERTS, GRID * GRID)   # writes verts (tid<NV) and indices (tid<cells)
        CI.bindStorageBuffer(self.cs_ix, 0, self.geo_ix)
        CI.bindStorageBuffer(self.cs_ix, 1, self.idx_ix)
        CI.bindStorageBuffer(self.cs_ix, 2, self.args_ix)
        CI.dispatch(self.cs_ix, (nthreads + 63) // 64, 1, 1)
        CI.endDispatchPhase()

    def onUiEvent(self, uievent):
        handled = self.uicam.uiEventHandler(uievent)
        if handled:
            self.camera.copyFrom(self.uicam.cameradata)
        return ui.HandlerResult()

    def onUpdate(self, updinfo):
        self.time = updinfo.absolutetime
        self.scene.updateScene(self.cameralut)

################################################################################

if __name__ == "__main__":
    app = IndirectApp()
    app.ezapp.mainThreadLoop()
