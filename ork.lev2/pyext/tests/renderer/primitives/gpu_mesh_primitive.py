#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# GPU-geometry slice 1 — the GpuMesh format keystone, rendered through the MATERIAL DSL.
#
# A compute shader GENERATES a procedural mesh into a standardized channel set (SoA: a separate array
# per attribute — P/N/B/uv/color) plus a small header holding the indirect-draw args (the vertex count
# the renderer reads straight off the GPU). A SoA "pull" vertex shader reads those channels by
# gl_VertexID and feeds them to a stock ptex3d PBR material (here: the white-matte `Solid`, so the
# generated SHAPE + normals read under lighting alone). This is the on-GPU mirror of the .ogeo
# attribute Geometry: nodes write channels, renderers read channels, everything count-sized.
#
# HOW IT RENDERS (matches the terrain viewer2 path, NOT the old forced-pipeline prototype):
#   * GpuMeshVertexSource delegates the SSBO-pull vertex side + the gen compute into a generated ptex3d
#     PBRMaterial via FWD_SSBO_CUSTOM (Ptex3d(dsl_class=Solid, vertex_source=...)).
#   * the ComputeDrawable is handed the MATERIAL (cdd.material = gmtl); the renderer auto-selects the
#     pipeline via findPipeline(RCID, _isSSBOSourced) -> FWD_SSBO_CUSTOM. We never build an
#     FxPipelinePermutation or force cdd.pipeline.
#
# TWO GROUND RULES THIS HONORS:
#   * No raw Vulkan. The "indirect-draw args" are 4 uints the ENGINE's DrawIndirect reads via the orkid
#     setIndirect(...) API. A mesh-graph author never sees them — the materializer emits the header.
#   * No C++ recompile for new mesh graphs. EVERYTHING here is Python + runtime-compiled shadlang
#     (JIT, same as ptex3d/terrain). New ops / new graphs = new Python + shadlang text. The eventual
#     C++ is ONE generic op-agnostic mesh-compute module + the GpuMesh register type, built once.
#
# Slice 1 scope: prove the format + the gen->material-render path. NON-INDEXED (no index buffer yet);
# the channels live in one shared sif_ptex_vtx block (gen compute + pull VS share it -> merged binding).
# NEXT slices: separate-SSBO-per-channel registers (true SoA dataflow register pool), indexing,
# directed-edge adjacency, displace+auto-finalize(recompute_tbn), sdf_to_mesh (nanovdb), lsystem, DSL.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from orkengine.lev2 import ComputeDrawableData
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
from ork.hypergraph.assets.materials.terrain.solid import Solid

tokens = CrcStringProxy()

################################################################################
# GpuMeshVertexSource — a FWD_SSBO_CUSTOM vertex source for a STORED-channel GpuMesh.
#
# Unlike the terrain source (where the VS computes positions on the fly from heights[]), here the gen
# compute WRITES the channels and the pull VS just READS them — that is the GpuMesh contract (a node
# materializes channels; the renderer reads them). Emits the 4 fragments _ssbo_block wants:
#   ssbo_layout  : the sif_ptex_vtx body (header + SoA channel arrays)
#   ssbo_lib     : (none)
#   ssbo_vs_body : pull -> position/normal/binormal/uv0/vtxcolor locals
#   ssbo_compute : the primitive generator (cs_mesh_setup writes the draw args; cs_mesh_gen fills channels)
################################################################################

class GpuMeshVertexSource:

  def __init__(self, grid=96, extent=8.0, amp=1.2, freq=2.2):
    self.grid   = int(grid)
    self.extent = float(extent)
    self.amp    = float(amp)
    self.freq   = float(freq)
    self.nv     = self.grid * self.grid * 6          # non-indexed verts (6 per quad)
    # std430 byte offsets within sif_ptex_vtx (args first -> args_offset = 0)
    self.ARGS_OFF = 0                                # uint a_vc,a_ic,a_fv,a_fi  (indirect draw args)
    self.HDR      = 32                               # + uint v_count,i_count,prim,flags
    self.P_OFF    = self.HDR
    self.N_OFF    = self.P_OFF  + self.nv * 16
    self.B_OFF    = self.N_OFF  + self.nv * 16
    self.UV_OFF   = self.B_OFF  + self.nv * 16
    self.CLR_OFF  = self.UV_OFF + self.nv * 16
    self.TOTAL    = self.CLR_OFF + self.nv * 16

  def _sub(self, t):
    for k, v in {"$NV$": self.nv, "$GRID$": self.grid, "$EXTENT$": self.extent,
                 "$AMP$": self.amp, "$FREQ$": self.freq}.items():
      t = t.replace(k, repr(v) if isinstance(v, float) else str(v))
    return t

  @property
  def layout(self):
    return self._sub(
      "uint a_vc; uint a_ic; uint a_fv; uint a_fi;          // indirect-draw args @0 (setIndirect reads a_vc)\n"
      "uint v_count; uint i_count; uint prim; uint flags;    // header @16\n"
      "vec4 ch_P[$NV$];                                      // position channel @32\n"
      "vec4 ch_N[$NV$];                                      // normal\n"
      "vec4 ch_B[$NV$];                                      // binormal (tangent = cross(N,B))\n"
      "vec4 ch_uv[$NV$];                                     // uv0 (xy)\n"
      "vec4 ch_clr[$NV$];                                    // vtxcolor")

  @property
  def lib(self):
    return ""    # gen is self-contained

  @property
  def vs_body(self):
    # produce the stock locals the FWD_SSBO_CUSTOM VS expects, straight from the stored channels.
    return (
      "uint i = uint(gl_VertexID);\n"
      "vec4 position = ch_P[i];\n"
      "vec3 normal   = ch_N[i].xyz;\n"
      "vec3 binormal = ch_B[i].xyz;\n"
      "vec2 uv0      = ch_uv[i].xy;\n"
      "vec4 vtxcolor = ch_clr[i];")

  @property
  def compute(self):
    return self._sub(
      "compute_interface cif_mesh : sif_ptex_vtx { inputs { layout(local_size_x = 64); } }\n"
      "////////////////////////////////////////\n"
      "// pass 0: header / draw args (count is fixed for a grid primitive; dynamic gen uses atomicAdd).\n"
      "compute_shader cs_mesh_setup : cif_mesh {\n"
      "  if (gl_GlobalInvocationID.x != 0u) { return; }\n"
      "  a_vc = uint($NV$); a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "  v_count = uint($NV$); i_count = 0u; prim = 0u; flags = 0u;\n"
      "}\n"
      "////////////////////////////////////////\n"
      "// pass 1: generate the grid (one thread / non-indexed vertex) — writes ALL channels (good state).\n"
      "compute_shader cs_mesh_gen : cif_mesh {\n"
      "  uint i = gl_GlobalInvocationID.x;\n"
      "  if (i >= uint($NV$)) { return; }\n"
      "  uint cell = i / 6u; uint corner = i % 6u;\n"
      "  uint cx = cell % uint($GRID$); uint cz = cell / uint($GRID$);\n"
      "  uint dx; uint dz;\n"
      "  // CCW-from-above winding so the geometric normal points +y (matches the analytic N below);\n"
      "  // consistent front-facing winding is part of a GpuMesh's 'good state'.\n"
      "  if      (corner == 0u) { dx = 0u; dz = 0u; }\n"
      "  else if (corner == 1u) { dx = 1u; dz = 1u; }\n"
      "  else if (corner == 2u) { dx = 1u; dz = 0u; }\n"
      "  else if (corner == 3u) { dx = 0u; dz = 0u; }\n"
      "  else if (corner == 4u) { dx = 0u; dz = 1u; }\n"
      "  else                   { dx = 1u; dz = 1u; }\n"
      "  float tx = float(cx + dx) / float($GRID$);\n"
      "  float tz = float(cz + dz) / float($GRID$);\n"
      "  float wx = (tx - 0.5) * float($EXTENT$);\n"
      "  float wz = (tz - 0.5) * float($EXTENT$);\n"
      "  float A = float($AMP$); float F = float($FREQ$);\n"
      "  float h   = A * sin(wx * F) * cos(wz * F);\n"
      "  float dhx = A * F * cos(wx * F) * cos(wz * F);   // analytic gradient -> exact normal + tangent\n"
      "  float dhz = -A * F * sin(wx * F) * sin(wz * F);\n"
      "  ch_P[i]   = vec4(wx, h, wz, 1.0);\n"
      "  ch_N[i]   = vec4(normalize(vec3(-dhx, 1.0, -dhz)), 0.0);\n"
      "  ch_B[i]   = vec4(normalize(vec3(1.0, dhx, 0.0)), 0.0);\n"
      "  ch_uv[i]  = vec4(tx, tz, 0.0, 0.0);\n"
      "  ch_clr[i] = vec4(0.55 + 0.35 * sin(wx), 0.6, 0.7 - 0.3 * cos(wz), 1.0);\n"
      "}")

  # ---- contracts ----
  def as_material_kwargs(self):
    return dict(ssbo_layout=self.layout, ssbo_lib=self.lib,
                ssbo_vs_body=self.vs_body, ssbo_compute=self.compute)

  def compute_passes(self):
    return [("cs_mesh_setup", 1, 1, 1),
            ("cs_mesh_gen",  (self.nv + 63) // 64, 1, 1)]

################################################################################

class GpuMeshApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
      "std_scenegraph", StandardSceneGraphComponent,
      eye=vec3(6, 5, 9), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
      grid_variant=None)
    self.createEzApp()

  def _onGpuInit(self, ctx):
    self.layer_fwd = self.SGC.layer_fwd
    FXI = ctx.FXI

    self._vsrc = GpuMeshVertexSource(grid=96, extent=8.0, amp=1.2, freq=2.2)
    vs = self._vsrc
    print(f"gpu_mesh slice1: grid={vs.grid} verts={vs.nv} ssbo={vs.TOTAL/1e6:.1f}MB "
          f"-> material-DSL Solid via FWD_SSBO_CUSTOM (auto-selected)", flush=True)

    # MATERIAL DSL: a stock white-matte ptex3d material, with our GpuMesh as its FWD_SSBO_CUSTOM
    # vertex source. as_gfx_material builds the generated PBRMaterial (SSBO-pull VS + gen compute baked in).
    self._mtl_wrap = Ptex3dAsset(dsl_class=Solid, vertex_source=vs,
                                 albedo=vec3(0.72, 0.74, 0.78), roughness=0.55)
    self._mtl_wrap._ctx = ctx
    gmtl = self._mtl_wrap.as_gfx_material
    self._gmtl = gmtl
    fs  = gmtl.freestyle                    # internal FreestyleMaterial (storage/compute access)
    sif = fs.storage("sif_ptex_vtx")

    self.mesh = FXI.createShaderStorageBufferWithLength(vs.TOTAL)   # the GpuMesh buffer (one block, slice 1)

    cdd = ComputeDrawableData()
    cdd.material = gmtl                      # AUTO-SELECT findPipeline(RCID,_isSSBOSourced) -> FWD_SSBO_CUSTOM
    cdd.addGraphicsStorage(sif, self.mesh)   # the vertex-source SSBO for the pull VS
    for (name, gx, gy, gz) in vs.compute_passes():
      cdd.addComputePass(fs.computeShader(name), [(sif, self.mesh)], gx, gy, gz)
    cdd.setIndirect(args=self.mesh, args_offset=vs.ARGS_OFF, primtype=tokens.TRIANGLES)
    self.node = self.layer_fwd.createDrawableNodeFromData("gpumesh", cdd)
    print("gpu_mesh slice1: compute-generated mesh -> SoA pull -> PBR material. orbit to inspect.", flush=True)

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _onUiEvent(self, uievent):
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()

################################################################################

if __name__ == "__main__":
  app = GpuMeshApp()
  app.ezapp.mainThreadLoop()
