////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
// Implementations of the SHARED GPU dataflow primitives declared in hmdflow_module.h:
//   MeshScan  — serial exclusive prefix-sum (the stream-compaction scan)
//   MeshSort  — stable, deterministic 1-bit-radix (key,payload) sort, built on MeshScan
//   MeshEdges — GPU-native unique-edge enumeration, built on MeshSort + MeshScan
// The shader text + method bodies live here (not the header) so they compile once.
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <algorithm>
#include <cstring>
#include <utility>
#include <unordered_map>

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// meshcheck — shared pre/postcondition checks (declared in hmdflow_module.h). Readback-based; factual.
///////////////////////////////////////////////////////////////////////////////
namespace meshcheck {

// ---- CPU cores ----
int coincidentVerts(const float* p4, int nv) {
  std::vector<uint32_t> remap, reps;
  return nv - weldVerts(p4, nv, remap, reps);               // verts collapsed onto an earlier rep = coincident (shared weld key)
}

EdgeHealth edgeHealth(const float* p4, int nv, const uint32_t* vidx, const uint32_t* faceoff, int nf) {
  std::vector<uint32_t> remap, reps;
  weldVerts(p4, nv, remap, reps);                          // WELD BY POSITION: un-welded split seams collapse so we
  std::unordered_map<uint64_t, int> ec;                    // measure REAL holes, not coincident-but-separate index seams
  for (int f = 0; f < nf; f++) {
    uint32_t a = faceoff[f], b = faceoff[f + 1], deg = b - a;
    for (uint32_t k = 0; k < deg; k++) {
      uint32_t v0 = remap[vidx[a + k]], v1 = remap[vidx[a + (k + 1) % deg]];
      if (v0 == v1) continue;                              // edge collapsed by the weld -> not a boundary
      ec[(uint64_t(std::min(v0, v1)) << 32) | std::max(v0, v1)]++;
    }
  }
  EdgeHealth h;
  for (auto& kv : ec) { if (kv.second == 1) h.boundary++; else if (kv.second > 2) h.nonmanifold++; }
  return h;
}

int degenerateFaces(const float* p4, const uint32_t* vidx, const uint32_t* faceoff, int nf) {
  auto P = [&](uint32_t c) { uint32_t v = vidx[c]; return fvec3(p4[4 * v], p4[4 * v + 1], p4[4 * v + 2]); };
  int degen = 0;
  for (int f = 0; f < nf; f++) {
    uint32_t a = faceoff[f], b = faceoff[f + 1], deg = b - a;
    if (deg < 3) { degen++; continue; }                     // not a polygon
    fvec3 p0 = P(a); float area = 0.0f;                     // fan area from corner 0
    for (uint32_t k = 1; k + 1 < deg; k++) area += 0.5f * (P(a + k) - p0).crossWith(P(a + k + 1) - p0).length();
    if (area < 1e-9f) degen++;                              // collinear / collapsed -> garbage frame
  }
  return degen;
}

int windingFlips(const float* p4, int nv, const uint32_t* vidx, const uint32_t* faceoff, int nf) {
  std::vector<uint32_t> remap, reps;
  weldVerts(p4, nv, remap, reps);                          // WELD BY POSITION so split-vert hard edges match up
  std::unordered_map<uint64_t, int> dir;                   // DIRECTED edge v0->v1 -> # faces traversing it that way
  for (int f = 0; f < nf; f++) {
    uint32_t a = faceoff[f], b = faceoff[f + 1], deg = b - a;
    for (uint32_t k = 0; k < deg; k++) {
      uint32_t v0 = remap[vidx[a + k]], v1 = remap[vidx[a + (k + 1) % deg]];
      if (v0 == v1) continue;                              // collapsed by the weld
      dir[(uint64_t(v0) << 32) | v1]++;
    }
  }
  int flips = 0;                                           // an edge co-traversed by >=2 faces => they wind it the
  for (auto& kv : dir) if (kv.second >= 2) flips++;        // SAME way => one is inverted (consistent => 1 each way)
  return flips;
}

int coplanarOverlaps(const float* p4, const uint32_t* vidx, const uint32_t* faceoff, int nf) {
  auto P = [&](uint32_t c) { uint32_t v = vidx[c]; return fvec3(p4[4 * v], p4[4 * v + 1], p4[4 * v + 2]); };
  struct Tri { fvec3 p0, p1, p2, n; float off; uint32_t v[3]; };
  std::vector<Tri> tr;
  for (int f = 0; f < nf; f++) {                              // fan-triangulate every face
    uint32_t a = faceoff[f], b = faceoff[f + 1], deg = b - a;
    if (deg < 3) continue;
    fvec3 q0 = P(a), fn = (P(a + 1) - q0).crossWith(P(a + 2) - q0); float L = fn.length();
    if (L < 1e-12f) continue; fn = fn * (1.0f / L);
    for (uint32_t k = 1; k + 1 < deg; k++)
      tr.push_back({q0, P(a + k), P(a + k + 1), fn, q0.dotWith(fn), {vidx[a], vidx[a + k], vidx[a + k + 1]}});
  }
  auto sat = [](const fvec2 A[3], const fvec2 B[3]) -> bool {  // 2D triangle interiors overlap? (separating-axis)
    for (int s = 0; s < 2; s++) { const fvec2* T = s ? B : A;
      for (int i = 0; i < 3; i++) {
        fvec2 e = T[(i + 1) % 3] - T[i]; fvec2 ax(-e.y, e.x);
        float a0 = 1e30f, a1 = -1e30f, b0 = 1e30f, b1 = -1e30f;
        for (int j = 0; j < 3; j++) { float pa = A[j].dotWith(ax), pb = B[j].dotWith(ax);
          a0 = std::min(a0, pa); a1 = std::max(a1, pa); b0 = std::min(b0, pb); b1 = std::max(b1, pb); }
        if (a0 >= b1 - 1e-7f or b0 >= a1 - 1e-7f) return false; // a separating axis -> no overlap
      }
    }
    return true;
  };
  int overlaps = 0, n = int(tr.size());
  for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
    if (tr[i].n.dotWith(tr[j].n) < 0.999f) continue;          // not coplanar (same-orientation planes only)
    if (std::fabs(tr[i].off - tr[j].off) > 1e-4f) continue;   // not the same plane
    int sh = 0; for (int x = 0; x < 3; x++) for (int y = 0; y < 3; y++) if (tr[i].v[x] == tr[j].v[y]) sh++;
    if (sh >= 2) continue;                                    // edge-adjacent (share an edge) -> not an overlap
    fvec3 nn = tr[i].n, u = (std::fabs(nn.x) < 0.9f) ? fvec3(1, 0, 0) : fvec3(0, 1, 0);
    u = u - nn * u.dotWith(nn); u = u * (1.0f / u.length()); fvec3 vv = nn.crossWith(u);
    auto pr = [&](fvec3 p) { return fvec2(p.dotWith(u), p.dotWith(vv)); };
    fvec2 A[3] = {pr(tr[i].p0), pr(tr[i].p1), pr(tr[i].p2)}, B[3] = {pr(tr[j].p0), pr(tr[j].p1), pr(tr[j].p2)};
    if (sat(A, B)) overlaps++;
  }
  return overlaps;
}

// ---- GPU wrappers (read back, then CPU core) ----
int coincidentVerts(Context* ctx, FxShaderStorageBuffer* pos, int nv) {
  auto m = ctx->FXI()->mapStorageBuffer(pos, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
  int r = coincidentVerts(reinterpret_cast<const float*>(m->_mappedaddr), nv);
  ctx->FXI()->unmapStorageBuffer(m.get());
  return r;
}

int coplanarOverlaps(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf) {
  auto rdu = [&](FxShaderStorageBuffer* buf, int n) {
    std::vector<uint32_t> v(std::max(1, n));
    auto m = ctx->FXI()->mapStorageBuffer(buf, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
    return v;
  };
  auto mp = ctx->FXI()->mapStorageBuffer(pos, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
  auto V = rdu(vidx, nc);
  auto F = rdu(faceoff, nf + 1);
  int r = coplanarOverlaps(reinterpret_cast<const float*>(mp->_mappedaddr), V.data(), F.data(), nf);
  ctx->FXI()->unmapStorageBuffer(mp.get());
  return r;
}

int windingFlips(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf) {
  auto rdu = [&](FxShaderStorageBuffer* buf, int n) {
    std::vector<uint32_t> v(std::max(1, n));
    auto m = ctx->FXI()->mapStorageBuffer(buf, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
    return v;
  };
  auto mp = ctx->FXI()->mapStorageBuffer(pos, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
  auto V = rdu(vidx, nc);
  auto F = rdu(faceoff, nf + 1);
  int r = windingFlips(reinterpret_cast<const float*>(mp->_mappedaddr), nv, V.data(), F.data(), nf);
  ctx->FXI()->unmapStorageBuffer(mp.get());
  return r;
}

EdgeHealth edgeHealth(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf) {
  auto rd = [&](FxShaderStorageBuffer* buf, int n) {
    std::vector<uint32_t> v(std::max(1, n));
    auto m = ctx->FXI()->mapStorageBuffer(buf, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
    return v;
  };
  auto mp = ctx->FXI()->mapStorageBuffer(pos, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
  auto V = rd(vidx, nc);
  auto F = rd(faceoff, nf + 1);
  EdgeHealth h = edgeHealth(reinterpret_cast<const float*>(mp->_mappedaddr), nv, V.data(), F.data(), nf);
  ctx->FXI()->unmapStorageBuffer(mp.get());
  return h;
}

int degenerateFaces(Context* ctx, FxShaderStorageBuffer* pos, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* faceoff, int nv, int nc, int nf) {
  auto rdu = [&](FxShaderStorageBuffer* buf, int n) {
    std::vector<uint32_t> v(std::max(1, n));
    auto m = ctx->FXI()->mapStorageBuffer(buf, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
    return v;
  };
  auto mp = ctx->FXI()->mapStorageBuffer(pos, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
  auto V = rdu(vidx, nc);
  auto F = rdu(faceoff, nf + 1);
  int r = degenerateFaces(reinterpret_cast<const float*>(mp->_mappedaddr), V.data(), F.data(), nf);
  ctx->FXI()->unmapStorageBuffer(mp.get());
  return r;
}

// ---- asserts (factual) ----
void assertWelded(int coincident, int nv, const char* op) {
  OrkAssertIFMT(coincident == 0,
    "[hypermesh %s] PRECONDITION failed: input is NOT welded — %d of %d verts are coincident-but-separate (same "
    "position, distinct index). This op needs a welded/manifold mesh. (smooth_normals welds; face_normals SPLITS.)",
    op, coincident, nv);
}

void assertNoDegenerateFaces(int degen, int nf, const char* op) {
  OrkAssertIFMT(degen == 0,
    "[hypermesh %s] PRECONDITION failed: %d of %d input faces are degenerate (< 3 verts or ~0 area). This op builds "
    "a per-face frame; a degenerate face yields a garbage frame.",
    op, degen, nf);
}

void assertNoCreatedDefects(const EdgeHealth& in, const EdgeHealth& out, const char* op) {
  OrkAssertIFMT(out.boundary <= in.boundary && out.nonmanifold <= in.nonmanifold,
    "[hypermesh %s] POSTCONDITION failed: output CREATED %d open (boundary) + %d non-manifold edges the input did "
    "NOT have (input %d/%d -> output %d/%d). The op must return a mesh no less manifold/closed than its input.",
    op, std::max(0, out.boundary - in.boundary), std::max(0, out.nonmanifold - in.nonmanifold),
    in.boundary, in.nonmanifold, out.boundary, out.nonmanifold);
}

void assertWindingConsistent(int flips, const char* op, const char* phase) {
  OrkAssertIFMT(flips == 0,
    "[hypermesh %s] %s failed: %d edge(s) are co-directed by 2+ faces => %d inverted (backwards-wound / "
    "negative-area) face(s). The mesh is not consistently wound; a flipped face renders inside-out and breaks "
    "downstream normals/lighting. (edgeHealth/manifold checks DON'T catch winding — an inverted face is still watertight.)",
    op, phase, flips, flips);
}

void assertNoOverlaps(int overlaps, const char* op, const char* phase) {
  OrkAssertIFMT(overlaps == 0,
    "[hypermesh %s] %s failed: %d coplanar triangle pair(s) OVERLAP (geometric self-intersection in the face "
    "plane). The mesh is malformed even though it is edge-manifold/watertight/consistently-wound — overlapping "
    "faces z-fight and the tessellation is not a valid surface. (watertight is TOPOLOGICAL; overlap is GEOMETRIC.)",
    op, phase, overlaps);
}

} // namespace meshcheck

///////////////////////////////////////////////////////////////////////////////
// MeshScan
///////////////////////////////////////////////////////////////////////////////

static std::string _scan_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface scf_cnt (descriptor_set 0) { buffer layout(std430) sccb  { uint CNT[];  }; }
storage_interface scf_bse (descriptor_set 0) { buffer layout(std430) scbb  { uint BASE[]; }; }
storage_interface scf_ct  (descriptor_set 0) { buffer layout(std430) scctb { uint p_n; uint total; uint s2; uint s3; }; }
storage_interface scf_bs  (descriptor_set 0) { buffer layout(std430) scbsb { uint BSUM[]; }; }
// ONE shared iface for all scan kernels (every kernel binds all 4 slots — the FaceTagger/triangulate
// pattern; avoids the shadlang subset-binding trap).
compute_interface iface { storage { scf_cnt scf_bse scf_ct scf_bs } inputs { layout(local_size_x = 64); } }
compute_shader cs_scan : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }    // serial fallback (tiny n / no n hint / over-cap)
  uint acc = 0u;
  for (uint i = 0u; i < p_n; i++) { BASE[i] = acc; acc += CNT[i]; }
  BASE[p_n] = acc;                                   // CSR final entry == total
  total = acc;
}
////////////////////////////////////////
// C.2 parallel path (3 stages, global memory only — no shared mem / no in-shader barriers, so it is
// trivially DETERMINISTIC: fixed iteration order, no atomics). Block = 256 elements per thread.
//   bsum  : thread b serially sums its block          -> BSUM[b]
//   bbase : single thread exclusive-scans BSUM in place (nblocks iters — tiny), publishes total/BASE[n]
//   apply : thread b re-walks its block emitting BASE[i] = scanned BSUM[b] + local prefix
compute_shader cs_scan_bsum : iface {
  uint b       = gl_GlobalInvocationID.x;
  uint nblocks = (p_n + 255u) / 256u;
  if (b >= nblocks) { return; }
  uint i0 = b * 256u;
  uint i1 = min(i0 + 256u, p_n);
  uint s  = 0u;
  for (uint i = i0; i < i1; i++) { s += CNT[i]; }
  BSUM[b] = s;
}
compute_shader cs_scan_bbase : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint nblocks = (p_n + 255u) / 256u;
  uint acc = 0u;
  for (uint b = 0u; b < nblocks; b++) { uint t = BSUM[b]; BSUM[b] = acc; acc += t; }
  BASE[p_n] = acc;
  total     = acc;
}
compute_shader cs_scan_apply : iface {
  uint b       = gl_GlobalInvocationID.x;
  uint nblocks = (p_n + 255u) / 256u;
  if (b >= nblocks) { return; }
  uint i0  = b * 256u;
  uint i1  = min(i0 + 256u, p_n);
  uint acc = BSUM[b];
  for (uint i = i0; i < i1; i++) { BASE[i] = acc; acc += CNT[i]; }
}
)S";
}

void MeshScan::init(Context* ctx) {
  if (_cs) return;
  auto fxi  = ctx->FXI();
  auto sh   = fxi->shaderFromShaderText("hypermesh_scan", _scan_text());
  _cs       = fxi->computeShader(sh, "cs_scan");
  _cs_bsum  = fxi->computeShader(sh, "cs_scan_bsum");
  _cs_bbase = fxi->computeShader(sh, "cs_scan_bbase");
  _cs_apply = fxi->computeShader(sh, "cs_scan_apply");
  // FIXED block-sum scratch (init-time — scan() runs INSIDE a dispatch phase, so it must never
  // allocate). kMaxBlocks * 256 elements = the parallel-path cap; beyond it (or with no n hint)
  // the serial kernel handles any n correctly, just slowly.
  _bsum = fxi->createStorageBuffer(size_t(kMaxBlocks) * 4, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
}
void MeshScan::scan(Context* ctx, FxShaderStorageBuffer* cnt, FxShaderStorageBuffer* base, FxShaderStorageBuffer* sct, int n) {
  auto ci   = ctx->CI();
  auto bind = [&](const FxComputeShader* cs) {
    ci->bindStorageBuffer(cs, 0, cnt);
    ci->bindStorageBuffer(cs, 1, base);
    ci->bindStorageBuffer(cs, 2, sct);
    ci->bindStorageBuffer(cs, 3, _bsum);
  };
  // C.2: the parallel path needs HOST-side dispatch sizing, so it runs only when the caller passes
  // its n (the API's optional hint — all callers compile unchanged). Identical results either way:
  // exclusive prefix in fixed order, no atomics, deterministic. ORK_HM_SCAN_SERIAL=1 forces the
  // serial kernel (debug/measurement escape hatch).
  static const bool s_force_serial = (getenv("ORK_HM_SCAN_SERIAL") != nullptr);
  int nblocks = (n + 255) / 256;
  if (n < 0 or nblocks > kMaxBlocks or s_force_serial) {
    bind(_cs);
    ci->dispatchCompute(_cs, 1, 1, 1);
    return;
  }
  int groups = std::max(1, (nblocks + 63) / 64);
  bind(_cs_bsum);
  ci->dispatchCompute(_cs_bsum, groups, 1, 1);
  ci->storageBarrier();
  bind(_cs_bbase);
  ci->dispatchCompute(_cs_bbase, 1, 1, 1);
  ci->storageBarrier();
  bind(_cs_apply);
  ci->dispatchCompute(_cs_apply, groups, 1, 1);
}

///////////////////////////////////////////////////////////////////////////////
// MeshSort
///////////////////////////////////////////////////////////////////////////////

static std::string _sort_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sof_sk (descriptor_set 0) { buffer layout(std430) sosk { uint SK[]; }; }
storage_interface sof_sp (descriptor_set 0) { buffer layout(std430) sosp { uint SP[]; }; }
storage_interface sof_dk (descriptor_set 0) { buffer layout(std430) sodk { uint DK[]; }; }
storage_interface sof_dp (descriptor_set 0) { buffer layout(std430) sodp { uint DP[]; }; }
storage_interface sof_fl (descriptor_set 0) { buffer layout(std430) sofl { uint FLG[]; }; }
storage_interface sof_bs (descriptor_set 0) { buffer layout(std430) sobs { uint BASE[]; }; }
storage_interface sof_ct (descriptor_set 0) { buffer layout(std430) soct { uint p_n; uint p_pass; uint s2; uint s3; }; }
compute_interface iface { storage { sof_sk sof_sp sof_dk sof_dp sof_fl sof_bs sof_ct }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_flag : iface {                           // FLG[i] = bit p_pass of SK[i]
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_n) { return; }
  FLG[i] = (SK[i] >> p_pass) & 1u;
}
compute_shader cs_scatter : iface {                        // stable split: 0s keep order, then 1s keep order
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_n) { return; }
  uint ones_before = BASE[i];
  uint total0      = p_n - BASE[p_n];
  uint pos = (FLG[i] == 0u) ? (i - ones_before) : (total0 + ones_before);
  DK[pos] = SK[i];
  DP[pos] = SP[i];
}
compute_shader cs_incpass : iface {                        // advance the radix bit (GPU-side; no host write)
  if (gl_GlobalInvocationID.x != 0u) { return; }
  p_pass = p_pass + 1u;
}
)S";
}

// M2.5 — GPU live-count setter: copy SRC[0] into the sort's p_n (in _ct) AND the internal
// scan's p_n (in _sct), so the 32-pass loop + its scans early-out at the live count with the
// scratch still alloc'd to capacity. p_pass (_ct[1]) is left as ensure() reset it.
static std::string _setcount_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface scc_src (descriptor_set 0) { buffer layout(std430) sccsb { uint SRC[]; }; }
storage_interface scc_ct  (descriptor_set 0) { buffer layout(std430) scccb { uint c_n; uint c_pass; uint c2; uint c3; }; }
storage_interface scc_sct (descriptor_set 0) { buffer layout(std430) sccss { uint s_n; uint s_t; uint s2; uint s3; }; }
compute_interface scc_iface { storage { scc_src scc_ct scc_sct } inputs { layout(local_size_x = 1); } }
compute_shader cs_setcount : scc_iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint v = SRC[0];
  c_n = v;   // sort main control p_n (c_pass untouched)
  s_n = v;   // sort's internal scan control p_n
}
)S";
}

void MeshSort::init(Context* ctx) {
  if (_cs_flag) return;
  auto sh     = ctx->FXI()->shaderFromShaderText("hypermesh_sort", _sort_text());
  _cs_flag    = ctx->FXI()->computeShader(sh, "cs_flag");
  _cs_scatter = ctx->FXI()->computeShader(sh, "cs_scatter");
  _cs_incpass = ctx->FXI()->computeShader(sh, "cs_incpass");
  auto scsh   = ctx->FXI()->shaderFromShaderText("hypermesh_sort_setcount", _setcount_text());
  _cs_setcount = ctx->FXI()->computeShader(scsh, "cs_setcount");
  _scan.init(ctx);
}
void MeshSort::setCount(Context* ctx, FxShaderStorageBuffer* src) {
  auto ci = ctx->CI();
  ci->bindStorageBuffer(_cs_setcount, 0, src);
  ci->bindStorageBuffer(_cs_setcount, 1, _ct);
  ci->bindStorageBuffer(_cs_setcount, 2, _sct);
  ci->dispatchCompute(_cs_setcount, 1, 1, 1);
}
void MeshSort::ensure(Context* ctx, int n) {
  auto fxi = ctx->FXI();
  if (n != _n) {
    _dk   = fxi->createStorageBuffer(std::max<size_t>(16, size_t(n) * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    _dp   = fxi->createStorageBuffer(std::max<size_t>(16, size_t(n) * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    _flg  = fxi->createStorageBuffer(std::max<size_t>(16, size_t(n) * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    _base = fxi->createStorageBuffer(std::max<size_t>(16, size_t(n + 1) * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    if (not _ct)  _ct  = fxi->createStorageBuffer(16);   // HOST: p_n + pass (host-reset, GPU-incremented)
    if (not _sct) _sct = fxi->createStorageBuffer(16);
    _n = n;
  }
  uint32_t ct[4]  = {uint32_t(n), 0u, 0u, 0u};           // p_n, pass=0
  auto m = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
  std::memcpy(m->_mappedaddr, ct, sizeof(ct)); fxi->unmapStorageBuffer(m.get());
  uint32_t sct[4] = {uint32_t(n), 0u, 0u, 0u};
  auto ms = fxi->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
  std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); fxi->unmapStorageBuffer(ms.get());
}
void MeshSort::sort(Context* ctx, FxShaderStorageBuffer* key, FxShaderStorageBuffer* payload) {
  auto ci = ctx->CI();
  int  n  = _n;
  FxShaderStorageBuffer *sk = key, *sp = payload, *dk = _dk, *dp = _dp;
  for (int b = 0; b < 32; b++) {
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, sk);  ci->bindStorageBuffer(cs, 1, sp);
      ci->bindStorageBuffer(cs, 2, dk);  ci->bindStorageBuffer(cs, 3, dp);
      ci->bindStorageBuffer(cs, 4, _flg); ci->bindStorageBuffer(cs, 5, _base);
      ci->bindStorageBuffer(cs, 6, _ct);
    };
    bind(_cs_flag);    ci->dispatchCompute(_cs_flag, (n + 63) / 64, 1, 1);
    ci->storageBarrier();
    _scan.scan(ctx, _flg, _base, _sct, n);               // BASE = exclusive #1s, BASE[n] = total1 (C.2 parallel)
    ci->storageBarrier();
    bind(_cs_scatter); ci->dispatchCompute(_cs_scatter, (n + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_incpass); ci->dispatchCompute(_cs_incpass, 1, 1, 1);
    ci->storageBarrier();
    std::swap(sk, dk); std::swap(sp, dp);                // ping-pong (32 = even -> result in key/payload)
  }
}

///////////////////////////////////////////////////////////////////////////////
// MeshEdges  (each pipeline stage = its OWN exact-fit iface, per the shadlang subset-binding trap)
///////////////////////////////////////////////////////////////////////////////

static std::string _edge_cf_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ef_ct (descriptor_set 0) { buffer layout(std430) ectb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface ef_fo (descriptor_set 0) { buffer layout(std430) efob { uint FO[]; }; }
storage_interface ef_cf (descriptor_set 0) { buffer layout(std430) ecfb { uint CFACE[]; }; }
compute_interface ifc { storage { ef_ct ef_fo ef_cf } inputs { layout(local_size_x = 64); } }
compute_shader cs_cornerface : ifc {
  uint f = gl_GlobalInvocationID.x;
  if (f >= e_nf) { return; }
  uint lo = FO[f]; uint hi = FO[f + 1u];
  for (uint c = lo; c < hi; c++) { CFACE[c] = f; }
}
)S";
}
static std::string _edge_emit_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ef_ct (descriptor_set 0) { buffer layout(std430) ectb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface ef_fo (descriptor_set 0) { buffer layout(std430) efob { uint FO[];  }; }
storage_interface ef_vi (descriptor_set 0) { buffer layout(std430) evib { uint VID[]; }; }
storage_interface ef_cf (descriptor_set 0) { buffer layout(std430) ecfb { uint CFACE[]; }; }
storage_interface ef_ky (descriptor_set 0) { buffer layout(std430) ekyb { uint KEY[]; }; }
storage_interface ef_py (descriptor_set 0) { buffer layout(std430) epyb { uint PAY[]; }; }
compute_interface ifc { storage { ef_ct ef_fo ef_vi ef_cf ef_ky ef_py } inputs { layout(local_size_x = 64); } }
compute_shader cs_emit : ifc {
  uint c = gl_GlobalInvocationID.x;
  if (c >= e_nc) { return; }
  uint f = CFACE[c]; uint lo = FO[f]; uint hi = FO[f + 1u];
  uint nc = (c + 1u >= hi) ? lo : (c + 1u);          // next corner within the same face (wrap)
  uint a = VID[c]; uint b = VID[nc];
  uint mn = min(a, b); uint mx = max(a, b);
  KEY[c] = (mn << 16) | mx;                          // canonical undirected key (verts < 65536)
  PAY[c] = c;
}
)S";
}
static std::string _edge_mark_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ef_ct (descriptor_set 0) { buffer layout(std430) ectb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface ef_ky (descriptor_set 0) { buffer layout(std430) ekyb { uint KEY[]; }; }
storage_interface ef_is (descriptor_set 0) { buffer layout(std430) eisb { uint ISTART[]; }; }
compute_interface ifc { storage { ef_ct ef_ky ef_is } inputs { layout(local_size_x = 64); } }
compute_shader cs_mark : ifc {
  uint i = gl_GlobalInvocationID.x;
  if (i >= e_nc) { return; }
  ISTART[i] = (i == 0u || KEY[i - 1u] != KEY[i]) ? 1u : 0u;
}
)S";
}
static std::string _edge_build_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ef_ct (descriptor_set 0) { buffer layout(std430) ectb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface ef_ky (descriptor_set 0) { buffer layout(std430) ekyb { uint KEY[]; }; }
storage_interface ef_py (descriptor_set 0) { buffer layout(std430) epyb { uint PAY[]; }; }
storage_interface ef_is (descriptor_set 0) { buffer layout(std430) eisb { uint ISTART[]; }; }
storage_interface ef_pr (descriptor_set 0) { buffer layout(std430) eprb { uint PRE[]; }; }
storage_interface ef_cf (descriptor_set 0) { buffer layout(std430) ecfb { uint CFACE[]; }; }
storage_interface ef_ed (descriptor_set 0) { buffer layout(std430) eedb { uint EDGE[]; }; }
storage_interface ef_ce (descriptor_set 0) { buffer layout(std430) eceb { uint CE[]; }; }   // corner -> unique edge id
compute_interface ifc { storage { ef_ct ef_ky ef_py ef_is ef_pr ef_cf ef_ed ef_ce } inputs { layout(local_size_x = 64); } }
compute_shader cs_build : ifc {
  uint i = gl_GlobalInvocationID.x;
  if (i == 0u) { e_ne = PRE[e_nc]; }                 // publish numEdges (GPU-side; no readback)
  if (i >= e_nc) { return; }
  CE[PAY[i]] = PRE[i] + ISTART[i] - 1u;              // every directed edge (corner) -> its unique edge id
  if (ISTART[i] == 0u) { return; }                   // only run-starts emit an edge record
  uint eid = PRE[i];                                 // exclusive-scan at a start == this edge's id
  uint k = KEY[i];
  uint f1 = 0xFFFFFFFFu;
  if (i + 1u < e_nc && KEY[i + 1u] == k) { f1 = CFACE[PAY[i + 1u]]; }   // 2nd half-edge -> interior
  EDGE[eid * 4u + 0u] = k >> 16;                      // va (min vert)
  EDGE[eid * 4u + 1u] = k & 0xFFFFu;                  // vb (max vert)
  EDGE[eid * 4u + 2u] = CFACE[PAY[i]];                // f0
  EDGE[eid * 4u + 3u] = f1;                           // f1 (~0 = boundary)
}
)S";
}

void MeshEdges::init(Context* ctx) {
  if (_cs_cf) return;
  auto fxi = ctx->FXI();
  _cs_cf = fxi->computeShader(fxi->shaderFromShaderText("hmedge_cf",    _edge_cf_text()),    "cs_cornerface");
  _cs_em = fxi->computeShader(fxi->shaderFromShaderText("hmedge_emit",  _edge_emit_text()),  "cs_emit");
  _cs_mk = fxi->computeShader(fxi->shaderFromShaderText("hmedge_mark",  _edge_mark_text()),  "cs_mark");
  _cs_bd = fxi->computeShader(fxi->shaderFromShaderText("hmedge_build", _edge_build_text()), "cs_build");
  _sort.init(ctx);
  _scan.init(ctx);
}
void MeshEdges::ensure(Context* ctx, int nc, int nf) {
  auto fxi = ctx->FXI();
  if (nc != _nc) {
    auto dev = [&](size_t n) { return fxi->createStorageBuffer(std::max<size_t>(16, n * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE); };
    _cface = dev(nc); _key = dev(nc); _pay = dev(nc); _istart = dev(nc); _pre = dev(size_t(nc) + 1); _edge = dev(size_t(nc) * 4);
    _corner_edge = dev(nc);
    if (not _ectl) _ectl = fxi->createStorageBuffer(16);
    if (not _sct)  _sct  = fxi->createStorageBuffer(16);
  }
  bool counts_changed = (nc != _nc) or (nf != _nf);
  _nc = nc; _nf = nf;
  // _ectl slot 2 is e_ne — the unique-edge count cs_build PUBLISHES GPU-side and every consumer
  // reads. Rewrite the control block ONLY when the counts moved (then the C.1c topo key moves too,
  // so build() re-runs and republishes e_ne); a cache-hit frame must NOT zero the published count.
  if (counts_changed) {
    uint32_t ectl[4] = {uint32_t(nc), uint32_t(nf), 0u, 0u};
    auto me = fxi->mapStorageBuffer(_ectl, 0, sizeof(ectl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(me->_mappedaddr, ectl, sizeof(ectl)); fxi->unmapStorageBuffer(me.get());
    uint32_t sct[4] = {uint32_t(nc), 0u, 0u, 0u};
    auto ms = fxi->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); fxi->unmapStorageBuffer(ms.get());
  }
  _sort.ensure(ctx, nc);
}
void MeshEdges::build(Context* ctx, gpumesh_ptr_t in) {
  // C.1c TOPOLOGY-KEYED CACHE: the EDGE table is connectivity-PURE (va/vb/f0/f1 vert IDS — midpoint/
  // length/dihedral are derived at USE time from P/N), so a positions-only frame reuses last build's
  // table outright. The key is the same composite the render gate uses: _topoVersion (explicit bumps
  // + the allocMesh auto-bump) plus counts and the vidx/face_offsets COW handles observed directly.
  // This kills the per-frame 32-pass serial sort for every edge consumer (select LINE, bevel) at once.
  if (in->_topoVersion == _builtTopoV and in->_num_corners == _bnc and in->_num_faces == _bnf and
      (const void*)in->_vidx.get() == _bvidx and (const void*)in->_face_offsets.get() == _bfo)
    return;
  buildRaw(ctx, in->_vidx->_ssbo, in->_face_offsets->_ssbo, in->_num_verts);
  _builtTopoV = in->_topoVersion;
  _bnc        = in->_num_corners;
  _bnf        = in->_num_faces;
  _bvidx      = in->_vidx.get();
  _bfo        = in->_face_offsets.get();
}
void MeshEdges::buildRaw(Context* ctx, FxShaderStorageBuffer* vidx, FxShaderStorageBuffer* fo, int nv) {
  // KEY-WIDTH GATE: the canonical undirected edge key packs BOTH vert ids into one uint32
  // (`KEY = (mn<<16)|mx`, cs_emit) -- vert ids >= 65536 alias and SILENTLY corrupt the edge table
  // (wrong dedup -> wrong manifold/dihedral/bevel under every edge-domain op). Fail loudly until the
  // key widens to 64-bit (a two-pass stable sort + key regather -- slated with the parallel-scan work,
  // since MeshSort only permutes a single payload today).
  if (nv >= 65536) {
    printf(
        "MeshEdges: mesh has %d verts but the edge key packs vert ids into 16 bits (max 65535) -- "
        "edge enumeration would silently corrupt. Widen the key before using edge ops at this scale.\n",
        nv);
    OrkAssert(false);
  }
  auto ci  = ctx->CI();
  auto FO  = fo;
  auto VID = vidx;
  ci->bindStorageBuffer(_cs_cf, 0, _ectl); ci->bindStorageBuffer(_cs_cf, 1, FO); ci->bindStorageBuffer(_cs_cf, 2, _cface);
  ci->dispatchCompute(_cs_cf, (_nf + 63) / 64, 1, 1); ci->storageBarrier();
  ci->bindStorageBuffer(_cs_em, 0, _ectl); ci->bindStorageBuffer(_cs_em, 1, FO); ci->bindStorageBuffer(_cs_em, 2, VID);
  ci->bindStorageBuffer(_cs_em, 3, _cface); ci->bindStorageBuffer(_cs_em, 4, _key); ci->bindStorageBuffer(_cs_em, 5, _pay);
  ci->dispatchCompute(_cs_em, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
  _sort.sort(ctx, _key, _pay); ci->storageBarrier();
  ci->bindStorageBuffer(_cs_mk, 0, _ectl); ci->bindStorageBuffer(_cs_mk, 1, _key); ci->bindStorageBuffer(_cs_mk, 2, _istart);
  ci->dispatchCompute(_cs_mk, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
  _scan.scan(ctx, _istart, _pre, _sct, _nc); ci->storageBarrier();   // C.2 parallel (n = corner count)
  ci->bindStorageBuffer(_cs_bd, 0, _ectl); ci->bindStorageBuffer(_cs_bd, 1, _key); ci->bindStorageBuffer(_cs_bd, 2, _pay);
  ci->bindStorageBuffer(_cs_bd, 3, _istart); ci->bindStorageBuffer(_cs_bd, 4, _pre); ci->bindStorageBuffer(_cs_bd, 5, _cface);
  ci->bindStorageBuffer(_cs_bd, 6, _edge); ci->bindStorageBuffer(_cs_bd, 7, _corner_edge);
  ci->dispatchCompute(_cs_bd, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
}

} // namespace ork::lev2::hypermesh
