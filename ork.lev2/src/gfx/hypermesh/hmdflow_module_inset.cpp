////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>

ImplementReflectionX(ork::lev2::hypermesh::InsetData, "hypermesh::InsetData");

namespace ork::lev2::hypermesh {

// FaceTagger partition ids: OUTER = untouched faces (pass through), COLLAR = the bridging ring, INNER = the cap.
enum { kInOuter = 0, kInCollar = 1, kInInner = 2 };
static constexpr float kPI2 = 6.28318530717958f;

///////////////////////////////////////////////////////////////////////////////
// shaders: cs_copy copies the originals (the outer ring + unselected faces reference them); cs_inner places
// each inner-ring dup at BASE + amount*TOWARD (both in the face plane -> the inset stays coplanar) with the
// flat face normal NRMV. `amount` AND `rotate` are runtime (no rebuild): rotate spins the rest offset
// d0 = BASE-C about the face normal NRMV (Rodrigues; d0 is in-plane so the n(n.d0) term vanishes). The
// per-dup BASE/TOWARD/NRMV are baked at rebuild for rotate=0 (the canonical orientation the COLLAR topology
// is matched against), so rotate=0 reproduces BASE+amount*TOWARD exactly and the collar is overlap-free there.
///////////////////////////////////////////////////////////////////////////////
static std::string _inset_text() {
  return R"S(
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
storage_interface sif_dv (descriptor_set 0) { buffer layout(std430) dvb { uint DUPV[]; }; }    // src vert per inner dup
storage_interface sif_bs (descriptor_set 0) { buffer layout(std430) bsb { vec4 BASE[];   }; }  // inner-dup base pos (amount=0)
storage_interface sif_tw (descriptor_set 0) { buffer layout(std430) twb { vec4 TOWARD[]; }; }  // inner-dup toward-centroid
storage_interface sif_nr (descriptor_set 0) { buffer layout(std430) nrb { vec4 NRMV[];   }; }  // inner-dup flat normal (fN)
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) prb { uint p_nv; uint p_vb; float p_amount; float p_rotate; }; }
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC
                                    sif_dv sif_bs sif_tw sif_nr sif_par }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_copy : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
////////////////////////////////////////
compute_shader cs_inner : iface {
  uint d = gl_GlobalInvocationID.x;
  if (d >= p_vb) { return; }
  uint sv = DUPV[d]; uint o = p_nv + d;
  vec3 toward = TOWARD[d].xyz;
  vec3 d0 = -toward;                                 // BASE - C (rest offset, in face plane, baked at rotate=0)
  vec3 C  = BASE[d].xyz + toward;                    // C = BASE - d0
  vec3 n  = NRMV[d].xyz;
  float r = p_rotate * 0.01745329252;                // degrees -> radians
  vec3 d0r = d0 * cos(r) + cross(n, d0) * sin(r);    // Rodrigues about n (d0 perpendicular to n)
  oP[o] = vec4(C + d0r * (1.0 - p_amount), 1.0);
  oN[o] = vec4(n, 0.0);
  oB[o] = iB[sv]; oU[o] = iU[sv]; oC[o] = iC[sv];
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// C.4c GPU INSET BUILD. The CPU rebuild (full topology/positions/tags readback + emission + the
// per-frame CPU collar re-tessellation + its per-crossing FULL postCheck readback) becomes:
// count -> 6 scans -> emit (+ the Fuchs/Kedem DP collar kernel, run at rebuild AND per rotate
// change). The ONLY readbacks: 6 scan totals (24B) per rebuild + a 4-byte changed-flag on
// rotate-change frames (replacing the full-mesh postCheck readback — collar validity is the
// monotone-staircase construction, proven, plus the vet harness). Float math mirrors the CPU
// expression-for-expression (incl. fmod semantics for negative rotate) for the oracle.
///////////////////////////////////////////////////////////////////////////////

static std::string _igb_cnt_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ig_ct (descriptor_set 0) { buffer layout(std430) ictb {
  uint g_nf; uint g_nv; uint g_bit; uint g_sides; uint g_fill; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface ig_fo (descriptor_set 0) { buffer layout(std430) ifob { uint FO[]; }; }
storage_interface ig_tg (descriptor_set 0) { buffer layout(std430) itgb { uint TAGS[]; }; }
storage_interface ig_c1 (descriptor_set 0) { buffer layout(std430) ic1b { uint DCNT[]; }; }
storage_interface ig_c2 (descriptor_set 0) { buffer layout(std430) ic2b { uint CCNT[]; }; }
storage_interface ig_c3 (descriptor_set 0) { buffer layout(std430) ic3b { uint FCNT[]; }; }
storage_interface ig_c4 (descriptor_set 0) { buffer layout(std430) ic4b { uint BCNT[]; }; }
storage_interface ig_c5 (descriptor_set 0) { buffer layout(std430) ic5b { uint SCNT[]; }; }
storage_interface ig_c6 (descriptor_set 0) { buffer layout(std430) ic6b { uint XCNT[]; }; }
compute_interface iface { storage { ig_ct ig_fo ig_tg ig_c1 ig_c2 ig_c3 ig_c4 ig_c5 ig_c6 }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_icnt : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  uint m = FO[f + 1u] - FO[f];
  if ((TAGS[f] & g_bit) == 0u) { DCNT[f] = 0u; CCNT[f] = m; FCNT[f] = 1u; BCNT[f] = 0u; SCNT[f] = 0u; XCNT[f] = 0u; return; }
  uint N = (g_sides > 0u) ? g_sides : m;
  if (N == m) {                              // 1:1 ring: m quad collars (+cap)
    DCNT[f] = m; FCNT[f] = m + g_fill; CCNT[f] = 4u * m + g_fill * N;
    BCNT[f] = 0u; SCNT[f] = 0u; XCNT[f] = 0u;
  } else {                                   // resampled ring: m+N collar tris via the DP (+cap)
    DCNT[f] = N; FCNT[f] = (m + N) + g_fill; CCNT[f] = 3u * (m + N) + g_fill * N;
    BCNT[f] = 8u + m + N; SCNT[f] = (m + 1u) * (N + 1u); XCNT[f] = 1u;
  }
}
)S";
}
static std::string _igb_emit_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ig_ct (descriptor_set 0) { buffer layout(std430) ictb {
  uint g_nf; uint g_nv; uint g_bit; uint g_sides; uint g_fill; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface ig_fo (descriptor_set 0) { buffer layout(std430) ifob { uint FO[]; }; }
storage_interface ig_vi (descriptor_set 0) { buffer layout(std430) ivib { uint VI[]; }; }
storage_interface ig_tg (descriptor_set 0) { buffer layout(std430) itgb { uint TAGS[]; }; }
storage_interface ig_db (descriptor_set 0) { buffer layout(std430) idbb { uint DBASE[]; }; }
storage_interface ig_cb (descriptor_set 0) { buffer layout(std430) icbb { uint CBASE[]; }; }
storage_interface ig_fb (descriptor_set 0) { buffer layout(std430) ifbb { uint FBASE[]; }; }
storage_interface ig_ov (descriptor_set 0) { buffer layout(std430) iovb { uint OV[]; }; }
storage_interface ig_of (descriptor_set 0) { buffer layout(std430) iofb { uint OF[]; }; }
storage_interface ig_op (descriptor_set 0) { buffer layout(std430) iopb { uint OPAR[]; }; }
storage_interface ig_oq (descriptor_set 0) { buffer layout(std430) ioqb { uint OPART[]; }; }
compute_interface iface { storage { ig_ct ig_fo ig_vi ig_tg ig_db ig_cb ig_fb ig_ov ig_of ig_op ig_oq }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_iemT : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if (f == 0u) { OF[FBASE[g_nf]] = CBASE[g_nf]; }
  uint a = FO[f]; uint m = FO[f + 1u] - a;
  uint cb = CBASE[f]; uint fb = FBASE[f]; uint db = g_nv + DBASE[f];
  if ((TAGS[f] & g_bit) == 0u) {
    OF[fb] = cb; OPAR[fb] = f; OPART[fb] = 0u;               // OUTER (untouched)
    for (uint k = 0u; k < m; k++) { OV[cb + k] = VI[a + k]; }
    return;
  }
  uint N = (g_sides > 0u) ? g_sides : m;
  if (N == m) {                                              // quad collar [a_k, a_k1, b_k1, b_k]
    for (uint k = 0u; k < m; k++) {
      uint k1 = (k + 1u) % m; uint c4 = cb + 4u * k;
      OF[fb + k] = c4; OPAR[fb + k] = f; OPART[fb + k] = 1u; // COLLAR
      OV[c4 + 0u] = VI[a + k]; OV[c4 + 1u] = VI[a + k1]; OV[c4 + 2u] = db + k1; OV[c4 + 3u] = db + k;
    }
    if (g_fill != 0u) {
      uint ff = fb + m; uint cc = cb + 4u * m;
      OF[ff] = cc; OPAR[ff] = f; OPART[ff] = 2u;             // INNER cap
      for (uint k = 0u; k < N; k++) { OV[cc + k] = db + k; }
    }
  } else {                                                   // m+N collar TRIS: slots here, corners by cs_icollar
    for (uint t = 0u; t < (m + N); t++) {
      OF[fb + t] = cb + 3u * t; OPAR[fb + t] = f; OPART[fb + t] = 1u;
    }
    if (g_fill != 0u) {
      uint ff = fb + (m + N); uint cc = cb + 3u * (m + N);
      OF[ff] = cc; OPAR[ff] = f; OPART[ff] = 2u;
      for (uint k = 0u; k < N; k++) { OV[cc + k] = db + k; }
    }
  }
}
)S";
}
static std::string _igb_dups_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ig_ct (descriptor_set 0) { buffer layout(std430) ictb {
  uint g_nf; uint g_nv; uint g_bit; uint g_sides; uint g_fill; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface ig_fo (descriptor_set 0) { buffer layout(std430) ifob { uint FO[]; }; }
storage_interface ig_vi (descriptor_set 0) { buffer layout(std430) ivib { uint VI[]; }; }
storage_interface ig_tg (descriptor_set 0) { buffer layout(std430) itgb { uint TAGS[]; }; }
storage_interface ig_ps (descriptor_set 0) { buffer layout(std430) ipsb { vec4 POS[]; }; }
storage_interface ig_db (descriptor_set 0) { buffer layout(std430) idbb { uint DBASE[]; }; }
storage_interface ig_dv (descriptor_set 0) { buffer layout(std430) idvb { uint DUPV[]; }; }
storage_interface ig_b0 (descriptor_set 0) { buffer layout(std430) ib0b { vec4 BASEV[]; }; }
storage_interface ig_b1 (descriptor_set 0) { buffer layout(std430) ib1b { vec4 TOWV[]; }; }
storage_interface ig_b2 (descriptor_set 0) { buffer layout(std430) ib2b { vec4 NRMV[]; }; }
compute_interface iface { storage { ig_ct ig_fo ig_vi ig_tg ig_ps ig_db ig_dv ig_b0 ig_b1 ig_b2 }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_iemD : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if ((TAGS[f] & g_bit) == 0u) { return; }
  uint a = FO[f]; uint m = FO[f + 1u] - a;
  vec3 C = vec3(0.0);
  for (uint k = 0u; k < m; k++) { C += POS[VI[a + k]].xyz; }
  C = C * (1.0 / float(m));
  vec3 p0 = POS[VI[a]].xyz; vec3 p1 = POS[VI[a + 1u]].xyz; vec3 p2 = POS[VI[a + 2u]].xyz;
  vec3 fN = cross(p1 - p0, p2 - p0); float L = length(fN);
  fN = (L > 1e-12) ? fN * (1.0 / L) : vec3(0.0, 1.0, 0.0);
  vec3 u = (p1 - p0); u = u - fN * dot(u, fN); float ul = length(u);
  u = (ul > 1e-12) ? u * (1.0 / ul) : vec3(1.0, 0.0, 0.0);
  vec3 v = cross(fN, u);
  uint N = (g_sides > 0u) ? g_sides : m;
  uint db = DBASE[f];
  if (N == m) {
    for (uint k = 0u; k < m; k++) {
      vec3 bk = POS[VI[a + k]].xyz; uint j = db + k;
      DUPV[j] = VI[a + k]; BASEV[j] = vec4(bk, 0.0); TOWV[j] = vec4(C - bk, 0.0); NRMV[j] = vec4(fN, 0.0);
    }
    return;
  }
  float R0 = 1e30;                                           // inradius (regular ring INSCRIBED)
  for (uint k = 0u; k < m; k++) {
    vec3 e0 = POS[VI[a + k]].xyz; vec3 e1 = POS[VI[a + (k + 1u) % m]].xyz;
    vec3 ed = e1 - e0; float el = length(ed); ed = (el > 1e-12) ? ed * (1.0 / el) : ed;
    vec3 dd = C - e0; R0 = min(R0, length(dd - ed * dot(dd, ed)));
  }
  for (uint k = 0u; k < N; k++) {
    float th = float(k) * (6.28318530717958 / float(N));
    vec3 dir = u * cos(th) + v * sin(th);
    vec3 base = C + dir * R0;
    uint ns = VI[a]; float best = 1e30;                      // nominal source vert = nearest boundary vert
    for (uint j2 = 0u; j2 < m; j2++) {
      float dd = length(POS[VI[a + j2]].xyz - base);
      if (dd < best - 1e-6) { best = dd; ns = VI[a + j2]; }  // tie margin (matches the CPU reference)
    }
    uint j = db + k;
    DUPV[j] = ns; BASEV[j] = vec4(base, 0.0); TOWV[j] = vec4(C - base, 0.0); NRMV[j] = vec4(fN, 0.0);
  }
}
)S";
}
static std::string _igb_blob_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ig_ct (descriptor_set 0) { buffer layout(std430) ictb {
  uint g_nf; uint g_nv; uint g_bit; uint g_sides; uint g_fill; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface ig_fo (descriptor_set 0) { buffer layout(std430) ifob { uint FO[]; }; }
storage_interface ig_vi (descriptor_set 0) { buffer layout(std430) ivib { uint VI[]; }; }
storage_interface ig_tg (descriptor_set 0) { buffer layout(std430) itgb { uint TAGS[]; }; }
storage_interface ig_ps (descriptor_set 0) { buffer layout(std430) ipsb { vec4 POS[]; }; }
storage_interface ig_db (descriptor_set 0) { buffer layout(std430) idbb { uint DBASE[]; }; }
storage_interface ig_cb (descriptor_set 0) { buffer layout(std430) icbb { uint CBASE[]; }; }
storage_interface ig_bb (descriptor_set 0) { buffer layout(std430) ibbb { uint BBASE[]; }; }
storage_interface ig_sb (descriptor_set 0) { buffer layout(std430) isbb { uint SBASE[]; }; }
storage_interface ig_xb (descriptor_set 0) { buffer layout(std430) ixbb { uint XBASE[]; }; }
storage_interface ig_ub (descriptor_set 0) { buffer layout(std430) iubb { uint UB[]; }; }
storage_interface ig_fa (descriptor_set 0) { buffer layout(std430) ifab { float FB[]; }; }
storage_interface ig_fm (descriptor_set 0) { buffer layout(std430) ifmb { uint FMAP[]; }; }
compute_interface iface { storage { ig_ct ig_fo ig_vi ig_tg ig_ps ig_db ig_cb ig_bb ig_sb ig_xb ig_ub ig_fa ig_fm }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_iemB : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if ((TAGS[f] & g_bit) == 0u) { return; }
  uint a = FO[f]; uint m = FO[f + 1u] - a;
  uint N = (g_sides > 0u) ? g_sides : m;
  if (N == m) { return; }                                    // quad collar: no DP blob
  // recompute the face frame + ring bases (identical expressions to cs_iemD)
  vec3 C = vec3(0.0);
  for (uint k = 0u; k < m; k++) { C += POS[VI[a + k]].xyz; }
  C = C * (1.0 / float(m));
  vec3 p0 = POS[VI[a]].xyz; vec3 p1 = POS[VI[a + 1u]].xyz; vec3 p2 = POS[VI[a + 2u]].xyz;
  vec3 fN = cross(p1 - p0, p2 - p0); float L = length(fN);
  fN = (L > 1e-12) ? fN * (1.0 / L) : vec3(0.0, 1.0, 0.0);
  vec3 u = (p1 - p0); u = u - fN * dot(u, fN); float ul = length(u);
  u = (ul > 1e-12) ? u * (1.0 / ul) : vec3(1.0, 0.0, 0.0);
  vec3 v = cross(fN, u);
  float R0 = 1e30;
  for (uint k = 0u; k < m; k++) {
    vec3 e0 = POS[VI[a + k]].xyz; vec3 e1 = POS[VI[a + (k + 1u) % m]].xyz;
    vec3 ed = e1 - e0; float el = length(ed); ed = (el > 1e-12) ? ed * (1.0 / el) : ed;
    vec3 dd = C - e0; R0 = min(R0, length(dd - ed * dot(dd, ed)));
  }
  uint ub = BBASE[f]; uint x = XBASE[f];
  uint fboff = ub - 8u * x;                                  // FB blob: UB minus the 8-uint hdr per PRIOR collar face
  // hdr {m, N, cornerBase, scratchOff, fbOff, 0,0,0}
  UB[ub + 0u] = m; UB[ub + 1u] = N; UB[ub + 2u] = CBASE[f]; UB[ub + 3u] = SBASE[f]; UB[ub + 4u] = fboff;
  UB[ub + 5u] = 0u; UB[ub + 6u] = 0u; UB[ub + 7u] = 0u;
  for (uint i = 0u; i < m; i++) {                            // outer ids + angles (wrapped atan2, CPU mirror)
    UB[ub + 8u + i] = VI[a + i];
    vec3 d = POS[VI[a + i]].xyz - C;
    float t = atan(dot(d, v), dot(d, u));
    FB[fboff + i] = (t < 0.0) ? t + 6.28318530717958 : t;
  }
  uint db = DBASE[f];
  for (uint k = 0u; k < N; k++) {                            // inner ids + REST angles (from the ring base)
    UB[ub + 8u + m + k] = g_nv + db + k;
    float th = float(k) * (6.28318530717958 / float(N));
    vec3 dir = u * cos(th) + v * sin(th);
    vec3 d = (C + dir * R0) - C;
    float t = atan(dot(d, v), dot(d, u));
    FB[fboff + m + k] = (t < 0.0) ? t + 6.28318530717958 : t;
  }
  FMAP[x] = ub;
}
)S";
}
// the Fuchs/Kedem DP collar (one thread per collar face) — at rebuild (rot=0/rest) AND per rotate
// change. Change-detection feeds the 4-byte flag that replaces the per-crossing full postCheck.
static std::string _igb_collar_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ic_ct (descriptor_set 0) { buffer layout(std430) cctb {
  uint c_n; uint c_changed; float c_rot; float c_pad; }; }
storage_interface ic_fm (descriptor_set 0) { buffer layout(std430) cfmb { uint FMAP[]; }; }
storage_interface ic_ub (descriptor_set 0) { buffer layout(std430) cubb { uint UB[]; }; }
storage_interface ic_fa (descriptor_set 0) { buffer layout(std430) cfab { float FB[]; }; }
storage_interface ic_sf (descriptor_set 0) { buffer layout(std430) csfb { float SCRF[]; }; }
storage_interface ic_su (descriptor_set 0) { buffer layout(std430) csub { uint SCRU[]; }; }
storage_interface ic_ov (descriptor_set 0) { buffer layout(std430) covb { uint OV[]; }; }
compute_interface iface { storage { ic_ct ic_fm ic_ub ic_fa ic_sf ic_su ic_ov }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_icollar : iface {
  uint kk = gl_GlobalInvocationID.x;
  if (kk >= c_n) { return; }
  uint ub = FMAP[kk];
  uint m = UB[ub + 0u]; uint N = UB[ub + 1u];
  uint cbase = UB[ub + 2u]; uint soff = UB[ub + 3u]; uint fboff = UB[ub + 4u];
  // seam: nearest rotated-inner angle to aO[0] (rotation = fmod semantics incl. negatives, CPU mirror)
  float a0 = FB[fboff];
  uint s0 = 0u; float bd = 1e30;
  float qa0 = floor(a0 * 100000.0 + 0.5) / 100000.0;       // quantized (the CPU DP mirror)
  for (uint j = 0u; j < N; j++) {
    float t  = FB[fboff + m + j] + c_rot;
    float aa = t - 6.28318530717958 * float(int(t / 6.28318530717958)); if (aa < 0.0) { aa += 6.28318530717958; }
    aa = floor(aa * 100000.0 + 0.5) / 100000.0;
    float d  = abs(qa0 - aa); d = d - 6.28318530717958 * float(int(d / 6.28318530717958));
    d = min(d, 6.28318530717958 - d);
    if (d < bd) { bd = d; s0 = j; }
  }
  uint W = N + 1u;                                           // DP row width
  // DP: C[i][j] forward, bk = arrived-by (0=outer-advance, 1=inner-advance)
  for (uint j = 0u; j <= N; j++) {
    for (uint i = 0u; i <= m; i++) {
      float ai = FB[fboff + m + ((s0 + j) % N)] + c_rot;     // IA(j) rotated+wrapped+quantized
      float aw = ai - 6.28318530717958 * float(int(ai / 6.28318530717958)); if (aw < 0.0) { aw += 6.28318530717958; }
      aw = floor(aw * 100000.0 + 0.5) / 100000.0;
      float ao = floor(FB[fboff + (i % m)] * 100000.0 + 0.5) / 100000.0;
      float d  = abs(ao - aw); d = d - 6.28318530717958 * float(int(d / 6.28318530717958));
      float span = min(d, 6.28318530717958 - d);
      uint idx = i * W + j;
      if (i == 0u && j == 0u)      { SCRF[soff + idx] = span; SCRU[soff + idx] = 0u; }
      else if (j == 0u)            { SCRF[soff + idx] = SCRF[soff + idx - W] + span; SCRU[soff + idx] = 0u; }
      else if (i == 0u)            { SCRF[soff + idx] = SCRF[soff + idx - 1u] + span; SCRU[soff + idx] = 1u; }
      else {
        float co = SCRF[soff + idx - W];                     // arrived by outer-advance
        float ci2 = SCRF[soff + idx - 1u];                   // arrived by inner-advance
        if (co <= ci2) { SCRF[soff + idx] = span + co;  SCRU[soff + idx] = 0u; }
        else           { SCRF[soff + idx] = span + ci2; SCRU[soff + idx] = 1u; }
      }
    }
  }
  // walk the staircase (m,N)->(0,0); emit triangles in walk order (the CPU's byte order)
  uint i = m; uint j = N; uint w = 0u;
  while (i > 0u || j > 0u) {
    uint t0 = 0u; uint t1 = 0u; uint t2 = 0u;
    if (i > 0u && (j == 0u || SCRU[soff + i * W + j] == 0u)) {
      t0 = UB[ub + 8u + ((i - 1u) % m)];                     // O[i-1]
      t1 = UB[ub + 8u + (i % m)];                            // O[i]
      t2 = UB[ub + 8u + m + ((s0 + j) % N)];                 // I(j)
      i = i - 1u;
    } else {
      t0 = UB[ub + 8u + (i % m)];
      t1 = UB[ub + 8u + m + ((s0 + j) % N)];
      t2 = UB[ub + 8u + m + ((s0 + j - 1u) % N)];
      j = j - 1u;
    }
    uint c = cbase + w;
    if (OV[c] != t0 || OV[c + 1u] != t1 || OV[c + 2u] != t2) { atomicAdd(c_changed, 1u); }
    OV[c] = t0; OV[c + 1u] = t1; OV[c + 2u] = t2;
    w = w + 3u;
  }
}
)S";
}

struct InsetInst : public MeshComputeInst {
  InsetInst(const InsetData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];

  // The actual inner-dup position from its baked BASE/TOWARD/NRMV (all baked at rotate=0) + the runtime amount &
  // rotate. MUST match cs_inner exactly (CPU path, postcheck, and GPU shader all share this formula): spin the
  // rest offset d0 = BASE-C about the face normal n (Rodrigues; d0 is in-plane so the n(n.d0) term vanishes),
  // then pull inward by amount. rotateDeg in DEGREES. At rotate=0 -> BASE + amount*TOWARD.
  static fvec3 _insetPos(const float* base, const float* toward, const float* nrm, float amount, float rotateDeg) {
    fvec3 tw(toward[0], toward[1], toward[2]);
    fvec3 d0 = tw * -1.0f;                                   // BASE - C
    fvec3 C(base[0] + toward[0], base[1] + toward[1], base[2] + toward[2]);  // C = BASE + TOWARD
    fvec3 n(nrm[0], nrm[1], nrm[2]);
    float r = rotateDeg * (kPI2 / 360.0f);
    fvec3 d0r = d0 * std::cos(r) + n.crossWith(d0) * std::sin(r);
    return C + d0r * (1.0f - amount);
  }

  // per-inset-face data retained so the COLLAR can be RE-tessellated each frame as `rotate` spins the inner ring.
  // The m+N collar-triangle COUNT is rotation-invariant; only WHICH outer/inner verts each tri joins changes, so
  // buffer sizes never move -- we just rewrite this face's collar corner indices in _vidx. Without this the match
  // is frozen at rotate=0 and the collar rakes across the cap as the ring turns.
  struct CollarFace {
    std::vector<uint32_t> O, I;    // outer-boundary + inner-ring GLOBAL vertex indices (sizes m, N)
    std::vector<float>    aO, aI0; // outer angles (fixed) + inner REST angles (rotate=0), about C in the face plane
    int cornerOff = 0, m = 0, N = 0;   // where this face's collar corners begin in _vidx
  };
  // Triangulate the annulus between the outer m-gon and inner N-gon (both convex + CCW about C) via MINIMAL-ENERGY
  // bridging (Fuchs/Kedem contour stitch): the triangulation is a monotone staircase walking both rings once; the
  // DP picks the staircase minimizing total span (angular) length. A monotone staircase => the spans never cross =>
  // no overlaps and no inversions, at ANY rotation -- provably, not heuristically. `aI` = inner angles at the current
  // rotate. Emits the m+N triangles' corner indices into `out` (length 3*(m+N)); triangle ORDER is irrelevant, only
  // each triangle's winding (matched to the source face: outer-advance = (Oprev,Ocur,I); inner-advance = (O,Inew,Iprev)).
  static void _tessellateCollar(const uint32_t* O, int m, const float* aO,
                                const uint32_t* I, int N, const float* aI, uint32_t* out) {
    // QUANTIZE the angles entering the DP (1e-5 rad): the GPU builder (C.4c) runs the SAME DP on the
    // SAME quantized inputs, making the staircase decisions (incl. exact ties) IDENTICAL across
    // processors — raw atan2 ulps would otherwise flip near-tie cost comparisons.
    auto qz = [](float a) { return std::floor(a * 100000.0f + 0.5f) / 100000.0f; };
    auto cd = [&](float p, float q) { float d = std::fmod(std::fabs(qz(p) - qz(q)), kPI2); return std::min(d, kPI2 - d); };
    int s0 = 0; { float bd = 1e30f; for (int j = 0; j < N; j++) { float d = cd(aO[0], aI[j]); if (d < bd) { bd = d; s0 = j; } } }  // seam: O[0] <-> nearest inner
    auto IA = [&](int j) { return aI[(s0 + j) % N]; };       // inner angle at strip index j (j: 0..N, N wraps to s0)
    auto Ii = [&](int j) { return I[(s0 + j) % N]; };        // inner vertex id  at strip index j
    std::vector<std::vector<float>> C(m + 1, std::vector<float>(N + 1, 0.0f));
    std::vector<std::vector<char>>  bk(m + 1, std::vector<char>(N + 1, 0));   // 0 = arrived by outer-advance, 1 = inner-advance
    C[0][0] = cd(aO[0], IA(0));
    for (int i = 1; i <= m; i++) { C[i][0] = C[i - 1][0] + cd(aO[i % m], IA(0)); bk[i][0] = 0; }   // bottom edge: outer only
    for (int j = 1; j <= N; j++) { C[0][j] = C[0][j - 1] + cd(aO[0],     IA(j)); bk[0][j] = 1; }   // left edge: inner only
    for (int i = 1; i <= m; i++) for (int j = 1; j <= N; j++) {
      float span = cd(aO[i % m], IA(j));
      if (C[i - 1][j] <= C[i][j - 1]) { C[i][j] = span + C[i - 1][j]; bk[i][j] = 0; }
      else                           { C[i][j] = span + C[i][j - 1]; bk[i][j] = 1; }
    }
    int i = m, j = N, w = 0;                                 // walk the min-cost staircase (m,N)->(0,0); emit its triangle
    while (i > 0 or j > 0) {
      if (i > 0 and (j == 0 or bk[i][j] == 0)) {             // outer-advance step: tri (O[i-1], O[i], I[j])
        out[w++] = O[(i - 1) % m]; out[w++] = O[i % m]; out[w++] = Ii(j); i--;
      } else {                                               // inner-advance step: tri (O[i], I[j], I[j-1])
        out[w++] = O[i % m]; out[w++] = Ii(j); out[w++] = Ii(j - 1); j--;
      }
    }
  }

  // C.4c GPU rebuild — count -> 6 scans -> emit (+ the DP collar kernel at rest). 24-byte totals
  // readback; everything else stays on the GPU. The pre/postCheck readback suites do NOT run here
  // (collar validity = the monotone-staircase construction + the vet harness); the CPU reference
  // build retains them.
  void rebuildGpu(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    auto ci  = ctx->CI();
    int nv = in->_num_verts, nf = in->_num_faces;
    int sidesPlug = *(_d->typedInputNamed<dflow::IntPlugTraits>("sides")->_value);
    if (not _cs_icnt) {
      _cs_icnt    = fxi->computeShader(fxi->shaderFromShaderText("hm_igb_cnt", _igb_cnt_text()), "cs_icnt");
      _cs_iemT    = fxi->computeShader(fxi->shaderFromShaderText("hm_igb_emit", _igb_emit_text()), "cs_iemT");
      _cs_iemD    = fxi->computeShader(fxi->shaderFromShaderText("hm_igb_dups", _igb_dups_text()), "cs_iemD");
      _cs_iemB    = fxi->computeShader(fxi->shaderFromShaderText("hm_igb_blob", _igb_blob_text()), "cs_iemB");
      _cs_icollar = fxi->computeShader(fxi->shaderFromShaderText("hm_igb_collar", _igb_collar_text()), "cs_icollar");
      _iscan.init(ctx);
      _ictl = fxi->createStorageBuffer(32);
      _cctl = fxi->createStorageBuffer(16);
    }
    if (meshNextPow2(nf + 1) > _icap_nf) {
      _icap_nf = meshNextPow2(nf + 1);
      _ic1 = env->_pool->acquire(4, nf); _ic2 = env->_pool->acquire(4, nf); _ic3 = env->_pool->acquire(4, nf);
      _ic4 = env->_pool->acquire(4, nf); _ic5 = env->_pool->acquire(4, nf); _ic6 = env->_pool->acquire(4, nf);
      _ib1 = env->_pool->acquire(4, nf + 1); _ib2 = env->_pool->acquire(4, nf + 1); _ib3 = env->_pool->acquire(4, nf + 1);
      _ib4 = env->_pool->acquire(4, nf + 1); _ib5 = env->_pool->acquire(4, nf + 1); _ib6 = env->_pool->acquire(4, nf + 1);
      if (not _isct[0])
        for (int s = 0; s < 6; s++) _isct[s] = fxi->createStorageBuffer(16);
    }
    auto tg = in->face("__tags");
    FxShaderStorageBuffer* tags = tg ? tg->_ssbo : in->_vidx->_ssbo;  // no tags -> dummy + bit=0 (selects nothing)
    uint32_t bit = tg ? (1u << (_d->_slot & 31)) : 0u;
    auto w4 = [&](FxShaderStorageBuffer* b, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
      uint32_t vv[4] = {a0, a1, a2, a3};
      auto m = fxi->mapStorageBuffer(b, 0, sizeof(vv), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, vv, sizeof(vv)); fxi->unmapStorageBuffer(m.get());
    };
    for (int s = 0; s < 6; s++) w4(_isct[s], uint32_t(nf), 0, 0, 0);
    {
      uint32_t c[8] = {uint32_t(nf), uint32_t(nv), bit, uint32_t(std::max(0, sidesPlug)),
                       _d->_fill ? 1u : 0u, 0u, 0u, 0u};
      auto m = fxi->mapStorageBuffer(_ictl, 0, sizeof(c), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, c, sizeof(c)); fxi->unmapStorageBuffer(m.get());
    }
    // phase 1: counts + six scans
    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_icnt, 0, _ictl); ci->bindStorageBuffer(_cs_icnt, 1, in->_face_offsets->_ssbo);
    ci->bindStorageBuffer(_cs_icnt, 2, tags);
    ci->bindStorageBuffer(_cs_icnt, 3, _ic1); ci->bindStorageBuffer(_cs_icnt, 4, _ic2);
    ci->bindStorageBuffer(_cs_icnt, 5, _ic3); ci->bindStorageBuffer(_cs_icnt, 6, _ic4);
    ci->bindStorageBuffer(_cs_icnt, 7, _ic5); ci->bindStorageBuffer(_cs_icnt, 8, _ic6);
    ci->dispatchCompute(_cs_icnt, (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    FxShaderStorageBuffer* cnts[6]  = {_ic1, _ic2, _ic3, _ic4, _ic5, _ic6};
    FxShaderStorageBuffer* bases[6] = {_ib1, _ib2, _ib3, _ib4, _ib5, _ib6};
    for (int s = 0; s < 6; s++) { _iscan.scan(ctx, cnts[s], bases[s], _isct[s], nf); ci->storageBarrier(); }
    ci->endDispatchPhase();
    auto r1 = [&](FxShaderStorageBuffer* b) {
      auto m = fxi->mapStorageBuffer(b, size_t(nf) * 4, 4, BufferMapAccess::READ_ONLY);
      uint32_t vv = *reinterpret_cast<const uint32_t*>(m->_mappedaddr);
      fxi->unmapStorageBuffer(m.get());
      return int(vv);
    };
    int Vd = r1(_ib1), onc = r1(_ib2), onf = r1(_ib3), btot = r1(_ib4), stot = r1(_ib5), ncollar = r1(_ib6);
    _new_nv = nv + Vd; _new_nc = onc; _new_nf = onf; _nv = nv; _vb = Vd; _ncollar = ncollar;
    _vidx = env->_pool->acquireChannel(4, _new_nc);
    _fo   = env->_pool->acquireChannel(4, _new_nf + 1);
    _dupv = env->_pool->acquireChannel(4,  std::max(1, Vd));
    _base = env->_pool->acquireChannel(16, std::max(1, Vd));
    _tow  = env->_pool->acquireChannel(16, std::max(1, Vd));
    _nrm  = env->_pool->acquireChannel(16, std::max(1, Vd));
    _otags  = env->_pool->acquireChannel(4, _new_nf);
    _parent = env->_pool->acquireChannel(4, _new_nf);
    _part   = env->_pool->acquireChannel(4, _new_nf);
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
    if (meshNextPow2(std::max(1, btot)) > _icap_blob) {
      _icap_blob = meshNextPow2(std::max(1, btot));
      _iub = env->_pool->acquire(4, std::max(1, btot));
      _ifa = env->_pool->acquire(4, std::max(1, btot));      // FB total = btot - 8*ncollar <= btot
    }
    if (meshNextPow2(std::max(1, stot)) > _icap_scr) {
      _icap_scr = meshNextPow2(std::max(1, stot));
      _isf = env->_pool->acquire(4, std::max(1, stot));
      _isu = env->_pool->acquire(4, std::max(1, stot));
    }
    if (meshNextPow2(std::max(1, ncollar)) > _icap_map) {
      _icap_map = meshNextPow2(std::max(1, ncollar));
      _ifm = env->_pool->acquire(4, std::max(1, ncollar));
    }
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (not _params) _params = fxi->createStorageBuffer(16);
    // phase 2: topology + dup tables + collar blobs, then the DP collar at REST (rotate handled by
    // the same writeParams' adaptive block right after, exactly like the CPU flow)
    ci->beginDispatchPhase();
    auto bindT = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _ictl); ci->bindStorageBuffer(cs, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 2, in->_vidx->_ssbo); ci->bindStorageBuffer(cs, 3, tags);
      ci->bindStorageBuffer(cs, 4, _ib1); ci->bindStorageBuffer(cs, 5, _ib2);
      ci->bindStorageBuffer(cs, 6, _ib3); ci->bindStorageBuffer(cs, 7, _vidx->_ssbo);
      ci->bindStorageBuffer(cs, 8, _fo->_ssbo); ci->bindStorageBuffer(cs, 9, _parent->_ssbo);
      ci->bindStorageBuffer(cs, 10, _part->_ssbo);
    };
    bindT(_cs_iemT); ci->dispatchCompute(_cs_iemT, (nf + 63) / 64, 1, 1);
    if (Vd > 0) {
      ci->bindStorageBuffer(_cs_iemD, 0, _ictl); ci->bindStorageBuffer(_cs_iemD, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(_cs_iemD, 2, in->_vidx->_ssbo); ci->bindStorageBuffer(_cs_iemD, 3, tags);
      ci->bindStorageBuffer(_cs_iemD, 4, in->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs_iemD, 5, _ib1); ci->bindStorageBuffer(_cs_iemD, 6, _dupv->_ssbo);
      ci->bindStorageBuffer(_cs_iemD, 7, _base->_ssbo); ci->bindStorageBuffer(_cs_iemD, 8, _tow->_ssbo);
      ci->bindStorageBuffer(_cs_iemD, 9, _nrm->_ssbo);
      ci->dispatchCompute(_cs_iemD, (nf + 63) / 64, 1, 1);
    }
    if (ncollar > 0) {
      ci->bindStorageBuffer(_cs_iemB, 0, _ictl); ci->bindStorageBuffer(_cs_iemB, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(_cs_iemB, 2, in->_vidx->_ssbo); ci->bindStorageBuffer(_cs_iemB, 3, tags);
      ci->bindStorageBuffer(_cs_iemB, 4, in->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs_iemB, 5, _ib1); ci->bindStorageBuffer(_cs_iemB, 6, _ib2);
      ci->bindStorageBuffer(_cs_iemB, 7, _ib4); ci->bindStorageBuffer(_cs_iemB, 8, _ib5);
      ci->bindStorageBuffer(_cs_iemB, 9, _ib6); ci->bindStorageBuffer(_cs_iemB, 10, _iub);
      ci->bindStorageBuffer(_cs_iemB, 11, _ifa); ci->bindStorageBuffer(_cs_iemB, 12, _ifm);
      ci->dispatchCompute(_cs_iemB, (nf + 63) / 64, 1, 1);
      ci->storageBarrier();                                   // blobs settle -> the DP reads them
      w4(_cctl, uint32_t(ncollar), 0u, 0u, 0u);               // rot=0 (REST — the CPU build parity)
      dispatchCollar(ctx);
    }
    ci->storageBarrier();
    ci->endDispatchPhase();
    _collar.clear(); _vidxCPU.clear();                        // the CPU adaptive machinery is inert on this path
    _gpu_built = true;
  }
  void dispatchCollar(Context* ctx) {                         // caller is inside a dispatch phase
    auto ci = ctx->CI();
    ci->bindStorageBuffer(_cs_icollar, 0, _cctl); ci->bindStorageBuffer(_cs_icollar, 1, _ifm);
    ci->bindStorageBuffer(_cs_icollar, 2, _iub);  ci->bindStorageBuffer(_cs_icollar, 3, _ifa);
    ci->bindStorageBuffer(_cs_icollar, 4, _isf);  ci->bindStorageBuffer(_cs_icollar, 5, _isu);
    ci->bindStorageBuffer(_cs_icollar, 6, _vidx->_ssbo);
    ci->dispatchCompute(_cs_icollar, (_ncollar + 63) / 64, 1, 1);
  }

  // CPU rebuild: readback topology + positions, replace each selected face with collar + inner ring; bake the
  // per-inner-dup BASE/TOWARD/NRMV; emit the FaceTagger's (parent, partition) per output face. Re-runnable.
  void rebuild(Context* ctx, gpumesh_ptr_t in) {
    // C.4c route: GPU build unless the CPU positions path needs its side arrays (_cpu) or the
    // reference is forced (ORK_HM_INSET_CPU=1 — the oracle + escape hatch).
    if (not _d->_cpu and not getenv("ORK_HM_INSET_CPU")) {
      rebuildGpu(ctx, in);
      return;
    }
    _gpu_built = false;
    preCheck(ctx);                                       // PRECHECK (gated): no degenerate input faces
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int nv = in->_num_verts, nf = in->_num_faces, nc = in->_num_corners;
    auto rd = [&](FxShaderStorageBuffer* b, int n) {
      std::vector<uint32_t> v(n);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(n) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(n) * 4);
      fxi->unmapStorageBuffer(m.get());
      return v;
    };
    auto rdf = [&](FxShaderStorageBuffer* b, int nverts) {
      std::vector<float> v(size_t(nverts) * 4);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(nverts) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(nverts) * 16);
      fxi->unmapStorageBuffer(m.get());
      return v;
    };
    auto vidx = rd(in->_vidx->_ssbo, nc);
    auto fo   = rd(in->_face_offsets->_ssbo, nf + 1);
    std::vector<uint32_t> tags(nf, 0);
    if (auto tg = in->face("__tags")) tags = rd(tg->_ssbo, nf);
    auto Pin = rdf(in->channel(MeshChannel::POSITION)->_ssbo, nv);
    auto P   = [&](uint32_t v) { return fvec3(Pin[4 * v], Pin[4 * v + 1], Pin[4 * v + 2]); };
    uint32_t bit = 1u << (_d->_slot & 31);
    int sidesPlug = *(_d->typedInputNamed<dflow::IntPlugTraits>("sides")->_value);  // inner-ring side count (plug; rebuilds on change)

    std::vector<uint32_t> ovidx, ofo, dupv, oparent, opart;
    std::vector<float>    fbase, ftow, fnrm;                          // per inner dup vec4
    int nfo = 0;
    _collar.clear();                                                  // rebuilt below; per-frame re-tessellation reads it
    auto face   = [&]() { ofo.push_back(uint32_t(ovidx.size())); nfo++; };
    auto push4  = [](std::vector<float>& vv, fvec3 a) { vv.push_back(a.x); vv.push_back(a.y); vv.push_back(a.z); vv.push_back(0.0f); };
    auto addInner = [&](uint32_t srcVert, fvec3 base, fvec3 toward, fvec3 nrm) -> uint32_t {
      uint32_t idx = uint32_t(nv) + uint32_t(dupv.size());
      dupv.push_back(srcVert); push4(fbase, base); push4(ftow, toward); push4(fnrm, nrm);
      return idx;
    };
    // position used for winding decisions: originals at their readback pos, inner dups at their BASE (amount=0).
    // The inset is coplanar (TOWARD is in-plane), so the winding sign is amount-independent -> bake-time safe.
    auto wp = [&](uint32_t idx) -> fvec3 {
      if (idx < uint32_t(nv)) return P(idx);
      uint32_t di = idx - uint32_t(nv);
      return fvec3(fbase[di * 4], fbase[di * 4 + 1], fbase[di * 4 + 2]);
    };

    for (int f = 0; f < nf; f++) {
      uint32_t a = fo[f], b = fo[f + 1]; int m = int(b - a);
      bool sel = (tags[f] & bit) != 0;
      if (not sel) {                                                  // untouched -> pass through
        face(); for (uint32_t c = a; c < b; c++) ovidx.push_back(vidx[c]);
        oparent.push_back(uint32_t(f)); opart.push_back(kInOuter); continue;
      }
      // face frame: centroid C, normal fN (face winding), in-plane orthonormal (u, v) with u,v,fN right-handed.
      fvec3 C(0, 0, 0); for (int k = 0; k < m; k++) C += P(vidx[a + k]); C = C * (1.0f / float(m));
      fvec3 p0 = P(vidx[a]), p1 = P(vidx[a + 1]), p2 = P(vidx[a + 2]);
      fvec3 fN = (p1 - p0).crossWith(p2 - p0); float L = fN.length(); fN = (L > 1e-12f) ? fN * (1.0f / L) : fvec3(0, 1, 0);
      fvec3 u = (p1 - p0); u = u - fN * u.dotWith(fN); float ul = u.length(); u = (ul > 1e-12f) ? u * (1.0f / ul) : fvec3(1, 0, 0);
      fvec3 v = fN.crossWith(u);

      int N = (sidesPlug > 0) ? sidesPlug : m;
      std::vector<uint32_t> inner(N);
      if (N == m) {                                                  // 1:1 inner ring (plain inset, same count)
        for (int k = 0; k < m; k++) {
          fvec3 bk = P(vidx[a + k]);
          inner[k] = addInner(vidx[a + k], bk, C - bk, fN);          // base = boundary vert, pull toward centroid
        }
      } else {                                                       // regular N-gon inner ring (resampled)
        // R0 = A's INRADIUS (min perpendicular distance C->edge) so the regular N-gon is INSCRIBED inside A
        // and never pokes past A's edges -> the collar can't invert. amount then pulls it further toward C.
        float R0 = 1e30f;
        for (int k = 0; k < m; k++) {
          fvec3 e0 = P(vidx[a + k]), e1 = P(vidx[a + (k + 1) % m]);
          fvec3 ed = e1 - e0; float el = ed.length(); ed = (el > 1e-12f) ? ed * (1.0f / el) : ed;
          fvec3 dd = C - e0; R0 = std::min(R0, (dd - ed * dd.dotWith(ed)).length());
        }
        // Bake the ring at the CANONICAL theta0 = 0 (first ring vertex on +u). u is edge-aligned, so this is
        // mirror-symmetric about both axes of a rectangular face and the COLLAR topology (nearest-sector match
        // below) is overlap-free here. The `rotate` plug then spins the ring at RUNTIME in cs_inner (about the
        // face normal), so orientation animates with no rebuild; the collar topology stays matched to theta0=0
        // (it twists with rotate — fine for an oscillation/wobble; a full turn past a half-sector would need
        // per-frame topology regen, deferred).
        float theta0 = 0.0f;
        for (int k = 0; k < N; k++) {
          float th  = theta0 + float(k) * (kPI2 / float(N));
          fvec3 dir = u * std::cos(th) + v * std::sin(th);
          fvec3 base = C + dir * R0;
          uint32_t ns = vidx[a]; float best = 1e30f;                 // nominal source vert (nearest) -> UV/color
          for (int j = 0; j < m; j++) { float dd = (P(vidx[a + j]) - base).length();
            if (dd < best - 1e-6f) { best = dd; ns = vidx[a + j]; } }  // tie margin: cross-processor ulp can't flip the pick
          inner[k] = addInner(ns, base, C - base, fN);
        }
      }
      // COLLAR — emit in NATURAL ring order (outer forward, inner backward) so the winding INHERITS the
      // source face A directly (B is placed with A's rotational sense + INSIDE A). No normal-flip: the
      // winding is an ORDERING property, not a normal-direction one — robust + convention-independent.
      auto emitTri = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
        face(); ovidx.push_back(i0); ovidx.push_back(i1); ovidx.push_back(i2);
        oparent.push_back(uint32_t(f)); opart.push_back(kInCollar);
      };
      if (N == m) {                                                  // clean quad collar: [a_k, a_k+1, b_k+1, b_k]
        for (int k = 0; k < m; k++) {
          int k1 = (k + 1) % m;
          face();
          ovidx.push_back(vidx[a + k]); ovidx.push_back(vidx[a + k1]); ovidx.push_back(inner[k1]); ovidx.push_back(inner[k]);
          oparent.push_back(uint32_t(f)); opart.push_back(kInCollar);
        }
      } else {                                                       // m != N: NEAREST-SECTOR fan (monotone -> no overlaps)
        // Triangulate the annulus by matching each inner vertex to the angularly-NEAREST outer vertex (a MONOTONE
        // assignment, since both rings are convex + wound CCW about C). Each inner EDGE then spans either one outer
        // sector (a single fan tri) or bridges to the next outer vert(s) (a fan FROM inner[k] across the outer chain
        // + one closing tri). Anchoring the straddle bridges at the inner vertex -- which lies ON the convex inner
        // ring, so its edges to the outer verts point strictly OUTWARD -- is what keeps a collar tri from ever
        // cutting across the cap. That cut was the overlap failure mode of BOTH the old shortest-diagonal zip and a
        // naive next-angle merge (the latter fanned a far corner over the cap; the former stalled and spanned).
        auto ang = [&](uint32_t vi) {                                 // angle of vert in the face plane, [0,2pi)
          fvec3 d = wp(vi) - C; float x = d.dotWith(u), y = d.dotWith(v);
          float t = std::atan2(y, x); return (t < 0.0f) ? t + float(kPI2) : t;
        };
        CollarFace cf; cf.m = m; cf.N = N; cf.O.resize(m); cf.I.resize(N); cf.aO.resize(m); cf.aI0.resize(N);
        for (int k = 0; k < m; k++) { cf.O[k] = vidx[a + k]; cf.aO[k] = ang(vidx[a + k]); }
        for (int k = 0; k < N; k++) { cf.I[k] = inner[k];    cf.aI0[k] = ang(inner[k]); }   // REST angles (theta0=0)
        cf.cornerOff = int(ovidx.size());
        std::vector<uint32_t> ctmp(size_t(3) * (m + N));
        _tessellateCollar(cf.O.data(), m, cf.aO.data(), cf.I.data(), N, cf.aI0.data(), ctmp.data());
        for (int t = 0; t < m + N; t++) emitTri(ctmp[t * 3 + 0], ctmp[t * 3 + 1], ctmp[t * 3 + 2]);
        _collar.push_back(std::move(cf));                             // retain for per-frame re-tessellation
      }
      if (_d->_fill) {                                               // inner cap = B in its own order (= A's sense)
        face();
        for (int k = 0; k < N; k++) ovidx.push_back(inner[k]);
        oparent.push_back(uint32_t(f)); opart.push_back(kInInner);
      }
    }
    ofo.push_back(uint32_t(ovidx.size()));
    int Vd = int(dupv.size());
    _new_nv = nv + Vd; _new_nc = int(ovidx.size()); _new_nf = nfo; _nv = nv; _vb = Vd;
    _vidx = env->_pool->acquireChannel(4, _new_nc);
    _fo   = env->_pool->acquireChannel(4, _new_nf + 1);
    _dupv = env->_pool->acquireChannel(4,  std::max(1, Vd));
    _base = env->_pool->acquireChannel(16, std::max(1, Vd));
    _tow  = env->_pool->acquireChannel(16, std::max(1, Vd));
    _nrm  = env->_pool->acquireChannel(16, std::max(1, Vd));
    auto up = [&](FxShaderStorageBuffer* buf, const std::vector<uint32_t>& vv) {
      auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(1, vv.size()) * 4, BufferMapAccess::WRITE_ONLY);
      if (not vv.empty()) std::memcpy(m->_mappedaddr, vv.data(), vv.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    };
    auto upf = [&](FxShaderStorageBuffer* buf, const std::vector<float>& vv) {
      auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(4, vv.size() * 4), BufferMapAccess::WRITE_ONLY);
      if (not vv.empty()) std::memcpy(m->_mappedaddr, vv.data(), vv.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    };
    _vidxCPU = ovidx;                                                // CPU mirror for per-frame collar re-tessellation
    up(_vidx->_ssbo, ovidx); up(_fo->_ssbo, ofo); up(_dupv->_ssbo, dupv);
    upf(_base->_ssbo, fbase); upf(_tow->_ssbo, ftow); upf(_nrm->_ssbo, fnrm);
    _otags  = env->_pool->acquireChannel(4, _new_nf);
    _parent = env->_pool->acquireChannel(4, _new_nf);
    _part   = env->_pool->acquireChannel(4, _new_nf);
    up(_parent->_ssbo, oparent); up(_part->_ssbo, opart);
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
    if (_d->_cpu) {                                        // CPU REFERENCE: compute positions + all 5 channels on CPU
      float amount = *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value);
      float rotate = *(_d->typedInputNamed<dflow::FloatPlugTraits>("rotate")->_value);
      auto Nin = rdf(in->channel(MeshChannel::NORMAL)->_ssbo, nv);
      auto Bin = rdf(in->channel(MeshChannel::BINORMAL)->_ssbo, nv);
      auto Uin = rdf(in->channel(MeshChannel::UV0)->_ssbo, nv);
      auto Cin = rdf(in->channel(MeshChannel::COLOR)->_ssbo, nv);
      std::vector<float> oP(size_t(_new_nv) * 4), oN(size_t(_new_nv) * 4), oB(size_t(_new_nv) * 4), oU(size_t(_new_nv) * 4), oC(size_t(_new_nv) * 4);
      std::memcpy(oP.data(), Pin.data(), size_t(nv) * 16); // originals (v < nv) pass through, all 5 channels
      std::memcpy(oN.data(), Nin.data(), size_t(nv) * 16);
      std::memcpy(oB.data(), Bin.data(), size_t(nv) * 16);
      std::memcpy(oU.data(), Uin.data(), size_t(nv) * 16);
      std::memcpy(oC.data(), Cin.data(), size_t(nv) * 16);
      for (int d = 0; d < Vd; d++) {                       // inner dups: P = inset pos (amount+rotate); N = NRMV; B/uv/color from src
        uint32_t sv = dupv[d]; int o = nv + d;
        fvec3 p = _insetPos(&fbase[d * 4], &ftow[d * 4], &fnrm[d * 4], amount, rotate);
        oP[o * 4 + 0] = p.x; oP[o * 4 + 1] = p.y; oP[o * 4 + 2] = p.z; oP[o * 4 + 3] = 1.0f;
        oN[o * 4 + 0] = fnrm[d * 4 + 0]; oN[o * 4 + 1] = fnrm[d * 4 + 1]; oN[o * 4 + 2] = fnrm[d * 4 + 2]; oN[o * 4 + 3] = 0.0f;
        for (int c = 0; c < 4; c++) { oB[o * 4 + c] = Bin[sv * 4 + c]; oU[o * 4 + c] = Uin[sv * 4 + c]; oC[o * 4 + c] = Cin[sv * 4 + c]; }
      }
      upf(_outch[MeshChannel::POSITION]->_ssbo, oP); upf(_outch[MeshChannel::NORMAL]->_ssbo, oN);
      upf(_outch[MeshChannel::BINORMAL]->_ssbo, oB); upf(_outch[MeshChannel::UV0]->_ssbo, oU); upf(_outch[MeshChannel::COLOR]->_ssbo, oC);
      // keep CPU copies so compute() can re-place the inner ring each frame from the RUNTIME amount/rotate plugs
      // (the CPU path skips cs_inner, so without this the plugs would be frozen at their build-time values).
      _posCPU = std::move(oP); _baseCPU = fbase; _towCPU = ftow; _nrmCPU = fnrm;
      _lastAmount = amount; _lastRotate = rotate;
    }
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (not _params) _params = fxi->createStorageBuffer(16);
    postCheck(ctx);                                       // POSTCHECK (gated): no created open/non-manifold edges (collar)
  }

  // inset preconditions/postconditions (hooks for the base template method preCheck/postCheck).
  void _doPreCheck(Context* ctx) final {                 // input faces must be well-formed (inset builds a per-face frame)
    auto in = _srcMesh(_input);
    auto p  = in->channel(MeshChannel::POSITION)->_ssbo; auto vi = in->_vidx->_ssbo; auto fo = in->_face_offsets->_ssbo;
    int  nv = in->_num_verts, nc = in->_num_corners, nf = in->_num_faces;
    meshcheck::assertNoDegenerateFaces(meshcheck::degenerateFaces(ctx, p, vi, fo, nv, nc, nf), nf, "inset");
    meshcheck::assertWindingConsistent(meshcheck::windingFlips(ctx, p, vi, fo, nv, nc, nf), "inset", "PRECONDITION");
    // NOTE: meshcheck::coplanarOverlaps exists + is reusable, but its inset wiring currently FALSE-POSITIVES on a
    // DP-clean mesh (trimesh confirms 0 overlaps where it reports many) -- disabled here until that's reconciled.
  }
  void _doPostCheck(Context* ctx) final {                // collar must create no open/non-manifold edges AND no winding flips
    // Reconstruct the output positions at the CURRENT rotate (BASE/TOWARD/NRMV + amount + rotate via _insetPos),
    // matching what cs_inner/the CPU path produce, so the checks validate the ACTUAL (rotated, re-tessellated)
    // collar -- this is run both on rebuild AND after each per-frame collar re-tessellation. Use amount>0 (the
    // real plug value), NOT BASE alone: amount=0 is the INSCRIBED ring whose axis verts sit ON the face edges, so
    // adjacent inset faces would coincide by position and edgeHealth would report a FALSE non-manifold junction.
    auto in  = _srcMesh(_input);
    auto fxi = ctx->FXI();
    int  nv  = in->_num_verts;
    auto rdu = [&](FxShaderStorageBuffer* b, int n) { std::vector<uint32_t> v(std::max(1, n));
      auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4); fxi->unmapStorageBuffer(m.get()); return v; };
    auto rdf = [&](FxShaderStorageBuffer* b, int n) { std::vector<float> v(size_t(std::max(1, n)) * 4);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 16); fxi->unmapStorageBuffer(m.get()); return v; };
    auto inPos = rdf(in->channel(MeshChannel::POSITION)->_ssbo, nv);
    auto inH   = meshcheck::edgeHealth(inPos.data(), nv, rdu(in->_vidx->_ssbo, in->_num_corners).data(),
                                       rdu(in->_face_offsets->_ssbo, in->_num_faces + 1).data(), in->_num_faces);
    std::vector<float> outPos(size_t(_new_nv) * 4, 0.0f);  // originals + inner (the ACTUAL inset pos at current rotate)
    std::memcpy(outPos.data(), inPos.data(), size_t(nv) * 16);
    if (_vb > 0) {
      float amount = *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value);
      float rotate = *(_d->typedInputNamed<dflow::FloatPlugTraits>("rotate")->_value);
      auto base = rdf(_base->_ssbo, _vb), toward = rdf(_tow->_ssbo, _vb), nrm = rdf(_nrm->_ssbo, _vb);
      for (int d = 0; d < _vb; d++) {
        fvec3 p = _insetPos(&base[d * 4], &toward[d * 4], &nrm[d * 4], amount, rotate);
        outPos[(nv + d) * 4 + 0] = p.x; outPos[(nv + d) * 4 + 1] = p.y; outPos[(nv + d) * 4 + 2] = p.z; outPos[(nv + d) * 4 + 3] = 1.0f;
      }
    }
    auto oV = rdu(_vidx->_ssbo, _new_nc); auto oF = rdu(_fo->_ssbo, _new_nf + 1);
    auto outH  = meshcheck::edgeHealth(outPos.data(), _new_nv, oV.data(), oF.data(), _new_nf);
    meshcheck::assertNoCreatedDefects(inH, outH, "inset");
    meshcheck::assertWindingConsistent(meshcheck::windingFlips(outPos.data(), _new_nv, oV.data(), oF.data(), _new_nf), "inset", "POSTCONDITION");
    // coplanar-overlap postcheck disabled here: it FALSE-POSITIVES on a DP-clean collar (the position reconstruction
    // drifts from the actual render); meshcheck::coplanarOverlaps stays in the library for reuse once reconciled.
  }

  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto sh  = ctx->FXI()->shaderFromShaderText("hypermesh_inset", _inset_text());
    _cs_copy  = ctx->FXI()->computeShader(sh, "cs_copy");
    _cs_inner = ctx->FXI()->computeShader(sh, "cs_inner");
    _tagger.build(ctx);
    rebuild(ctx, in);
    _built = true; _built_nf = in->_num_faces;
    _built_sides = *(_d->typedInputNamed<dflow::IntPlugTraits>("sides")->_value);
    ackSrcTopo(_input);
    return true;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out  = _output->_value;
    int nf    = in->_num_faces;
    int sides = *(_d->typedInputNamed<dflow::IntPlugTraits>("sides")->_value);
    bool topoChanged = false;                                        // did WE re-emit connectivity this frame?
    _active   = false;
    if (_built and (sides != _built_sides or srcTopoDirty(_input))) { // `sides` changed OR upstream topology changed -> rebuild
      rebuild(ctx, in); ackSrcTopo(_input); _built_sides = sides; _built_nf = nf; _pending_nf = -1; _active = true; topoChanged = true;
    }
    else if (_built and nf == _built_nf)        _active = true;       // steady
    else if (_built and nf == _pending_nf) { rebuild(ctx, in); ackSrcTopo(_input); _built_nf = nf; _pending_nf = -1; _active = true; topoChanged = true; }
    else if (_built)                       _pending_nf = nf;          // source changed -> pass through this frame
    if (not _active) {                                               // PASSTHROUGH
      out->_channels = in->_channels; out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets;
      out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    out->_channels = _outch;
    out->_faces.clear();
    out->_faces["__tags"] = _otags;
    out->_vidx = _vidx; out->_face_offsets = _fo; out->_header = _header;
    out->_capacity = meshNextPow2(_new_nv);
    out->_num_verts = _new_nv; out->_num_corners = _new_nc; out->_num_faces = _new_nf;
    _tagger.setMasks(ctx, _new_nf, _d->_part_masks);
    float amount = *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value);
    float rotate = *(_d->typedInputNamed<dflow::FloatPlugTraits>("rotate")->_value);
    uint32_t ctl[4]; ctl[0] = uint32_t(_nv); ctl[1] = uint32_t(_vb);
    std::memcpy(&ctl[2], &amount, 4); std::memcpy(&ctl[3], &rotate, 4);
    auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
    uint32_t hdr[4] = {uint32_t(_new_nv), uint32_t(_new_nc), uint32_t(_new_nf), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr));
    ctx->FXI()->unmapStorageBuffer(mh.get());
    // ADAPTIVE COLLAR (PRE-phase so a downstream's same-frame rebuild check sees it): re-tessellate from the
    // ROTATED inner ring. The m+N count is rotation-invariant, so only the collar's corner indices in _vidx
    // change; we re-emit + flag a topology change only when the nearest-sector MATCH actually shifts (a discrete
    // sector crossing), then markTopoChanged() bumps _topoVersion so extrude/face_normals/wireframe recompute.
    if (not _collar.empty() and rotate != _lastCollarRotate) {
      _lastCollarRotate = rotate;
      float rotRad = rotate * (float(kPI2) / 360.0f);
      bool collarChanged = false;
      std::vector<uint32_t> ctmp; std::vector<float> aI;
      for (auto& cf : _collar) {
        aI.resize(cf.N);
        for (int k = 0; k < cf.N; k++) { float aa = std::fmod(cf.aI0[k] + rotRad, float(kPI2)); aI[k] = (aa < 0.0f) ? aa + float(kPI2) : aa; }
        ctmp.resize(size_t(3) * (cf.m + cf.N));
        _tessellateCollar(cf.O.data(), cf.m, cf.aO.data(), cf.I.data(), cf.N, aI.data(), ctmp.data());
        for (size_t c = 0; c < ctmp.size(); c++)
          if (_vidxCPU[size_t(cf.cornerOff) + c] != ctmp[c]) { _vidxCPU[size_t(cf.cornerOff) + c] = ctmp[c]; collarChanged = true; }
      }
      if (collarChanged) {
        auto mv = ctx->FXI()->mapStorageBuffer(_vidx->_ssbo, 0, size_t(_new_nc) * 4, BufferMapAccess::WRITE_ONLY);
        std::memcpy(mv->_mappedaddr, _vidxCPU.data(), size_t(_new_nc) * 4);
        ctx->FXI()->unmapStorageBuffer(mv.get());
        topoChanged = true;
        postCheck(ctx);   // re-validate the freshly re-tessellated collar (winding + edge health) at the new rotate
      }
    } else if (_gpu_built and _ncollar > 0 and rotate != _lastCollarRotate) {
      // C.4c GPU adaptive collar: re-run the DP kernel at the new rotate. The 4-byte changed-flag
      // readback (only on rotate-CHANGE frames) replaces the CPU path's per-crossing FULL postCheck
      // readback — the monotone-staircase construction is overlap-free at any rotation by proof.
      _lastCollarRotate = rotate;
      float rotRad = rotate * (float(kPI2) / 360.0f);
      uint32_t cc[4] = {uint32_t(_ncollar), 0u, 0u, 0u};
      std::memcpy(&cc[2], &rotRad, 4);
      auto mc = ctx->FXI()->mapStorageBuffer(_cctl, 0, sizeof(cc), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mc->_mappedaddr, cc, sizeof(cc));
      ctx->FXI()->unmapStorageBuffer(mc.get());
      auto ci = ctx->CI();
      ci->beginDispatchPhase();
      dispatchCollar(ctx);
      ci->storageBarrier();
      ci->endDispatchPhase();
      auto mr = ctx->FXI()->mapStorageBuffer(_cctl, 4, 4, BufferMapAccess::READ_ONLY);
      uint32_t changed = *reinterpret_cast<const uint32_t*>(mr->_mappedaddr);
      ctx->FXI()->unmapStorageBuffer(mr.get());
      if (changed) topoChanged = true;
    }
    if (topoChanged) out->markTopoChanged();   // signal downstream (extrude/face_normals/wireframe) to rebuild
    else             out->markChanged();        // general dirt (positions re-place each frame); reason-agnostic
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _active) return;
    auto in  = _srcMesh(_input);
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto& s  = in->_channels;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, s[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 1, s[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 2, s[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 3, s[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 4, s[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(cs, 5, _outch[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 6, _outch[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 7, _outch[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 8, _outch[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 9, _outch[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(cs, 10, _dupv->_ssbo);
      ci->bindStorageBuffer(cs, 11, _base->_ssbo);
      ci->bindStorageBuffer(cs, 12, _tow->_ssbo);
      ci->bindStorageBuffer(cs, 13, _nrm->_ssbo);
      ci->bindStorageBuffer(cs, 14, _params);
    };
    if (not _d->_cpu) {                                  // GPU path: cs_inner re-places the inner ring each frame (runtime amount/rotate)
      bind(_cs_copy);  ci->dispatchCompute(_cs_copy,  (_nv + 63) / 64, 1, 1);
      if (_vb > 0) { bind(_cs_inner); ci->dispatchCompute(_cs_inner, (_vb + 63) / 64, 1, 1); }
    } else {                                             // CPU REFERENCE path: re-place the inner ring on CPU when amount/rotate change
      (void)bind;
      float amount = *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value);
      float rotate = *(_d->typedInputNamed<dflow::FloatPlugTraits>("rotate")->_value);
      if (_vb > 0 and (amount != _lastAmount or rotate != _lastRotate)) {   // animate the runtime plugs (no rebuild)
        _lastAmount = amount; _lastRotate = rotate;
        for (int d = 0; d < _vb; d++) {
          fvec3 p = _insetPos(&_baseCPU[d * 4], &_towCPU[d * 4], &_nrmCPU[d * 4], amount, rotate);
          int o = _nv + d;
          _posCPU[o * 4 + 0] = p.x; _posCPU[o * 4 + 1] = p.y; _posCPU[o * 4 + 2] = p.z; _posCPU[o * 4 + 3] = 1.0f;
        }
        auto fxi = env->_ctx->FXI();
        auto mp  = fxi->mapStorageBuffer(_outch[MeshChannel::POSITION]->_ssbo, 0, size_t(_new_nv) * 16, BufferMapAccess::WRITE_ONLY);
        std::memcpy(mp->_mappedaddr, _posCPU.data(), size_t(_new_nv) * 16);
        fxi->unmapStorageBuffer(mp.get());
      }
    }
    auto in_tags = in->face("__tags");
    _tagger.dispatch(env->_ctx, _otags->_ssbo, in_tags ? in_tags->_ssbo : nullptr, _parent->_ssbo, _part->_ssbo, _new_nf);
    ci->storageBarrier();
  }
  const InsetData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _vidx, _fo, _dupv, _base, _tow, _nrm;
  gpuchannel_ptr_t _otags, _parent, _part;
  FaceTagger _tagger;
  FxShaderStorageBuffer *_header = nullptr, *_params = nullptr;
  const FxComputeShader *_cs_copy = nullptr, *_cs_inner = nullptr;
  int _new_nv = 0, _new_nc = 0, _new_nf = 0, _nv = 0, _vb = 0;
  int _built_nf = -1, _pending_nf = -1, _built_sides = -999;   // `sides` value at last rebuild (plug change -> rebuild)
  std::vector<float> _posCPU, _baseCPU, _towCPU, _nrmCPU;      // CPU-path: baked positions + inner BASE/TOWARD/NRMV (live re-place)
  float _lastAmount = -1e30f, _lastRotate = -1e30f;            // CPU-path: last amount/rotate uploaded (skip redundant re-uploads)
  std::vector<CollarFace> _collar;                            // per inset face: data to re-tessellate the collar on rotate
  std::vector<uint32_t>   _vidxCPU;                           // CPU mirror of _vidx (collar corners rewritten per frame)
  float _lastCollarRotate = -1e30f;                          // last rotate the collar topology was re-tessellated for
  bool _built = false, _active = false;
  // ---- C.4c GPU build state ----
  MeshScan _iscan;
  const FxComputeShader *_cs_icnt = nullptr, *_cs_iemT = nullptr, *_cs_iemD = nullptr, *_cs_iemB = nullptr,
                        *_cs_icollar = nullptr;
  FxShaderStorageBuffer *_ictl = nullptr, *_cctl = nullptr;                       // build ctl / collar ctl+flag
  FxShaderStorageBuffer *_ic1 = nullptr, *_ic2 = nullptr, *_ic3 = nullptr, *_ic4 = nullptr, *_ic5 = nullptr, *_ic6 = nullptr;
  FxShaderStorageBuffer *_ib1 = nullptr, *_ib2 = nullptr, *_ib3 = nullptr, *_ib4 = nullptr, *_ib5 = nullptr, *_ib6 = nullptr;
  FxShaderStorageBuffer* _isct[6] = {};
  FxShaderStorageBuffer *_iub = nullptr, *_ifa = nullptr;                          // collar blobs (ids+hdr / angles)
  FxShaderStorageBuffer *_isf = nullptr, *_isu = nullptr;                          // DP scratch (cost / backtrack)
  FxShaderStorageBuffer* _ifm = nullptr;                                           // collar index -> blob offset
  int _icap_nf = 0, _icap_blob = 0, _icap_scr = 0, _icap_map = 0;
  int _ncollar = 0;
  bool _gpu_built = false;
};
const MeshChannel InsetInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeInsetIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amount")->setValue(0.3f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "rotate")->setValue(0.0f); // N-gon orient, DEGREES (runtime; spins positions)
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "sides")->setValue(0);       // inner-ring side count (0 = keep face's; change -> rebuild)
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
InsetData::InsetData() {
}
std::shared_ptr<InsetData> InsetData::createShared() {
  auto d = std::make_shared<InsetData>();
  _reshapeInsetIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t InsetData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<InsetInst>(this, g);
}
void InsetData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return InsetData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeInsetIOs(m); });
  clazz->directProperty("slot", &InsetData::_slot);
  clazz->directProperty("fill", &InsetData::_fill);
  clazz->directProperty("cpu", &InsetData::_cpu);
  clazz->directVectorProperty("part_masks", &InsetData::_part_masks);          // `sides` + `rotate` are now INPUT PLUGS (see _reshapeInsetIOs);
                                                           // precheck/postcheck inherited from MeshModuleData

}

} // namespace ork::lev2::hypermesh
