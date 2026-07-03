////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hmdflow_module.h — shared contract for the hypermesh GPU compute modules. Each module's
// implementation lives in its own hmdflow_module_<name>.cpp and includes this; the common
// stuff (MeshPool, plug instantiations, the bake driver, the self-test) is in hmdflow.cpp.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <ork/kernel/string/string.h>

namespace ork::lev2::hypermesh {

namespace dflow = ::ork::dataflow;

// v2 header buffer layout (bytes): counts(16: num_verts,num_corners,num_faces,flags) + bbox(32) = 48, padded to 64.
// (the render's DrawIndexedIndirect args are a SEPARATE render-owned buffer; the header is for readback.)
static constexpr int kMeshHeaderBytes     = 64;
static constexpr int kMeshHeaderUsedBytes = 48; // writers fill counts+bbox only; the pad is never written

///////////////////////////////////////////////////////////////////////////////
// shared helpers
///////////////////////////////////////////////////////////////////////////////

// substitute %KEY% -> val in a shader template (mirrors terrain _shadersub).
inline void _shadersub(std::string& s, const std::string& key, const std::string& val) {
  size_t pos = 0;
  while ((pos = s.find(key, pos)) != std::string::npos) {
    s.replace(pos, key.size(), val);
    pos += val.size();
  }
}

// position-quantization weld key: snap an xyz to an integer lattice (eps cell) so coincident-but-
// distinct verts (un-shared primitives carry per-face dup verts for flat normals) collapse to one
// key. SHARED by every op that needs true connectivity from an un-shared mesh (smooth_normals welds
// per-selected-face; subdivide(smooth) welds the whole control cage — CC needs real adjacency).
// `P4` is the vec4-stride position array (the channel's CPU readback); `v` indexes verts.
inline std::tuple<int, int, int> weldKey(const float* P4, uint32_t v, float eps = 1e-4f) {
  return std::tuple<int, int, int>(int(std::lround(P4[4 * v + 0] / eps)),
                                   int(std::lround(P4[4 * v + 1] / eps)),
                                   int(std::lround(P4[4 * v + 2] / eps)));
}

// weld a whole vert set by position: fills `remap` (input vert -> welded id) and `reps` (welded id ->
// a representative input vert, for reading its live position each frame). Returns the welded vert count.
// Used where the ENTIRE mesh must be welded (subdivide(smooth)); the per-face/selection-aware weld in
// smooth_normals shares only weldKey() above since it also tracks pass-through + adjacency.
inline int weldVerts(const float* P4, int nv, std::vector<uint32_t>& remap, std::vector<uint32_t>& reps, float eps = 1e-4f) {
  remap.resize(nv);
  reps.clear();
  std::map<std::tuple<int, int, int>, uint32_t> g;
  for (int v = 0; v < nv; v++) {
    auto key = weldKey(P4, uint32_t(v), eps);
    auto it  = g.find(key);
    uint32_t w;
    if (it == g.end()) { w = uint32_t(reps.size()); g[key] = w; reps.push_back(uint32_t(v)); }
    else                 w = it->second;
    remap[v] = w;
  }
  return int(reps.size());
}

// ---- shared PRE/POSTCONDITION checks: a small library any topology op invokes from its precheck/postcheck.
// They GPU->CPU read back the mesh (so ops gate them behind precheck/postcheck flags, default on). The measure
// fns return FACTS; the assert fns emit FACTUAL diagnostics (the measured defect + the op's contract) — they do
// NOT guess at the cause. Reused across modules (bevel today; extrude/inset/subdivide as they adopt it). ----
namespace meshcheck {
struct EdgeHealth { int boundary = 0; int nonmanifold = 0; };   // edges by incident-face count (by vertex index)

// ---- CPU cores: for CPU-rebuild ops (inset/extrude/subdivide) that ALREADY have the readback arrays in hand. ----
// # verts coincident with another by position but a DISTINCT index (unwelded mesh > 0). Uses the shared weldKey.
int coincidentVerts(const float* p4, int nv);
// classify edges by incident-face count: 1 = boundary, >2 = non-manifold. WELDS BY POSITION first (shared weldKey)
// so coincident-but-separate verts (un-welded split seams) collapse -> measures REAL holes, not index seams.
EdgeHealth edgeHealth(const float* p4, int nv, const uint32_t* vidx, const uint32_t* faceoff, int nf);
// # faces that are malformed for any frame-building op: < 3 verts, or ~0 area (degenerate -> garbage normal/uv frame).
int degenerateFaces(const float* p4, const uint32_t* vidx, const uint32_t* faceoff, int nf);
// # WINDING FLIPS: position-welded directed edges traversed the SAME way by >=2 faces. A consistently-wound
// orientable mesh has every interior edge traversed once each way; a backwards-wound (inverted / negative-area)
// face makes its shared edges co-directed -> a flip. WELDS BY POSITION (matches split-vert hard edges). 0 = OK.
int windingFlips(const float* p4, int nv, const uint32_t* vidx, const uint32_t* faceoff, int nf);
// # COPLANAR OVERLAP pairs: pairs of NON-edge-adjacent triangles that lie in the same plane AND overlap in 2D
// (separating-axis test). A GEOMETRIC self-intersection that watertight/manifold/winding checks DO NOT catch (the
// edge graph stays a valid manifold). Faces are fan-triangulated; pairs sharing >=2 verts (edge-adjacent) skipped.
int coplanarOverlaps(const float* p4, const uint32_t* vidx, const uint32_t* faceoff, int nf);

// ---- GPU wrappers: for ops whose data is on the GPU. Read back, then call the CPU core. ----
int coincidentVerts(Context* ctx, FxShaderStorageBuffer* pos, int nv);
EdgeHealth edgeHealth(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf);
int degenerateFaces(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf);
int windingFlips(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf);
int coplanarOverlaps(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf);

// ---- asserts (FACTUAL: the measured defect + the op's contract; no guessed cause) ----
void assertWelded(int coincident, int nv, const char* op);             // PRE: op needs a welded mesh
void assertNoDegenerateFaces(int degen, int nf, const char* op);       // PRE: op needs well-formed faces
void assertNoCreatedDefects(const EdgeHealth& in, const EdgeHealth& out, const char* op);  // POST: no new open/non-manifold edges
void assertWindingConsistent(int flips, const char* op, const char* phase);  // PRE/POST: mesh must be consistently wound (no inverted faces)
void assertNoOverlaps(int overlaps, const char* op, const char* phase);      // PRE/POST: no coplanar self-overlapping faces (geometric)
} // namespace meshcheck

// grab a float input pluginst AND copy its data-plug default into the inst (an unconnected
// inst plug is not auto-populated — same caveat as terrain).
inline dflow::float_inp_pluginst_ptr_t
_floatPlug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::FloatPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::FloatPlugTraits>(name)->_value;
  return p;
}

inline dflow::int_inp_pluginst_ptr_t
_intPlug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::IntPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::IntPlugTraits>(name)->_value;
  return p;
}

// the SOURCE mesh for a mesh input = the CONNECTED output's value.
inline gpumesh_ptr_t _srcMesh(mesh_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<mesh_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// vertex->face CSR adjacency, lazily built + CACHED on the mesh (keyed by its (vidx, face_offsets) buffers,
// so it rebuilds when topology changes). SHARED connectivity for any module that needs it (normals now;
// smoothing / curvature / weld / subdivide later) — not a graph node, not duplicated per module. Returns
// (off[nv+1] CSR offsets, face[nc] face ids). One CPU readback of vidx/face_offsets per topology.
inline std::pair<gpuchannel_ptr_t, gpuchannel_ptr_t>
vtxFaceAdjacency(Context* ctx, meshenv_ptr_t env, gpumesh_ptr_t mesh) {
  if (mesh->_adj_off and mesh->_adj_keyV == mesh->_vidx->_ssbo and mesh->_adj_keyF == mesh->_face_offsets->_ssbo)
    return {mesh->_adj_off, mesh->_adj_face};                       // cache hit (same topology)
  auto fxi = ctx->FXI();
  int nv = mesh->_num_verts, nf = mesh->_num_faces, nc = mesh->_num_corners;
  auto rd = [&](FxShaderStorageBuffer* b, int n) {
    std::vector<uint32_t> v(std::max(1, n));
    auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
    fxi->unmapStorageBuffer(m.get());
    return v;
  };
  auto vidx = rd(mesh->_vidx->_ssbo, nc);
  auto fo   = rd(mesh->_face_offsets->_ssbo, nf + 1);
  std::vector<uint32_t> off(nv + 1, 0);
  for (int f = 0; f < nf; f++) for (uint32_t c = fo[f]; c < fo[f + 1]; c++) off[vidx[c] + 1]++;  // counts
  for (int v = 0; v < nv; v++) off[v + 1] += off[v];                                            // prefix sum
  std::vector<uint32_t> adj(std::max(1, nc)), pos(off.begin(), off.begin() + nv);
  for (int f = 0; f < nf; f++) for (uint32_t c = fo[f]; c < fo[f + 1]; c++) adj[pos[vidx[c]]++] = uint32_t(f);
  auto offc = env->_pool->acquireChannel(4, nv + 1);
  auto adjc = env->_pool->acquireChannel(4, std::max(1, nc));
  auto up = [&](FxShaderStorageBuffer* b, const std::vector<uint32_t>& vv) {
    auto m = fxi->mapStorageBuffer(b, 0, std::max<size_t>(1, vv.size()) * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, vv.data(), vv.size() * 4);
    fxi->unmapStorageBuffer(m.get());
  };
  up(offc->_ssbo, off); up(adjc->_ssbo, adj);
  mesh->_adj_off = offc; mesh->_adj_face = adjc;
  mesh->_adj_keyV = mesh->_vidx->_ssbo; mesh->_adj_keyF = mesh->_face_offsets->_ssbo;
  return {offc, adjc};
}

// CPU build of a vertex->face CSR (off[nv+1], adj[nc]) directly from a topology already on the host
// (vidx + face_offsets vectors). Same structure vtxFaceAdjacency uploads, but for modules that already
// hold the topology on the CPU (e.g. subdivide builds each level's output here) — no GPU readback.
inline void buildVtxFaceCSR(const std::vector<uint32_t>& vidx, const std::vector<uint32_t>& fo, int nv, int nf,
                            std::vector<uint32_t>& off, std::vector<uint32_t>& adj) {
  off.assign(nv + 1, 0);
  for (int f = 0; f < nf; f++)
    for (uint32_t c = fo[f]; c < fo[f + 1]; c++) off[vidx[c] + 1]++;
  for (int v = 0; v < nv; v++) off[v + 1] += off[v];
  adj.assign(std::max<size_t>(1, off[nv]), 0);
  std::vector<uint32_t> cur(off.begin(), off.end() - 1);
  for (int f = 0; f < nf; f++)
    for (uint32_t c = fo[f]; c < fo[f + 1]; c++) adj[cur[vidx[c]]++] = uint32_t(f);
}

// SHARED smooth-normal gather shader (cs_norm): per OUTPUT vert, accumulate Newell face normals (area-
// weighted, ngon/non-planar robust) + a UV-aligned tangent over its adjacent faces, write N + BINORMAL
// (tangent=cross(N,B)). Used by the Normals module AND subdivide(smooth) (a CC surface must derive its
// normals from the resulting geometry, not from the control cage). Bind order: 0 oP,1 oN,2 oB,3 oUV,
// 4 oVID,5 oFO,6 AOFF,7 ADJ,8 ct{p_nv}. A vert with no adjacent faces (AOFF[o]==AOFF[o+1]) is left as-is.
inline std::string _gatherNormalsText() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface nif_oP (descriptor_set 0) { buffer layout(std430) nopb { vec4 oP[];   }; }
storage_interface nif_oN (descriptor_set 0) { buffer layout(std430) nonb { vec4 oN[];   }; }
storage_interface nif_oB (descriptor_set 0) { buffer layout(std430) nobb { vec4 oB[];   }; }
storage_interface nif_ou (descriptor_set 0) { buffer layout(std430) noub { vec4 oUV[];  }; }
storage_interface nif_vi (descriptor_set 0) { buffer layout(std430) nvib { uint oVID[]; }; }
storage_interface nif_fo (descriptor_set 0) { buffer layout(std430) nfob { uint oFO[];  }; }
storage_interface nif_ao (descriptor_set 0) { buffer layout(std430) naob { uint AOFF[]; }; }
storage_interface nif_aj (descriptor_set 0) { buffer layout(std430) najb { uint ADJ[];  }; }
storage_interface nif_ct (descriptor_set 0) { buffer layout(std430) nctb { uint p_nv; uint p0; uint p1; uint p2; }; }
compute_interface ifn { storage { nif_oP nif_oN nif_oB nif_ou nif_vi nif_fo nif_ao nif_aj nif_ct }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_norm : ifn {
  uint o = gl_GlobalInvocationID.x;
  if (o >= p_nv) { return; }
  uint k0 = AOFF[o]; uint k1 = AOFF[o + 1u];
  if (k0 == k1) { return; }
  vec3 Nacc = vec3(0.0); vec3 Tacc = vec3(0.0);
  for (uint k = k0; k < k1; k++) {
    uint f = ADJ[k];
    uint a = oFO[f]; uint nn = oFO[f + 1u] - a;
    if (nn < 3u) { continue; }
    vec3 fn = vec3(0.0);
    for (uint c = 0u; c < nn; c++) {
      vec3 cur = oP[oVID[a + c]].xyz;
      vec3 nxt = oP[oVID[a + ((c + 1u) % nn)]].xyz;
      fn.x += (cur.y - nxt.y) * (cur.z + nxt.z);
      fn.y += (cur.z - nxt.z) * (cur.x + nxt.x);
      fn.z += (cur.x - nxt.x) * (cur.y + nxt.y);
    }
    vec3 p0 = oP[oVID[a]].xyz; vec3 p1 = oP[oVID[a + 1u]].xyz; vec3 p2 = oP[oVID[a + 2u]].xyz;
    vec2 w0 = oUV[oVID[a]].xy; vec2 w1 = oUV[oVID[a + 1u]].xy; vec2 w2 = oUV[oVID[a + 2u]].xy;
    vec3 dp1 = p1 - p0; vec3 dp2 = p2 - p0; vec2 du1 = w1 - w0; vec2 du2 = w2 - w0;
    float det = du1.x * du2.y - du2.x * du1.y;
    vec3 ft = (abs(det) > 1e-9) ? (dp1 * du2.y - dp2 * du1.y) / det : dp1;
    Nacc += fn; Tacc += ft;
  }
  vec3 N = (length(Nacc) > 1e-12) ? normalize(Nacc) : vec3(0.0, 1.0, 0.0);
  vec3 T = Tacc - N * dot(N, Tacc);
  T = (length(T) > 1e-12) ? normalize(T) : vec3(1.0, 0.0, 0.0);
  vec3 B = cross(T, N);
  oN[o] = vec4(N, 0.0);
  oB[o] = vec4(B, 0.0);
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// MeshScan — the SHARED GPU stream-compaction primitive. Every topology-changing op (delete / mirror /
// future insert / append / compact, and the GPU retrofit of subdivide/extrude/inset) is the same skeleton:
//   count   (OP) per element -> how many outputs it produces (0 = dropped)
//   scan    (THIS) exclusive prefix-sum of counts -> per-element BASE offset + total
//   scatter (OP) each element writes its outputs at its base
// Only the ordered scan is shared (atomicAdd-append is non-contiguous -> breaks the render's CSR, which
// reads FOd[f]..FOd[f+1]). BASE holds n+1 entries: BASE[i] = sum of counts before i, BASE[n] = total —
// so scanning per-face CORNER counts yields the output `face_offsets` CSR directly. SCT = {p_n, total,
// _, _}: p_n host-written PRE-dispatch, total written by the scan (read GPU-side, or read back 1 uint when
// an op needs exact counts). ELEMENT-TYPE / DOMAIN AGNOSTIC (verts / lines / faces / future control-point
// grids). v1 = single-thread serial loop — correct + negligible at our element counts; parallelize
// (Blelloch / multi-block) later if a mesh gets huge.
struct MeshScan {                                            // impl in hmdflow_primitives.cpp
  void init(Context* ctx);
  // BASE must hold (n+1) uints; SCT.x (p_n) host-written before this runs.
  // C.2: pass `n` (the same count as SCT.x) to take the PARALLEL path (3-stage block scan,
  // 256 elements/block-thread, global-memory only -> deterministic, no atomics). Without the hint
  // (or past kMaxBlocks*256 elements) the serial single-thread kernel runs — identical results.
  void scan(Context* ctx, FxShaderStorageBuffer* cnt, FxShaderStorageBuffer* base, FxShaderStorageBuffer* sct, int n = -1);
  static constexpr int kMaxBlocks = 131072;                  // parallel-path cap: 131072*256 = 33.5M elements
                                                             // (covers sdf_to_mesh up to ~dim 322; the bbase
                                                             // middle stage serial-scans only N/256 block-sums,
                                                             // so a higher cap stays cheap. _bsum = 512KB.)
  const FxComputeShader* _cs       = nullptr;                // serial fallback
  const FxComputeShader* _cs_bsum  = nullptr;                // per-block partial sums
  const FxComputeShader* _cs_bbase = nullptr;                // block-sum exclusive scan (+ total publish)
  const FxComputeShader* _cs_apply = nullptr;                // per-block prefix emit
  FxShaderStorageBuffer* _bsum     = nullptr;                // fixed scratch (init-time; never mid-phase)
};

///////////////////////////////////////////////////////////////////////////////
// MeshSort — SHARED GPU-native STABLE sort of (key,payload) uint pairs by key. The foundational primitive
// under group-by-key: edge enumeration (dedup directed edges), manifold weld (group coincident verts),
// any ordered grouping. STABLE (1-bit LSB radix, scan-based scatter — not atomics) => DETERMINISTIC,
// matching the bake-determinism contract. 32 passes; each = flag(bit) -> scan(flags) -> scatter, with the
// per-pass bit advanced by a GPU counter (cs_incpass) so NOTHING is host-written mid-dispatch. Result
// lands back in the caller's key/payload buffers (32 = even # of ping-pong swaps). No readback. Built on
// MeshScan, so when the scan parallelizes (P2) the sort speeds up with ZERO caller change — the 1-bit
// radix is an internal width knob, not the API. ensure() (PRE phase: alloc scratch + host-reset p_n/pass);
// sort() (dispatch phase: the 32-pass loop).
struct MeshSort {                                            // impl in hmdflow_primitives.cpp
  void init(Context* ctx);
  void ensure(Context* ctx, int n);                          // PRE phase: alloc scratch to n (CAPACITY) + host-reset p_n/pass
  void sort(Context* ctx, FxShaderStorageBuffer* key, FxShaderStorageBuffer* payload);  // dispatch: 32-pass sort
  // GPU-DRIVEN LIVE SIZING (M2.5): overwrite the active element count (p_n in _ct + _sct) from a GPU
  // value at SRC[0] — every sort/scan kernel early-outs on p_n, and the serial scan stage derives its
  // block count from it, so the WORK shrinks to the live count with the scratch still alloc'd to capacity
  // and NO host readback. Call in the dispatch phase AFTER ensure() (which reset p_pass), BEFORE sort().
  void setCount(Context* ctx, FxShaderStorageBuffer* src); // dispatch: _ct.p_n = _sct.p_n = SRC[0]
  MeshScan _scan;
  FxShaderStorageBuffer *_dk = nullptr, *_dp = nullptr, *_flg = nullptr, *_base = nullptr, *_ct = nullptr, *_sct = nullptr;
  const FxComputeShader *_cs_flag = nullptr, *_cs_scatter = nullptr, *_cs_incpass = nullptr, *_cs_setcount = nullptr;
  int _n = -1;
};

///////////////////////////////////////////////////////////////////////////////
// MeshEdges — SHARED GPU-native edge enumeration: from a mesh's topology (vidx corner->vert + face_offsets
// CSR), build the UNIQUE undirected edge table with adjacency, with NO readback. The foundation for edge
// (LINE) selection and bevel. Pipeline (all GPU, on MeshSort + MeshScan):
//   cornerface : CFACE[c] = which face owns corner c (per-face inner loop over its corner span)
//   emit       : per corner c emit one directed edge -> KEY[c] = (min(va,vb)<<16)|max(va,vb)  [verts<65536],
//                PAY[c] = c. (canonical key => the two half-edges of a shared edge collide.)
//   SORT       : MeshSort(KEY,PAY) -> equal keys (= same undirected edge) become adjacent, stable.
//   mark       : ISTART[i] = 1 at each run start (KEY[i] != KEY[i-1]).
//   SCAN       : exclusive prefix of ISTART -> PRE; PRE[nc] = numEdges; PRE[i] at a run start = that edge's id.
//   build      : at each run start i, write EDGE[E] = {va, vb, f0, f1}; f1=~0 if boundary (run length 1),
//                else CFACE of the run's 2nd half-edge. EN(numEdges) published GPU-side (consumers early-out
//                on it; no readback). EDGE is 4 uints/edge; midpoint/length/dihedral are derived at use time
//                from va/vb/f0/f1 + the P/N channels (minimal table). Counts come from the CPU-side live
//                _num_corners/_num_faces (the upstream populates them) -> exact sizing, sentinel-free.
// Per the shadlang trap (see normals): EACH stage is its OWN shader module with an exact-fit iface.
struct MeshEdges {                                           // impl in hmdflow_primitives.cpp
  static constexpr uint32_t kNoFace = 0xFFFFFFFFu;           // EDGE[..+3] sentinel: edge is a boundary
  void init(Context* ctx);
  void ensure(Context* ctx, int nc, int nf);                 // PRE phase: alloc to nc/nf + host count block
  void build(Context* ctx, gpumesh_ptr_t in);                // dispatch: enumerate the unique-edge table
  // RAW form (C.4): enumerate from bare topology buffers — for consumers whose source isn't a GpuMesh
  // (subdivide's level cascade chains its own per-level vidx/fo). NO topology-key caching (the caller
  // decides when to rebuild); counts come from the preceding ensure().
  void buildRaw(Context* ctx, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* fo, int nv);
  MeshSort _sort;
  MeshScan _scan;
  FxShaderStorageBuffer *_cface = nullptr, *_key = nullptr, *_pay = nullptr, *_istart = nullptr, *_pre = nullptr,
                        *_edge = nullptr, *_corner_edge = nullptr, *_ectl = nullptr, *_sct = nullptr;
  const FxComputeShader *_cs_cf = nullptr, *_cs_em = nullptr, *_cs_mk = nullptr, *_cs_bd = nullptr;
  int _nc = -1, _nf = -1;
  // C.1c topology-keyed cache (see build()): the source topo key the current EDGE table was built
  // from; build() early-outs while it stands (the table is connectivity-pure).
  uint64_t _builtTopoV = ~0ull;
  int _bnc = -1, _bnf = -1;
  const void* _bvidx = nullptr;
  const void* _bfo   = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// FaceTagger — generic per-face __tags retag pass shared by the topology ops (generators / modifiers).
// Each output face INHERITS its PARENT input face's tags (or 0 for a brand-new face / a generator),
// then has its PARTITION's MaskOp applied:  tags_out = ((inherited & AND) | OR) ^ XOR. So a Select made
// upstream survives a topology op, and each op partition (extrude wall/cap/base, a generator sub-part,
// a subdivide child set) can add/clear/isolate groups. The partition MaskOps are RUNTIME (re-uploaded
// each frame) — tweaking/animating them never recompiles the shell. Bind by slot (compute-only SSBOs).
///////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t kFaceNoParent = 0xFFFFFFFFu;  // a brand-new face (no parent -> inherits 0)
static constexpr int kMaxFaceParts = 8;                  // max partitions per op (PRTd values 0..7)
enum FacePart : uint32_t { kPartBase = 0, kPartCap = 1, kPartWall = 2 };  // extrude partition ids

struct FaceTagger {
  void build(Context* ctx) {
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_retag", _text());
    _cs      = fxi->computeShader(sh, "cs_retag");
    _masks   = fxi->createStorageBuffer(16 + kMaxFaceParts * 16); // (nf,K,pad,pad) + 4 uints per part
  }
  // upload nf + the partition triples ([and,or,xor] each, in partition-id order); a missing/short entry
  // is identity (pure inheritance: AND=~0, OR=0, XOR=0).
  void setMasks(Context* ctx, int nf, const std::vector<uint32_t>& triples) {
    std::vector<uint32_t> buf(4 + kMaxFaceParts * 4, 0);
    buf[0] = uint32_t(nf);
    buf[1] = uint32_t(std::max(1, int(triples.size() / 3)));
    for (int p = 0; p < kMaxFaceParts; p++) {
      uint32_t a = 0xFFFFFFFFu, o = 0u, x = 0u;   // identity
      if (p * 3 + 2 < int(triples.size())) { a = triples[p*3]; o = triples[p*3+1]; x = triples[p*3+2]; }
      buf[4 + p*4 + 0] = a; buf[4 + p*4 + 1] = o; buf[4 + p*4 + 2] = x;
    }
    auto m = ctx->FXI()->mapStorageBuffer(_masks, 0, buf.size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, buf.data(), buf.size() * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
  }
  // out tags[f] <- transform( in tags[parent[f]], masks[part[f]] ). `in` may be null (no parent map -> 0).
  void dispatch(Context* ctx, FxShaderStorageBuffer* out, FxShaderStorageBuffer* in,
                FxShaderStorageBuffer* parent, FxShaderStorageBuffer* part, int nf) {
    auto ci = ctx->CI();
    ci->bindStorageBuffer(_cs, 0, out);
    ci->bindStorageBuffer(_cs, 1, in ? in : out);  // dummy when there's no parent (TI is then unread)
    ci->bindStorageBuffer(_cs, 2, parent);
    ci->bindStorageBuffer(_cs, 3, part);
    ci->bindStorageBuffer(_cs, 4, _masks);
    ci->dispatchCompute(_cs, (nf + 63) / 64, 1, 1);
  }
  static const char* _text() {
    return R"S(
fxconfig fxcfg_default {}
storage_interface sif_to (descriptor_set 0) { buffer layout(std430) tob { uint TO[]; }; }     // out tags
storage_interface sif_ti (descriptor_set 0) { buffer layout(std430) tib { uint TI[]; }; }     // in tags
storage_interface sif_pr (descriptor_set 0) { buffer layout(std430) prb { uint PARd[]; }; }   // parent face id
storage_interface sif_pt (descriptor_set 0) { buffer layout(std430) ptb { uint PRTd[]; }; }   // partition id
storage_interface sif_mk (descriptor_set 0) { buffer layout(std430) mkb {
  uint p_nf; uint p_K; uint p0; uint p1; uint Md[]; }; }                                       // (and,or,xor,_)/part
compute_interface iface { storage { sif_to sif_ti sif_pr sif_pt sif_mk }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_retag : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint par  = PARd[f];
  uint base = (par == 0xFFFFFFFFu) ? 0u : TI[par];   // inherit parent tags (0 if brand-new)
  uint pi   = PRTd[f] * 4u;
  uint nt   = ((base & Md[pi + 0u]) | Md[pi + 1u]) ^ Md[pi + 2u];
  TO[f] = (base & 0xFFF00000u) | (nt & 0x000FFFFFu);  // gid [20:32) LOCKED: inherit-only; assign_gid is the sole gid write verb
}
)S";
  }
  const FxComputeShader* _cs    = nullptr;
  FxShaderStorageBuffer* _masks = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// MeshComputeInst — shared base for the GPU mesh compute ops.
///////////////////////////////////////////////////////////////////////////////

struct MeshComputeInst : public dflow::DgModuleInst, public dflowgfx::IPrePhaseParams {
  MeshComputeInst(const dflow::DgModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g), _meshData(dynamic_cast<const MeshModuleData*>(d)) {
  }
  const MeshModuleData* _meshData = nullptr;   // for the precheck/postcheck flags (every mesh module carries them)

  // ---- self-defending PRE/POSTCONDITION checks (TEMPLATE METHOD). preCheck/postCheck are the non-virtual
  // skeleton: gate on the module's _precheck/_postcheck flags, then call the op's hook. Each op overrides
  // _doPreCheck / _doPostCheck with its specific checks (via the meshcheck library); default hooks are no-ops
  // (an op with no preconditions needs nothing). Ops call preCheck(ctx) before building, postCheck(ctx) after. ----
  void preCheck(Context* ctx)  { if (_meshData and _meshData->_precheck)  _doPreCheck(ctx); }
  void postCheck(Context* ctx) { if (_meshData and _meshData->_postcheck) _doPostCheck(ctx); }
  virtual void _doPreCheck(Context* ctx) {}
  virtual void _doPostCheck(Context* ctx) {}

  // ---- topology DIRTY propagation (the mesh's dirty signal; see GpuMesh::_topoVersion). A CONSUMER rebuilds
  // its topology when its input mesh's _topoVersion advanced past the one it last built from; a PRODUCER calls
  // markOutTopo() whenever it re-emits connectivity (vidx/face_offsets/counts), so the next consumer goes dirty
  // and the whole chain (extrude -> face_normals -> render/wireframe) recomputes. ack after a (re)build. This is
  // hypermesh-local — particles/terrain don't consume it, so their full-recompute-every-frame is unchanged. ----
  uint64_t _builtSrcTopoV = ~0ull;
  bool srcTopoDirty(mesh_inpluginst_ptr_t inp) const {
    auto in = _srcMesh(inp);
    return in && in->_topoVersion != _builtSrcTopoV;
  }
  void ackSrcTopo(mesh_inpluginst_ptr_t inp) {
    auto in = _srcMesh(inp);
    if (in) _builtSrcTopoV = in->_topoVersion;
  }
  static void markOutTopo(mesh_outpluginst_ptr_t out) { if (out and out->_value) out->_value->markTopoChanged(); }
  static void markOut(mesh_outpluginst_ptr_t out)     { if (out and out->_value) out->_value->markChanged(); }

  // PRE-dispatch-phase host write of any runtime params (read from the module's uniform plugs).
  // A host map MID dispatch-phase is invisible, so the driver calls this BEFORE beginDispatchPhase;
  // compute() then dispatches reading them. Default no-op (modules that bake everything need none).
  // The hook itself is the FAMILY-NEUTRAL dflowgfx::IPrePhaseParams (drivers cast to THAT, so
  // terrain field modules riding a mesh graph get the same per-frame host write).
  void writeParams(Context* ctx) override {}

  // called by the driver ONCE, after the first full graph eval + endFrame (so any producer's GPU
  // topology is computed + host-readable). Modules that need a connected mesh's topology on the CPU
  // (subdivide reads back vidx/face_offsets to build shared-edge-midpoint tables) do it here. Returns
  // true if the graph must be re-evaluated afterward (the module produced nothing useful on eval 1).
  virtual bool onTopologyReady(Context* ctx) { return false; }

  // ---- cook cache (E.6/2.19 — the terrain pattern brought to meshes; engaged only when the
  // GraphData is _cacheable, which the DSL sets for STATIC graphs). Node identity is CONTENT-ONLY
  // (hypermeshModuleIdentityHash: reflected state with uuid envelopes stripped) + a per-class
  // KERNEL-VERSION salt + the upstream Merkle hashes. cookStore serializes the node's full output
  // mesh (counts + vertex/named/face channels + vidx/face_offsets + header blob) after the driver's
  // final sync; cookLoad re-acquires pooled buffers + uploads, so downstream consumes without this
  // node ever computing. Edge tables / adjacency are NOT stored (lazy caches — consumers rebuild). ----
  // BUMP the salt when THIS op's kernel logic changes (the hash can't see shader text).
  virtual const char* _cookSalt() const { return "v1"; }
  uint64_t cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const override;
  datablock_ptr_t cookStore() const override;
  bool cookLoad(datablock_constptr_t db) override;
  // the node's mesh output (resolved by name so we don't shadow each derived inst's own _output).
  gpumesh_ptr_t _outMesh() const {
    auto self = const_cast<MeshComputeInst*>(this);
    auto outp = self->typedOutputNamed<MeshPlugTraits>("Out");
    return outp ? outp->_value : nullptr;
  }

  // (re)acquire pooled VERTEX channels + topology buffers (vidx, face_offsets) + header for the
  // INDEXED mesh, sized to (num_verts, num_corners, num_faces). Re-assigning the COW handles drops
  // the previous buffers back to their pow2 pools — so this is safe to call every frame for ops
  // whose counts change (dynamic topology). FACE-domain channels are acquired by the module itself.
  void allocMesh(
      meshenv_ptr_t env,
      gpumesh_ptr_t mesh,
      int num_verts,
      int num_corners,
      int num_faces,
      std::initializer_list<MeshChannel> chans) {
    mesh->_num_verts   = num_verts;
    mesh->_num_corners = num_corners;
    mesh->_num_faces   = num_faces;
    mesh->_capacity    = meshNextPow2(num_verts);
    for (auto c : chans)
      mesh->_channels[c] = env->_pool->acquireChannel(c, num_verts);
    mesh->_vidx         = env->_pool->acquireChannel(4, num_corners);
    mesh->_face_offsets = env->_pool->acquireChannel(4, num_faces + 1);
    if (not mesh->_header)
      mesh->_header = env->_pool->acquire(kMeshHeaderBytes, 1);
    mesh->markTopoChanged(); // re-emission IS the dirty signal (the documented _topoVersion contract);
  }                          // auto-bump here so no module can forget (ops self-defend)

  // GENERATOR / whole-mesh face tagging (no parent -> no FaceTagger needed). Host-fill `mesh`'s __tags
  // FACE channel with each face's partition mask VALUE (base 0 -> ((0&AND)|OR)^XOR = OR^XOR), and set
  // mesh->_faces["__tags"]. `masks` = per-partition triples [and,or,xor]; `parts[f]` selects the
  // partition (empty -> whole-mesh partition 0). RUNTIME: re-derived from the current masks (dirty-
  // checked re-upload) -> no recompile when a mask changes. No-op when masks is empty (stays untagged).
  void tagFacesConst(Context* ctx, gpumesh_ptr_t mesh, const std::vector<uint32_t>& masks,
                     const std::vector<uint32_t>& parts = {}) {
    if (masks.empty()) return;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    int nf   = mesh->_num_faces;
    bool repool = (meshNextPow2(nf) != _ftcap);
    if (repool) { _fttags = env->_pool->acquireChannel(4, nf); _ftcap = meshNextPow2(nf); }
    if (repool or nf != _ftnf or masks != _ftmask or parts != _ftparts) {  // dirty -> refill
      std::vector<uint32_t> vals(nf);
      int K = int(masks.size() / 3);
      for (int f = 0; f < nf; f++) {
        int p = (f < int(parts.size())) ? int(parts[f]) : 0;
        if (p >= K) p = 0;
        vals[f] = masks[p * 3 + 1] ^ masks[p * 3 + 2];
      }
      auto m = ctx->FXI()->mapStorageBuffer(_fttags->_ssbo, 0, size_t(nf) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, vals.data(), size_t(nf) * 4);
      ctx->FXI()->unmapStorageBuffer(m.get());
      _ftnf = nf; _ftmask = masks; _ftparts = parts;
    }
    mesh->_faces["__tags"] = _fttags;
  }
  gpuchannel_ptr_t _fttags;
  int _ftcap = -1, _ftnf = -1;
  std::vector<uint32_t> _ftmask, _ftparts;
};

} // namespace ork::lev2::hypermesh
