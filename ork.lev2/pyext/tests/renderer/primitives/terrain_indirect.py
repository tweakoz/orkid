#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# Chunked, GPU-culled terrain via ComputeDrawableData (the mesh-shader replacement path).
#
# Per frame, in the drawable's onPreRender hook (driven by the scenegraph, with the camera):
#   pass 0 (reset)  : zero the visible counter + draw command
#   pass 1 (cull)   : 1 thread/chunk, 2D-frustum-test the chunk's XZ square (height = full extent)
#                     vs the camera VP -> atomic-append visible chunk indices
#   pass 2 (gen)    : emit triangles for visible chunks from the height field (matching
#                     _build_terrain_mesh_arrays) -> vertices + the VkDrawIndirectCommand
# then DrawIndirectEML pulls the generated verts.
#
# ALL data lives in ONE storage interface (sif_terr) used by the VS + every compute pass. That keeps
# it a single MERGED resource at binding 0 — sidestepping shadlang's compute-only fallback-binding
# bug (which gives sparse/mismatched bindings to interfaces only seen by compute). Same single-SSBO
# shape proven by datasources/computeshader.py.
################################################################################

import math, sys
import numpy as np
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

DIM    = 512
EXTENT = 1000.0
HSCALE = 120.0
CHUNK  = 128
CPS    = (DIM + CHUNK - 1) // CHUNK
NCHUNK = CPS * CPS
VPC    = CHUNK * CHUNK * 6
MAXV   = NCHUNK * VPC
DIMSQ  = DIM * DIM

# ---- single-SSBO std430 layout (must match the sif_terr block below) ----
CAM_OFF     = 0                              # mat4 vp + mat4 ivp + vec4 eye + vec4 misc
ARGS_OFF    = 160                            # uint vc, ic, fv, fi  (VkDrawIndirectCommand)
VIS_OFF     = 176                            # uint count + pad[3]
VLIST_OFF   = 192                            # uint v_list[NCHUNK]
HEIGHTS_OFF = VLIST_OFF + NCHUNK * 4         # float heights[DIMSQ]
def _align16(x): return (x + 15) & ~15
POS_OFF     = _align16(HEIGHTS_OFF + DIMSQ * 4)   # vec4 positions[MAXV]
COL_OFF     = POS_OFF + MAXV * 16                 # vec4 colors[MAXV]
TOTAL       = COL_OFF + MAXV * 16

################################################################################

SHADER = """
fxconfig fxcfg_default {}
uniform_block ublock_vtx (descriptor_set 0) { mat4 mvp; }
////////////////////////////////////////
// ONE storage interface for everything (cam/args/vis/heights/verts), shared by the VS + all
// compute passes -> single merged resource @ binding 0 (no compute-only fallback bindings).
storage_interface sif_terr (descriptor_set 0) {
  buffer layout(std430) terr_data {
    mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;     // CamBlk @0 (written by C++ setCameraParams)
    uint a_vc; uint a_ic; uint a_fv; uint a_fi;          // VkDrawIndirectCommand @160
    uint v_count; uint v_p0; uint v_p1; uint v_p2;       // visible-chunk header
    uint v_list[$$$NCHUNK$$$];                           // visible chunk indices
    float heights[$$$DIMSQ$$$];                          // heightfield
    vec4 positions[$$$MAXV$$$];                          // generated verts (read by VS)
    vec4 colors[$$$MAXV$$$];
  };
}
////////////////////////////////////////
vertex_interface iface_vtx : ublock_vtx : sif_terr { outputs { vec4 frg_clr; } }
fragment_interface iface_frg { inputs { vec4 frg_clr; } outputs { layout(location = 0) vec4 out_clr; } }
vertex_shader vs_terrain : iface_vtx {
  gl_Position = mvp * positions[gl_VertexID];
  frg_clr = colors[gl_VertexID];
}
fragment_shader ps_terrain : iface_frg { out_clr = frg_clr; }
technique tek_terrain {
  fxconfig = fxcfg_default;
  pass p0 { vertex_shader = vs_terrain; fragment_shader = ps_terrain; state_block = default; }
}
////////////////////////////////////////
libblock lib_terr {
  vec3 terr_pos(uint tx, uint tz) {   // matches _build_terrain_mesh_arrays
    uint cx = min(tx, uint($$$DIM$$$) - 1u);
    uint cz = min(tz, uint($$$DIM$$$) - 1u);
    float x = ((float(tx) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
    float z = ((float(tz) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
    float y = heights[cz * uint($$$DIM$$$) + cx] * float($$$HSCALE$$$);
    return vec3(x, y, z);
  }
}
////////////////////////////////////////
compute_interface iface_terr { storage { sif_terr } inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_reset : iface_terr {
  v_count = 0u; a_vc = 0u; a_ic = 1u; a_fv = 0u; a_fi = 0u;
}
////////////////////////////////////////
compute_shader cs_cull : iface_terr {
  uint ci = gl_GlobalInvocationID.x;
  if (ci >= uint($$$NCHUNK$$$)) { return; }
  uint cx = ci % uint($$$CPS$$$);
  uint cz = ci / uint($$$CPS$$$);
  float x0 = ((float(cx * uint($$$CHUNK$$$)) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
  float x1 = ((float((cx + 1u) * uint($$$CHUNK$$$)) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
  float z0 = ((float(cz * uint($$$CHUNK$$$)) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
  float z1 = ((float((cz + 1u) * uint($$$CHUNK$$$)) + 0.5) / float($$$DIM$$$) - 0.5) * float($$$EXTENT$$$);
  vec3 mn = vec3(x0, 0.0, z0);
  vec3 mx = vec3(x1, float($$$HSCALE$$$), z1);
  vec4 rx = vec4(c_vp[0].x, c_vp[1].x, c_vp[2].x, c_vp[3].x);
  vec4 ry = vec4(c_vp[0].y, c_vp[1].y, c_vp[2].y, c_vp[3].y);
  vec4 rz = vec4(c_vp[0].z, c_vp[1].z, c_vp[2].z, c_vp[3].z);
  vec4 rw = vec4(c_vp[0].w, c_vp[1].w, c_vp[2].w, c_vp[3].w);
  vec4 pl[6];
  pl[0] = rw + rx; pl[1] = rw - rx; pl[2] = rw + ry; pl[3] = rw - ry; pl[4] = rz; pl[5] = rw - rz;
  bool inside = true;
  for (int p = 0; p < 6; p++) {
    vec3 pv = vec3(pl[p].x >= 0.0 ? mx.x : mn.x, pl[p].y >= 0.0 ? mx.y : mn.y, pl[p].z >= 0.0 ? mx.z : mn.z);
    if ((dot(pl[p].xyz, pv) + pl[p].w) < 0.0) { inside = false; }
  }
  if (inside) { uint slot = atomicAdd(v_count, 1u); v_list[slot] = ci; }
}
////////////////////////////////////////
compute_shader cs_gen : iface_terr : lib_terr {
  uint i = gl_GlobalInvocationID.x;
  uint slot = i / uint($$$VPC$$$);
  if (slot >= v_count) { return; }
  uint chunk = v_list[slot];
  uint ccx = chunk % uint($$$CPS$$$);
  uint ccz = chunk / uint($$$CPS$$$);
  uint local = i % uint($$$VPC$$$);
  uint cell = local / 6u;
  uint corner = local % 6u;
  uint lx = cell % uint($$$CHUNK$$$);
  uint lz = cell / uint($$$CHUNK$$$);
  uint baseC = ccx * uint($$$CHUNK$$$) + lx;
  uint baseR = ccz * uint($$$CHUNK$$$) + lz;
  uint dR; uint dC;
  if      (corner == 0u) { dR = 0u; dC = 0u; }
  else if (corner == 1u) { dR = 1u; dC = 0u; }
  else if (corner == 2u) { dR = 0u; dC = 1u; }
  else if (corner == 3u) { dR = 0u; dC = 1u; }
  else if (corner == 4u) { dR = 1u; dC = 0u; }
  else                   { dR = 1u; dC = 1u; }
  vec3 p = terr_pos(baseC + dC, baseR + dR);
  positions[i] = vec4(p, 1.0);
  float h = clamp(p.y / float($$$HSCALE$$$), 0.0, 1.0);
  colors[i] = vec4(0.25 + 0.6 * h, 0.45, 0.7 - 0.4 * h, 1.0);
  if (i == 0u) { a_vc = v_count * uint($$$VPC$$$); a_ic = 1u; a_fv = 0u; a_fi = 0u; }
}
"""

def _subst(txt):
    for k, v in {"$$$DIMSQ$$$": DIMSQ, "$$$DIM$$$": DIM, "$$$EXTENT$$$": EXTENT, "$$$HSCALE$$$": HSCALE,
                 "$$$CHUNK$$$": CHUNK, "$$$CPS$$$": CPS, "$$$NCHUNK$$$": NCHUNK, "$$$VPC$$$": VPC,
                 "$$$MAXV$$$": MAXV}.items():
        txt = txt.replace(k, str(v))
    return txt

def synth_height():
    xs = (np.arange(DIM, dtype=np.float32) / DIM - 0.5)
    X, Z = np.meshgrid(xs, xs)
    h = (0.5 + 0.25 * np.sin(X * 18.0) * np.cos(Z * 15.0)
             + 0.20 * np.exp(-((X * 2.2) ** 2 + (Z * 2.2) ** 2)))
    return np.clip(h, 0.0, 1.0).astype(np.float32).reshape(-1)

################################################################################

class TerrainIndirectApp(ComponentizedApplication):
    def __init__(self):
        super().__init__()
        self.SGC = self.addComponent(
            "std_scenegraph",
            StandardSceneGraphComponent,
            eye=vec3(0, 250, 600),
            tgt=vec3(0, 0, 0),
            up=vec3(0, 1, 0),
            near=2.0,
            far=4000.0,
            explicit_near_far=True,
            grid_variant=None)
        self.createEzApp()

    def _onGpuInit(self, ctx):
        self.scene  = self.SGC.scenegraph
        self.layer1 = self.SGC.layer_fwd
        FXI = ctx.FXI
        print(f"terrain_indirect: DIM={DIM} CHUNK={CHUNK} chunks={NCHUNK} maxverts={MAXV} ssbo={TOTAL/1e6:.0f}MB", flush=True)

        self.terr = FXI.createShaderStorageBufferWithLength(TOTAL)   # one SSBO for everything
        FXI.copyDataIntoShaderStorageBuffer(synth_height(), self.terr, HEIGHTS_OFF)

        mtl = lev2.FreestyleMaterial()
        mtl.gpuInitFromShaderText(ctx, "terrain_indirect", _subst(SHADER))
        mtl.rasterstate.culltest  = tokens.OFF
        mtl.rasterstate.depthtest = tokens.LEQUALS
        permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
        permu.technique = mtl.shader.technique("tek_terrain")
        pipe = mtl.fxcache.findPipeline(permu)
        pipe.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        pipe.bindStorage(mtl.storage("sif_terr"), self.terr)
        pipe.sharedMaterial = mtl
        sif      = mtl.storage("sif_terr")
        cs_reset = mtl.computeShader("cs_reset")
        cs_cull  = mtl.computeShader("cs_cull")
        cs_gen   = mtl.computeShader("cs_gen")

        cdd = lev2.ComputeDrawableData()
        cdd.pipeline = pipe
        cdd.setCameraParams(self.terr, CAM_OFF)
        cdd.addComputePass(cs_reset, [(sif, self.terr)], 1, 1, 1)
        cdd.addComputePass(cs_cull,  [(sif, self.terr)], (NCHUNK + 63) // 64, 1, 1)
        cdd.addComputePass(cs_gen,   [(sif, self.terr)], (MAXV + 63) // 64, 1, 1)
        cdd.setIndirect(args=self.terr, args_offset=ARGS_OFF, primtype=tokens.TRIANGLES)
        self.node = self.layer1.createDrawableNodeFromData("terrain", cdd)

        self.scene.lightingmanager.gpuInit(ctx)
        print("terrain_indirect: ready — fly around; off-frustum chunks are never generated.", flush=True)

    def _onUiEvent(self, uievent):
        self.SGC._onCameraUiEvent(uievent)
        return lev2.ui.HandlerResult()

    def _onUpdate(self, updinfo):
        self.SGC.scenegraph.updateScene(self.SGC.cameralut)

################################################################################

if __name__ == "__main__":
    app = TerrainIndirectApp()
    app.ezapp.mainThreadLoop()
