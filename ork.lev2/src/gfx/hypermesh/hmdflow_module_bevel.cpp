////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <algorithm>

ImplementReflectionX(ork::lev2::hypermesh::BevelData, "hypermesh::BevelData");

namespace ork::lev2::hypermesh {

// FaceTagger partitions: FACE = shrunk original faces, CHAMFER = per-edge bridge quads, CAP = corner n-gons.
enum { kBevFace = 0, kBevChamfer = 1, kBevCap = 2 };

///////////////////////////////////////////////////////////////////////////////
// Bevel — EDGE CHAMFER, GPU-NATIVE (the first GPU topology-ADD op; no topology readback). Consumes the
// upstream LINE-Select's edge __tags on the GpuMesh edge domain; runs its OWN MeshEdges (deterministic ->
// same edge ids, so the select's __tags align by id) for the edge table + corner->edge map. The output mesh
// is sized to UPPER BOUNDS (CPU-known from nc); the GPU computes the live counts via MeshScan and writes the
// header; only the final vertex COUNT is read back (after the build is finished) to set the CPU-side count
// dump_obj/downstream read. STAGE B1 (this file): beveled-vert mark -> count/scan offset verts -> compute the
// inward offset geometry in-shader (bisector/perp/centroid) -> remap the shrunk faces. (B2 adds chamfer
// quads with by-construction winding; Stage C adds the MeshSort corner caps.)
///////////////////////////////////////////////////////////////////////////////

// ---- per-stage shaders, each its own exact-fit iface (shadlang subset-binding trap) ----
static std::string _ctl_block() {   // shared control block layout (bound to most passes)
  return "storage_interface bif_ct (descriptor_set 0) { buffer layout(std430) bctb {\n"
         "  uint p_nv; uint p_nc; uint p_nf; uint p_bit; float p_amount; uint p_voff; uint p_s6; uint p_s7; }; }\n";
}
static std::string _clearbv_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
compute_interface ifc { storage { bif_ct bif_bv } inputs { layout(local_size_x = 64); } }
compute_shader cs_clearbv : ifc {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  BVERT[v] = 0u;
}
)S";
}
static std::string _markbv_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_ec (descriptor_set 0) { buffer layout(std430) becb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface bif_ed (descriptor_set 0) { buffer layout(std430) bedb { uint EDGE[]; }; }
storage_interface bif_tg (descriptor_set 0) { buffer layout(std430) btgb { uint ETAG[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
compute_interface ifc { storage { bif_ct bif_ec bif_ed bif_tg bif_bv } inputs { layout(local_size_x = 64); } }
compute_shader cs_markbv : ifc {                        // a vert is beveled if it endpoints a selected edge
  uint e = gl_GlobalInvocationID.x;
  if (e >= e_ne) { return; }
  if ((ETAG[e] & (1u << p_bit)) == 0u) { return; }
  BVERT[EDGE[e * 4u + 0u]] = 1u;
  BVERT[EDGE[e * 4u + 1u]] = 1u;
}
)S";
}
static std::string _voffcnt_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
storage_interface bif_cn (descriptor_set 0) { buffer layout(std430) bcnb { uint VOFFCNT[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_bv bif_lb bif_cn } inputs { layout(local_size_x = 64); } }
compute_shader cs_voffcnt : ifc {                       // 1 offset vert per SECTOR (representative corner)
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  VOFFCNT[c] = (BVERT[VID[c]] != 0u && LABEL[c] == c) ? 1u : 0u;
}
)S";
}
static std::string _cornoff_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
storage_interface bif_bs (descriptor_set 0) { buffer layout(std430) bbsb { uint VOFFBASE[]; }; }
storage_interface bif_co (descriptor_set 0) { buffer layout(std430) bcob { uint CORNEROFF[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_bv bif_lb bif_bs bif_co } inputs { layout(local_size_x = 64); } }
compute_shader cs_cornoff : ifc {                       // corner -> its SECTOR's output vertex (or the original)
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  CORNEROFF[c] = (BVERT[VID[c]] != 0u) ? (p_nv + VOFFBASE[LABEL[c]]) : VID[c];
}
)S";
}
// ---- SECTORING: corners of a beveled vertex joined by NON-beveled edges share one offset vert (the surface
// stays welded there); BEVELED edges split sectors. LABEL[c] = the sector representative (min corner reachable
// via non-beveled edges around the vertex). This is what makes bevel robust to MIXED edge selection. ----
static std::string _label_init_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
compute_interface ifc { storage { bif_ct bif_lb } inputs { layout(local_size_x = 64); } }
compute_shader cs_label_init : ifc { uint c = gl_GlobalInvocationID.x; if (c < p_nc) { LABEL[c] = c; } }
)S";
}
static std::string _label_prop_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_fo (descriptor_set 0) { buffer layout(std430) bfob { uint FO[]; }; }
storage_interface bif_cf (descriptor_set 0) { buffer layout(std430) bcfb { uint CFACE[]; }; }
storage_interface bif_ce (descriptor_set 0) { buffer layout(std430) bceb { uint CE[]; }; }
storage_interface bif_ed (descriptor_set 0) { buffer layout(std430) bedb { uint EDGE[]; }; }
storage_interface bif_tg (descriptor_set 0) { buffer layout(std430) btgb { uint ETAG[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_fo bif_cf bif_ce bif_ed bif_tg bif_bv bif_lb }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_label_prop : ifc {                    // min-propagate label across NON-beveled edges (monotone)
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  uint v = VID[c]; if (BVERT[v] == 0u) { return; }
  uint f = CFACE[c]; uint lo = FO[f]; uint hi = FO[f + 1u]; uint m = hi - lo; uint k = c - lo;
  uint pc = lo + ((k + m - 1u) % m);
  uint lab = LABEL[c];
  for (uint side = 0u; side < 2u; side++) {
    uint e = (side == 0u) ? CE[c] : CE[pc];
    if ((ETAG[e] & (1u << p_bit)) != 0u) { continue; }  // beveled edge = sector boundary -> don't merge across
    uint f0 = EDGE[e * 4u + 2u]; uint f1 = EDGE[e * 4u + 3u];
    uint of = (f0 == f) ? f1 : f0;
    if (of == 0xFFFFFFFFu) { continue; }
    uint olo = FO[of]; uint ohi = FO[of + 1u];
    for (uint cc = olo; cc < ohi; cc++) { if (VID[cc] == v) { lab = min(lab, LABEL[cc]); } }
  }
  LABEL[c] = lab;
}
)S";
}
// per-corner offset position: bisector (both edges beveled), or SLIDE along the un-beveled edge (one beveled,
// keeps the corner on the edge that stays -> welds correctly), or stay at v (neither). Written per corner;
// the sector then averages them. amount-scaled per frame.
static std::string _coffpos_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_fo (descriptor_set 0) { buffer layout(std430) bfob { uint FO[]; }; }
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_P  (descriptor_set 0) { buffer layout(std430) bpb  { vec4 P[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_ce (descriptor_set 0) { buffer layout(std430) bceb { uint CE[]; }; }
storage_interface bif_tg (descriptor_set 0) { buffer layout(std430) btgb { uint ETAG[]; }; }
storage_interface bif_cp (descriptor_set 0) { buffer layout(std430) bcpb { vec4 COFFPOS[]; }; }
storage_interface bif_cnn (descriptor_set 0) { buffer layout(std430) bcnnb { vec4 COFFNRM[]; }; }
compute_interface ifc { storage { bif_ct bif_fo bif_vi bif_P bif_bv bif_ce bif_tg bif_cp bif_cnn }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_coffpos : ifc {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint a = FO[f]; uint b = FO[f + 1u]; uint m = b - a;
  vec3 C = vec3(0.0); for (uint k = 0u; k < m; k++) { C += P[VID[a + k]].xyz; } C = C / float(m);
  vec3 fN = normalize(cross(P[VID[a + 1u]].xyz - P[VID[a]].xyz, P[VID[a + 2u]].xyz - P[VID[a]].xyz));
  for (uint k = 0u; k < m; k++) {
    uint c = a + k; uint v = VID[c];
    if (BVERT[v] == 0u) { continue; }
    uint pc = a + ((k + m - 1u) % m); uint nc2 = a + ((k + 1u) % m);
    vec3 Pv = P[v].xyz;
    vec3 eo = normalize(P[VID[nc2]].xyz - Pv); vec3 ei = normalize(P[VID[pc]].xyz - Pv);
    bool so = (ETAG[CE[c]]  & (1u << p_bit)) != 0u;
    bool si = (ETAG[CE[pc]] & (1u << p_bit)) != 0u;
    float cc = clamp(dot(eo, ei), -1.0, 1.0);
    vec3 toward;
    if (so && si) {
      vec3 bis = eo + ei; float bl = length(bis);
      if (bl > 1e-5) { float s = sqrt(max(1e-6, (1.0 - cc) * 0.5)); toward = (bis / bl) * (1.0 / s); }
      else { vec3 p = cross(fN, eo); if (dot(p, C - Pv) < 0.0) p = -p; toward = normalize(p); }
    } else if (so) { float s = sqrt(max(1e-3, 1.0 - cc * cc)); toward = ei * (1.0 / s); }   // slide along edge_in
    else if (si)   { float s = sqrt(max(1e-3, 1.0 - cc * cc)); toward = eo * (1.0 / s); }    // slide along edge_out
    else { toward = vec3(0.0); }                                                             // stay at v
    COFFPOS[c] = vec4(Pv + p_amount * toward, 1.0);
    COFFNRM[c] = vec4(fN, 0.0);
  }
}
)S";
}
// per SECTOR representative: average its corners' offset positions/normals -> the one shared sector vert.
static std::string _secoffset_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
storage_interface bif_bs (descriptor_set 0) { buffer layout(std430) bbsb { uint VOFFBASE[]; }; }
storage_interface bif_cp (descriptor_set 0) { buffer layout(std430) bcpb { vec4 COFFPOS[]; }; }
storage_interface bif_cnn (descriptor_set 0) { buffer layout(std430) bcnnb { vec4 COFFNRM[]; }; }
storage_interface bif_iB (descriptor_set 0) { buffer layout(std430) bibb { vec4 iB[]; }; }
storage_interface bif_iU (descriptor_set 0) { buffer layout(std430) biub { vec4 iU[]; }; }
storage_interface bif_iC (descriptor_set 0) { buffer layout(std430) bicb { vec4 iC[]; }; }
storage_interface bif_oP (descriptor_set 0) { buffer layout(std430) bopb { vec4 oP[]; }; }
storage_interface bif_oN (descriptor_set 0) { buffer layout(std430) bonb { vec4 oN[]; }; }
storage_interface bif_oB (descriptor_set 0) { buffer layout(std430) bobb { vec4 oB[]; }; }
storage_interface bif_oU (descriptor_set 0) { buffer layout(std430) boub { vec4 oU[]; }; }
storage_interface bif_oC (descriptor_set 0) { buffer layout(std430) bocb { vec4 oC[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_bv bif_lb bif_bs bif_cp bif_cnn bif_iB bif_iU bif_iC
                                  bif_oP bif_oN bif_oB bif_oU bif_oC }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_secoffset : ifc {
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  uint v = VID[c];
  if (BVERT[v] == 0u || LABEL[c] != c) { return; }      // only sector representatives
  vec3 ps = vec3(0.0); vec3 ns = vec3(0.0); uint cnt = 0u;
  for (uint cc = 0u; cc < p_nc; cc++) { if (LABEL[cc] == c) { ps += COFFPOS[cc].xyz; ns += COFFNRM[cc].xyz; cnt++; } }
  uint o = p_nv + VOFFBASE[c];
  oP[o] = vec4(ps / float(max(cnt, 1u)), 1.0); oN[o] = vec4(normalize(ns), 0.0);
  oB[o] = iB[v]; oU[o] = iU[v]; oC[o] = iC[v];
}
)S";
}
static std::string _shrink_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_co (descriptor_set 0) { buffer layout(std430) bcob { uint CORNEROFF[]; }; }
storage_interface bif_fo (descriptor_set 0) { buffer layout(std430) bfob { uint FO[]; }; }
storage_interface bif_ov (descriptor_set 0) { buffer layout(std430) bovb { uint oVID[]; }; }
storage_interface bif_of (descriptor_set 0) { buffer layout(std430) bofb { uint oFO[]; }; }
compute_interface ifc { storage { bif_ct bif_co bif_fo bif_ov bif_of } inputs { layout(local_size_x = 64); } }
compute_shader cs_shrink : ifc {                        // shrunk faces: same face structure, corners remapped
  uint c = gl_GlobalInvocationID.x;
  if (c <= p_nf) { oFO[c] = FO[c]; }                    // shrunk face offsets == input (corners [0,nc))
  if (c >= p_nc) { return; }
  oVID[c] = CORNEROFF[c];
}
)S";
}
// ---- B2: chamfer quads. One quad per selected INTERIOR edge, bridging the 4 offset verts (va/vb in f0/f1).
// winding is BY CONSTRUCTION (not a geometric guess): derived from which way f0 traverses the edge, so the
// chamfer is manifold-consistent with the shrunk faces (correct for convex AND concave). ----
static std::string _chamcnt_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_ec (descriptor_set 0) { buffer layout(std430) becb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface bif_ed (descriptor_set 0) { buffer layout(std430) bedb { uint EDGE[]; }; }
storage_interface bif_tg (descriptor_set 0) { buffer layout(std430) btgb { uint ETAG[]; }; }
storage_interface bif_cn (descriptor_set 0) { buffer layout(std430) bccb { uint CHAMCNT[]; }; }
compute_interface ifc { storage { bif_ct bif_ec bif_ed bif_tg bif_cn } inputs { layout(local_size_x = 64); } }
compute_shader cs_chamcnt : ifc {                       // 1 chamfer per selected INTERIOR edge (f1 != boundary)
  uint e = gl_GlobalInvocationID.x;
  if (e >= p_nc) { return; }                            // p_nc == edge upper bound
  uint v = 0u;
  if (e < e_ne) { if ((ETAG[e] & (1u << p_bit)) != 0u && EDGE[e * 4u + 3u] != 0xFFFFFFFFu) v = 1u; }
  CHAMCNT[e] = v;
}
)S";
}
static std::string _chamfer_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_ec (descriptor_set 0) { buffer layout(std430) becb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface bif_ed (descriptor_set 0) { buffer layout(std430) bedb { uint EDGE[]; }; }
storage_interface bif_tg (descriptor_set 0) { buffer layout(std430) btgb { uint ETAG[]; }; }
storage_interface bif_cb (descriptor_set 0) { buffer layout(std430) bcbb { uint CHAMBASE[]; }; }
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_fo (descriptor_set 0) { buffer layout(std430) bfob { uint FO[]; }; }
storage_interface bif_co (descriptor_set 0) { buffer layout(std430) bcob { uint CORNEROFF[]; }; }
storage_interface bif_ov (descriptor_set 0) { buffer layout(std430) bovb { uint oVID[]; }; }
storage_interface bif_of (descriptor_set 0) { buffer layout(std430) bofb { uint oFO[]; }; }
compute_interface ifc { storage { bif_ct bif_ec bif_ed bif_tg bif_cb bif_vi bif_fo bif_co bif_ov bif_of }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_chamfer : ifc {
  uint e = gl_GlobalInvocationID.x;
  if (e >= p_nc || e >= e_ne) { return; }
  if ((ETAG[e] & (1u << p_bit)) == 0u) { return; }
  uint f1 = EDGE[e * 4u + 3u]; if (f1 == 0xFFFFFFFFu) { return; }   // boundary edge -> no chamfer (B2)
  uint va = EDGE[e * 4u + 0u]; uint vb = EDGE[e * 4u + 1u]; uint f0 = EDGE[e * 4u + 2u];
  uint lo0 = FO[f0]; uint hi0 = FO[f0 + 1u]; uint lo1 = FO[f1]; uint hi1 = FO[f1 + 1u];
  uint c0a = lo0; uint c0b = lo0; uint c1a = lo1; uint c1b = lo1;
  for (uint c = lo0; c < hi0; c++) { if (VID[c] == va) c0a = c; else if (VID[c] == vb) c0b = c; }
  for (uint c = lo1; c < hi1; c++) { if (VID[c] == va) c1a = c; else if (VID[c] == vb) c1b = c; }
  uint n0 = (c0a + 1u >= hi0) ? lo0 : (c0a + 1u);
  bool f0vavb = (VID[n0] == vb);                         // does f0 traverse va->vb ?
  uint ci   = CHAMBASE[e];
  uint base = p_nc + 4u * ci;                            // chamfer corners start after the nc shrunk corners
  oFO[p_nf + ci] = base;
  if (f0vavb) { oVID[base] = CORNEROFF[c0b]; oVID[base + 1u] = CORNEROFF[c0a]; oVID[base + 2u] = CORNEROFF[c1a]; oVID[base + 3u] = CORNEROFF[c1b]; }
  else        { oVID[base] = CORNEROFF[c0a]; oVID[base + 1u] = CORNEROFF[c0b]; oVID[base + 2u] = CORNEROFF[c1b]; oVID[base + 3u] = CORNEROFF[c1a]; }
}
)S";
}
// ---- Stage C: corner caps. Each beveled vertex's offset verts form a ring around the vertex hole; the cap
// is the n-gon closing it. Per-vertex gather of the offset verts + angular sort around the vertex normal
// (the locally-correct outward for convex AND concave — the cap fills the vertex hole), wound to face it. ----
static std::string _valence_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
storage_interface bif_va (descriptor_set 0) { buffer layout(std430) bvab { uint VALENCE[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_bv bif_lb bif_va } inputs { layout(local_size_x = 64); } }
compute_shader cs_valence : ifc {                       // # SECTORS (= cap corners = offset verts) at a beveled vert
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  if (BVERT[v] == 0u) { VALENCE[v] = 0u; return; }
  uint d = 0u;
  for (uint c = 0u; c < p_nc; c++) { if (VID[c] == v && LABEL[c] == c) d++; }
  VALENCE[v] = d;
}
)S";
}
static std::string _capcnt_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_va (descriptor_set 0) { buffer layout(std430) bvab { uint VALENCE[]; }; }
storage_interface bif_fc (descriptor_set 0) { buffer layout(std430) bfcb { uint CAPFCNT[]; }; }
storage_interface bif_cc (descriptor_set 0) { buffer layout(std430) bccb2 { uint CAPCCNT[]; }; }
compute_interface ifc { storage { bif_ct bif_bv bif_va bif_fc bif_cc } inputs { layout(local_size_x = 64); } }
compute_shader cs_capcnt : ifc {                        // 1 cap face (valence corners) per beveled vert with deg>=3
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  uint cap = (BVERT[v] != 0u && VALENCE[v] >= 3u) ? 1u : 0u;
  CAPFCNT[v] = cap;
  CAPCCNT[v] = cap * VALENCE[v];
}
)S";
}
static std::string _cap_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_vi (descriptor_set 0) { buffer layout(std430) bvib { uint VID[]; }; }
storage_interface bif_P  (descriptor_set 0) { buffer layout(std430) bpb  { vec4 P[]; }; }
storage_interface bif_bv (descriptor_set 0) { buffer layout(std430) bbvb { uint BVERT[]; }; }
storage_interface bif_va (descriptor_set 0) { buffer layout(std430) bvab { uint VALENCE[]; }; }
storage_interface bif_lb (descriptor_set 0) { buffer layout(std430) blbb { uint LABEL[]; }; }
storage_interface bif_co (descriptor_set 0) { buffer layout(std430) bcob { uint CORNEROFF[]; }; }
storage_interface bif_cf (descriptor_set 0) { buffer layout(std430) bcfb { uint CFACE[]; }; }
storage_interface bif_fo (descriptor_set 0) { buffer layout(std430) bfob { uint FO[]; }; }
storage_interface bif_cb (descriptor_set 0) { buffer layout(std430) bcbb { uint CHAMBASE[]; }; }
storage_interface bif_fb (descriptor_set 0) { buffer layout(std430) bfbb { uint CAPFBASE[]; }; }
storage_interface bif_cc (descriptor_set 0) { buffer layout(std430) bccb3 { uint CAPCBASE[]; }; }
storage_interface bif_oP (descriptor_set 0) { buffer layout(std430) bopb { vec4 oP[]; }; }
storage_interface bif_ov (descriptor_set 0) { buffer layout(std430) bovb { uint oVID[]; }; }
storage_interface bif_of (descriptor_set 0) { buffer layout(std430) bofb { uint oFO[]; }; }
compute_interface ifc { storage { bif_ct bif_vi bif_P bif_bv bif_va bif_lb bif_co bif_cf bif_fo bif_cb bif_fb bif_cc bif_oP bif_ov bif_of }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_cap : ifc {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  if (BVERT[v] == 0u || VALENCE[v] < 3u) { return; }
  vec3 vP = P[v].xyz; vec3 vN = vec3(0.0);
  uint offs[32]; float ang[32]; uint noff = 0u;
  for (uint c = 0u; c < p_nc; c++) {                      // vertex normal over ALL faces; ring = SECTOR reps only
    if (VID[c] != v) { continue; }
    uint f = CFACE[c]; uint a = FO[f];
    vN += normalize(cross(P[VID[a + 1u]].xyz - P[VID[a]].xyz, P[VID[a + 2u]].xyz - P[VID[a]].xyz));
    if (LABEL[c] == c && noff < 32u) { offs[noff] = CORNEROFF[c]; noff++; }
  }
  vN = normalize(vN);
  vec3 rf = (abs(vN.x) < 0.9) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
  vec3 T1 = normalize(cross(vN, rf)); vec3 T2 = cross(vN, T1);
  for (uint j = 0u; j < noff; j++) { vec3 d = oP[offs[j]].xyz - vP; ang[j] = atan(dot(d, T2), dot(d, T1)); }
  for (uint i = 1u; i < noff; i++) {                     // insertion sort by angle (degree is small)
    float ka = ang[i]; uint kv = offs[i]; uint jj = i;
    while (jj > 0u && ang[jj - 1u] > ka) { ang[jj] = ang[jj - 1u]; offs[jj] = offs[jj - 1u]; jj--; }
    ang[jj] = ka; offs[jj] = kv;
  }
  vec3 cN = cross(oP[offs[1]].xyz - oP[offs[0]].xyz, oP[offs[2]].xyz - oP[offs[0]].xyz);
  bool rev = dot(cN, vN) < 0.0;                           // orient the cap to face the vertex normal (outward)
  uint echam   = CHAMBASE[p_nc];
  uint capface = p_nf + echam + CAPFBASE[v];
  uint capcorn = p_nc + 4u * echam + CAPCBASE[v];
  oFO[capface] = capcorn;
  for (uint j = 0u; j < noff; j++) { oVID[capcorn + j] = rev ? offs[noff - 1u - j] : offs[j]; }
}
)S";
}
static std::string _finalfo_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_cb (descriptor_set 0) { buffer layout(std430) bcbb { uint CHAMBASE[]; }; }
storage_interface bif_bs (descriptor_set 0) { buffer layout(std430) bbsb { uint VOFFBASE[]; }; }
storage_interface bif_fb (descriptor_set 0) { buffer layout(std430) bfbb { uint CAPFBASE[]; }; }
storage_interface bif_cc (descriptor_set 0) { buffer layout(std430) bccb4 { uint CAPCBASE[]; }; }
storage_interface bif_of (descriptor_set 0) { buffer layout(std430) bofb { uint oFO[]; }; }
storage_interface bif_hd (descriptor_set 0) { buffer layout(std430) bhdb { uint num_verts; uint num_corners; uint num_faces; uint flags; vec4 bbmin; vec4 bbmax; }; }
compute_interface ifc { storage { bif_ct bif_cb bif_bs bif_fb bif_cc bif_of bif_hd } inputs { layout(local_size_x = 64); } }
compute_shader cs_finalfo : ifc {                       // terminal CSR entry + the final GPU header counts (incl. caps)
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint echam   = CHAMBASE[p_nc];                         // p_nc == edge upper bound -> scan total
  uint voff    = VOFFBASE[p_nc];
  uint ncap    = CAPFBASE[p_nv];                         // cap scans run over nv verts
  uint capcorn = CAPCBASE[p_nv];
  uint corn = p_nc + 4u * echam + capcorn;
  uint face = p_nf + echam + ncap;
  oFO[face] = corn;
  num_verts = p_nv + voff; num_corners = corn; num_faces = face; flags = 0u;
  bbmin = vec4(-1e9); bbmax = vec4(1e9);
}
)S";
}
static std::string _copyv_text() {
  return R"S(
fxconfig fxcfg_default {}
)S" + _ctl_block() + R"S(
storage_interface bif_iP (descriptor_set 0) { buffer layout(std430) bipb { vec4 iP[]; }; }
storage_interface bif_iN (descriptor_set 0) { buffer layout(std430) binb { vec4 iN[]; }; }
storage_interface bif_iB (descriptor_set 0) { buffer layout(std430) bibb { vec4 iB[]; }; }
storage_interface bif_iU (descriptor_set 0) { buffer layout(std430) biub { vec4 iU[]; }; }
storage_interface bif_iC (descriptor_set 0) { buffer layout(std430) bicb { vec4 iC[]; }; }
storage_interface bif_oP (descriptor_set 0) { buffer layout(std430) bopb { vec4 oP[]; }; }
storage_interface bif_oN (descriptor_set 0) { buffer layout(std430) bonb { vec4 oN[]; }; }
storage_interface bif_oB (descriptor_set 0) { buffer layout(std430) bobb { vec4 oB[]; }; }
storage_interface bif_oU (descriptor_set 0) { buffer layout(std430) boub { vec4 oU[]; }; }
storage_interface bif_oC (descriptor_set 0) { buffer layout(std430) bocb { vec4 oC[]; }; }
compute_interface ifc { storage { bif_ct bif_iP bif_iN bif_iB bif_iU bif_iC bif_oP bif_oN bif_oB bif_oU bif_oC }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_copyv : ifc {                         // pass-through originals (non-beveled corners use them)
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
)S";
}

struct BevelInst : public MeshComputeInst {
  BevelInst(const BevelData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];

  // bevel preconditions/postconditions (hooks for the base template method preCheck/postCheck).
  void _doPreCheck(Context* ctx) final {                 // input must be a WELDED (manifold) mesh
    auto in = _srcMesh(_input);
    meshcheck::assertWelded(meshcheck::coincidentVerts(ctx, in->channel(MeshChannel::POSITION)->_ssbo, in->_num_verts), in->_num_verts, "bevel");
  }
  void _doPostCheck(Context* ctx) final {                // must not CREATE open/non-manifold edges (e.g. unsealed mixed verts)
    auto in   = _srcMesh(_input);
    auto inH  = meshcheck::edgeHealth(ctx, in->channel(MeshChannel::POSITION)->_ssbo, in->_vidx->_ssbo, in->_face_offsets->_ssbo,
                                      in->_num_verts, in->_num_corners, in->_num_faces);
    int out_nv = _nv + int(_voff_count);                 // bevel output positions are ready (synchronous gpuPasses)
    int out_nc = _nc + 4 * int(_echam_count) + int(_capcorn_count);
    int out_nf = _nf + int(_echam_count) + int(_ncap_count);
    auto outH = meshcheck::edgeHealth(ctx, _outch[MeshChannel::POSITION]->_ssbo, _ovid->_ssbo, _ofo->_ssbo, out_nv, out_nc, out_nf);
    meshcheck::assertNoCreatedDefects(inH, outH, "bevel");
  }

  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    if (not in->_vidx or not in->_face_offsets or not in->channel(MeshChannel::POSITION)) return false;
    preCheck(ctx);                                       // PRECHECK (gated on _precheck): input is a welded mesh
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    auto cs  = [&](const std::string& nm, const std::string& txt, const char* ep) {
      return fxi->computeShader(fxi->shaderFromShaderText(nm, txt), ep);
    };
    _cs_clearbv = cs("hmbev_clearbv", _clearbv_text(), "cs_clearbv");
    _cs_markbv  = cs("hmbev_markbv",  _markbv_text(),  "cs_markbv");
    _cs_voffcnt = cs("hmbev_voffcnt", _voffcnt_text(), "cs_voffcnt");
    _cs_cornoff = cs("hmbev_cornoff", _cornoff_text(), "cs_cornoff");
    _cs_labinit = cs("hmbev_labinit", _label_init_text(), "cs_label_init");
    _cs_labprop = cs("hmbev_labprop", _label_prop_text(), "cs_label_prop");
    _cs_coffpos = cs("hmbev_coffpos", _coffpos_text(), "cs_coffpos");
    _cs_secoff  = cs("hmbev_secoff",  _secoffset_text(), "cs_secoffset");
    _cs_shrink  = cs("hmbev_shrink",  _shrink_text(),  "cs_shrink");
    _cs_chamcnt = cs("hmbev_chamcnt", _chamcnt_text(), "cs_chamcnt");
    _cs_chamfer = cs("hmbev_chamfer", _chamfer_text(), "cs_chamfer");
    _cs_valence = cs("hmbev_valence", _valence_text(), "cs_valence");
    _cs_capcnt  = cs("hmbev_capcnt",  _capcnt_text(),  "cs_capcnt");
    _cs_cap     = cs("hmbev_cap",     _cap_text(),     "cs_cap");
    _cs_finalfo = cs("hmbev_finalfo", _finalfo_text(), "cs_finalfo");
    _cs_copyv   = cs("hmbev_copyv",   _copyv_text(),   "cs_copyv");
    _edges.init(ctx);
    alloc(ctx, in);
    syncBuildCount(ctx, in);                             // one-time: build on GPU + read back the vertex count
    _built = true; _built_nc = in->_num_corners;
    return true;
  }

  // host writes (must precede any dispatch phase — mid-dispatch host maps deadlock): edge scratch + the
  // control block (counts + runtime amount) + the scan control.
  void hostSetup(Context* ctx) {
    _edges.ensure(ctx, _nc, _nf);
    float amount = *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value);
    uint32_t ctl[8] = {uint32_t(_nv), uint32_t(_nc), uint32_t(_nf), uint32_t(_d->_slot & 31), 0u, 0u, 0u, 0u};
    std::memcpy(&ctl[4], &amount, 4);
    auto mc = ctx->FXI()->mapStorageBuffer(_ctl, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mc->_mappedaddr, ctl, sizeof(ctl)); ctx->FXI()->unmapStorageBuffer(mc.get());
    uint32_t sct[4] = {uint32_t(_nc), 0u, 0u, 0u};       // both scans run over nc (corner / edge upper bound)
    auto ms = ctx->FXI()->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); ctx->FXI()->unmapStorageBuffer(ms.get());
    auto ms2 = ctx->FXI()->mapStorageBuffer(_sct2, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms2->_mappedaddr, sct, sizeof(sct)); ctx->FXI()->unmapStorageBuffer(ms2.get());
    uint32_t sctv[4] = {uint32_t(_nv), 0u, 0u, 0u};      // cap scans run over nv verts
    for (auto* s : {_sct3, _sct4}) { auto m = ctx->FXI()->mapStorageBuffer(s, 0, sizeof(sctv), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, sctv, sizeof(sctv)); ctx->FXI()->unmapStorageBuffer(m.get()); }
  }
  // run the GPU topology pipeline once in its OWN frame/dispatch phase, then read back the offset-vert count
  // (the scan total). The dispatch phase blocks on the GPU at endDispatchPhase, so the count is ready -> this
  // is the "read back when finished" count, works even with a single compute (animated=False).
  void syncBuildCount(Context* ctx, gpumesh_ptr_t in) {
    hostSetup(ctx);
    ctx->beginFrame();
    ctx->CI()->beginDispatchPhase();
    gpuPasses(ctx, in);
    ctx->CI()->endDispatchPhase();
    ctx->endFrame();
    { auto m = ctx->FXI()->mapStorageBuffer(_sct, 0, 16, BufferMapAccess::READ_ONLY);
      _voff_count = reinterpret_cast<uint32_t*>(m->_mappedaddr)[1]; ctx->FXI()->unmapStorageBuffer(m.get()); }
    { auto m = ctx->FXI()->mapStorageBuffer(_sct2, 0, 16, BufferMapAccess::READ_ONLY);
      _echam_count = reinterpret_cast<uint32_t*>(m->_mappedaddr)[1]; ctx->FXI()->unmapStorageBuffer(m.get()); }
    { auto m = ctx->FXI()->mapStorageBuffer(_sct3, 0, 16, BufferMapAccess::READ_ONLY);
      _ncap_count = reinterpret_cast<uint32_t*>(m->_mappedaddr)[1]; ctx->FXI()->unmapStorageBuffer(m.get()); }
    { auto m = ctx->FXI()->mapStorageBuffer(_sct4, 0, 16, BufferMapAccess::READ_ONLY);
      _capcorn_count = reinterpret_cast<uint32_t*>(m->_mappedaddr)[1]; ctx->FXI()->unmapStorageBuffer(m.get()); }
    postCheck(ctx);                                      // POSTCHECK (gated on _postcheck): no created open/non-manifold edges
  }

  void alloc(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    _nv = in->_num_verts; _nc = in->_num_corners; _nf = in->_num_faces;
    int ovcap = _nv + _nc;                                // upper bound: +1 offset vert / corner
    auto dev = [&](size_t n) { return fxi->createStorageBuffer(std::max<size_t>(16, n * 4), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE); };
    _bvert    = dev(_nv);
    _voffcnt  = dev(_nc);
    _voffbase = dev(size_t(_nc) + 1);
    _corneroff = dev(_nc);
    _label    = dev(_nc);
    _coffpos  = dev(size_t(_nc) * 4); _coffnrm = dev(size_t(_nc) * 4);
    _chamcnt  = dev(_nc);                                 // edge upper bound == nc
    _chambase = dev(size_t(_nc) + 1);
    _valence  = dev(_nv);
    _capfcnt  = dev(_nv); _capccnt = dev(_nv);
    _capfbase = dev(size_t(_nv) + 1); _capcbase = dev(size_t(_nv) + 1);
    if (not _ctl)  _ctl  = fxi->createStorageBuffer(32);
    if (not _sct)  _sct  = fxi->createStorageBuffer(16);
    if (not _sct2) _sct2 = fxi->createStorageBuffer(16);
    if (not _sct3) _sct3 = fxi->createStorageBuffer(16);
    if (not _sct4) _sct4 = fxi->createStorageBuffer(16);
    auto out = _output->_value;
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, ovcap);
    // corners: nc shrunk + up to 4*nc chamfer + up to nc cap (cap corners = #offset verts <= nc)
    _ovid = env->_pool->acquireChannel(4, _nc + 4 * _nc + _nc);
    _ofo  = env->_pool->acquireChannel(4, _nf + _nc + _nv + 1);  // shrunk + chamfer (<=nc) + cap (<=nv) faces + terminal
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    _ovcap = ovcap;
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    int nc = in->_num_corners;
    _active = false;
    if (_built and nc == _built_nc)        _active = true;
    else if (_built and nc == _pending_nc) { alloc(ctx, in); syncBuildCount(ctx, in); _built_nc = nc; _pending_nc = -1; _active = true; }
    else if (_built)                       _pending_nc = nc;
    if (not _active) {                                   // PASSTHROUGH while topology settles
      out->_channels = in->_channels; out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets;
      out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    hostSetup(ctx);                                       // refresh the runtime amount (animatable)
    out->_channels.clear();
    for (auto c : kCh) out->_channels[c] = _outch[c];
    out->_faces.clear(); out->_edges.clear();
    out->_edge_table = nullptr; out->_edge_count = nullptr; out->_edge_cap = 0;
    out->_vidx = _ovid; out->_face_offsets = _ofo; out->_header = _header;
    out->_capacity = meshNextPow2(_ovcap);
    out->_num_verts   = _nv + int(_voff_count);
    out->_num_corners = _nc + 4 * int(_echam_count) + int(_capcorn_count);
    out->_num_faces   = _nf + int(_echam_count) + int(_ncap_count);
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _active) return;
    auto in = _srcMesh(_input);
    if (not in or not in->_vidx or not in->channel(MeshChannel::POSITION)) return;
    gpuPasses(inst->_impl.getShared<MeshEnv>()->_ctx, in);
  }
  // the GPU topology pipeline (dispatch phase): edge enum -> beveled-vert mark -> count/scan offset verts ->
  // remap shrunk faces -> offset geometry -> copy originals. Called per-frame from compute AND once
  // synchronously from onTopologyReady (for the count). MUST run inside a begin/endDispatchPhase.
  void gpuPasses(Context* ctx, gpumesh_ptr_t in) {
    auto ci   = ctx->CI();
    _edges.build(ctx, in);                               // edge table + corner->edge map (its own enumeration)
    ci->storageBarrier();
    auto etag = in->edge("__tags");                       // the upstream selection (edge ids match _edges')
    auto chb  = [&](MeshChannel c) { auto ch = in->channel(c); return ch ? ch->_ssbo : in->channel(MeshChannel::POSITION)->_ssbo; };
    auto P    = chb(MeshChannel::POSITION);
    auto Nin  = chb(MeshChannel::NORMAL);
    auto inB  = chb(MeshChannel::BINORMAL);
    auto inU  = chb(MeshChannel::UV0);
    auto inC  = chb(MeshChannel::COLOR);
    auto VID  = in->_vidx->_ssbo;
    auto FO   = in->_face_offsets->_ssbo;
    auto B    = [&](const FxComputeShader* cs) { ci->bindStorageBuffer(cs, 0, _ctl); };
    B(_cs_clearbv); ci->bindStorageBuffer(_cs_clearbv, 1, _bvert);
    ci->dispatchCompute(_cs_clearbv, (_nv + 63) / 64, 1, 1); ci->storageBarrier();
    B(_cs_markbv); ci->bindStorageBuffer(_cs_markbv, 1, _edges._ectl); ci->bindStorageBuffer(_cs_markbv, 2, _edges._edge);
    ci->bindStorageBuffer(_cs_markbv, 3, etag->_ssbo); ci->bindStorageBuffer(_cs_markbv, 4, _bvert);
    ci->dispatchCompute(_cs_markbv, (_edges._nc + 63) / 64, 1, 1); ci->storageBarrier();
    // SECTORING: label corners by sector (corners joined across NON-beveled edges share a label); min-propagate.
    B(_cs_labinit); ci->bindStorageBuffer(_cs_labinit, 1, _label);
    ci->dispatchCompute(_cs_labinit, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    for (int it = 0; it < 32; it++) {                    // valence-bounded monotone min-propagation
      B(_cs_labprop); ci->bindStorageBuffer(_cs_labprop, 1, VID); ci->bindStorageBuffer(_cs_labprop, 2, FO);
      ci->bindStorageBuffer(_cs_labprop, 3, _edges._cface); ci->bindStorageBuffer(_cs_labprop, 4, _edges._corner_edge);
      ci->bindStorageBuffer(_cs_labprop, 5, _edges._edge); ci->bindStorageBuffer(_cs_labprop, 6, etag->_ssbo);
      ci->bindStorageBuffer(_cs_labprop, 7, _bvert); ci->bindStorageBuffer(_cs_labprop, 8, _label);
      ci->dispatchCompute(_cs_labprop, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    }
    B(_cs_voffcnt); ci->bindStorageBuffer(_cs_voffcnt, 1, VID); ci->bindStorageBuffer(_cs_voffcnt, 2, _bvert);
    ci->bindStorageBuffer(_cs_voffcnt, 3, _label); ci->bindStorageBuffer(_cs_voffcnt, 4, _voffcnt);
    ci->dispatchCompute(_cs_voffcnt, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    _edges._scan.scan(ctx, _voffcnt, _voffbase, _sct); ci->storageBarrier();   // VOFFBASE; sct.total = #sectors
    B(_cs_cornoff); ci->bindStorageBuffer(_cs_cornoff, 1, VID); ci->bindStorageBuffer(_cs_cornoff, 2, _bvert);
    ci->bindStorageBuffer(_cs_cornoff, 3, _label); ci->bindStorageBuffer(_cs_cornoff, 4, _voffbase); ci->bindStorageBuffer(_cs_cornoff, 5, _corneroff);
    ci->dispatchCompute(_cs_cornoff, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    // per-corner offset position (slide along un-beveled edge) -> per-sector average -> the shared offset vert
    B(_cs_coffpos); ci->bindStorageBuffer(_cs_coffpos, 1, FO); ci->bindStorageBuffer(_cs_coffpos, 2, VID); ci->bindStorageBuffer(_cs_coffpos, 3, P);
    ci->bindStorageBuffer(_cs_coffpos, 4, _bvert); ci->bindStorageBuffer(_cs_coffpos, 5, _edges._corner_edge); ci->bindStorageBuffer(_cs_coffpos, 6, etag->_ssbo);
    ci->bindStorageBuffer(_cs_coffpos, 7, _coffpos); ci->bindStorageBuffer(_cs_coffpos, 8, _coffnrm);
    ci->dispatchCompute(_cs_coffpos, (_nf + 63) / 64, 1, 1); ci->storageBarrier();
    B(_cs_secoff); ci->bindStorageBuffer(_cs_secoff, 1, VID); ci->bindStorageBuffer(_cs_secoff, 2, _bvert); ci->bindStorageBuffer(_cs_secoff, 3, _label);
    ci->bindStorageBuffer(_cs_secoff, 4, _voffbase); ci->bindStorageBuffer(_cs_secoff, 5, _coffpos); ci->bindStorageBuffer(_cs_secoff, 6, _coffnrm);
    ci->bindStorageBuffer(_cs_secoff, 7, inB); ci->bindStorageBuffer(_cs_secoff, 8, inU); ci->bindStorageBuffer(_cs_secoff, 9, inC);
    ci->bindStorageBuffer(_cs_secoff, 10, _outch[MeshChannel::POSITION]->_ssbo); ci->bindStorageBuffer(_cs_secoff, 11, _outch[MeshChannel::NORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_secoff, 12, _outch[MeshChannel::BINORMAL]->_ssbo); ci->bindStorageBuffer(_cs_secoff, 13, _outch[MeshChannel::UV0]->_ssbo);
    ci->bindStorageBuffer(_cs_secoff, 14, _outch[MeshChannel::COLOR]->_ssbo);
    ci->dispatchCompute(_cs_secoff, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    B(_cs_shrink); ci->bindStorageBuffer(_cs_shrink, 1, _corneroff); ci->bindStorageBuffer(_cs_shrink, 2, FO);
    ci->bindStorageBuffer(_cs_shrink, 3, _ovid->_ssbo); ci->bindStorageBuffer(_cs_shrink, 4, _ofo->_ssbo);
    ci->dispatchCompute(_cs_shrink, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    // B2: chamfer quads — count selected interior edges -> scan -> scatter one quad each (winding by construction)
    B(_cs_chamcnt); ci->bindStorageBuffer(_cs_chamcnt, 1, _edges._ectl); ci->bindStorageBuffer(_cs_chamcnt, 2, _edges._edge);
    ci->bindStorageBuffer(_cs_chamcnt, 3, etag->_ssbo); ci->bindStorageBuffer(_cs_chamcnt, 4, _chamcnt);
    ci->dispatchCompute(_cs_chamcnt, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    _edges._scan.scan(ctx, _chamcnt, _chambase, _sct2); ci->storageBarrier();   // CHAMBASE; sct2.total = Echam
    B(_cs_chamfer); ci->bindStorageBuffer(_cs_chamfer, 1, _edges._ectl); ci->bindStorageBuffer(_cs_chamfer, 2, _edges._edge);
    ci->bindStorageBuffer(_cs_chamfer, 3, etag->_ssbo); ci->bindStorageBuffer(_cs_chamfer, 4, _chambase); ci->bindStorageBuffer(_cs_chamfer, 5, VID);
    ci->bindStorageBuffer(_cs_chamfer, 6, FO); ci->bindStorageBuffer(_cs_chamfer, 7, _corneroff);
    ci->bindStorageBuffer(_cs_chamfer, 8, _ovid->_ssbo); ci->bindStorageBuffer(_cs_chamfer, 9, _ofo->_ssbo);
    ci->dispatchCompute(_cs_chamfer, (_nc + 63) / 64, 1, 1); ci->storageBarrier();
    // Stage C: corner caps — valence -> count -> scan (faces + corners) -> per-vertex gather/sort/emit
    B(_cs_valence); ci->bindStorageBuffer(_cs_valence, 1, VID); ci->bindStorageBuffer(_cs_valence, 2, _bvert);
    ci->bindStorageBuffer(_cs_valence, 3, _label); ci->bindStorageBuffer(_cs_valence, 4, _valence);
    ci->dispatchCompute(_cs_valence, (_nv + 63) / 64, 1, 1); ci->storageBarrier();
    B(_cs_capcnt); ci->bindStorageBuffer(_cs_capcnt, 1, _bvert); ci->bindStorageBuffer(_cs_capcnt, 2, _valence);
    ci->bindStorageBuffer(_cs_capcnt, 3, _capfcnt); ci->bindStorageBuffer(_cs_capcnt, 4, _capccnt);
    ci->dispatchCompute(_cs_capcnt, (_nv + 63) / 64, 1, 1); ci->storageBarrier();
    _edges._scan.scan(ctx, _capfcnt, _capfbase, _sct3); ci->storageBarrier();   // CAPFBASE; sct3.total = Ncap
    _edges._scan.scan(ctx, _capccnt, _capcbase, _sct4); ci->storageBarrier();   // CAPCBASE; sct4.total = capcorn
    B(_cs_cap); ci->bindStorageBuffer(_cs_cap, 1, VID); ci->bindStorageBuffer(_cs_cap, 2, P); ci->bindStorageBuffer(_cs_cap, 3, _bvert);
    ci->bindStorageBuffer(_cs_cap, 4, _valence); ci->bindStorageBuffer(_cs_cap, 5, _label); ci->bindStorageBuffer(_cs_cap, 6, _corneroff);
    ci->bindStorageBuffer(_cs_cap, 7, _edges._cface); ci->bindStorageBuffer(_cs_cap, 8, FO); ci->bindStorageBuffer(_cs_cap, 9, _chambase);
    ci->bindStorageBuffer(_cs_cap, 10, _capfbase); ci->bindStorageBuffer(_cs_cap, 11, _capcbase);
    ci->bindStorageBuffer(_cs_cap, 12, _outch[MeshChannel::POSITION]->_ssbo);
    ci->bindStorageBuffer(_cs_cap, 13, _ovid->_ssbo); ci->bindStorageBuffer(_cs_cap, 14, _ofo->_ssbo);
    ci->dispatchCompute(_cs_cap, (_nv + 63) / 64, 1, 1); ci->storageBarrier();
    B(_cs_finalfo); ci->bindStorageBuffer(_cs_finalfo, 1, _chambase); ci->bindStorageBuffer(_cs_finalfo, 2, _voffbase);
    ci->bindStorageBuffer(_cs_finalfo, 3, _capfbase); ci->bindStorageBuffer(_cs_finalfo, 4, _capcbase);
    ci->bindStorageBuffer(_cs_finalfo, 5, _ofo->_ssbo); ci->bindStorageBuffer(_cs_finalfo, 6, _header);
    ci->dispatchCompute(_cs_finalfo, 1, 1, 1); ci->storageBarrier();
    B(_cs_copyv); ci->bindStorageBuffer(_cs_copyv, 1, P); ci->bindStorageBuffer(_cs_copyv, 2, Nin); ci->bindStorageBuffer(_cs_copyv, 3, inB);
    ci->bindStorageBuffer(_cs_copyv, 4, inU); ci->bindStorageBuffer(_cs_copyv, 5, inC);
    ci->bindStorageBuffer(_cs_copyv, 6, _outch[MeshChannel::POSITION]->_ssbo); ci->bindStorageBuffer(_cs_copyv, 7, _outch[MeshChannel::NORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_copyv, 8, _outch[MeshChannel::BINORMAL]->_ssbo); ci->bindStorageBuffer(_cs_copyv, 9, _outch[MeshChannel::UV0]->_ssbo);
    ci->bindStorageBuffer(_cs_copyv, 10, _outch[MeshChannel::COLOR]->_ssbo);
    ci->dispatchCompute(_cs_copyv, (_nv + 63) / 64, 1, 1); ci->storageBarrier();
  }
  const BevelData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  MeshEdges _edges;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _ovid, _ofo;
  FxShaderStorageBuffer *_bvert = nullptr, *_voffcnt = nullptr, *_voffbase = nullptr, *_corneroff = nullptr,
                        *_chamcnt = nullptr, *_chambase = nullptr, *_valence = nullptr, *_capfcnt = nullptr,
                        *_capccnt = nullptr, *_capfbase = nullptr, *_capcbase = nullptr,
                        *_label = nullptr, *_coffpos = nullptr, *_coffnrm = nullptr,
                        *_ctl = nullptr, *_sct = nullptr, *_sct2 = nullptr, *_sct3 = nullptr, *_sct4 = nullptr, *_header = nullptr;
  const FxComputeShader *_cs_clearbv = nullptr, *_cs_markbv = nullptr, *_cs_voffcnt = nullptr, *_cs_cornoff = nullptr,
                        *_cs_labinit = nullptr, *_cs_labprop = nullptr, *_cs_coffpos = nullptr, *_cs_secoff = nullptr,
                        *_cs_shrink = nullptr, *_cs_chamcnt = nullptr, *_cs_chamfer = nullptr,
                        *_cs_valence = nullptr, *_cs_capcnt = nullptr, *_cs_cap = nullptr,
                        *_cs_finalfo = nullptr, *_cs_copyv = nullptr;
  int _nv = 0, _nc = 0, _nf = 0, _ovcap = 0;
  uint32_t _voff_count = 0, _echam_count = 0, _ncap_count = 0, _capcorn_count = 0;   // counts read back once at build
  int _built_nc = -1, _pending_nc = -1;
  bool _built = false, _active = false;
};
const MeshChannel BevelInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeBevelIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amount")->setValue(0.1f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
BevelData::BevelData() {}
std::shared_ptr<BevelData> BevelData::createShared() {
  auto d = std::make_shared<BevelData>();
  _reshapeBevelIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t BevelData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<BevelInst>(this, g);
}
void BevelData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return BevelData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeBevelIOs(m); });
  clazz->directProperty("slot", &BevelData::_slot);
  clazz->directVectorProperty("part_masks", &BevelData::_part_masks);
}

} // namespace ork::lev2::hypermesh
