////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <unordered_map>
#include <set>
#include <map>
#include <tuple>

ImplementReflectionX(ork::lev2::hypermesh::SubdivideModuleData, "hypermesh::SubdivideModuleData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// SubdivideModule (v2, INDEXED, UNIFORM midpoint subdivision) — `level` int plug (dynamic). One round
// splits EVERY edge at its midpoint and PRESERVES face type:
//    triangle -> 4 triangles  (3 edge midpoints; corner tris + the center tri)
//    quad     -> 4 quads      (4 edge midpoints + 1 face center, Catmull-Clark refinement)
//    n-gon    -> n quads       (n edge midpoints + 1 face center)
// so EVERY edge halves each level -> faces stay uniform relative to peers across all levels (a grid
// stays a grid, the uvsphere's pole tris refine in place). Midpoints are SHARED between the (up to 2)
// faces of an edge (deterministic per-edge vertex) -> watertight, reusable, crack-free.
//
// The input topology is FIXED, so ALL of the topology is precomputed ON THE CPU once (onTopologyReady,
// after the driver readback of the source vidx/face_offsets): the unique-edge table, the midpoint/
// center vertex assignment, and each level's output vidx/face_offsets — uploaded as constant buffers.
// Per frame the GPU only INTERPOLATES vertex positions (copy originals, average edge endpoints / face
// corners) cascading level->level; topology never re-derives. `level` selects which precomputed level.
///////////////////////////////////////////////////////////////////////////////

static constexpr int kSubMaxLevel = 4;

// one subdivision level's precomputed topology + the position-interp shaders. Buffers are constant
// (uploaded once); only positions recompute per frame. vidx/fo are COW handles (the output mesh
// aliases them); the edge/center tables are raw pooled buffers (bound only in compute).
struct SubLevel {
  int src_nv = 0, nedges = 0, ncenters = 0;   // source verts / unique edges / centered faces (baked)
  int nv = 0, nc = 0, nf = 0;                  // output counts
  gpuchannel_ptr_t vidx, fo;                   // output topology (mesh aliases these)
  FxShaderStorageBuffer* ea      = nullptr;    // uint[nedges]  edge endpoint A (source vert idx)
  FxShaderStorageBuffer* eb      = nullptr;    // uint[nedges]  edge endpoint B
  FxShaderStorageBuffer* cen_off = nullptr;    // uint[ncenters+1] CSR offsets into cen_vi
  FxShaderStorageBuffer* cen_vi  = nullptr;    // uint[...] centered-face corner vert ids (source idx)
  const FxComputeShader* cs_copy   = nullptr;
  const FxComputeShader* cs_edge   = nullptr;
  const FxComputeShader* cs_center = nullptr;
  // CC (smooth) extras: per-edge adjacent face-points + vertex->face-point / vertex->1-ring CSR tables, and
  // the CC position shaders (the topology is baked; these recompute positions on the GPU each frame).
  FxShaderStorageBuffer *ecL = nullptr, *ecR = nullptr, *vfo = nullptr, *vfc = nullptr, *vno = nullptr, *vnn = nullptr;
  const FxComputeShader *cs_edge_cc = nullptr, *cs_vert_cc = nullptr;
  // output vert->face CSR + the shared gather shader: CC derives smooth normals from the RESULT geometry.
  FxShaderStorageBuffer *aoff = nullptr, *adj = nullptr;
  const FxComputeShader *cs_norm = nullptr;
  // masked render: un-shared dup verts (flat unselected faces) -> cs_dup copies them from their shared source.
  int nv_render = 0;
  FxShaderStorageBuffer *dsrc = nullptr;
  const FxComputeShader *cs_dup = nullptr;
  // C.4 GPU-built levels: interp counts are RUNTIME (this ctl SSBO, slot 14) so the interp shaders
  // compile ONCE per module instance — a topology rebuild re-writes counts, never recompiles.
  FxShaderStorageBuffer* ictl = nullptr;
  // pow2 size-classes of the pooled table buffers (re-acquire only on growth — rebuilds don't leak)
  int cap_ne = 0, cap_cv = 0, cap_nc = 0, cap_nf = 0;
};

// CPU build of ONE level from a source (vidx, face_offsets, src_nv, src_nf): enumerate unique edges
// (shared midpoints), assign center verts to n>=4 faces, then emit the refined faces.
struct LevelTopo {
  std::vector<uint32_t> out_vidx, out_fo, ea, eb, cen_off, cen_vi;
  std::vector<uint32_t> ecL, ecR;            // CC: per-edge adjacent face-point ids (ecL high bit = boundary/crease)
  std::vector<uint32_t> vf_off, vf_cen;      // CC: vertex -> adjacent face-point vert ids (Q + valence)
  std::vector<uint32_t> vn_off, vn_nbr;      // CC: vertex -> unique 1-ring neighbor source verts (R)
  std::vector<char>     out_sel;             // per OUTPUT face: selected? (propagates the mask to the next level)
  std::vector<uint32_t> r_vidx;              // RENDER topology: unselected faces un-shared (== out_vidx if none)
  std::vector<uint32_t> dup_src;             // render dup vert -> the shared vert it copies (flat-normal split)
  int nv = 0, nc = 0, nf = 0, nedges = 0, ncenters = 0, nv_render = 0;
};
static constexpr uint32_t kEdgeBndBit = 0x80000000u;  // ecL high bit: this edge point is a boundary/crease

// Build ONE level. `selFace[f]` (all-1 = whole mesh) selects which faces SUBDIVIDE. A SELECTED face refines
// (uniform/CC -> n quads via a face point, incl tri->3; linear -> tri->4tri / quad->4); an UNSELECTED face
// stays ONE face but absorbs the midpoints its selected neighbors put on the shared edges (-> n-gon,
// watertight). Only edges touching a selected face get a midpoint vertex. `uniform` (= smooth) additionally
// builds the CC tables; a boundary edge (exactly one selected side) is flagged (crease -> straight midpoint)
// and a boundary/unselected vertex gets empty CC tables (pinned -> stays put).
static LevelTopo _buildLevel(const std::vector<uint32_t>& vidx, const std::vector<uint32_t>& fo,
                             int src_nv, int src_nf, bool uniform, const std::vector<char>& selFace) {
  LevelTopo L;
  const uint32_t SENT = 0xFFFFFFFFu;
  std::unordered_map<uint64_t, uint32_t> emap;       // (min<<32|max) -> FULL edge id
  std::vector<uint32_t> fea, feb, eF0, eF1;          // FULL edges + their (up to) two adjacent faces
  std::vector<std::vector<int>> faceEdges(src_nf);   // per-face full edge ids (for pass 2)
  auto edgeId = [&](uint32_t a, uint32_t b, int f) -> uint32_t {
    uint32_t lo = std::min(a, b), hi = std::max(a, b);
    uint64_t key = (uint64_t(lo) << 32) | uint64_t(hi);
    auto it = emap.find(key);
    if (it != emap.end()) { uint32_t id = it->second; if (eF1[id] == SENT) eF1[id] = uint32_t(f); return id; }
    uint32_t id = uint32_t(fea.size());
    fea.push_back(a); feb.push_back(b); eF0.push_back(uint32_t(f)); eF1.push_back(SENT);
    emap[key] = id;
    return id;
  };
  for (int f = 0; f < src_nf; f++) {
    uint32_t o = fo[f], p = fo[f + 1]; int n = int(p - o);
    for (int i = 0; i < n; i++) faceEdges[f].push_back(int(edgeId(vidx[o + i], vidx[o + ((i + 1) % n)], f)));
  }
  int center_min = uniform ? 3 : 4;                  // selected faces get a face point (uniform: tris too)
  std::vector<int> center_index(src_nf, -1);
  for (int f = 0; f < src_nf; f++) {
    if (not selFace[f]) continue;
    uint32_t o = fo[f], p = fo[f + 1]; int n = int(p - o);
    if (n >= center_min) {
      center_index[f] = L.ncenters++;
      L.cen_off.push_back(uint32_t(L.cen_vi.size()));
      for (int i = 0; i < n; i++) L.cen_vi.push_back(vidx[o + i]);
    }
  }
  L.cen_off.push_back(uint32_t(L.cen_vi.size()));
  int nfull = int(fea.size());                       // active edges (touch >=1 selected face) -> compact midpoint index
  // CANONICAL (sorted-key) active-edge numbering — NOT first-encounter order. This matches the GPU
  // MeshEdges enumeration (sorted by (min<<16)|max), so the C.4 GPU topology build and this CPU
  // reference produce IDENTICAL midpoint vertex ids (the equivalence oracle compares bytes).
  std::vector<int> emid(nfull, -1);
  {
    std::vector<int> act;
    for (int e = 0; e < nfull; e++) {
      bool s0 = selFace[eF0[e]] != 0;
      bool s1 = (eF1[e] != SENT) && selFace[eF1[e]];
      if (s0 or s1) act.push_back(e);
    }
    std::sort(act.begin(), act.end(), [&](int a, int b) {
      uint64_t ka = (uint64_t(std::min(fea[a], feb[a])) << 32) | std::max(fea[a], feb[a]);
      uint64_t kb = (uint64_t(std::min(fea[b], feb[b])) << 32) | std::max(fea[b], feb[b]);
      return ka < kb;
    });
    for (int e : act) { emid[e] = L.nedges++; L.ea.push_back(fea[e]); L.eb.push_back(feb[e]); }
  }
  uint32_t edge_base = uint32_t(src_nv);
  uint32_t cen_base  = uint32_t(src_nv + L.nedges);
  auto addFace = [&](std::initializer_list<uint32_t> verts, char sel) {
    L.out_fo.push_back(uint32_t(L.out_vidx.size()));
    for (auto v : verts) L.out_vidx.push_back(v);
    L.out_sel.push_back(sel);
  };
  for (int f = 0; f < src_nf; f++) {
    uint32_t o = fo[f], p = fo[f + 1]; int n = int(p - o);
    if (selFace[f]) {                                // SELECTED -> refine (all its edges are active)
      std::vector<uint32_t> m(n);
      for (int i = 0; i < n; i++) m[i] = edge_base + uint32_t(emid[faceEdges[f][i]]);
      if (n == 3 and not uniform) {                  // LINEAR triangle -> 4 triangles
        uint32_t c0 = vidx[o], c1 = vidx[o + 1], c2 = vidx[o + 2];
        addFace({c0, m[0], m[2]}, 1); addFace({m[0], c1, m[1]}, 1); addFace({m[2], m[1], c2}, 1); addFace({m[0], m[1], m[2]}, 1);
      } else {                                       // n quads via the face point (CC + ngon)
        uint32_t C = cen_base + uint32_t(center_index[f]);
        for (int i = 0; i < n; i++) addFace({vidx[o + i], m[i], C, m[(i + n - 1) % n]}, 1);
      }
    } else {                                         // UNSELECTED -> ONE n-gon, inserting boundary midpoints
      L.out_fo.push_back(uint32_t(L.out_vidx.size()));
      for (int i = 0; i < n; i++) {
        L.out_vidx.push_back(vidx[o + i]);
        int e = faceEdges[f][i];
        if (emid[e] >= 0) L.out_vidx.push_back(edge_base + uint32_t(emid[e]));
      }
      L.out_sel.push_back(0);
    }
  }
  L.out_fo.push_back(uint32_t(L.out_vidx.size()));
  L.nv = src_nv + L.nedges + L.ncenters;
  L.nc = int(L.out_vidx.size());
  L.nf = int(L.out_fo.size()) - 1;
  if (uniform) {                                     // CC adjacency (active edges + smooth verts only)
    L.ecL.resize(L.nedges);                          // indexed by the CANONICAL emid (not discovery order)
    L.ecR.resize(L.nedges);
    for (int e = 0; e < nfull; e++) {
      if (emid[e] < 0) continue;
      bool s0 = selFace[eF0[e]] != 0;
      bool s1 = (eF1[e] != SENT) && selFace[eF1[e]];
      uint32_t fpSel = s0 ? (cen_base + uint32_t(center_index[eF0[e]])) : (cen_base + uint32_t(center_index[eF1[e]]));
      if (s0 and s1) {                               // interior: the two adjacent face points
        L.ecL[emid[e]] = cen_base + uint32_t(center_index[eF0[e]]);
        L.ecR[emid[e]] = cen_base + uint32_t(center_index[eF1[e]]);
      } else {                                       // boundary/crease: straight midpoint (face point unused)
        L.ecL[emid[e]] = kEdgeBndBit | fpSel;
        L.ecR[emid[e]] = fpSel;
      }
    }
    // a vertex is SMOOTH only if ALL its incident faces are selected; else PINNED (empty tables -> stays put).
    std::vector<char> vAllSel(src_nv, 1), vAny(src_nv, 0);
    for (int f = 0; f < src_nf; f++) {
      uint32_t o = fo[f], p = fo[f + 1]; int n = int(p - o);
      for (int i = 0; i < n; i++) { uint32_t v = vidx[o + i]; vAny[v] = 1; if (not selFace[f]) vAllSel[v] = 0; }
    }
    std::vector<std::vector<uint32_t>> vfL(src_nv);
    std::vector<std::set<uint32_t>>    vnS(src_nv);
    for (int f = 0; f < src_nf; f++) {
      if (not selFace[f]) continue;
      uint32_t o = fo[f], p = fo[f + 1]; int n = int(p - o);
      uint32_t cv = cen_base + uint32_t(center_index[f]);
      for (int i = 0; i < n; i++) {
        uint32_t v = vidx[o + i];
        if (not (vAllSel[v] and vAny[v])) continue;  // pinned -> leave tables empty
        vfL[v].push_back(cv);
        vnS[v].insert(vidx[o + ((i + 1) % n)]);
        vnS[v].insert(vidx[o + ((i + n - 1) % n)]);
      }
    }
    L.vf_off.push_back(0);
    for (int v = 0; v < src_nv; v++) { for (auto c : vfL[v]) L.vf_cen.push_back(c); L.vf_off.push_back(uint32_t(L.vf_cen.size())); }
    L.vn_off.push_back(0);
    for (int v = 0; v < src_nv; v++) { for (auto nb : vnS[v]) L.vn_nbr.push_back(nb); L.vn_off.push_back(uint32_t(L.vn_nbr.size())); }
  }
  // RENDER topology (uniform/smooth only): un-share UNSELECTED faces with fresh dup verts so each flat face
  // gets its OWN verts -> the normal gather yields a FLAT per-face normal there (the welded shared verts would
  // otherwise smooth the flat region). Selected faces keep shared verts (smooth). Coincident dups -> watertight
  // crease at the seam. SEPARATE from out_vidx: the cascade chains out_vidx (shared) to stay watertight across
  // levels; only the final level's render topology is drawn. (No unselected faces -> r_vidx == out_vidx.)
  L.nv_render = L.nv;
  if (uniform) {
    L.r_vidx = L.out_vidx;
    for (int f = 0; f < L.nf; f++) {
      if (L.out_sel[f]) continue;
      for (uint32_t c = L.out_fo[f]; c < L.out_fo[f + 1]; c++) {
        uint32_t d = uint32_t(L.nv + L.dup_src.size());
        L.dup_src.push_back(L.out_vidx[c]);
        L.r_vidx[c] = d;
      }
    }
    L.nv_render = L.nv + int(L.dup_src.size());
  } else {
    L.r_vidx = L.out_vidx;
  }
  return L;
}

// position-interp shaders for one level (counts BAKED): copy originals, average edge endpoints, average
// centered-face corners. Linear (silhouette-preserving); N/B re-normalized after averaging.
static std::string _interp_text(int src_nv, int nedges, int ncenters) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_iP  (descriptor_set 0) { buffer layout(std430) ipb { vec4 iP[];  }; }
storage_interface sif_iN  (descriptor_set 0) { buffer layout(std430) inb { vec4 iN[];  }; }
storage_interface sif_iB  (descriptor_set 0) { buffer layout(std430) ibb { vec4 iB[];  }; }
storage_interface sif_iU  (descriptor_set 0) { buffer layout(std430) iub { vec4 iU[];  }; }
storage_interface sif_iC  (descriptor_set 0) { buffer layout(std430) icb { vec4 iC[];  }; }
storage_interface sif_oP  (descriptor_set 0) { buffer layout(std430) opb { vec4 oP[];  }; }
storage_interface sif_oN  (descriptor_set 0) { buffer layout(std430) onb { vec4 oN[];  }; }
storage_interface sif_oB  (descriptor_set 0) { buffer layout(std430) obb { vec4 oB[];  }; }
storage_interface sif_oU  (descriptor_set 0) { buffer layout(std430) oub { vec4 oU[];  }; }
storage_interface sif_oC  (descriptor_set 0) { buffer layout(std430) ocb { vec4 oC[];  }; }
storage_interface sif_ea  (descriptor_set 0) { buffer layout(std430) eab { uint EA[];  }; }
storage_interface sif_eb  (descriptor_set 0) { buffer layout(std430) ebb { uint EB[];  }; }
storage_interface sif_co  (descriptor_set 0) { buffer layout(std430) cob { uint CO[];  }; }
storage_interface sif_cv  (descriptor_set 0) { buffer layout(std430) cvb { uint CV[];  }; }
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC
                                    sif_ea sif_eb sif_co sif_cv }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
// pass A: copy source verts through unchanged.
compute_shader cs_copy : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= %SNVU%) { return; }
  oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
////////////////////////////////////////
// pass B: edge midpoints (one thread / unique edge) -> SHARED vertex src_nv+e.
compute_shader cs_edge : iface {
  uint e = gl_GlobalInvocationID.x;
  if (e >= %NEU%) { return; }
  uint a = EA[e]; uint b = EB[e]; uint o = %SNVU% + e;
  oP[o] = vec4(0.5 * (iP[a].xyz + iP[b].xyz), 1.0);
  oN[o] = vec4(normalize(iN[a].xyz + iN[b].xyz), 0.0);
  oB[o] = vec4(normalize(iB[a].xyz + iB[b].xyz), 0.0);
  oU[o] = 0.5 * (iU[a] + iU[b]);
  oC[o] = 0.5 * (iC[a] + iC[b]);
}
////////////////////////////////////////
// pass C: face centers (one thread / centered face) -> vertex src_nv+nedges+k = avg of its corners.
compute_shader cs_center : iface {
  uint k = gl_GlobalInvocationID.x;
  if (k >= %NCU%) { return; }
  uint o0 = CO[k]; uint o1 = CO[k + 1u]; uint n = o1 - o0;
  vec3 sp = vec3(0.0); vec3 sn = vec3(0.0); vec3 sb = vec3(0.0); vec4 su = vec4(0.0); vec4 sc = vec4(0.0);
  for (uint i = 0u; i < n; i++) {
    uint vtx = CV[o0 + i];
    sp += iP[vtx].xyz; sn += iN[vtx].xyz; sb += iB[vtx].xyz; su += iU[vtx]; sc += iC[vtx];
  }
  float inv = 1.0 / float(n); uint o = %SNVU% + %NEU% + k;
  oP[o] = vec4(sp * inv, 1.0);
  oN[o] = vec4(normalize(sn), 0.0);
  oB[o] = vec4(normalize(sb), 0.0);
  oU[o] = su * inv; oC[o] = sc * inv;
}
)S";
  _shadersub(t, "%SNVU%", FormatString("%du", src_nv));
  _shadersub(t, "%NEU%", FormatString("%du", nedges));
  _shadersub(t, "%NCU%", FormatString("%du", ncenters));
  return t;
}

// CATMULL-CLARK position shaders. cs_center (face points = centroids) FIRST, then a barrier, then cs_edge_cc
// (edge point = avg of the 2 endpoints + the 2 adjacent face points) and cs_vert_cc (the valence rule). N/B/
// UV/C stay linear (averaged/copied) — recompute exact normals downstream with smooth_normals if wanted.
static std::string _interp_text_cc(int src_nv, int nedges, int ncenters) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_iP  (descriptor_set 0) { buffer layout(std430) ipb { vec4 iP[];  }; }
storage_interface sif_iN  (descriptor_set 0) { buffer layout(std430) inb { vec4 iN[];  }; }
storage_interface sif_iB  (descriptor_set 0) { buffer layout(std430) ibb { vec4 iB[];  }; }
storage_interface sif_iU  (descriptor_set 0) { buffer layout(std430) iub { vec4 iU[];  }; }
storage_interface sif_iC  (descriptor_set 0) { buffer layout(std430) icb { vec4 iC[];  }; }
storage_interface sif_oP  (descriptor_set 0) { buffer layout(std430) opb { vec4 oP[];  }; }
storage_interface sif_oN  (descriptor_set 0) { buffer layout(std430) onb { vec4 oN[];  }; }
storage_interface sif_oB  (descriptor_set 0) { buffer layout(std430) obb { vec4 oB[];  }; }
storage_interface sif_oU  (descriptor_set 0) { buffer layout(std430) oub { vec4 oU[];  }; }
storage_interface sif_oC  (descriptor_set 0) { buffer layout(std430) ocb { vec4 oC[];  }; }
storage_interface sif_co  (descriptor_set 0) { buffer layout(std430) cob { uint CO[];  }; }
storage_interface sif_cv  (descriptor_set 0) { buffer layout(std430) cvb { uint CV[];  }; }
storage_interface sif_ea  (descriptor_set 0) { buffer layout(std430) eab { uint EA[];  }; }
storage_interface sif_eb  (descriptor_set 0) { buffer layout(std430) ebb { uint EB[];  }; }
storage_interface sif_ecl (descriptor_set 0) { buffer layout(std430) eclb { uint ECL[]; }; }   // edge -> face-point L
storage_interface sif_ecr (descriptor_set 0) { buffer layout(std430) ecrb { uint ECR[]; }; }
storage_interface sif_vfo (descriptor_set 0) { buffer layout(std430) vfob { uint VFO[]; }; }   // vert -> face-points CSR
storage_interface sif_vfc (descriptor_set 0) { buffer layout(std430) vfcb { uint VFC[]; }; }
storage_interface sif_vno (descriptor_set 0) { buffer layout(std430) vnob { uint VNO[]; }; }   // vert -> 1-ring CSR
storage_interface sif_vnn (descriptor_set 0) { buffer layout(std430) vnnb { uint VNN[]; }; }
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC
                                    sif_co sif_cv sif_ea sif_eb sif_ecl sif_ecr sif_vfo sif_vfc sif_vno sif_vnn }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_center : iface {            // face point = centroid (run FIRST; edge/vert read it)
  uint k = gl_GlobalInvocationID.x;
  if (k >= %NCU%) { return; }
  uint o0 = CO[k]; uint o1 = CO[k + 1u]; uint n = o1 - o0;
  vec3 sp = vec3(0.0); vec3 sn = vec3(0.0); vec3 sb = vec3(0.0); vec4 su = vec4(0.0); vec4 sc = vec4(0.0);
  for (uint i = 0u; i < n; i++) { uint vtx = CV[o0 + i]; sp += iP[vtx].xyz; sn += iN[vtx].xyz; sb += iB[vtx].xyz; su += iU[vtx]; sc += iC[vtx]; }
  float inv = 1.0 / float(n); uint o = %SNVU% + %NEU% + k;
  oP[o] = vec4(sp * inv, 1.0); oN[o] = vec4(normalize(sn), 0.0); oB[o] = vec4(normalize(sb), 0.0); oU[o] = su * inv; oC[o] = sc * inv;
}
////////////////////////////////////////
compute_shader cs_edge_cc : iface {           // interior: (A+B+facePtL+facePtR)/4; boundary/crease: (A+B)/2
  uint e = gl_GlobalInvocationID.x;
  if (e >= %NEU%) { return; }
  uint a = EA[e]; uint b = EB[e]; uint o = %SNVU% + e;
  uint ecl = ECL[e];
  if ((ecl & 0x80000000u) != 0u) {            // boundary/crease edge (high bit) -> straight midpoint
    oP[o] = vec4(0.5 * (iP[a].xyz + iP[b].xyz), 1.0);
  } else {
    oP[o] = vec4(0.25 * (iP[a].xyz + iP[b].xyz + oP[ecl].xyz + oP[ECR[e]].xyz), 1.0);
  }
  oN[o] = vec4(normalize(iN[a].xyz + iN[b].xyz), 0.0);
  oB[o] = vec4(normalize(iB[a].xyz + iB[b].xyz), 0.0);
  oU[o] = 0.5 * (iU[a] + iU[b]); oC[o] = 0.5 * (iC[a] + iC[b]);
}
////////////////////////////////////////
compute_shader cs_vert_cc : iface {           // vertex = (Q + 2R + (n-3)P)/n  ==  (Q + meanNbr + (n-2)P)/n
  uint v = gl_GlobalInvocationID.x;
  if (v >= %SNVU%) { return; }
  vec3 P = iP[v].xyz;
  uint pr0 = VNO[v]; uint pr1 = VNO[v + 1u];
  if (pr0 == pr1) {                           // PINNED (boundary / unselected -> empty tables): stay put
    oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
    return;
  }
  uint q0 = VFO[v]; uint q1 = VFO[v + 1u]; uint nf = q1 - q0;
  vec3 Q = vec3(0.0); for (uint k = q0; k < q1; k++) Q += oP[VFC[k]].xyz;
  Q = Q / float(max(1u, nf));
  uint r0 = VNO[v]; uint r1 = VNO[v + 1u]; uint nn = r1 - r0;
  vec3 M = vec3(0.0); for (uint k = r0; k < r1; k++) M += iP[VNN[k]].xyz;
  M = M / float(max(1u, nn));
  float nval = float(max(1u, nn));
  oP[v] = vec4((Q + M + (nval - 2.0) * P) / nval, 1.0);
  oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
)S";
  _shadersub(t, "%SNVU%", FormatString("%du", src_nv));
  _shadersub(t, "%NEU%", FormatString("%du", nedges));
  _shadersub(t, "%NCU%", FormatString("%du", ncenters));
  return t;
}

// CC weld pre-pass: build the WELDED control cage from the (possibly un-shared) input each frame. A welded
// vert reads its representative input vert via WSRC, so an animated/deformed source still flows through. CC
// needs true connectivity — without this, an un-shared primitive (per-face dup verts) smooths each face
// independently and the faces separate. Runs ONCE before the level cascade; the cascade then reads _wbase.
static std::string _weld_text(int welded_nv) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_iP (descriptor_set 0) { buffer layout(std430) ipb { vec4 iP[]; }; }
storage_interface sif_iN (descriptor_set 0) { buffer layout(std430) inb { vec4 iN[]; }; }
storage_interface sif_iB (descriptor_set 0) { buffer layout(std430) ibb { vec4 iB[]; }; }
storage_interface sif_iU (descriptor_set 0) { buffer layout(std430) iub { vec4 iU[]; }; }
storage_interface sif_iC (descriptor_set 0) { buffer layout(std430) icb { vec4 iC[]; }; }
storage_interface sif_oP (descriptor_set 0) { buffer layout(std430) opb { vec4 oP[]; }; }
storage_interface sif_oN (descriptor_set 0) { buffer layout(std430) onb { vec4 oN[]; }; }
storage_interface sif_oB (descriptor_set 0) { buffer layout(std430) obb { vec4 oB[]; }; }
storage_interface sif_oU (descriptor_set 0) { buffer layout(std430) oub { vec4 oU[]; }; }
storage_interface sif_oC (descriptor_set 0) { buffer layout(std430) ocb { vec4 oC[]; }; }
storage_interface sif_WS (descriptor_set 0) { buffer layout(std430) wsb { uint WSRC[]; }; }
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC sif_WS }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_weld : iface {
  uint w = gl_GlobalInvocationID.x;
  if (w >= %WNVU%) { return; }
  uint s = WSRC[w];
  oP[w] = iP[s]; oN[w] = iN[s]; oB[w] = iB[s]; oU[w] = iU[s]; oC[w] = iC[s];
}
)S";
  _shadersub(t, "%WNVU%", FormatString("%du", welded_nv));
  return t;
}

// RENDER dup pass: copy each un-shared render vert (unselected face, flat-normal split) from the shared vert
// it duplicates. Runs AFTER the cascade (positions ready), BEFORE the gather. Reads + writes the fin set
// (different indices, no conflict). One buffer set + DSRC.
static std::string _dup_text(int nv_shared, int ndup) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface dif_oP (descriptor_set 0) { buffer layout(std430) dopb { vec4 oP[]; }; }
storage_interface dif_oN (descriptor_set 0) { buffer layout(std430) donb { vec4 oN[]; }; }
storage_interface dif_oB (descriptor_set 0) { buffer layout(std430) dobb { vec4 oB[]; }; }
storage_interface dif_oU (descriptor_set 0) { buffer layout(std430) doub { vec4 oU[]; }; }
storage_interface dif_oC (descriptor_set 0) { buffer layout(std430) docb { vec4 oC[]; }; }
storage_interface dif_DS (descriptor_set 0) { buffer layout(std430) ddsb { uint DSRC[]; }; }
compute_interface iface { storage { dif_oP dif_oN dif_oB dif_oU dif_oC dif_DS }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_dup : iface {
  uint d = gl_GlobalInvocationID.x;
  if (d >= %NDUPU%) { return; }
  uint o = %NVSU% + d; uint s = DSRC[d];
  oP[o] = oP[s]; oN[o] = oN[s]; oB[o] = oB[s]; oU[o] = oU[s]; oC[o] = oC[s];
}
)S";
  _shadersub(t, "%NVSU%", FormatString("%du", nv_shared));
  _shadersub(t, "%NDUPU%", FormatString("%du", ndup));
  return t;
}

static void _uploadU32(Context* ctx, FxShaderStorageBuffer* buf, const std::vector<uint32_t>& v) {
  if (v.empty() or not buf) return;
  auto m = ctx->FXI()->mapStorageBuffer(buf, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
  std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
  ctx->FXI()->unmapStorageBuffer(m.get());
}
static uint32_t _readU32(Context* ctx, FxShaderStorageBuffer* buf, size_t byte_offset) {
  auto m = ctx->FXI()->mapStorageBuffer(buf, byte_offset, 4, BufferMapAccess::READ_ONLY);
  uint32_t v = *reinterpret_cast<const uint32_t*>(m->_mappedaddr);
  ctx->FXI()->unmapStorageBuffer(m.get());
  return v;
}

///////////////////////////////////////////////////////////////////////////////
// C.4 GPU TOPOLOGY BUILD (linear UNIFORM whole-mesh path). The level tables that _buildLevel derives
// on the CPU (after a full vidx/fo readback) are instead derived ON the GPU from the MeshEdges
// enumeration: edge midpoints ARE the unique-edge table (midpoint vert id = src_nv + corner_edge[c]),
// the refined CSR/vidx come from a count->scan->emit pass (the delete/compact pattern). The ONLY
// readback is 5 scan/edge TOTALS (20 bytes) per level per topology-CHANGE — the steady frame stays
// positions-only. Edge numbering is canonical (sorted key) on BOTH paths, so CPU and GPU builds are
// byte-identical (the equivalence oracle). Per the shadlang subset-binding trap, each stage is its
// OWN shader module with an exact-fit iface.
///////////////////////////////////////////////////////////////////////////////

// stage 1: per-face counts (out-corners / out-faces / center flag / center corners).
static std::string _gpub_cnt_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface gb_ct (descriptor_set 0) { buffer layout(std430) gctb { uint g_nf; uint g_nv; uint g_ne; uint g_p3; }; }
storage_interface gb_fo (descriptor_set 0) { buffer layout(std430) gfob { uint FO[]; }; }
storage_interface gb_c1 (descriptor_set 0) { buffer layout(std430) gc1b { uint FCORN[]; }; }
storage_interface gb_c2 (descriptor_set 0) { buffer layout(std430) gc2b { uint FFACE[]; }; }
storage_interface gb_c3 (descriptor_set 0) { buffer layout(std430) gc3b { uint CFLG[]; }; }
storage_interface gb_c4 (descriptor_set 0) { buffer layout(std430) gc4b { uint CCNT[]; }; }
compute_interface iface { storage { gb_ct gb_fo gb_c1 gb_c2 gb_c3 gb_c4 } inputs { layout(local_size_x = 64); } }
compute_shader cs_gcnt : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  uint n = FO[f + 1u] - FO[f];
  FCORN[f] = (n == 3u) ? 12u : (4u * n);    // tri -> 4 tris (12 corners), n>=4 -> n quads (4n)
  FFACE[f] = (n == 3u) ? 4u : n;
  CFLG[f]  = (n >= 4u) ? 1u : 0u;           // linear: only n>=4 faces get a center point
  CCNT[f]  = (n >= 4u) ? n : 0u;
}
)S";
}
// stage 2 (post-totals): EA/EB extraction from the EDGE table.
static std::string _gpub_ext_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface gb_ct (descriptor_set 0) { buffer layout(std430) gctb { uint g_nf; uint g_nv; uint g_ne; uint g_p3; }; }
storage_interface gb_ed (descriptor_set 0) { buffer layout(std430) gedb { uint EDGE[]; }; }
storage_interface gb_ea (descriptor_set 0) { buffer layout(std430) geab { uint EA[]; }; }
storage_interface gb_eb (descriptor_set 0) { buffer layout(std430) gebb { uint EB[]; }; }
compute_interface iface { storage { gb_ct gb_ed gb_ea gb_eb } inputs { layout(local_size_x = 64); } }
compute_shader cs_gext : iface {
  uint e = gl_GlobalInvocationID.x;
  if (e >= g_ne) { return; }
  EA[e] = EDGE[e * 4u + 0u];
  EB[e] = EDGE[e * 4u + 1u];
}
)S";
}
// stage 2: centered-face CSR (cen_off/cen_vi) from the center scans.
static std::string _gpub_cen_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface gb_ct (descriptor_set 0) { buffer layout(std430) gctb { uint g_nf; uint g_nv; uint g_ne; uint g_p3; }; }
storage_interface gb_fo (descriptor_set 0) { buffer layout(std430) gfob { uint FO[]; }; }
storage_interface gb_vi (descriptor_set 0) { buffer layout(std430) gvib { uint VI[]; }; }
storage_interface gb_bx (descriptor_set 0) { buffer layout(std430) gbxb { uint CIDX[]; }; }
storage_interface gb_bv (descriptor_set 0) { buffer layout(std430) gbvb { uint CVOFF[]; }; }
storage_interface gb_co (descriptor_set 0) { buffer layout(std430) gcob { uint CO[]; }; }
storage_interface gb_cv (descriptor_set 0) { buffer layout(std430) gcvb { uint CV[]; }; }
compute_interface iface { storage { gb_ct gb_fo gb_vi gb_bx gb_bv gb_co gb_cv } inputs { layout(local_size_x = 64); } }
compute_shader cs_gcen : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if (f == 0u) { CO[CIDX[g_nf]] = CVOFF[g_nf]; }   // CSR tail = total centered corners
  uint o = FO[f]; uint n = FO[f + 1u] - o;
  if (n < 4u) { return; }
  uint ci  = CIDX[f];
  uint off = CVOFF[f];
  CO[ci] = off;
  for (uint i = 0u; i < n; i++) { CV[off + i] = VI[o + i]; }
}
)S";
}
// stage 2: refined-face emission (vidx + CSR). Midpoint vert = g_nv + corner_edge; center = g_nv+g_ne+ci.
// The emit patterns MIRROR _buildLevel exactly (tri: corner tris then the center tri; n>=4: n quads
// {v_i, m_i, C, m_(i+n-1)%n}) so CPU- and GPU-built topology are byte-identical.
static std::string _gpub_emit_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface gb_ct (descriptor_set 0) { buffer layout(std430) gctb { uint g_nf; uint g_nv; uint g_ne; uint g_p3; }; }
storage_interface gb_fo (descriptor_set 0) { buffer layout(std430) gfob { uint FO[]; }; }
storage_interface gb_vi (descriptor_set 0) { buffer layout(std430) gvib { uint VI[]; }; }
storage_interface gb_ce (descriptor_set 0) { buffer layout(std430) gceb { uint CE[]; }; }
storage_interface gb_bc (descriptor_set 0) { buffer layout(std430) gbcb { uint CBASE[]; }; }
storage_interface gb_bf (descriptor_set 0) { buffer layout(std430) gbfb { uint FBASE[]; }; }
storage_interface gb_bx (descriptor_set 0) { buffer layout(std430) gbxb { uint CIDX[]; }; }
storage_interface gb_ov (descriptor_set 0) { buffer layout(std430) govb { uint OV[]; }; }
storage_interface gb_of (descriptor_set 0) { buffer layout(std430) gofb { uint OF[]; }; }
compute_interface iface { storage { gb_ct gb_fo gb_vi gb_ce gb_bc gb_bf gb_bx gb_ov gb_of }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_gemit : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if (f == 0u) { OF[FBASE[g_nf]] = CBASE[g_nf]; }  // out CSR tail = total out corners
  uint o = FO[f]; uint n = FO[f + 1u] - o;
  uint fb = FBASE[f]; uint cb = CBASE[f];
  if (n == 3u) {                                   // tri -> 4 tris: (c0,m0,m2)(m0,c1,m1)(m2,m1,c2)(m0,m1,m2)
    uint c0 = VI[o]; uint c1 = VI[o + 1u]; uint c2 = VI[o + 2u];
    uint m0 = g_nv + CE[o]; uint m1 = g_nv + CE[o + 1u]; uint m2 = g_nv + CE[o + 2u];
    OF[fb + 0u] = cb;      OV[cb + 0u]  = c0; OV[cb + 1u]  = m0; OV[cb + 2u]  = m2;
    OF[fb + 1u] = cb + 3u; OV[cb + 3u]  = m0; OV[cb + 4u]  = c1; OV[cb + 5u]  = m1;
    OF[fb + 2u] = cb + 6u; OV[cb + 6u]  = m2; OV[cb + 7u]  = m1; OV[cb + 8u]  = c2;
    OF[fb + 3u] = cb + 9u; OV[cb + 9u]  = m0; OV[cb + 10u] = m1; OV[cb + 11u] = m2;
  } else {                                         // n>=4 -> n quads via the face point
    uint C = g_nv + g_ne + CIDX[f];
    for (uint i = 0u; i < n; i++) {
      uint b = cb + i * 4u;
      OF[fb + i]  = b;
      OV[b + 0u]  = VI[o + i];
      OV[b + 1u]  = g_nv + CE[o + i];
      OV[b + 2u]  = C;
      OV[b + 3u]  = g_nv + CE[o + ((i + n - 1u) % n)];
    }
  }
}
)S";
}
// runtime-count LINEAR interp (the GPU-built levels): IDENTICAL arithmetic to _interp_text, but the
// counts come from a per-level ctl SSBO (slot 14) -> compiled ONCE; rebuilds re-write counts only.
static std::string _interp_text_rt() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_iP  (descriptor_set 0) { buffer layout(std430) ipb { vec4 iP[];  }; }
storage_interface sif_iN  (descriptor_set 0) { buffer layout(std430) inb { vec4 iN[];  }; }
storage_interface sif_iB  (descriptor_set 0) { buffer layout(std430) ibb { vec4 iB[];  }; }
storage_interface sif_iU  (descriptor_set 0) { buffer layout(std430) iub { vec4 iU[];  }; }
storage_interface sif_iC  (descriptor_set 0) { buffer layout(std430) icb { vec4 iC[];  }; }
storage_interface sif_oP  (descriptor_set 0) { buffer layout(std430) opb { vec4 oP[];  }; }
storage_interface sif_oN  (descriptor_set 0) { buffer layout(std430) onb { vec4 oN[];  }; }
storage_interface sif_oB  (descriptor_set 0) { buffer layout(std430) obb { vec4 oB[];  }; }
storage_interface sif_oU  (descriptor_set 0) { buffer layout(std430) oub { vec4 oU[];  }; }
storage_interface sif_oC  (descriptor_set 0) { buffer layout(std430) ocb { vec4 oC[];  }; }
storage_interface sif_ea  (descriptor_set 0) { buffer layout(std430) eab { uint EA[];  }; }
storage_interface sif_eb  (descriptor_set 0) { buffer layout(std430) ebb { uint EB[];  }; }
storage_interface sif_co  (descriptor_set 0) { buffer layout(std430) cob { uint CO[];  }; }
storage_interface sif_cv  (descriptor_set 0) { buffer layout(std430) cvb { uint CV[];  }; }
storage_interface sif_ic  (descriptor_set 0) { buffer layout(std430) sicb { uint p_snv; uint p_ne; uint p_ncen; uint p_p3; }; }
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC
                                    sif_ea sif_eb sif_co sif_cv sif_ic }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_copy : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_snv) { return; }
  oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
compute_shader cs_edge : iface {
  uint e = gl_GlobalInvocationID.x;
  if (e >= p_ne) { return; }
  uint a = EA[e]; uint b = EB[e]; uint o = p_snv + e;
  oP[o] = vec4(0.5 * (iP[a].xyz + iP[b].xyz), 1.0);
  oN[o] = vec4(normalize(iN[a].xyz + iN[b].xyz), 0.0);
  oB[o] = vec4(normalize(iB[a].xyz + iB[b].xyz), 0.0);
  oU[o] = 0.5 * (iU[a] + iU[b]);
  oC[o] = 0.5 * (iC[a] + iC[b]);
}
compute_shader cs_center : iface {
  uint k = gl_GlobalInvocationID.x;
  if (k >= p_ncen) { return; }
  uint o0 = CO[k]; uint o1 = CO[k + 1u]; uint n = o1 - o0;
  vec3 sp = vec3(0.0); vec3 sn = vec3(0.0); vec3 sb = vec3(0.0); vec4 su = vec4(0.0); vec4 sc = vec4(0.0);
  for (uint i = 0u; i < n; i++) {
    uint vtx = CV[o0 + i];
    sp += iP[vtx].xyz; sn += iN[vtx].xyz; sb += iB[vtx].xyz; su += iU[vtx]; sc += iC[vtx];
  }
  float inv = 1.0 / float(n); uint o = p_snv + p_ne + k;
  oP[o] = vec4(sp * inv, 1.0);
  oN[o] = vec4(normalize(sn), 0.0);
  oB[o] = vec4(normalize(sb), 0.0);
  oU[o] = su * inv; oC[o] = sc * inv;
}
)S";
}

struct SubdivideModuleInst : public MeshComputeInst {
  SubdivideModuleInst(const SubdivideModuleData* d, dflow::GraphInst* g)
      : MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];

  // C.4: build the level tables — GPU-native for the linear UNIFORM whole-mesh path (MeshEdges
  // midpoints + count/scan/emit; 20-byte totals readback per level), CPU readback build otherwise
  // (smooth/CC needs the welded cage; masked needs tag-aware selection propagation). Both paths
  // produce byte-identical tables (canonical edge order) — the equivalence oracle asserts it.
  // ORK_HM_SUBD_CPU=1 forces the CPU path (the oracle's reference + escape hatch).
  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    bool force_cpu = (getenv("ORK_HM_SUBD_CPU") != nullptr);  // LIVE read: the oracle toggles it per-bake
    bool gpu_ok = (not force_cpu) and (not _d->_smooth) and (_d->_slot < 0) and (in->_num_verts < 65536);
    if (gpu_ok and onTopologyReadyGPU(ctx, in)) {
      _built = true;
      ackSrcTopo(_input);
      return true;
    }
    return onTopologyReadyCPU(ctx);
  }

  // ---- C.4 GPU builder. Returns false if a deeper level trips the MeshEdges 65536-vert key gate
  //      (the caller then falls back to the CPU build wholesale — correct, just slower). ----
  void compileGpuBuild(Context* ctx) {
    if (_cs_gcnt) return;
    auto fxi  = ctx->FXI();
    _cs_gcnt  = fxi->computeShader(fxi->shaderFromShaderText("hm_subgb_cnt", _gpub_cnt_text()), "cs_gcnt");
    _cs_gext  = fxi->computeShader(fxi->shaderFromShaderText("hm_subgb_ext", _gpub_ext_text()), "cs_gext");
    _cs_gcen  = fxi->computeShader(fxi->shaderFromShaderText("hm_subgb_cen", _gpub_cen_text()), "cs_gcen");
    _cs_gemit = fxi->computeShader(fxi->shaderFromShaderText("hm_subgb_emit", _gpub_emit_text()), "cs_gemit");
    auto rsh     = fxi->shaderFromShaderText("hm_subdiv_rt", _interp_text_rt());
    _cs_rt_copy   = fxi->computeShader(rsh, "cs_copy");
    _cs_rt_edge   = fxi->computeShader(rsh, "cs_edge");
    _cs_rt_center = fxi->computeShader(rsh, "cs_center");
    _gedges.init(ctx);
    _gscan.init(ctx);
    _gctl = fxi->createStorageBuffer(16);
  }
  bool onTopologyReadyGPU(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    auto ci  = ctx->CI();
    compileGpuBuild(ctx);
    FxShaderStorageBuffer* cvi = in->_vidx->_ssbo;
    FxShaderStorageBuffer* cfo = in->_face_offsets->_ssbo;
    int nv = in->_num_verts, nc = in->_num_corners, nf = in->_num_faces;
    for (int Lv = 1; Lv <= kSubMaxLevel; Lv++) {
      if (nv >= 65536) return false;               // key-gate: fall back to the CPU build wholesale
      auto& s = _lv[Lv];
      _gedges.ensure(ctx, nc, nf);
      if (meshNextPow2(nf + 1) > _gcap_nf) {       // count/base/sct scratch (grow-only, rebuilds don't leak)
        _gcap_nf = meshNextPow2(nf + 1);
        _gc1 = env->_pool->acquire(4, nf); _gc2 = env->_pool->acquire(4, nf);
        _gc3 = env->_pool->acquire(4, nf); _gc4 = env->_pool->acquire(4, nf);
        _gbc = env->_pool->acquire(4, nf + 1); _gbf = env->_pool->acquire(4, nf + 1);
        _gbx = env->_pool->acquire(4, nf + 1); _gbv = env->_pool->acquire(4, nf + 1);
        if (not _gs1) { _gs1 = fxi->createStorageBuffer(16); _gs2 = fxi->createStorageBuffer(16);
                        _gs3 = fxi->createStorageBuffer(16); _gs4 = fxi->createStorageBuffer(16); }
      }
      auto wctl = [&](FxShaderStorageBuffer* b, uint32_t a, uint32_t bb, uint32_t c) {
        uint32_t v[4] = {a, bb, c, 0u};
        auto m = fxi->mapStorageBuffer(b, 0, sizeof(v), BufferMapAccess::WRITE_ONLY);
        std::memcpy(m->_mappedaddr, v, sizeof(v)); fxi->unmapStorageBuffer(m.get());
      };
      wctl(_gs1, uint32_t(nf), 0, 0); wctl(_gs2, uint32_t(nf), 0, 0);
      wctl(_gs3, uint32_t(nf), 0, 0); wctl(_gs4, uint32_t(nf), 0, 0);
      wctl(_gctl, uint32_t(nf), uint32_t(nv), 0u);
      // phase 1: edge enumeration + per-face counts + the four scans
      ci->beginDispatchPhase();
      _gedges.buildRaw(ctx, cvi, cfo, nv);
      ci->bindStorageBuffer(_cs_gcnt, 0, _gctl); ci->bindStorageBuffer(_cs_gcnt, 1, cfo);
      ci->bindStorageBuffer(_cs_gcnt, 2, _gc1);  ci->bindStorageBuffer(_cs_gcnt, 3, _gc2);
      ci->bindStorageBuffer(_cs_gcnt, 4, _gc3);  ci->bindStorageBuffer(_cs_gcnt, 5, _gc4);
      ci->dispatchCompute(_cs_gcnt, (nf + 63) / 64, 1, 1);
      ci->storageBarrier();
      _gscan.scan(ctx, _gc1, _gbc, _gs1, nf); ci->storageBarrier();
      _gscan.scan(ctx, _gc2, _gbf, _gs2, nf); ci->storageBarrier();
      _gscan.scan(ctx, _gc3, _gbx, _gs3, nf); ci->storageBarrier();
      _gscan.scan(ctx, _gc4, _gbv, _gs4, nf); ci->storageBarrier();
      ci->endDispatchPhase();
      // the ONLY readback: 5 totals (20 bytes) -> exact host counts (downstream/render contracts keep working)
      int ne     = int(_readU32(ctx, _gedges._ectl, 8));
      int out_nc = int(_readU32(ctx, _gbc, size_t(nf) * 4));
      int out_nf = int(_readU32(ctx, _gbf, size_t(nf) * 4));
      int ncen   = int(_readU32(ctx, _gbx, size_t(nf) * 4));
      int cvt    = int(_readU32(ctx, _gbv, size_t(nf) * 4));
      s.src_nv = nv; s.nedges = ne; s.ncenters = ncen;
      s.nv = nv + ne + ncen; s.nc = out_nc; s.nf = out_nf; s.nv_render = s.nv;
      s.vidx = env->_pool->acquireChannel(4, out_nc);          // COW (auto-returns on rebuild reassign)
      s.fo   = env->_pool->acquireChannel(4, out_nf + 1);
      if (meshNextPow2(std::max(1, ne)) > s.cap_ne) {
        s.cap_ne = meshNextPow2(std::max(1, ne));
        s.ea = env->_pool->acquire(4, std::max(1, ne));
        s.eb = env->_pool->acquire(4, std::max(1, ne));
        s.cen_off = env->_pool->acquire(4, std::max(1, ne + 1)); // ncen <= ne always (one center/face, face has >= n edges)
      }
      if (meshNextPow2(std::max(1, cvt)) > s.cap_cv) {
        s.cap_cv = meshNextPow2(std::max(1, cvt));
        s.cen_vi = env->_pool->acquire(4, std::max(1, cvt));
      }
      if (not s.ictl) s.ictl = fxi->createStorageBuffer(16);
      wctl(_gctl, uint32_t(nf), uint32_t(nv), uint32_t(ne));
      wctl(s.ictl, uint32_t(nv), uint32_t(ne), uint32_t(ncen));
      // phase 2: extract EA/EB, centered-face CSR, refined vidx/CSR emission
      ci->beginDispatchPhase();
      if (ne > 0) {
        ci->bindStorageBuffer(_cs_gext, 0, _gctl); ci->bindStorageBuffer(_cs_gext, 1, _gedges._edge);
        ci->bindStorageBuffer(_cs_gext, 2, s.ea);  ci->bindStorageBuffer(_cs_gext, 3, s.eb);
        ci->dispatchCompute(_cs_gext, (ne + 63) / 64, 1, 1);
      }
      ci->bindStorageBuffer(_cs_gcen, 0, _gctl); ci->bindStorageBuffer(_cs_gcen, 1, cfo);
      ci->bindStorageBuffer(_cs_gcen, 2, cvi);   ci->bindStorageBuffer(_cs_gcen, 3, _gbx);
      ci->bindStorageBuffer(_cs_gcen, 4, _gbv);  ci->bindStorageBuffer(_cs_gcen, 5, s.cen_off);
      ci->bindStorageBuffer(_cs_gcen, 6, s.cen_vi);
      ci->dispatchCompute(_cs_gcen, (nf + 63) / 64, 1, 1);
      ci->bindStorageBuffer(_cs_gemit, 0, _gctl); ci->bindStorageBuffer(_cs_gemit, 1, cfo);
      ci->bindStorageBuffer(_cs_gemit, 2, cvi);   ci->bindStorageBuffer(_cs_gemit, 3, _gedges._corner_edge);
      ci->bindStorageBuffer(_cs_gemit, 4, _gbc);  ci->bindStorageBuffer(_cs_gemit, 5, _gbf);
      ci->bindStorageBuffer(_cs_gemit, 6, _gbx);  ci->bindStorageBuffer(_cs_gemit, 7, s.vidx->_ssbo);
      ci->bindStorageBuffer(_cs_gemit, 8, s.fo->_ssbo);
      ci->dispatchCompute(_cs_gemit, (nf + 63) / 64, 1, 1);
      ci->storageBarrier();
      ci->endDispatchPhase();
      s.cs_copy = _cs_rt_copy; s.cs_edge = _cs_rt_edge; s.cs_center = _cs_rt_center;
      s.cs_edge_cc = nullptr; s.cs_vert_cc = nullptr; s.cs_norm = nullptr; s.cs_dup = nullptr;
      s.dsrc = nullptr;                                                  // linear whole-mesh: no CC/mask extras
      cvi = s.vidx->_ssbo; cfo = s.fo->_ssbo;
      nv = s.nv; nc = out_nc; nf = out_nf;
    }
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    return true;
  }

  // build ALL levels' topology on the CPU from the (now-computed) source mesh, upload as constant
  // buffers, compile the per-level interp shaders. Source topology read back ONCE here.
  bool onTopologyReadyCPU(Context* ctx) {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    // readback the source topology (vidx + face_offsets) — fixed, so this is one-time.
    std::vector<uint32_t> cur_vidx(in->_num_corners), cur_fo(in->_num_faces + 1);
    {
      auto mv = fxi->mapStorageBuffer(in->_vidx->_ssbo, 0, size_t(in->_num_corners) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(cur_vidx.data(), mv->_mappedaddr, size_t(in->_num_corners) * 4);
      fxi->unmapStorageBuffer(mv.get());
      auto mf = fxi->mapStorageBuffer(in->_face_offsets->_ssbo, 0, size_t(in->_num_faces + 1) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(cur_fo.data(), mf->_mappedaddr, size_t(in->_num_faces + 1) * 4);
      fxi->unmapStorageBuffer(mf.get());
    }
    int cur_nv = in->_num_verts, cur_nf = in->_num_faces;
    bool smooth = _d->_smooth;
    if (smooth) {
      // CC needs a WELDED control cage. Read the source positions, weld coincident verts (shared weldVerts),
      // re-index the topology to welded ids, and stand up a per-frame welded base (cs_weld + WSRC). The level
      // cascade then builds + runs on the welded cage; cs_weld rebuilds positions each frame (animatable).
      std::vector<float> Pin(size_t(cur_nv) * 4);
      {
        auto mp = fxi->mapStorageBuffer(in->channel(MeshChannel::POSITION)->_ssbo, 0, size_t(cur_nv) * 16, BufferMapAccess::READ_ONLY);
        std::memcpy(Pin.data(), mp->_mappedaddr, size_t(cur_nv) * 16);
        fxi->unmapStorageBuffer(mp.get());
      }
      std::vector<uint32_t> remap, reps;
      _welded_nv = weldVerts(Pin.data(), cur_nv, remap, reps);
      for (auto& c : cur_vidx) c = remap[c];                 // re-index the cage to welded vert ids
      cur_nv = _welded_nv;
      for (auto ch : kCh) _wbase[ch] = env->_pool->acquireChannel(ch, _welded_nv);
      _wsrc = env->_pool->acquire(4, std::max(1, _welded_nv));
      _uploadU32(ctx, _wsrc, reps);
      auto wsh = fxi->shaderFromShaderText("hm_subdiv_weld", _weld_text(_welded_nv));
      _cs_weld = fxi->computeShader(wsh, "cs_weld");
    }
    // level-1 selection from the __tags bit (slot<0 -> whole mesh). Propagated to higher levels via out_sel
    // (a selected face's children stay selected). The weld doesn't touch faces, so this is valid post-weld.
    std::vector<char> selFace(cur_nf, 1);
    if (_d->_slot >= 0) {
      uint32_t bit = 1u << (_d->_slot & 31);
      std::vector<uint32_t> tags(cur_nf, 0);
      if (auto tg = in->face("__tags")) {
        auto mt = fxi->mapStorageBuffer(tg->_ssbo, 0, size_t(cur_nf) * 4, BufferMapAccess::READ_ONLY);
        std::memcpy(tags.data(), mt->_mappedaddr, size_t(cur_nf) * 4);
        fxi->unmapStorageBuffer(mt.get());
      }
      for (int f = 0; f < cur_nf; f++) selFace[f] = (tags[f] & bit) ? 1 : 0;
    }
    for (int Lv = 1; Lv <= kSubMaxLevel; Lv++) {
      auto lt    = _buildLevel(cur_vidx, cur_fo, cur_nv, cur_nf, smooth, selFace);
      auto& s    = _lv[Lv];
      // REBUILD HYGIENE: a SubLevel may hold a previous build's state (possibly the GPU path's) —
      // clear the per-path bits so compute() branches purely on what THIS build set.
      s.cs_copy = s.cs_edge = s.cs_center = s.cs_edge_cc = s.cs_vert_cc = s.cs_norm = s.cs_dup = nullptr;
      s.dsrc = nullptr; s.ictl = nullptr;
      s.src_nv   = cur_nv; s.nedges = lt.nedges; s.ncenters = lt.ncenters;
      s.nv = lt.nv; s.nc = lt.nc; s.nf = lt.nf; s.nv_render = lt.nv_render;
      s.vidx     = env->_pool->acquireChannel(4, lt.nc);          // COW handles (mesh aliases them)
      s.fo       = env->_pool->acquireChannel(4, lt.nf + 1);
      s.ea       = env->_pool->acquire(4, std::max(1, lt.nedges)); // raw (compute-only)
      s.eb       = env->_pool->acquire(4, std::max(1, lt.nedges));
      s.cen_off  = env->_pool->acquire(4, std::max(1, lt.ncenters + 1));
      s.cen_vi   = env->_pool->acquire(4, std::max(1, int(lt.cen_vi.size())));
      _uploadU32(ctx, s.vidx->_ssbo, lt.r_vidx);                  // RENDER topology (unselected un-shared)
      _uploadU32(ctx, s.fo->_ssbo, lt.out_fo);
      _uploadU32(ctx, s.ea, lt.ea);
      _uploadU32(ctx, s.eb, lt.eb);
      _uploadU32(ctx, s.cen_off, lt.cen_off);
      _uploadU32(ctx, s.cen_vi, lt.cen_vi);
      if (smooth) {                                               // CC tables + position shaders
        s.ecL = env->_pool->acquire(4, std::max(1, lt.nedges));
        s.ecR = env->_pool->acquire(4, std::max(1, lt.nedges));
        s.vfo = env->_pool->acquire(4, cur_nv + 1);
        s.vfc = env->_pool->acquire(4, std::max(1, int(lt.vf_cen.size())));
        s.vno = env->_pool->acquire(4, cur_nv + 1);
        s.vnn = env->_pool->acquire(4, std::max(1, int(lt.vn_nbr.size())));
        _uploadU32(ctx, s.ecL, lt.ecL);   _uploadU32(ctx, s.ecR, lt.ecR);
        _uploadU32(ctx, s.vfo, lt.vf_off); _uploadU32(ctx, s.vfc, lt.vf_cen);
        _uploadU32(ctx, s.vno, lt.vn_off); _uploadU32(ctx, s.vnn, lt.vn_nbr);
        auto sh = fxi->shaderFromShaderText(FormatString("hm_subdiv_cc_L%d", Lv), _interp_text_cc(cur_nv, lt.nedges, lt.ncenters));
        s.cs_center  = fxi->computeShader(sh, "cs_center");
        s.cs_edge_cc = fxi->computeShader(sh, "cs_edge_cc");
        s.cs_vert_cc = fxi->computeShader(sh, "cs_vert_cc");
        // output vert->face CSR over the RENDER topology (un-shared unselected -> flat there) + the shared
        // smooth-normal gather. A CC surface's normals come from the RESULT geometry, not the flat cage.
        std::vector<uint32_t> noff, nadj;
        buildVtxFaceCSR(lt.r_vidx, lt.out_fo, lt.nv_render, lt.nf, noff, nadj);
        s.aoff = env->_pool->acquire(4, lt.nv_render + 1);
        s.adj  = env->_pool->acquire(4, std::max<size_t>(1, nadj.size()));
        _uploadU32(ctx, s.aoff, noff);
        _uploadU32(ctx, s.adj, nadj);
        auto nsh   = fxi->shaderFromShaderText(FormatString("hm_subdiv_norm_L%d", Lv), _gatherNormalsText());
        s.cs_norm  = fxi->computeShader(nsh, "cs_norm");
        if (not lt.dup_src.empty()) {                            // masked: copy un-shared dup verts post-cascade
          s.dsrc = env->_pool->acquire(4, int(lt.dup_src.size()));
          _uploadU32(ctx, s.dsrc, lt.dup_src);
          auto dsh  = fxi->shaderFromShaderText(FormatString("hm_subdiv_dup_L%d", Lv), _dup_text(lt.nv, int(lt.dup_src.size())));
          s.cs_dup  = fxi->computeShader(dsh, "cs_dup");
        }
      } else {
        auto sh = fxi->shaderFromShaderText(FormatString("hm_subdiv_L%d", Lv), _interp_text(cur_nv, lt.nedges, lt.ncenters));
        s.cs_copy   = fxi->computeShader(sh, "cs_copy");
        s.cs_edge   = fxi->computeShader(sh, "cs_edge");
        s.cs_center = fxi->computeShader(sh, "cs_center");
      }
      // next level subdivides THIS level's output topology; selection propagates via out_sel.
      cur_vidx = lt.out_vidx; cur_fo = lt.out_fo; cur_nv = lt.nv; cur_nf = lt.nf;
      selFace = lt.out_sel;
    }
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (smooth and not _ct) _ct = env->_pool->acquire(4, 4); // p_nv (output vert count) for the gather pass
    _built  = true;
    ackSrcTopo(_input);                              // the tables match the CURRENT source topology
    return true; // eval-1 was a passthrough stub -> re-eval so the output reflects real subdivision
  }

  // CPU sizing (PRE phase): pick `level`, re-pool the position ping-pong sets, wire output topology.
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    // C.4 / 2.7 STALENESS FIX: the tables were built ONCE and never tracked the source. Now: when the
    // producer re-emits topology, defer ONE frame (its re-emitted buffers aren't computed yet — the
    // extrude pattern), then rebuild the level tables against the settled topology. The GPU build
    // makes change-frames cheap (20-byte totals readback/level); the CPU paths re-read-back.
    if (_built and _topoPending) {
      _topoPending = false;
      _built       = false;
      onTopologyReady(ctx);                          // rebuild (opens its own dispatch phases; we are PRE-phase)
    } else if (_built and srcTopoDirty(_input)) {
      ackSrcTopo(_input);
      _topoPending = true;                           // content lands this frame's compute -> rebuild next frame
    }
    int level = *(_d->typedInputNamed<dflow::IntPlugTraits>("level")->_value);
    level     = level < 0 ? 0 : (level > kSubMaxLevel ? kSubMaxLevel : level);
    _level    = level;
    auto out  = _output->_value;
    if (not _built or level == 0) {                 // pre-tables OR passthrough: alias the input
      out->_channels     = in->_channels;
      out->_vidx         = in->_vidx;
      out->_face_offsets = in->_face_offsets;
      out->_header       = in->_header;
      out->_capacity     = in->_capacity;
      out->_num_verts    = in->_num_verts;
      out->_num_corners  = in->_num_corners;
      out->_num_faces    = in->_num_faces;
      return;
    }
    auto env  = _graphinst->_impl.getShared<MeshEnv>();
    auto& top = _lv[level];
    int nvr   = top.nv_render;                      // includes the un-shared dup verts (== nv when unmasked)
    int vcap  = meshNextPow2(nvr);
    if (vcap != _vcap) {                            // size-class changed -> re-pool position sets
      for (auto c : kCh) {
        _setA[c] = env->_pool->acquireChannel(c, nvr);
        _setB[c] = env->_pool->acquireChannel(c, nvr);
      }
      _vcap = vcap;
    }
    auto& fin          = (level % 2 == 1) ? _setA : _setB;  // final positions land here
    out->_channels     = fin;
    out->_vidx         = top.vidx;                  // precomputed constant topology (aliased)
    out->_face_offsets = top.fo;
    out->_header       = _header;
    out->_capacity     = vcap;
    out->_num_verts    = nvr;
    out->_num_corners  = top.nc;
    out->_num_faces    = top.nf;
    uint32_t hdr[4] = {uint32_t(nvr), uint32_t(top.nc), uint32_t(top.nf), 0u};
    auto m = ctx->FXI()->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, hdr, sizeof(hdr));
    ctx->FXI()->unmapStorageBuffer(m.get());
    if (_ct) {                                      // gather-pass p_nv (PRE phase — never map mid-dispatch)
      uint32_t cnt[4] = {uint32_t(nvr), 0u, 0u, 0u};
      auto mc = ctx->FXI()->mapStorageBuffer(_ct, 0, sizeof(cnt), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mc->_mappedaddr, cnt, sizeof(cnt));
      ctx->FXI()->unmapStorageBuffer(mc.get());
    }
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _built or _level == 0) return;          // passthrough -> output already aliases input
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcMesh(_input);
    std::map<MeshChannel, gpuchannel_ptr_t> src = in->_channels;   // level-1 source positions
    if (_cs_weld) {                                                // CC: build the welded cage from the input first
      ci->bindStorageBuffer(_cs_weld, 0, in->_channels[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 1, in->_channels[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 2, in->_channels[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 3, in->_channels[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 4, in->_channels[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 5, _wbase[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 6, _wbase[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 7, _wbase[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 8, _wbase[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 9, _wbase[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(_cs_weld, 10, _wsrc);
      ci->dispatchCompute(_cs_weld, (_welded_nv + 63) / 64, 1, 1);
      ci->storageBarrier();
      src = _wbase;                                               // the cascade subdivides the welded cage
    }
    for (int Lv = 1; Lv <= _level; Lv++) {
      auto& s   = _lv[Lv];
      auto& dst = (Lv % 2 == 1) ? _setA : _setB;
      auto chans = [&](const FxComputeShader* cs) {   // slots 0-9: src + dst vertex channels (both paths)
        ci->bindStorageBuffer(cs, 0, src[MeshChannel::POSITION]->_ssbo);
        ci->bindStorageBuffer(cs, 1, src[MeshChannel::NORMAL]->_ssbo);
        ci->bindStorageBuffer(cs, 2, src[MeshChannel::BINORMAL]->_ssbo);
        ci->bindStorageBuffer(cs, 3, src[MeshChannel::UV0]->_ssbo);
        ci->bindStorageBuffer(cs, 4, src[MeshChannel::COLOR]->_ssbo);
        ci->bindStorageBuffer(cs, 5, dst[MeshChannel::POSITION]->_ssbo);
        ci->bindStorageBuffer(cs, 6, dst[MeshChannel::NORMAL]->_ssbo);
        ci->bindStorageBuffer(cs, 7, dst[MeshChannel::BINORMAL]->_ssbo);
        ci->bindStorageBuffer(cs, 8, dst[MeshChannel::UV0]->_ssbo);
        ci->bindStorageBuffer(cs, 9, dst[MeshChannel::COLOR]->_ssbo);
      };
      if (s.cs_vert_cc) {                              // CATMULL-CLARK: face points FIRST, then edges + verts
        auto bindcc = [&](const FxComputeShader* cs) {
          chans(cs);
          ci->bindStorageBuffer(cs, 10, s.cen_off); ci->bindStorageBuffer(cs, 11, s.cen_vi);
          ci->bindStorageBuffer(cs, 12, s.ea);       ci->bindStorageBuffer(cs, 13, s.eb);
          ci->bindStorageBuffer(cs, 14, s.ecL);      ci->bindStorageBuffer(cs, 15, s.ecR);
          ci->bindStorageBuffer(cs, 16, s.vfo);      ci->bindStorageBuffer(cs, 17, s.vfc);
          ci->bindStorageBuffer(cs, 18, s.vno);      ci->bindStorageBuffer(cs, 19, s.vnn);
        };
        bindcc(s.cs_center);  ci->dispatchCompute(s.cs_center,  (s.ncenters + 63) / 64, 1, 1);
        ci->storageBarrier();                         // face points must be written before edges/verts read them
        bindcc(s.cs_edge_cc); ci->dispatchCompute(s.cs_edge_cc, (s.nedges + 63) / 64, 1, 1);
        bindcc(s.cs_vert_cc); ci->dispatchCompute(s.cs_vert_cc, (s.src_nv + 63) / 64, 1, 1);
        ci->storageBarrier();
      } else {                                        // LINEAR
        auto bind = [&](const FxComputeShader* cs) {
          chans(cs);
          ci->bindStorageBuffer(cs, 10, s.ea);      ci->bindStorageBuffer(cs, 11, s.eb);
          ci->bindStorageBuffer(cs, 12, s.cen_off); ci->bindStorageBuffer(cs, 13, s.cen_vi);
          if (s.ictl) ci->bindStorageBuffer(cs, 14, s.ictl);   // GPU-built level: runtime counts
        };
        bind(s.cs_copy);   ci->dispatchCompute(s.cs_copy,   (s.src_nv + 63) / 64, 1, 1);
        bind(s.cs_edge);   ci->dispatchCompute(s.cs_edge,   (s.nedges + 63) / 64, 1, 1);
        if (s.ncenters > 0) { bind(s.cs_center); ci->dispatchCompute(s.cs_center, (s.ncenters + 63) / 64, 1, 1); }
        ci->storageBarrier();
      }
      src = dst;
    }
    // CC: recompute smooth normals (+ UV binormal) from the RESULT geometry over the final level's topology.
    // `src` now aliases the final position set (fin). The control cage's flat normals are discarded.
    auto& top = _lv[_level];
    if (top.cs_dup) {                               // masked: fill the un-shared dup verts from their sources
      auto cs = top.cs_dup;
      ci->bindStorageBuffer(cs, 0, src[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 1, src[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 2, src[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 3, src[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 4, src[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(cs, 5, top.dsrc);
      ci->dispatchCompute(cs, ((top.nv_render - top.nv) + 63) / 64, 1, 1);
      ci->storageBarrier();
    }
    if (top.cs_norm) {                              // _ct already populated in writeParams (PRE phase)
      auto cs = top.cs_norm;
      ci->bindStorageBuffer(cs, 0, src[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 1, src[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 2, src[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 3, src[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 4, top.vidx->_ssbo);
      ci->bindStorageBuffer(cs, 5, top.fo->_ssbo);
      ci->bindStorageBuffer(cs, 6, top.aoff);
      ci->bindStorageBuffer(cs, 7, top.adj);
      ci->bindStorageBuffer(cs, 8, _ct);
      ci->dispatchCompute(cs, (top.nv_render + 63) / 64, 1, 1);
      ci->storageBarrier();
    }
  }

  const SubdivideModuleData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  std::map<MeshChannel, gpuchannel_ptr_t> _setA, _setB; // ping-pong POSITION sets (re-pooled on level change)
  std::map<MeshChannel, gpuchannel_ptr_t> _wbase;       // CC welded control cage (rebuilt each frame by cs_weld)
  FxShaderStorageBuffer* _wsrc = nullptr;               // welded vert -> representative input vert
  const FxComputeShader* _cs_weld = nullptr;            // non-null only when smooth (CC)
  FxShaderStorageBuffer* _ct = nullptr;                 // gather-pass count buffer (p_nv)
  int _welded_nv = 0;
  SubLevel _lv[kSubMaxLevel + 1] = {};
  FxShaderStorageBuffer* _header = nullptr;
  int _level = 1, _vcap = 0;
  bool _built       = false;
  bool _topoPending = false;                            // 2.7: source topo re-emitted -> rebuild next frame
  // ---- C.4 GPU topology build state ----
  MeshEdges _gedges;                                    // per-level unique-edge enumeration (raw form)
  MeshScan _gscan;
  const FxComputeShader *_cs_gcnt = nullptr, *_cs_gext = nullptr, *_cs_gcen = nullptr, *_cs_gemit = nullptr;
  const FxComputeShader *_cs_rt_copy = nullptr, *_cs_rt_edge = nullptr, *_cs_rt_center = nullptr;
  FxShaderStorageBuffer* _gctl = nullptr;               // build ctl {nf, nv, ne} (rewritten per level)
  FxShaderStorageBuffer *_gc1 = nullptr, *_gc2 = nullptr, *_gc3 = nullptr, *_gc4 = nullptr;   // count scratch
  FxShaderStorageBuffer *_gbc = nullptr, *_gbf = nullptr, *_gbx = nullptr, *_gbv = nullptr;   // scan bases
  FxShaderStorageBuffer *_gs1 = nullptr, *_gs2 = nullptr, *_gs3 = nullptr, *_gs4 = nullptr;   // scan ctls
  int _gcap_nf = 0;
};
const MeshChannel SubdivideModuleInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeSubdivIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "level")->setValue(1);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SubdivideModuleData::SubdivideModuleData() {
}
std::shared_ptr<SubdivideModuleData> SubdivideModuleData::createShared() {
  auto d = std::make_shared<SubdivideModuleData>();
  _reshapeSubdivIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SubdivideModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SubdivideModuleInst>(this, g);
}
void SubdivideModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SubdivideModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSubdivIOs(m); });
  clazz->directProperty("smooth", &SubdivideModuleData::_smooth);
  clazz->directProperty("slot", &SubdivideModuleData::_slot);
}

} // namespace ork::lev2::hypermesh
