////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hmdflow_render.cpp — render-time, on-GPU triangulation of the indexed attribute mesh.
//
// The v2 GpuMesh is an INDEXED polygon mesh with MIXED faces (tri/quad/ngon via face_offsets). The
// graphics pipeline wants triangles, so each frame a small raw-FXI compute (the SAME compute API the
// generator modules use — pure on-GPU, no readback) fan-triangulates the faces into a uint32 tri
// vertex-index buffer + writes a VkDrawIndexedIndirectCommand. The drawable then DrawIndexedIndirect's
// off those GPU buffers; the pull VS reads vert attrs by gl_VertexIndex (the index buffer maps each
// drawn index -> a mesh vertex). Shaders compile ONCE; per frame is just bind+dispatch (+ a host write
// of num_faces, and an index-buffer re-pool only when the topology grows past a pow2 boundary).
//
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <chrono>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // terrain::BakeEnv — the clock mirror for field subgraphs (E.1b)

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////

// E.3 — gid render bucketing. The index buffer is built in CONTIGUOUS PER-GID RANGES (A1 tag
// contract: gid = __tags[20:32), the per-face material-class key) and the args buffer is an ARRAY
// of 4096 VkDrawIndexedIndirectCommand slots — one per gid; the drawable issues one indexed-indirect
// draw per BOUND gid (its own material) at argsOffset = gid*20. Gids with no bound material FOLD TO
// SLOT 0 at count/scatter time (the host-written BND bitmask), so an assign_gid without a material
// binding renders with the default material instead of vanishing (ops self-defend). No __tags
// channel -> everything lands in slot 0 = the classic single-draw shape.
static constexpr int kGidSlots = 4096;

static std::string _triangulate_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) parb {
  uint num_faces; uint inst_count; uint has_tags; uint pad2; }; } // inst_count -> indirect instanceCount (1 = non-instanced)
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[]; }; }  // CSR offsets
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[]; }; }  // corner->vert
storage_interface sif_idx (descriptor_set 0) { buffer layout(std430) idb { uint IDX[]; }; }  // out tri vert-indices
storage_interface sif_arg (descriptor_set 0) { buffer layout(std430) arb { uint ARG[]; }; }  // 4096 x VkDrawIndexedIndirectCommand (5 uints each)
storage_interface sif_tf  (descriptor_set 0) { buffer layout(std430) tfb { uint TFd[]; }; }   // out-tri -> source FACE id
storage_interface sif_lidx (descriptor_set 0) { buffer layout(std430) lib { uint LIDX[]; }; } // out LINE vert-indices (edges)
storage_interface sif_larg (descriptor_set 0) { buffer layout(std430) lab {                   // VkDrawIndexedIndirectCommand (lines)
  uint l_indexCount; uint l_instanceCount; uint l_firstIndex; uint l_vertexOffset; uint l_firstInstance; }; }
storage_interface sif_hist (descriptor_set 0) { buffer layout(std430) hsb { uint HIST[]; }; } // per-gid tri-index counts -> write cursors
storage_interface sif_bnd  (descriptor_set 0) { buffer layout(std430) bnb { uint BND[]; }; }  // 128 uints: bound-gid bitmask (bit 0 always)
storage_interface sif_tags (descriptor_set 0) { buffer layout(std430) tgb { uint TAGS[]; }; } // face __tags (dummy when has_tags=0)
storage_interface sif_P    (descriptor_set 0) { buffer layout(std430) ppb { vec4 Pd[]; }; }   // positions (E.5 ear clip)
// ONE shared interface for all shaders so each storage resolves to a SINGLE global binding id
// (sif_par=0 sif_fo=1 sif_vi=2 sif_idx=3 sif_arg=4 sif_tf=5 sif_lidx=6 sif_larg=7 sif_hist=8
//  sif_bnd=9 sif_tags=10 sif_P=11); every shader binds all slots consistently.
compute_interface iface { storage { sif_par sif_fo sif_vi sif_idx sif_arg sif_tf sif_lidx sif_larg sif_hist sif_bnd sif_tags sif_P }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
// reset: one thread per gid slot zeroes its histogram bin + draw command; thread 0 also resets lines.
compute_shader cs_reset : iface {
  uint s = gl_GlobalInvocationID.x;
  if (s == 0u) {
    l_indexCount = 0u; l_instanceCount = inst_count; l_firstIndex = 0u; l_vertexOffset = 0u; l_firstInstance = 0u;
  }
  if (s >= 4096u) { return; }
  HIST[s] = 0u;
  ARG[s * 5u + 0u] = 0u;          // indexCount
  ARG[s * 5u + 1u] = inst_count;  // instanceCount
  ARG[s * 5u + 2u] = 0u;          // firstIndex
  ARG[s * 5u + 3u] = 0u;          // vertexOffset
  ARG[s * 5u + 4u] = 0u;          // firstInstance
}
////////////////////////////////////////
// count: per-face triangle-index counts into the face's gid slot (unbound gids fold to slot 0).
compute_shader cs_count : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 3u) { return; }
  uint g = 0u;
  if (has_tags != 0u) {
    g = (TAGS[f] >> 20u) & 0xFFFu;
    if (((BND[g >> 5u] >> (g & 31u)) & 1u) == 0u) { g = 0u; }
  }
  atomicAdd(HIST[g], (n - 2u) * 3u);
}
////////////////////////////////////////
// scan: serial exclusive prefix over the 4096 bins (1 thread; runs only on topo-dirty frames) ->
// each slot's draw command gets {indexCount, firstIndex}; HIST becomes the scatter write-cursor.
compute_shader cs_scan : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint running = 0u;
  for (uint s = 0u; s < 4096u; s++) {
    uint cnt = HIST[s];
    ARG[s * 5u + 0u] = cnt;       // indexCount
    ARG[s * 5u + 2u] = running;   // firstIndex
    HIST[s] = running;            // becomes the bucket write cursor for cs_tri
    running += cnt;
  }
}
////////////////////////////////////////
// emit one LINE segment per POLYGON edge (1 thread / face): n corners -> n edges -> 2n line indices.
// uses the actual polygon edges (CSR), NOT the triangulation -> clean quad/ngon outlines (no diagonals).
compute_shader cs_lines : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 2u) { return; }
  uint base = atomicAdd(l_indexCount, n * 2u);
  for (uint k = 0u; k < n; k++) {
    LIDX[base + k * 2u + 0u] = VId[a + k];
    LIDX[base + k * 2u + 1u] = VId[a + ((k + 1u) % n)];
  }
}
////////////////////////////////////////
// E.5 — triangulate one face (1 thread / face) into its gid bucket's contiguous index range.
// tris (exact), quads and small ngons EAR-CLIP on the Newell best-fit plane (CONCAVE-safe;
// ring-order emission preserves the polygon's winding); n > 60 or degenerate falls back to
// the classic corner-0 fan. Output is ALWAYS exactly (n-2) triangles, so bucket histograms
// and index capacity are method-agnostic.
compute_shader cs_tri : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 3u) { return; }
  uint g = 0u;
  if (has_tags != 0u) {
    g = (TAGS[f] >> 20u) & 0xFFFu;
    if (((BND[g >> 5u] >> (g & 31u)) & 1u) == 0u) { g = 0u; }
  }
  uint ntris = n - 2u;
  uint base  = atomicAdd(HIST[g], ntris * 3u);      // reserve this face's slice of its bucket
  uint tbase = base / 3u;
  uint emit  = 0u;                                   // triangles emitted so far
  if (n == 3u) {
    IDX[base + 0u] = VId[a]; IDX[base + 1u] = VId[a + 1u]; IDX[base + 2u] = VId[a + 2u];
    TFd[tbase] = f;
    return;
  }
  bool earclip = (n <= 60u);
  if (earclip) {
    // Newell normal -> best-fit plane basis (U, V)
    vec3 nrm = vec3(0.0, 0.0, 0.0);
    for (uint k = 0u; k < n; k++) {
      vec3 p0 = Pd[VId[a + k]].xyz;
      vec3 p1 = Pd[VId[a + ((k + 1u) % n)]].xyz;
      nrm += vec3((p0.y - p1.y) * (p0.z + p1.z),
                  (p0.z - p1.z) * (p0.x + p1.x),
                  (p0.x - p1.x) * (p0.y + p1.y));
    }
    float nl = length(nrm);
    if (nl < 1.0e-12) {
      earclip = false;                               // degenerate polygon -> fan
    } else {
      vec3 NN  = nrm / nl;
      vec3 ax  = (abs(NN.x) < 0.9) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
      vec3 U   = normalize(cross(NN, ax));
      vec3 V   = cross(NN, U);
      vec2 pp[60];
      uint ring[60];
      vec3 c0 = Pd[VId[a]].xyz;
      for (uint k = 0u; k < n; k++) {
        vec3 p  = Pd[VId[a + k]].xyz - c0;
        pp[k]   = vec2(dot(p, U), dot(p, V));
        ring[k] = k;
      }
      // orientation of the projected ring (signed area x2)
      float area2 = 0.0;
      for (uint k = 0u; k < n; k++) {
        vec2 q0 = pp[k]; vec2 q1 = pp[(k + 1u) % n];
        area2 += q0.x * q1.y - q1.x * q0.y;
      }
      float orient = (area2 >= 0.0) ? 1.0 : -1.0;
      uint remaining = n;
      // clip ears until a triangle remains (bounded; each iteration removes one
      // vertex). NOTE: `pass` is a shadlang KEYWORD (technique pass blocks) —
      // never use it as an identifier in shader text.
      for (uint clipiter = 0u; clipiter < 60u; clipiter++) {
        if (remaining <= 3u) { break; }
        uint ear = 0xFFFFFFFFu;
        for (uint i = 0u; i < remaining; i++) {
          uint ip = (i + remaining - 1u) % remaining;
          uint in_ = (i + 1u) % remaining;
          vec2 A2 = pp[ring[ip]]; vec2 B2 = pp[ring[i]]; vec2 C2 = pp[ring[in_]];
          float cr = (B2.x - A2.x) * (C2.y - B2.y) - (B2.y - A2.y) * (C2.x - B2.x);
          if (cr * orient <= 1.0e-12) { continue; }  // reflex or degenerate corner
          // no other remaining vertex inside the candidate ear
          bool clear = true;
          for (uint j = 0u; j < remaining; j++) {
            if (j == ip || j == i || j == in_) { continue; }
            vec2 Q = pp[ring[j]];
            float s0 = ((B2.x - A2.x) * (Q.y - A2.y) - (B2.y - A2.y) * (Q.x - A2.x)) * orient;
            float s1 = ((C2.x - B2.x) * (Q.y - B2.y) - (C2.y - B2.y) * (Q.x - B2.x)) * orient;
            float s2 = ((A2.x - C2.x) * (Q.y - C2.y) - (A2.y - C2.y) * (Q.x - C2.x)) * orient;
            if (s0 >= 0.0 && s1 >= 0.0 && s2 >= 0.0) { clear = false; }
          }
          if (clear) { ear = i; break; }
        }
        if (ear == 0xFFFFFFFFu) { break; }           // no ear (self-intersecting?) -> fan the rest
        uint ip = (ear + remaining - 1u) % remaining;
        uint in_ = (ear + 1u) % remaining;
        IDX[base + emit * 3u + 0u] = VId[a + ring[ip]];
        IDX[base + emit * 3u + 1u] = VId[a + ring[ear]];
        IDX[base + emit * 3u + 2u] = VId[a + ring[in_]];
        TFd[tbase + emit]          = f;
        emit++;
        for (uint k = ear; k + 1u < remaining; k++) { ring[k] = ring[k + 1u]; }
        remaining--;
      }
      // emit the remainder as a fan from ring[0] (the final triangle, or the
      // no-ear fallback — total is exactly n-2 either way)
      for (uint k = 1u; k + 1u < remaining; k++) {
        IDX[base + emit * 3u + 0u] = VId[a + ring[0]];
        IDX[base + emit * 3u + 1u] = VId[a + ring[k]];
        IDX[base + emit * 3u + 2u] = VId[a + ring[k + 1u]];
        TFd[tbase + emit]          = f;
        emit++;
      }
      return;
    }
  }
  // classic corner-0 fan (n > 60 or degenerate-normal fallback)
  uint v0 = VId[a];
  for (uint k = 0u; k < ntris; k++) {
    IDX[base + k * 3u + 0u] = v0;
    IDX[base + k * 3u + 1u] = VId[a + k + 1u];
    IDX[base + k * 3u + 2u] = VId[a + k + 2u];
    TFd[tbase + k]          = f;
  }
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// M2.5 — GPU-RESIDENT COUNT import. For a mesh whose LIVE face count lives on the
// GPU (GpuMesh::_gpuResidentCount; the producer writes it into _header each frame
// with NO readback), this 1-thread kernel copies that live count into the
// triangulator's _params.num_faces BEFORE cs_count/cs_tri run. The host-written
// _params then carries the CAPACITY (for dispatch sizing); this overwrites only the
// num_faces field with the live value, so the fan-triangulation early-outs at the
// REAL primitive count -> exact draw, no degenerate tail, no CPU sync. Header layout
// (see SdfToMeshInst::onTopologyReady / GpuMeshData): uint[0]=nv uint[1]=nc uint[2]=nf.
static std::string _importcount_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ic_par (descriptor_set 0) { buffer layout(std430) icpb { uint num_faces; uint inst_count; uint has_tags; uint pad2; }; }
storage_interface ic_hdr (descriptor_set 0) { buffer layout(std430) ichb { uint HDR[]; }; }
compute_interface ic_iface { storage { ic_par ic_hdr } inputs { layout(local_size_x = 1); } }
compute_shader cs_importcount : ic_iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  num_faces = HDR[2];   // live face count (producer's GPU-side write); inst_count/has_tags untouched
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// E.4 — per-view GPU frustum cull for INSTANCED hypermesh. Sphere-vs-frustum
// per instance (the rigid_primitive cs_cull_mtxonly recipe), compacting BOTH
// the matrices AND the E.2 typed attrs into culled buffers the graphics
// pipes bind; the visible count fans out into EVERY bound gid slot's
// indirect instanceCount (E.3 buckets share one instance set). Runs in
// ComputeDrawable::_perViewCompute (per VP, own dispatch phase).
///////////////////////////////////////////////////////////////////////////////

static std::string _instcull_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface cif_par (descriptor_set 0) { buffer layout(std430) cpar {
  mat4 u_vp; vec4 u_bound; uint u_count; uint u_visible; uint u_p0; uint u_p1; }; }
storage_interface cif_inm  (descriptor_set 0) { buffer layout(std430) cinm  { mat4 IN_M[];  }; }
storage_interface cif_outm (descriptor_set 0) { buffer layout(std430) coutm { mat4 OUT_M[]; }; }
storage_interface cif_ina  (descriptor_set 0) { buffer layout(std430) cina  { vec4 IN_A[];  }; }
storage_interface cif_outa (descriptor_set 0) { buffer layout(std430) couta { vec4 OUT_A[]; }; }
storage_interface cif_arg  (descriptor_set 0) { buffer layout(std430) carg  { uint ARG[];  }; }
storage_interface cif_bnd  (descriptor_set 0) { buffer layout(std430) cbnd  { uint BND[];  }; }
compute_interface ciface { storage { cif_par cif_inm cif_outm cif_ina cif_outa cif_arg cif_bnd }
                           inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_cull_reset : ciface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  u_visible = 0u;
}
////////////////////////////////////////
compute_shader cs_cull : ciface {
  uint i = gl_GlobalInvocationID.x;
  if (i >= u_count) { return; }
  mat4 M  = IN_M[i];
  vec3 c  = (M * vec4(u_bound.xyz, 1.0)).xyz;
  float r = u_bound.w * max(length(M[0].xyz), max(length(M[1].xyz), length(M[2].xyz)));
  vec4 rx = vec4(u_vp[0].x, u_vp[1].x, u_vp[2].x, u_vp[3].x);
  vec4 ry = vec4(u_vp[0].y, u_vp[1].y, u_vp[2].y, u_vp[3].y);
  vec4 rz = vec4(u_vp[0].z, u_vp[1].z, u_vp[2].z, u_vp[3].z);
  vec4 rw = vec4(u_vp[0].w, u_vp[1].w, u_vp[2].w, u_vp[3].w);
  vec4 pl0 = rw + rx; vec4 pl1 = rw - rx;
  vec4 pl2 = rw + ry; vec4 pl3 = rw - ry;
  vec4 pl4 = rz;      vec4 pl5 = rw - rz;
  bool inside = true;
  if ((dot(pl0.xyz, c) + pl0.w) < (-r * length(pl0.xyz))) { inside = false; }
  if ((dot(pl1.xyz, c) + pl1.w) < (-r * length(pl1.xyz))) { inside = false; }
  if ((dot(pl2.xyz, c) + pl2.w) < (-r * length(pl2.xyz))) { inside = false; }
  if ((dot(pl3.xyz, c) + pl3.w) < (-r * length(pl3.xyz))) { inside = false; }
  if ((dot(pl4.xyz, c) + pl4.w) < (-r * length(pl4.xyz))) { inside = false; }
  if ((dot(pl5.xyz, c) + pl5.w) < (-r * length(pl5.xyz))) { inside = false; }
  if (inside) {
    uint slot = atomicAdd(u_visible, 1u);
    OUT_M[slot] = M;
    OUT_A[slot] = IN_A[i];
  }
}
////////////////////////////////////////
// fan the visible count into every BOUND gid slot's indirect instanceCount
// (slot 0 = the default bucket, always bound).
compute_shader cs_cull_fanout : ciface {
  uint s = gl_GlobalInvocationID.x;
  if (s >= 4096u) { return; }
  bool bound = (s == 0u) || (((BND[s >> 5u] >> (s & 31u)) & 1u) != 0u);
  if (bound) {
    ARG[s * 5u + 1u] = u_visible;
  }
}
)S";
}

struct MeshInstCull {
  Context* _ctx                        = nullptr;
  const FxComputeShader* _cs_reset     = nullptr;
  const FxComputeShader* _cs_cull      = nullptr;
  const FxComputeShader* _cs_fanout    = nullptr;
  FxShaderStorageBuffer* _params       = nullptr; // {vp, bound, count, visible}
  FxShaderStorageBuffer* _culledMtx    = nullptr;
  FxShaderStorageBuffer* _culledAttr   = nullptr;
  FxShaderStorageBuffer* _srcMtx       = nullptr;
  FxShaderStorageBuffer* _srcAttr      = nullptr;
  FxShaderStorageBuffer* _args         = nullptr; // the triangulator's 4096-slot command array
  FxShaderStorageBuffer* _boundGidMask = nullptr;
  fvec4 _bound; // xyz = object-space center, w = radius
  int _count = 0;

  void build(Context* ctx, int count) {
    _ctx     = ctx;
    _count   = count;
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_instcull", _instcull_text());
    _cs_reset  = fxi->computeShader(sh, "cs_cull_reset");
    _cs_cull   = fxi->computeShader(sh, "cs_cull");
    _cs_fanout = fxi->computeShader(sh, "cs_cull_fanout");
    _params    = fxi->createStorageBuffer(64 + 16 + 16);
    _culledMtx  = fxi->createStorageBuffer(size_t(count) * 64);
    _culledAttr = fxi->createStorageBuffer(size_t(count) * 16);
  }
  void perView(Context* ctx, const CameraMatrices& cammtx) {
    // params: PRE-phase host write (vp + bound + count; visible zeroed in-phase)
    struct P { float vp[16]; float bound[4]; uint32_t count; uint32_t visible; uint32_t p0; uint32_t p1; } p;
    std::memcpy(p.vp, cammtx.GetVPMatrix().asArray(), 64);
    p.bound[0] = _bound.x; p.bound[1] = _bound.y; p.bound[2] = _bound.z; p.bound[3] = _bound.w;
    p.count = uint32_t(_count); p.visible = 0; p.p0 = p.p1 = 0;
    auto fxi = ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(p), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, sizeof(p));
    fxi->unmapStorageBuffer(m.get());
    auto ci   = ctx->CI();
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _params);
      ci->bindStorageBuffer(cs, 1, _srcMtx);
      ci->bindStorageBuffer(cs, 2, _culledMtx);
      ci->bindStorageBuffer(cs, 3, _srcAttr);
      ci->bindStorageBuffer(cs, 4, _culledAttr);
      ci->bindStorageBuffer(cs, 5, _args);
      ci->bindStorageBuffer(cs, 6, _boundGidMask);
    };
    ci->beginDispatchPhase();
    bind(_cs_reset);
    ci->dispatchCompute(_cs_reset, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_cull);
    ci->dispatchCompute(_cs_cull, (uint32_t(_count) + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_fanout);
    ci->dispatchCompute(_cs_fanout, 4096 / 64, 1, 1);
    ci->endDispatchPhase(); // submit + WAIT -> u_visible readable below
    // ORKID_DEBUG_CULL=1 — throttled visibility readback (the phase waited)
    static const bool s_dbg = (getenv("ORKID_DEBUG_CULL") != nullptr);
    static int s_ctr        = 0;
    if (s_dbg and ((s_ctr++ & 127) == 0)) {
      struct P { float vp[16]; float bound[4]; uint32_t count; uint32_t visible; uint32_t p0; uint32_t p1; } rb;
      auto m = fxi->mapStorageBuffer(_params, 0, sizeof(rb), BufferMapAccess::READ_ONLY);
      std::memcpy(&rb, m->_mappedaddr, sizeof(rb));
      fxi->unmapStorageBuffer(m.get());
      printf("[instcull] count<%u> visible<%u> bound<%.2f %.2f %.2f r=%.2f> vp0<%.3f %.3f %.3f %.3f>\n",
             rb.count, rb.visible, rb.bound[0], rb.bound[1], rb.bound[2], rb.bound[3],
             rb.vp[0], rb.vp[5], rb.vp[10], rb.vp[14]);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// MeshRenderTri — the persistent triangulator state (compiled once; buffers pooled-by-growth).
///////////////////////////////////////////////////////////////////////////////

struct MeshRenderTri {
  Context* _ctx                    = nullptr;
  const FxComputeShader* _cs_reset = nullptr;
  const FxComputeShader* _cs_count = nullptr; // E.3: per-gid histogram
  const FxComputeShader* _cs_scan  = nullptr; // E.3: prefix -> per-gid commands/cursors
  const FxComputeShader* _cs_tri   = nullptr;
  const FxComputeShader* _cs_lines = nullptr;
  const FxComputeShader* _cs_importcount = nullptr; // M2.5: load live count from _header (GPU-resident-count meshes)
  FxShaderStorageBuffer* _triIndex = nullptr; // uint32[ >= 3*num_corners ]
  FxShaderStorageBuffer* _triFace  = nullptr; // uint32[ >= num_corners ]  out-tri -> source face id (face-viz)
  FxShaderStorageBuffer* _args     = nullptr; // E.3: kGidSlots x VkDrawIndexedIndirectCommand (slot g at byte g*20)
  FxShaderStorageBuffer* _hist     = nullptr; // E.3: kGidSlots uints (counts -> cursors)
  FxShaderStorageBuffer* _boundGidMask = nullptr; // E.3: 128 uints (bit g = gid g has a bound material)
  FxShaderStorageBuffer* _dummyTags    = nullptr; // bound at the TAGS slot when the mesh has no __tags
  std::vector<int> _boundGids;                    // gids with their own material (slot 0 implicit)
  FxShaderStorageBuffer* _lineIndex = nullptr; // uint32[ >= 2*num_corners ] (wireframe: per-edge LINE indices)
  FxShaderStorageBuffer* _lineArgs  = nullptr; // VkDrawIndexedIndirectCommand (lines)
  FxShaderStorageBuffer* _params   = nullptr; // { num_faces, ... } (host-written PRE dispatch phase)
  int _indexCap                    = 0;       // uint capacity of _triIndex
  int _faceCap                     = 0;       // uint capacity of _triFace
  int _lineCap                     = 0;       // uint capacity of _lineIndex
  bool _wireframe                  = false;   // also build the per-edge LINE index buffer each frame
  int _instCount                   = 1;       // indirect instanceCount (1 = non-instanced); -> cs_reset
  FxShaderStorageBuffer* _instMtx  = nullptr; // per-instance matrices SSBO (mat4[_instCount]); null if not instanced
  // C.1b DIRTY GATE — the topology KEY this triangulation was built from. Triangulation (cs_reset/
  // cs_tri/cs_lines) depends ONLY on connectivity, so it re-runs ONLY when the key moves: _topoVersion
  // covers explicit rebuild bumps (extrude/inset/normals + the allocMesh auto-bump); counts + the
  // vidx/face_offsets COW handles are observed directly (catches count-set-within-capacity paths,
  // e.g. ripple's else-branch). Positions-only animation never re-triangulates; static meshes
  // triangulate exactly once.
  uint64_t _builtTopoV = ~0ull;
  int _builtNF = -1, _builtNC = -1;
  const void* _builtVidx = nullptr;
  const void* _builtFO   = nullptr;
  bool topoDirty(gpumesh_ptr_t m) const {
    return m->_topoVersion != _builtTopoV or m->_num_faces != _builtNF or m->_num_corners != _builtNC or
           (const void*)m->_vidx.get() != _builtVidx or (const void*)m->_face_offsets.get() != _builtFO;
  }
  void ackTopo(gpumesh_ptr_t m) {
    _builtTopoV = m->_topoVersion;
    _builtNF    = m->_num_faces;
    _builtNC    = m->_num_corners;
    _builtVidx  = m->_vidx.get();
    _builtFO    = m->_face_offsets.get();
  }

  void build(Context* ctx) {
    _ctx     = ctx;
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_triangulate", _triangulate_text());
    _cs_reset = fxi->computeShader(sh, "cs_reset");
    _cs_count = fxi->computeShader(sh, "cs_count");
    _cs_scan  = fxi->computeShader(sh, "cs_scan");
    _cs_tri   = fxi->computeShader(sh, "cs_tri");
    _cs_lines = fxi->computeShader(sh, "cs_lines");
    auto icsh = fxi->shaderFromShaderText("hypermesh_importcount", _importcount_text()); // M2.5 (own 2-binding iface)
    _cs_importcount = fxi->computeShader(icsh, "cs_importcount");
    _args     = fxi->createStorageBuffer(size_t(kGidSlots) * 5 * 4); // one command per gid slot
    _hist     = fxi->createStorageBuffer(size_t(kGidSlots) * 4);
    _boundGidMask = fxi->createStorageBuffer(128 * 4);
    _dummyTags    = fxi->createStorageBuffer(4);
    _lineArgs = fxi->createStorageBuffer(sizeof(uint32_t) * 8); // line draw command (always allocated)
    _lineIndex = fxi->createStorageBuffer(4);                   // dummy until ensureIndex (so slot 6 binds)
    _lineCap  = 1;
    _params   = fxi->createStorageBuffer(16);
  }
  // grow the index buffer to hold the worst-case tri count (<= 3*num_corners indices) + the per-tri
  // face-id buffer (<= num_corners tris). pow2-stepped; only re-creates on genuine growth.
  void ensureIndex(int num_corners) {
    int need = meshNextPow2(num_corners * 3 + 1);
    if (need > _indexCap) {
      _triIndex = _ctx->FXI()->createStorageBuffer(size_t(need) * 4);
      _indexCap = need;
    }
    int fneed = meshNextPow2(num_corners + 1);
    if (fneed > _faceCap) {
      _triFace = _ctx->FXI()->createStorageBuffer(size_t(fneed) * 4);
      _faceCap = fneed;
    }
    if (_wireframe) {                       // worst case: 2 line indices per corner (one edge / corner)
      int lneed = meshNextPow2(num_corners * 2 + 1);
      if (lneed > _lineCap) {
        _lineIndex = _ctx->FXI()->createStorageBuffer(size_t(lneed) * 4);
        _lineCap   = lneed;
      }
    }
  }
  // PRE-dispatch-phase host write (a map mid-phase is invisible to the dispatch).
  void writeParams(int num_faces, bool has_tags) {
    uint32_t p[4] = {uint32_t(num_faces), uint32_t(_instCount), has_tags ? 1u : 0u, 0u};
    auto m = _ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(p), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, p, sizeof(p));
    _ctx->FXI()->unmapStorageBuffer(m.get());
    // bound-gid bitmask (slot 0 always set — the default-material bucket;
    // unbound gids FOLD to it at count/scatter time)
    uint32_t mask[128] = {1u};
    for (int g : _boundGids)
      if (g >= 0 and g < kGidSlots)
        mask[g >> 5] |= (1u << (g & 31));
    auto bm = _ctx->FXI()->mapStorageBuffer(_boundGidMask, 0, sizeof(mask), BufferMapAccess::WRITE_ONLY);
    std::memcpy(bm->_mappedaddr, mask, sizeof(mask));
    _ctx->FXI()->unmapStorageBuffer(bm.get());
  }
  // IN-dispatch-phase: reset the draw command, then fan-triangulate every face. Both shaders share
  // ONE interface, so both bind all 5 slots (par,fo,vi,idx,arg) — cs_reset writes only arg, but the
  // descriptor set covers the full interface (mirrors the generator modules' cs_setup binding).
  void dispatch(gpumesh_ptr_t mesh) {
    auto ci   = _ctx->CI();
    auto tags = mesh->face("__tags");
    auto tags_ssbo = tags ? tags->_ssbo : _dummyTags; // has_tags=0 -> never read
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _params);
      ci->bindStorageBuffer(cs, 1, mesh->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 2, mesh->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 3, _triIndex);
      ci->bindStorageBuffer(cs, 4, _args);
      ci->bindStorageBuffer(cs, 5, _triFace);
      ci->bindStorageBuffer(cs, 6, _lineIndex);
      ci->bindStorageBuffer(cs, 7, _lineArgs);
      ci->bindStorageBuffer(cs, 8, _hist);
      ci->bindStorageBuffer(cs, 9, _boundGidMask);
      ci->bindStorageBuffer(cs, 10, tags_ssbo);
      auto pch = mesh->channel(MeshChannel::POSITION);
      ci->bindStorageBuffer(cs, 11, pch ? pch->_ssbo : _dummyTags); // E.5: ear clip reads P
    };
    int fgroups = (mesh->_num_faces + 63) / 64;
    // M2.5 — GPU-RESIDENT COUNT: _params.num_faces was host-written to the CAPACITY (mesh->_num_faces);
    // overwrite it on the GPU with the LIVE count from _header so cs_count/cs_tri early-out at the real
    // primitive count. fgroups dispatches the full capacity; threads past the live count early-out (cheap).
    // No CPU readback anywhere -> the producer's per-frame count never touches the host.
    if (mesh->_gpuResidentCount and mesh->_header) {
      ci->bindStorageBuffer(_cs_importcount, 0, _params);
      ci->bindStorageBuffer(_cs_importcount, 1, mesh->_header);
      ci->dispatchCompute(_cs_importcount, 1, 1, 1);
      ci->storageBarrier();
    }
    bind(_cs_reset);
    ci->dispatchCompute(_cs_reset, kGidSlots / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_count);
    ci->dispatchCompute(_cs_count, fgroups, 1, 1);
    ci->storageBarrier();
    bind(_cs_scan);
    ci->dispatchCompute(_cs_scan, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_tri);
    ci->dispatchCompute(_cs_tri, fgroups, 1, 1);
    ci->storageBarrier();
    if (_wireframe) {                       // build the per-edge LINE index buffer (wireframe overlay)
      bind(_cs_lines);
      ci->dispatchCompute(_cs_lines, fgroups, 1, 1);
      ci->storageBarrier();
    }
  }
};

///////////////////////////////////////////////////////////////////////////////

// faceViz=true -> the per-triangle source-face-id buffer (_triFace) is exposed + kept refreshed for the
// face-visualization FS (which reads TFd[gl_PrimitiveID]); returns that buffer so make_drawable can bind
// it to the FS storage block (graphics-storage index 5, right after the 5 vertex channels). Returns null
// when faceViz=false. The vert channels live at graphics-storage indices 0..4 (see make_drawable).
MeshRenderBuffers setupMeshRender(
    ComputeDrawableData* cdd, livehypermesh_ptr_t live, Context* ctx, bool animated, bool faceViz,
    bool tagViz, bool wireframe, int instanceCount, const std::vector<float>& instanceMatrices,
    const std::vector<int>& boundGids, bool cull, const fvec4& cullBound) {
  faceViz = faceViz or tagViz; // the tag-viz FS indexes __tags by the per-triangle face id (slot 5)
  auto tri = std::make_shared<MeshRenderTri>();
  tri->_wireframe = wireframe;
  tri->_boundGids = boundGids; // E.3: the fold mask (everything else lands in slot 0)
  tri->build(ctx);
  auto mesh0 = live->_mesh;
  tri->ensureIndex(mesh0 ? mesh0->_num_corners : 1);
  // INSTANCING: cs_reset writes instanceCount into the indirect command.
  // E.2 TYPED EDGE: a graph-carried InstanceSet (live->_instances, a ScatterSource output)
  // IS the instance source — its matrices + attrs SSBOs bind as-is, count from the set.
  // LEGACY (instances= floats): buffers are created here; the matrix BOTTOM ROWS are split
  // into a synthesized attrs SSBO and ZEROED on upload — the instanced VS now ALWAYS reads
  // per-instance data from storage_inst_attr (the bottom-row smuggle is retired).
  FxShaderStorageBuffer* inst_attr = nullptr;
  if (live->_instances and live->_instances->_count > 0) {
    tri->_instCount = live->_instances->_count;
    tri->_instMtx   = live->_instances->_matrices;
    inst_attr       = live->_instances->_attrs;
  } else {
    tri->_instCount = (instanceCount > 1) ? instanceCount : 1;
    if (tri->_instCount > 1 and not instanceMatrices.empty()) {
      const size_t n     = size_t(tri->_instCount);
      const size_t bytes = n * 64; // mat4 = 64 bytes
      std::vector<float> mats(n * 16, 0.0f);
      std::memcpy(mats.data(), instanceMatrices.data(),
                  std::min(bytes, instanceMatrices.size() * sizeof(float)));
      std::vector<float> attrs(n * 4, 0.0f);
      for (size_t i = 0; i < n; i++) { // bottom row (m[0..2].w = floats 3/7/11) -> attrs; rows zeroed
        attrs[i * 4 + 0] = mats[i * 16 + 3];
        attrs[i * 4 + 1] = mats[i * 16 + 7];
        attrs[i * 4 + 2] = mats[i * 16 + 11];
        attrs[i * 4 + 3] = 1.0f;
        mats[i * 16 + 3] = mats[i * 16 + 7] = mats[i * 16 + 11] = 0.0f;
        mats[i * 16 + 15] = 1.0f;
      }
      auto fxi      = ctx->FXI();
      tri->_instMtx = fxi->createStorageBuffer(bytes);
      auto m = fxi->mapStorageBuffer(tri->_instMtx, 0, bytes, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, mats.data(), bytes);
      fxi->unmapStorageBuffer(m.get());
      inst_attr = fxi->createStorageBuffer(n * 16);
      auto a = fxi->mapStorageBuffer(inst_attr, 0, n * 16, BufferMapAccess::WRITE_ONLY);
      std::memcpy(a->_mappedaddr, attrs.data(), n * 16);
      fxi->unmapStorageBuffer(a.get());
    }
  }
  // E.4 — per-view instance cull: compacts matrices+attrs into culled buffers
  // (the graphics pipes bind THOSE) and fans the visible count into every
  // bound gid slot's indirect command. Requires a positive bound radius.
  FxShaderStorageBuffer* gfx_instMtx  = tri->_instMtx;
  FxShaderStorageBuffer* gfx_instAttr = inst_attr;
  if (cull and tri->_instCount > 1 and tri->_instMtx) {
    if (cullBound.w <= 0.0f) {
      printf("hypermesh::setupMeshRender: cull requested but bound radius <= 0 — cull DISABLED\n");
    } else {
      auto culler = std::make_shared<MeshInstCull>();
      culler->build(ctx, tri->_instCount);
      culler->_bound  = cullBound;
      culler->_srcMtx = tri->_instMtx;
      if (inst_attr) {
        culler->_srcAttr = inst_attr;
      } else { // matrices-only: a zeroed attr source keeps the shader uniform
        auto fxi         = ctx->FXI();
        culler->_srcAttr = fxi->createStorageBuffer(size_t(tri->_instCount) * 16);
        std::vector<float> zz(size_t(tri->_instCount) * 4, 0.0f);
        auto zm = fxi->mapStorageBuffer(culler->_srcAttr, 0, zz.size() * 4, BufferMapAccess::WRITE_ONLY);
        std::memcpy(zm->_mappedaddr, zz.data(), zz.size() * 4);
        fxi->unmapStorageBuffer(zm.get());
      }
      culler->_args         = tri->_args;
      culler->_boundGidMask = tri->_boundGidMask;
      cdd->_perViewCompute  = [culler](Context* c, const CameraMatrices& m) { culler->perView(c, m); };
      gfx_instMtx  = culler->_culledMtx;
      gfx_instAttr = culler->_culledAttr;
    }
  }
  cdd->setIndirect(tri->_args, 0, tri->_triIndex, PrimitiveType::TRIANGLES, 4);
  if (wireframe) // the OVERLAY draw (lines over the fill); caller sets the overlay material + P/N storage
    cdd->setOverlayIndirect(tri->_lineArgs, 0, tri->_lineIndex, PrimitiveType::LINES, 4);

  // the per-frame, IN-FRAME hook (ComputeDrawable::onPreRender runs this; _passes stays empty). One
  // dispatch phase: (animated) graph re-eval -> triangulate -> refresh the drawable's live bindings.
  cdd->_liveRecompute = [tri, live, animated, faceViz, tagViz, wireframe](Context* ctx, ComputeDrawable* drw) {
    auto mesh = live->_mesh;
    if (not mesh) return;
    // ---- B.4 CLOCK FEED: the LIVE path's time source (steady, incremental). Fills the family env
    // (module writeParams reads it — e.g. the extrude S.time slot) AND the graph UpdateData (module
    // compute() parity with the ECS host contract). BAKE paths never run this hook -> stay t=0.
    // PAUSE: while (global || per-graph) paused, the accumulator holds and dt=0 — resume is seamless.
    {
      using clk = std::chrono::steady_clock;
      double now = std::chrono::duration<double>(clk::now().time_since_epoch()).count();
      if (live->_clock_last < 0.0)
        live->_clock_last = now;
      bool paused = clockPaused() or live->_paused;
      double dt   = paused ? 0.0 : (now - live->_clock_last);
      live->_clock_last = now;
      live->_clock_abstime += dt;
      if (auto env = live->_ginst->_impl.getShared<MeshEnv>()) {
        env->_dt      = dt;
        env->_abstime = live->_clock_abstime;
      }
      // mirror the SAME clock into the terrain field env, when a field subgraph rides this
      // graph (E.1b: fbm/noise offset_vel pan reads it — one clock, every family env).
      if (auto fenv = live->_ginst->_impl.getShared<terrain::BakeEnv>()) {
        fenv->_dt      = dt;
        fenv->_abstime = live->_clock_abstime;
      }
      live->_updata->_abstime = live->_clock_abstime;
      live->_updata->_dt      = dt;
    }
    // ---- C.1b DIRTY GATE ----
    // graph eval runs only when the graph can actually move: animated AND unpaused. Pause = the
    // graph STOPS (plug edits made while paused take effect on unpause — the B.4 pause contract);
    // a static mesh never re-evals here (initial eval = materializeLive's recompute; a manual
    // live.recompute() between frames mutates topology, which the topo key below catches).
    bool paused    = clockPaused() or live->_paused;
    bool run_graph = animated and not paused;
    auto ci        = ctx->CI();
    bool triangulated = false;
    using perfclk = std::chrono::steady_clock;
    auto perf_g0  = perfclk::now();                 // full-eval wall (HmPerf; clean frames don't count)
    double perf_gw0 = ctx->CI()->_gpuWaitAccum;     // gpu-fence-wait baseline (cpu = full - gpu-wait)
    if (run_graph) {
      // PRE dispatch phase: each module's CPU sizing + uniform host-write (subdivide re-pools
      // here, sets counts). Count/handle topology changes land HERE (allocMesh), so the topo key
      // sampled after this predicts almost every re-triangulation — letting the triangulate ride
      // the SAME dispatch phase as the graph compute (one submit, the pre-C.1b shape).
      for (auto inst : live->_ginst->_ordered_module_insts)
        if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst)) { // family-neutral
          auto perf_t0 = perfclk::now();
          pp->writeParams(ctx);
          HmPerf::instance().addModule(
              inst->_dgmodule_data->GetClass()->Name().c_str(),
              std::chrono::duration<double>(perfclk::now() - perf_t0).count());
        }
      bool pre_dirty = tri->topoDirty(mesh);
      if (pre_dirty) {
        tri->ensureIndex(mesh->_num_corners);
        tri->writeParams(mesh->_num_faces, mesh->face("__tags") != nullptr);
      }
      ci->beginDispatchPhase();
      live->_ginst->compute(live->_updata);
      if (pre_dirty) {
        ci->storageBarrier(); // graph writes channels/vidx/face_offsets -> triangulate reads them
        tri->dispatch(mesh);
      }
      ci->endDispatchPhase(); // submit + WAIT
      if (pre_dirty) {
        tri->ackTopo(mesh);   // ack POST-phase: absorbs same-frame compute-time bumps the in-phase
        triangulated = true;  // triangulate already saw (it ran after the barrier)
      }
    }
    // post-eval check — catches the rare COMPUTE-TIME topology rebuild the pre-phase key couldn't
    // see (e.g. an extrude rebuilding off srcTopoDirty cascade); also the first-frame build for the
    // static path and a manual live.recompute() between frames. Positions-only eval (ripple) and
    // the paused/static steady states dispatch NOTHING.
    static const bool s_force_dirty = (getenv("ORK_HM_FORCE_DIRTY") != nullptr); // bisect knob
    bool topo_dirty = (not triangulated) and (s_force_dirty or tri->topoDirty(mesh));
    if (not topo_dirty and not run_graph)
      return;                                      // fully clean: zero GPU work, bindings stand
    if (topo_dirty) {
      tri->ensureIndex(mesh->_num_corners);
      tri->writeParams(mesh->_num_faces, mesh->face("__tags") != nullptr);
      ci->beginDispatchPhase();
      tri->dispatch(mesh);
      ci->endDispatchPhase(); // submit + WAIT
      tri->ackTopo(mesh);
    }
    // ---- refresh the drawable to the mesh's CURRENT pooled buffers (dynamic topology) ----
    size_t n = drw->_graphicsStorage.size();
    for (size_t i = 0; i < n && i < 5; i++) {
      auto ch = mesh->channel(MeshChannel(int(i))); // render order P,N,B,uv,color (see make_drawable)
      if (ch) drw->_graphicsStorage[i].second = ch->_ssbo;
    }
    // E.3: each gid bucket's storage list shares the same first-5-channels contract
    for (auto& bucket : drw->_bucketDraws) {
      size_t bn = bucket._graphicsStorage.size();
      for (size_t i = 0; i < bn && i < 5; i++) {
        auto ch = mesh->channel(MeshChannel(int(i)));
        if (ch) bucket._graphicsStorage[i].second = ch->_ssbo;
      }
    }
    if (faceViz and n > 5)
      drw->_graphicsStorage[5].second = tri->_triFace; // FS reads TFd[gl_PrimitiveID]; re-pools on growth
    if (tagViz and n > 6) {                            // FS reads STd[ TFd[gl_PrimitiveID] ] (selection groups)
      auto tags = mesh->face("__tags");
      if (tags) drw->_graphicsStorage[6].second = tags->_ssbo; // re-pools on face-count growth
    }
    drw->_argsSSBO  = tri->_args;
    drw->_indexSSBO = tri->_triIndex;
    if (wireframe) {                                   // refresh the wireframe OVERLAY draw (lines on top)
      drw->_overlayArgsSSBO  = tri->_lineArgs;
      drw->_overlayIndexSSBO = tri->_lineIndex;        // re-pools on topology growth -> rebind
      size_t on = drw->_overlayGraphicsStorage.size(); // overlay pull-VS reads P (slot 0) + N (slot 1)
      if (on > 0) { auto ch = mesh->channel(MeshChannel::POSITION); if (ch) drw->_overlayGraphicsStorage[0].second = ch->_ssbo; }
      if (on > 1) { auto ch = mesh->channel(MeshChannel::NORMAL);   if (ch) drw->_overlayGraphicsStorage[1].second = ch->_ssbo; }
    }
    HmPerf::instance().addGraph(
        std::chrono::duration<double>(perfclk::now() - perf_g0).count(),
        ctx->CI()->_gpuWaitAccum - perf_gw0,
        mesh->_num_faces,
        mesh->_num_verts,
        tri->_instCount);
    HmPerf::instance().report();                       // rate-limited: prints once per 5s window
  };
  MeshRenderBuffers out;
  out._faceid   = faceViz ? tri->_triFace : nullptr;
  out._instMtx  = gfx_instMtx;   // E.4: the CULLED buffers when the cull is active
  out._instAttr = gfx_instAttr;
  return out;
}

///////////////////////////////////////////////////////////////////////////////
// E.5 gate — hypermeshTriangulationSelfTest: hand-built polygons through the
// REAL triangulator (MeshRenderTri dispatch), index readback, and the AREA
// ORACLE: per-triangle orientation must match the polygon (a folded fan emits
// flipped tris) and the summed triangle area must equal the polygon area (a
// folded fan double-covers). Convex sanity + the concave cases the old fan
// provably broke. Returns the number of FAILED cases.
///////////////////////////////////////////////////////////////////////////////

int hypermeshTriangulationSelfTest(Context* ctx) {
  auto fxi  = ctx->FXI();
  int fails = 0;

  // one polygon in the XY plane (z=0) -> GpuMesh -> triangulate -> readback
  auto run_case = [&](const char* name, const std::vector<fvec2>& poly) -> bool {
    int n = int(poly.size());
    // ---- hand-build the minimal GpuMesh the triangulator consumes ----
    auto mesh = std::make_shared<GpuMesh>(std::make_shared<GpuMeshData>());
    auto mk_chan = [&](int stride, int count) {
      auto ch       = std::make_shared<GpuChannel>();
      ch->_stride   = stride;
      ch->_capacity = count;
      ch->_ssbo     = fxi->createStorageBuffer(size_t(stride) * count);
      return ch;
    };
    auto pch = mk_chan(16, n);
    {
      std::vector<float> P(size_t(n) * 4, 0.0f);
      for (int i = 0; i < n; i++) {
        P[i * 4 + 0] = poly[i].x;
        P[i * 4 + 1] = poly[i].y;
      }
      auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, P.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, P.data(), P.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    mesh->_channels[MeshChannel::POSITION] = pch;
    auto vch = mk_chan(4, n);
    auto fch = mk_chan(4, 2);
    {
      std::vector<uint32_t> vidx(n);
      for (int i = 0; i < n; i++) vidx[i] = uint32_t(i);
      auto m = fxi->mapStorageBuffer(vch->_ssbo, 0, size_t(n) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, vidx.data(), size_t(n) * 4);
      fxi->unmapStorageBuffer(m.get());
      uint32_t fo[2] = {0u, uint32_t(n)};
      auto m2 = fxi->mapStorageBuffer(fch->_ssbo, 0, 8, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m2->_mappedaddr, fo, 8);
      fxi->unmapStorageBuffer(m2.get());
    }
    mesh->_vidx         = vch;
    mesh->_face_offsets = fch;
    mesh->_num_verts    = n;
    mesh->_num_corners  = n;
    mesh->_num_faces    = 1;

    // ---- the REAL triangulator ----
    auto tri = std::make_shared<MeshRenderTri>();
    tri->build(ctx);
    tri->ensureIndex(n);
    tri->writeParams(1, false);
    auto ci = ctx->CI();
    ci->beginDispatchPhase();
    tri->dispatch(mesh);
    ci->endDispatchPhase();

    // ---- readback: slot-0 command + the index buffer ----
    uint32_t cmd[5];
    {
      auto m = fxi->mapStorageBuffer(tri->_args, 0, 20, BufferMapAccess::READ_ONLY);
      std::memcpy(cmd, m->_mappedaddr, 20);
      fxi->unmapStorageBuffer(m.get());
    }
    int ntris = int(n - 2);
    if (cmd[0] != uint32_t(ntris * 3)) {
      printf("[hm tri-oracle %s] FAIL indexCount<%u> want<%d>\n", name, cmd[0], ntris * 3);
      return false;
    }
    std::vector<uint32_t> idx(size_t(ntris) * 3);
    {
      auto m = fxi->mapStorageBuffer(tri->_triIndex, 0, idx.size() * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(idx.data(), m->_mappedaddr, idx.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    // ---- the AREA ORACLE (XY plane; polygon authored CCW) ----
    auto tri_area2 = [&](uint32_t a, uint32_t b, uint32_t c) -> float { // signed, x2
      fvec2 A = poly[a], B = poly[b], C = poly[c];
      return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    };
    float poly_area2 = 0.0f;
    for (int i = 0; i < n; i++) {
      fvec2 A = poly[i], B = poly[(i + 1) % n];
      poly_area2 += A.x * B.y - B.x * A.y;
    }
    float sum2  = 0.0f;
    bool flipped = false;
    for (int t = 0; t < ntris; t++) {
      uint32_t a = idx[t * 3 + 0], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
      if (a >= uint32_t(n) or b >= uint32_t(n) or c >= uint32_t(n)) {
        printf("[hm tri-oracle %s] FAIL out-of-range index\n", name);
        return false;
      }
      float a2 = tri_area2(a, b, c);
      if (a2 <= 0.0f) flipped = true;  // CCW polygon must emit CCW tris only
      sum2 += a2;
    }
    bool area_ok = std::fabs(sum2 - poly_area2) < 1.0e-4f * std::fabs(poly_area2);
    if (flipped or not area_ok) {
      printf("[hm tri-oracle %s] FAIL flipped<%d> area(sum=%.5f poly=%.5f)\n",
             name, int(flipped), sum2 * 0.5f, poly_area2 * 0.5f);
      return false;
    }
    printf("[hm tri-oracle %s] PASS (%d tris, area %.5f)\n", name, ntris, sum2 * 0.5f);
    return true;
  };

  auto P2 = [](std::initializer_list<std::pair<float, float>> pts) {
    std::vector<fvec2> v;
    for (auto& p : pts)
      v.push_back(fvec2(p.first, p.second));
    return v;
  };
  // 1) convex quad (sanity)
  fails += run_case("convex-quad", P2({{0, 0}, {2, 0}, {2, 2}, {0, 2}})) ? 0 : 1;
  // 2) concave (dart) quad — reflex at (1.0, 0.8); a corner-0 fan folds
  fails += run_case("dart-quad", P2({{0, 0}, {2, 0}, {1.0f, 0.8f}, {1.0f, 3.0f}})) ? 0 : 1;
  // 3) L-shaped hexagon — the classic concave case
  fails += run_case("L-hex", P2({{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}})) ? 0 : 1;
  // 4) comb octagon — two notches, multiple reflex vertices
  fails += run_case("comb-oct",
      P2({{0, 0}, {4, 0}, {4, 2}, {3, 2}, {3, 1}, {2, 1}, {2, 2}, {0, 2}})) ? 0 : 1;
  return fails;
}

} // namespace ork::lev2::hypermesh
