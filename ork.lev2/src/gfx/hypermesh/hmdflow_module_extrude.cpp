////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <unordered_map>

ImplementReflectionX(ork::lev2::hypermesh::ExtrudeFacesData, "hypermesh::ExtrudeFacesData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// ExtrudeFaces — lift the faces selected by `__tags` bit `slot` along the vertex normal by `distance`,
// bridging the region boundary with side-wall quads (REGION extrude). The boundary topology is built
// on the CPU (readback of vidx/face_offsets + the tags), uploaded, and REBUILT when the SOURCE topology
// changes (e.g. an animated subdivision upstream): writeParams detects a source face-count change and
// re-derives. Because the new selection (tags) is computed in-dispatch, the transition frame PASSES
// THROUGH (extrude blinks off for one frame) and the rebuild happens the next frame, when the tags are
// synced — crash-safe (never reads a half-re-pooled tag buffer). Per frame: 2 position passes; nv/vb are
// runtime (one compiled shader, no recompile on rebuild). New verts=nv+Vb, faces=nf+Eb, corners=nc+4*Eb.
///////////////////////////////////////////////////////////////////////////////

static std::string _extrude_text() {
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
storage_interface sif_lf (descriptor_set 0) { buffer layout(std430) lfb { uint LIFT[]; }; }
storage_interface sif_dv (descriptor_set 0) { buffer layout(std430) dvb { uint DUPV[]; }; }
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) prb { uint p_nv; uint p_vb; float p_dist; float p_usedir; }; }
storage_interface sif_fn  (descriptor_set 0) { buffer layout(std430) fnb { vec4 FNV[];  }; } // FACE per-dup: .xyz baked lift dir (faceN), .w lift weight (1 cap/rim-top, 0 base)
storage_interface sif_nrm (descriptor_set 0) { buffer layout(std430) nrb { vec4 NRMV[]; }; } // FACE per-dup: .xyz POSITION inset dir (C-v, 0 for base), .w isCap
storage_interface sif_bin (descriptor_set 0) { buffer layout(std430) bnb { vec4 BINV[]; }; } // FACE per-dup: .xyz edge/cap tangent (binormal)
storage_interface sif_inn (descriptor_set 0) { buffer layout(std430) innb { vec4 INNRM[]; }; } // FACE per-dup: .xyz quad-consistent inset dir (rim normal)
storage_interface sif_dpr (descriptor_set 0) { buffer layout(std430) dprb { uint DPAR[];  }; } // FACE per-dup: parent INPUT face index
storage_interface sif_fl0 (descriptor_set 0) { buffer layout(std430) fl0b { vec4 FLD0[];  }; } // single: per-face (dist,inset,_,_); seg: per-(face,ring) FA=(Cacc.xyz, scale)
storage_interface sif_fl1 (descriptor_set 0) { buffer layout(std430) fl1b { vec4 FLD1[];  }; } // single: per-face (dir.xyz,_); seg: per-(face,ring) FB=(dir.xyz, twist)
storage_interface sif_fl2 (descriptor_set 0) { buffer layout(std430) fl2b { vec4 FLD2[];  }; } // seg only: per-(face,ring) FC=(Cbase.xyz, inset)
compute_interface iface { storage { sif_iP sif_iN sif_iB sif_iU sif_iC sif_oP sif_oN sif_oB sif_oU sif_oC
                                    sif_lf sif_dv sif_par sif_fn sif_nrm sif_bin sif_inn sif_dpr sif_fl0 sif_fl1 sif_fl2 }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_orig : iface {            // copy/lift the original verts (interior-selected lift in place)
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  float L = float(LIFT[v]) * p_dist;
  oP[v] = vec4(iP[v].xyz + iN[v].xyz * L, 1.0);
  oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
}
////////////////////////////////////////
compute_shader cs_dup : iface {             // VERTEX mode: lifted boundary duplicates (lifted by dist along vN)
  uint d = gl_GlobalInvocationID.x;
  if (d >= p_vb) { return; }
  uint sv = DUPV[d]; uint o = p_nv + d;
  oP[o] = vec4(iP[sv].xyz + iN[sv].xyz * p_dist, 1.0);
  oN[o] = iN[sv]; oB[o] = iB[sv]; oU[o] = iU[sv]; oC[o] = iC[sv];
}
////////////////////////////////////////
compute_shader cs_face : iface {            // FACE mode: position + flat-normal each per-face/per-wall dup.
  uint d = gl_GlobalInvocationID.x;         // per-face fields (FLD0 dist/inset, FLD1 dir) come from cs_field;
  if (d >= p_vb) { return; }                // FNV.w=lift weight, NRMV.xyz=position inset dir, NRMV.w=isCap,
  uint sv = DUPV[d]; uint o = p_nv + d;     // BINV.xyz=edge tangent, INNRM.xyz=quad inset dir (rim normal).
  uint pf    = DPAR[d];
  float dist = FLD0[pf].x * p_dist;         // per-face distance x the global `distance` plug
  float ins  = FLD0[pf].y;                  // per-face inset (in-plane shrink toward centroid)
  float lw   = FNV[d].w;                    // 1 = lifted (cap/rim-top), 0 = base (outer ring)
  vec3  ldir = (p_usedir > 0.5) ? normalize(FLD1[pf].xyz) : FNV[d].xyz;   // per-face dir expr, else baked faceN
  vec3  pos  = iP[sv].xyz + NRMV[d].xyz * ins + ldir * (dist * lw);
  oP[o] = vec4(pos, 1.0);
  vec3 nrm;
  if (NRMV[d].w > 0.5) {                     // CAP: a planar polygon perpendicular to the lift dir
    nrm = normalize(ldir);
  } else {                                   // RIM/WALL: flat normal _|_ edge & the base->top slant (runtime)
    vec3 slant = INNRM[d].xyz * ins + ldir * dist;
    vec3 cx    = cross(BINV[d].xyz, slant);
    float cl   = length(cx);
    nrm = (cl > 1e-9) ? cx * (1.0 / cl) : normalize(ldir);
  }
  oN[o] = vec4(nrm, 0.0);
  oB[o] = vec4(BINV[d].xyz, 0.0);
  oU[o] = iU[sv]; oC[o] = iC[sv];
}
////////////////////////////////////////
compute_shader cs_face_seg : iface {        // MULTI-SEGMENT: each dup carries DPAR = field index (face*(SEGN+1)+ring).
  uint d = gl_GlobalInvocationID.x;         // cs_field_seg filled FLD0=FA(Cacc,scale) FLD1=FB(dir,twist) FLD2=FC(Cbase,inset).
  if (d >= p_vb) { return; }                // position = ring centroid + twisted/scaled base offset; absolute cross-section.
  uint sv = DUPV[d]; uint o = p_nv + d;
  uint fi   = DPAR[d];
  vec3  Cacc = FLD0[fi].xyz;  float sc = FLD0[fi].w;
  vec3  dir  = FLD1[fi].xyz;  float tw = FLD1[fi].w;
  vec3  Cb   = FLD2[fi].xyz;  float ins = FLD2[fi].w;
  vec3  off  = (iP[sv].xyz - Cb) * sc;                                  // base profile offset, scaled (absolute)
  float ct = cos(tw); float st = sin(tw);                              // Rodrigues twist about the local axis dir
  vec3  offr = off * ct + cross(dir, off) * st + dir * (dot(dir, off) * (1.0 - ct));
  offr = offr * (1.0 - ins);                                           // optional in-plane inset on top of scale
  oP[o] = vec4(Cacc + offr, 1.0);
  vec3 nrm = (NRMV[d].w > 0.5) ? normalize(dir) : ((length(offr) > 1e-9) ? normalize(offr) : normalize(dir));
  oN[o] = vec4(nrm, 0.0);                                              // provisional; face_normals/smooth_normals refine
  oB[o] = vec4(BINV[d].xyz, 0.0);
  oU[o] = iU[sv]; oC[o] = iC[sv];
}
)S";
}

// per-INPUT-face field evaluator: computes fP/fN/fArea/_tags per face (the Select kernel) and runs the
// distance/inset/direction predicates (SelExpr-traced GLSL; empty -> default), writing FLD0=(dist,inset,_,_)
// + FLD1=(dir.xyz,_). Runs each frame -> animates for free. Separate shader module (its own descriptor set).
static std::string _field_text(const std::string& distPred, const std::string& insetPred, const std::string& dirPred) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface fif_P  (descriptor_set 0) { buffer layout(std430) fpb  { vec4 Pd[];   }; }
storage_interface fif_vi (descriptor_set 0) { buffer layout(std430) fvib { uint VId[];  }; }
storage_interface fif_fo (descriptor_set 0) { buffer layout(std430) ffob { uint FOd[];  }; }
storage_interface fif_tg (descriptor_set 0) { buffer layout(std430) ftgb { uint TAGd[]; }; }
storage_interface fif_o0 (descriptor_set 0) { buffer layout(std430) fo0b { vec4 FLD0[]; }; }
storage_interface fif_o1 (descriptor_set 0) { buffer layout(std430) fo1b { vec4 FLD1[]; }; }
storage_interface fif_uv (descriptor_set 0) { buffer layout(std430) fuvb { vec4 UVd[]; }; }   // input UV0 (per-vert)
storage_interface fif_pr (descriptor_set 0) { buffer layout(std430) fprb { vec4 EXPRP[]; }; } // generic runtime params (DSL `param()`)
storage_interface fif_ct (descriptor_set 0) { buffer layout(std430) fctb { uint f_count; uint fc0; uint fc1; uint fc2; }; }
compute_interface ifield { storage { fif_P fif_vi fif_fo fif_tg fif_o0 fif_o1 fif_uv fif_pr fif_ct }
                           inputs { layout(local_size_x = 64); } }
compute_shader cs_field : ifield {
  uint _eid = gl_GlobalInvocationID.x;
  if (_eid >= f_count) { return; }
  uint a = FOd[_eid]; uint b = FOd[_eid + 1u]; uint n = b - a;
  vec3 fP = vec3(0.0);
  vec3 fUV = vec3(0.0);                                   // per-face UV centroid (.xy=uv); strands inherit the root UV
  for (uint k = 0u; k < n; k++) { fP += Pd[VId[a + k]].xyz; fUV.xy += UVd[VId[a + k]].xy; }
  fP = fP / float(n);  fUV = fUV / float(n);
  vec3 q0 = Pd[VId[a]].xyz; vec3 q1 = Pd[VId[a + 1u]].xyz; vec3 q2 = Pd[VId[a + 2u]].xyz;
  vec3 fN = normalize(cross(q1 - q0, q2 - q0));
  float fArea = 0.0;
  for (uint k = 1u; k + 1u < n; k++) {
    vec3 e1 = Pd[VId[a + k]].xyz - q0; vec3 e2 = Pd[VId[a + k + 1u]].xyz - q0;
    fArea += 0.5 * length(cross(e1, e2));
  }
  uint _tags = TAGd[_eid];
  float _segt = 1.0; float _segi = 1.0;                  // single-segment: S.t/S.seg degrade to the cap (end of lift)
  float _dist  = 1.0;
  float _inset = 0.0;
  vec3  _dir   = fN;
  %DISTPRED%
  %INSETPRED%
  %DIRPRED%
  FLD0[_eid] = vec4(_dist, _inset, 0.0, 0.0);
  FLD1[_eid] = vec4(normalize(_dir), 0.0);
}
)S";
  _shadersub(t, "%DISTPRED%",  distPred);
  _shadersub(t, "%INSETPRED%", insetPred);
  _shadersub(t, "%DIRPRED%",   dirPred);
  return t;
}

// MULTI-SEGMENT field evaluator: one thread per (input face, ring r in 0..SEGN). Recomputes the face geometry
// (fP/fN/fArea/fUV/_tags), then PREFIX-ACCUMULATES the centroid path C_r = fP + sum_{j=1..r} dir(t_j)*dist(t_j)*GDIST
// (so a per-t `direction` bends the strand), capturing the ABSOLUTE ring-r twist/scale/inset/dir. Writes
// FA=(C_r, scale) FB=(dir, twist) FC=(fP, inset) at index face*(SEGN+1)+r. Ring 0 = identity (loop body skipped).
static std::string _field_seg_text(const std::string& distPred, const std::string& insetPred,
                                   const std::string& dirPred, const std::string& twistPred,
                                   const std::string& scalePred) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface gif_P  (descriptor_set 0) { buffer layout(std430) gpb  { vec4 Pd[];   }; }
storage_interface gif_vi (descriptor_set 0) { buffer layout(std430) gvib { uint VId[];  }; }
storage_interface gif_fo (descriptor_set 0) { buffer layout(std430) gfob { uint FOd[];  }; }
storage_interface gif_tg (descriptor_set 0) { buffer layout(std430) gtgb { uint TAGd[]; }; }
storage_interface gif_a  (descriptor_set 0) { buffer layout(std430) gab  { vec4 FA[]; }; }   // (Cacc.xyz, scale)
storage_interface gif_b  (descriptor_set 0) { buffer layout(std430) gbb  { vec4 FB[]; }; }   // (dir.xyz, twist)
storage_interface gif_uv (descriptor_set 0) { buffer layout(std430) guvb { vec4 UVd[]; }; }
storage_interface gif_pr (descriptor_set 0) { buffer layout(std430) gprb { vec4 EXPRP[]; }; }
storage_interface gif_ct (descriptor_set 0) { buffer layout(std430) gctb { uint f_count; uint SEGN; float GDIST; uint gc2; }; }
storage_interface gif_c  (descriptor_set 0) { buffer layout(std430) gcb  { vec4 FC[]; }; }   // (Cbase.xyz, inset)
compute_interface igseg { storage { gif_P gif_vi gif_fo gif_tg gif_a gif_b gif_uv gif_pr gif_ct gif_c }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_field_seg : igseg {
  uint _gid  = gl_GlobalInvocationID.x;
  uint _ringN = SEGN + 1u;
  uint _ring = _gid % _ringN;                            // 0..SEGN
  uint _eid  = _gid / _ringN;                            // input face index
  if (_eid >= f_count) { return; }
  uint a = FOd[_eid]; uint b = FOd[_eid + 1u]; uint n = b - a;
  vec3 fP = vec3(0.0); vec3 fUV = vec3(0.0);
  for (uint k = 0u; k < n; k++) { fP += Pd[VId[a + k]].xyz; fUV.xy += UVd[VId[a + k]].xy; }
  fP = fP / float(n);  fUV = fUV / float(n);
  vec3 q0 = Pd[VId[a]].xyz; vec3 q1 = Pd[VId[a + 1u]].xyz; vec3 q2 = Pd[VId[a + 2u]].xyz;
  vec3 fN = normalize(cross(q1 - q0, q2 - q0));
  float fArea = 0.0;
  for (uint k = 1u; k + 1u < n; k++) {
    vec3 e1 = Pd[VId[a + k]].xyz - q0; vec3 e2 = Pd[VId[a + k + 1u]].xyz - q0;
    fArea += 0.5 * length(cross(e1, e2));
  }
  uint _tags = TAGd[_eid];
  vec3  Cacc   = fP;                                     // ring 0 (identity): the loop runs zero times
  float _scale = 1.0; float _twist = 0.0; float _inset = 0.0; vec3 _dir = fN;
  for (uint j = 1u; j <= _ring; j++) {
    float _segt = float(j) / float(SEGN);
    float _segi = float(j);
    float _dist = 1.0; _inset = 0.0; _scale = 1.0; _twist = 0.0; _dir = fN;
    %DISTPRED%
    %INSETPRED%
    %DIRPRED%
    %TWISTPRED%
    %SCALEPRED%
    Cacc += normalize(_dir) * (_dist * GDIST);           // accumulate the centroid path (per-ring step)
  }
  uint _idx = _eid * _ringN + _ring;
  FA[_idx] = vec4(Cacc, _scale);
  FB[_idx] = vec4(normalize(_dir), _twist);
  FC[_idx] = vec4(fP, _inset);
}
)S";
  _shadersub(t, "%DISTPRED%",  distPred);
  _shadersub(t, "%INSETPRED%", insetPred);
  _shadersub(t, "%DIRPRED%",   dirPred);
  _shadersub(t, "%TWISTPRED%", twistPred);
  _shadersub(t, "%SCALEPRED%", scalePred);
  return t;
}

static constexpr int kModeVertex = 0;  // region: welded, vertex-normal lift, boundary walls (NON-planar)
static constexpr int kModeFace   = 1;  // individual: face-normal lift, faces separate, PLANAR walls
static constexpr uint32_t kPartFloor = 3;  // keep_base partition: the re-closed footprint (FaceTagger slot 3)

///////////////////////////////////////////////////////////////////////////////
// C.4b GPU FACE-MODE TOPOLOGY BUILD. Face mode is per-face independent (no edge table, no sort), so
// the CPU rebuild (full vidx/fo/tags/P/N readback + emission) becomes count -> 3 scans -> emit, with
// tags read ON the GPU and the per-dup float tables (face normal / centroid / edge tangents / inset
// dirs) computed in-shader from POSITION with the same expressions the CPU used (the equivalence
// oracle compares bakes byte-for-byte). The ONLY readback is the 3 scan totals (12 bytes) per
// topology-change rebuild. Vertex/region mode keeps the CPU build (boundary classification needs the
// edge map — ports later). Each stage = its own shader module, exact-fit iface (the shadlang trap).
///////////////////////////////////////////////////////////////////////////////

// stage 1: per-face dup/corner/face counts (selected faces fan out, unselected pass through).
static std::string _xgb_cnt_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface xg_ct (descriptor_set 0) { buffer layout(std430) xctb {
  uint g_nf; uint g_nv; uint g_bit; uint g_kb; uint g_segs; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface xg_fo (descriptor_set 0) { buffer layout(std430) xfob { uint FO[]; }; }
storage_interface xg_tg (descriptor_set 0) { buffer layout(std430) xtgb { uint TAGS[]; }; }
storage_interface xg_dc (descriptor_set 0) { buffer layout(std430) xdcb { uint DCNT[]; }; }
storage_interface xg_cc (descriptor_set 0) { buffer layout(std430) xccb { uint CCNT[]; }; }
storage_interface xg_fc (descriptor_set 0) { buffer layout(std430) xfcb { uint FCNT[]; }; }
compute_interface iface { storage { xg_ct xg_fo xg_tg xg_dc xg_cc xg_fc } inputs { layout(local_size_x = 64); } }
compute_shader cs_xcnt : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  uint n   = FO[f + 1u] - FO[f];
  bool sel = (TAGS[f] & g_bit) != 0u;
  if (!sel) { DCNT[f] = 0u; CCNT[f] = n; FCNT[f] = 1u; return; }
  if (g_segs > 1u) {                       // multi-segment: N+1 rings of n dups; cap + N*n walls (+floor)
    DCNT[f] = n * (g_segs + 1u);
    CCNT[f] = n + 4u * g_segs * n + g_kb * n;
    FCNT[f] = 1u + g_segs * n + g_kb;
  } else {                                 // single: n cap dups + 4n wall dups; cap + n walls (+floor)
    DCNT[f] = 5u * n;
    CCNT[f] = 5u * n + g_kb * n;
    FCNT[f] = 1u + n + g_kb;
  }
}
)S";
}
// stage 2a: refined topology (vidx/CSR/parent/partition) + the all-zero LIFT table.
static std::string _xgb_topo_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface xg_ct (descriptor_set 0) { buffer layout(std430) xctb {
  uint g_nf; uint g_nv; uint g_bit; uint g_kb; uint g_segs; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface xg_fo (descriptor_set 0) { buffer layout(std430) xfob { uint FO[]; }; }
storage_interface xg_vi (descriptor_set 0) { buffer layout(std430) xvib { uint VI[]; }; }
storage_interface xg_tg (descriptor_set 0) { buffer layout(std430) xtgb { uint TAGS[]; }; }
storage_interface xg_db (descriptor_set 0) { buffer layout(std430) xdbb { uint DBASE[]; }; }
storage_interface xg_cb (descriptor_set 0) { buffer layout(std430) xcbb { uint CBASE[]; }; }
storage_interface xg_fb (descriptor_set 0) { buffer layout(std430) xfbb { uint FBASE[]; }; }
storage_interface xg_ov (descriptor_set 0) { buffer layout(std430) xovb { uint OV[]; }; }
storage_interface xg_of (descriptor_set 0) { buffer layout(std430) xofb { uint OF[]; }; }
storage_interface xg_op (descriptor_set 0) { buffer layout(std430) xopb { uint OPAR[]; }; }
storage_interface xg_oq (descriptor_set 0) { buffer layout(std430) xoqb { uint OPART[]; }; }
storage_interface xg_lf (descriptor_set 0) { buffer layout(std430) xlfb { uint LIFT[]; }; }
compute_interface iface { storage { xg_ct xg_fo xg_vi xg_tg xg_db xg_cb xg_fb xg_ov xg_of xg_op xg_oq xg_lf }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_xzlift : iface {            // face mode never lifts originals in place
  uint v = gl_GlobalInvocationID.x;
  if (v >= g_nv) { return; }
  LIFT[v] = 0u;
}
compute_shader cs_xtopo : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if (f == 0u) { OF[FBASE[g_nf]] = CBASE[g_nf]; }            // out CSR tail
  uint a = FO[f]; uint n = FO[f + 1u] - a;
  uint db = g_nv + DBASE[f]; uint cb = CBASE[f]; uint fb = FBASE[f];
  bool sel = (TAGS[f] & g_bit) != 0u;
  if (!sel) {                                                // unchanged (original verts)
    OF[fb] = cb; OPAR[fb] = f; OPART[fb] = 0u;               // kPartBase
    for (uint k = 0u; k < n; k++) { OV[cb + k] = VI[a + k]; }
    return;
  }
  if (g_segs > 1u) {                                         // rings: ring(r,k) = db + r*n + k
    uint N = g_segs;
    OF[fb] = cb; OPAR[fb] = f; OPART[fb] = 1u;               // CAP = top ring
    for (uint k = 0u; k < n; k++) { OV[cb + k] = db + N * n + k; }
    for (uint r = 1u; r <= N; r++) {
      for (uint k = 0u; k < n; k++) {
        uint k1 = (k + 1u) % n;
        uint w  = (r - 1u) * n + k;
        uint c4 = cb + n + 4u * w;
        OF[fb + 1u + w] = c4; OPAR[fb + 1u + w] = 0xFFFFFFFFu; OPART[fb + 1u + w] = 2u;  // WALL
        OV[c4 + 0u] = db + (r - 1u) * n + k;
        OV[c4 + 1u] = db + (r - 1u) * n + k1;
        OV[c4 + 2u] = db + r * n + k1;
        OV[c4 + 3u] = db + r * n + k;
      }
    }
    if (g_kb != 0u) {                                        // FLOOR = ring 0 (re-close the footprint)
      uint ff = fb + 1u + N * n; uint cc = cb + n + 4u * N * n;
      OF[ff] = cc; OPAR[ff] = f; OPART[ff] = 3u;
      for (uint k = 0u; k < n; k++) { OV[cc + k] = db + k; }
    }
  } else {
    OF[fb] = cb; OPAR[fb] = f; OPART[fb] = 1u;               // CAP (dups 0..n-1)
    for (uint k = 0u; k < n; k++) { OV[cb + k] = db + k; }
    for (uint k = 0u; k < n; k++) {                          // walls: dups n+4k+(wa,wb,wbp,wap)
      uint c4 = cb + n + 4u * k;
      OF[fb + 1u + k] = c4; OPAR[fb + 1u + k] = 0xFFFFFFFFu; OPART[fb + 1u + k] = 2u;
      OV[c4 + 0u] = db + n + 4u * k + 0u;
      OV[c4 + 1u] = db + n + 4u * k + 1u;
      OV[c4 + 2u] = db + n + 4u * k + 2u;
      OV[c4 + 3u] = db + n + 4u * k + 3u;
    }
    if (g_kb != 0u) {                                        // FLOOR = the original face
      uint ff = fb + 1u + n; uint cc = cb + 5u * n;
      OF[ff] = cc; OPAR[ff] = f; OPART[ff] = 3u;
      for (uint k = 0u; k < n; k++) { OV[cc + k] = VI[a + k]; }
    }
  }
}
)S";
}
// stage 2b: the per-dup attribute tables — the float math MIRRORS the CPU rebuild exactly
// (manual x*(1/L) normalization, same accumulation order) for byte-identical bakes.
static std::string _xgb_dups_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface xg_ct (descriptor_set 0) { buffer layout(std430) xctb {
  uint g_nf; uint g_nv; uint g_bit; uint g_kb; uint g_segs; uint g_p5; uint g_p6; uint g_p7; }; }
storage_interface xg_fo (descriptor_set 0) { buffer layout(std430) xfob { uint FO[]; }; }
storage_interface xg_vi (descriptor_set 0) { buffer layout(std430) xvib { uint VI[]; }; }
storage_interface xg_tg (descriptor_set 0) { buffer layout(std430) xtgb { uint TAGS[]; }; }
storage_interface xg_ps (descriptor_set 0) { buffer layout(std430) xpsb { vec4 POS[]; }; }
storage_interface xg_db (descriptor_set 0) { buffer layout(std430) xdbb { uint DBASE[]; }; }
storage_interface xg_fs (descriptor_set 0) { buffer layout(std430) xfsb { uint FSRC[]; }; }
storage_interface xg_l0 (descriptor_set 0) { buffer layout(std430) xl0b { vec4 FLDIR[]; }; }
storage_interface xg_l1 (descriptor_set 0) { buffer layout(std430) xl1b { vec4 FINPOS[]; }; }
storage_interface xg_l2 (descriptor_set 0) { buffer layout(std430) xl2b { vec4 FBIN[]; }; }
storage_interface xg_l3 (descriptor_set 0) { buffer layout(std430) xl3b { vec4 FINNRM[]; }; }
storage_interface xg_dp (descriptor_set 0) { buffer layout(std430) xdpb { uint FDPAR[]; }; }
compute_interface iface { storage { xg_ct xg_fo xg_vi xg_tg xg_ps xg_db xg_fs xg_l0 xg_l1 xg_l2 xg_l3 xg_dp }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_xdups : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= g_nf) { return; }
  if ((TAGS[f] & g_bit) == 0u) { return; }                   // unselected: no dups
  uint a = FO[f]; uint n = FO[f + 1u] - a;
  uint db = DBASE[f];                                        // dup-table index (NOT +g_nv: tables are dup-local)
  vec3 p0 = POS[VI[a]].xyz; vec3 p1 = POS[VI[a + 1u]].xyz;
  vec3 ctan = p1 - p0; float CL = length(ctan);
  ctan = (CL > 1e-12) ? ctan * (1.0 / CL) : vec3(1.0, 0.0, 0.0);
  if (g_segs > 1u) {                                         // rings: const lift dir; etan per base edge
    uint N = g_segs;
    for (uint r = 0u; r <= N; r++) {
      float isCap = (r == N) ? 1.0 : 0.0;
      for (uint k = 0u; k < n; k++) {
        uint k1 = (k + 1u) % n;
        vec3 edge = POS[VI[a + k1]].xyz - POS[VI[a + k]].xyz; float EL = length(edge);
        vec3 et = (EL > 1e-12) ? edge * (1.0 / EL) : ctan;
        uint j = db + r * n + k;
        FSRC[j]   = VI[a + k];
        FLDIR[j]  = vec4(0.0, 1.0, 0.0, 0.0);
        FINPOS[j] = vec4(0.0, 0.0, 0.0, isCap);
        FBIN[j]   = vec4(et, 0.0);
        FINNRM[j] = vec4(0.0);
        FDPAR[j]  = f * (N + 1u) + r;                        // per-(face,ring) FIELD index
      }
    }
    return;
  }
  vec3 p2 = POS[VI[a + 2u]].xyz;                             // single: real face normal + centroid insets
  vec3 fn = cross(p1 - p0, p2 - p0); float L = length(fn);
  fn = (L > 1e-12) ? fn * (1.0 / L) : vec3(0.0, 1.0, 0.0);
  vec3 C = vec3(0.0);
  for (uint k = 0u; k < n; k++) { C += POS[VI[a + k]].xyz; }
  C = C * (1.0 / float(n));
  for (uint k = 0u; k < n; k++) {                            // cap dups (j = db+k)
    uint j = db + k;
    vec3 vk = POS[VI[a + k]].xyz;
    FSRC[j]   = VI[a + k];
    FLDIR[j]  = vec4(fn, 1.0);
    FINPOS[j] = vec4(C - vk, 1.0);
    FBIN[j]   = vec4(ctan, 0.0);
    FINNRM[j] = vec4(0.0);
    FDPAR[j]  = f;
  }
  for (uint k = 0u; k < n; k++) {                            // wall dups (j = db+n+4k+{wa,wb,wbp,wap})
    uint k1 = (k + 1u) % n;
    vec3 Pa = POS[VI[a + k]].xyz; vec3 Pb = POS[VI[a + k1]].xyz;
    vec3 edge = Pb - Pa; float EL = length(edge);
    vec3 et = (EL > 1e-12) ? edge * (1.0 / EL) : ctan;
    vec3 insA = C - Pa; vec3 insB = C - Pb; vec3 insMid = (insA + insB) * 0.5;
    uint j = db + n + 4u * k;
    FSRC[j + 0u] = VI[a + k];  FLDIR[j + 0u] = vec4(fn, 0.0); FINPOS[j + 0u] = vec4(0.0);
    FBIN[j + 0u] = vec4(et, 0.0); FINNRM[j + 0u] = vec4(insMid, 0.0); FDPAR[j + 0u] = f;
    FSRC[j + 1u] = VI[a + k1]; FLDIR[j + 1u] = vec4(fn, 0.0); FINPOS[j + 1u] = vec4(0.0);
    FBIN[j + 1u] = vec4(et, 0.0); FINNRM[j + 1u] = vec4(insMid, 0.0); FDPAR[j + 1u] = f;
    FSRC[j + 2u] = VI[a + k1]; FLDIR[j + 2u] = vec4(fn, 1.0); FINPOS[j + 2u] = vec4(insB, 0.0);
    FBIN[j + 2u] = vec4(et, 0.0); FINNRM[j + 2u] = vec4(insMid, 0.0); FDPAR[j + 2u] = f;
    FSRC[j + 3u] = VI[a + k];  FLDIR[j + 3u] = vec4(fn, 1.0); FINPOS[j + 3u] = vec4(insA, 0.0);
    FBIN[j + 3u] = vec4(et, 0.0); FINNRM[j + 3u] = vec4(insMid, 0.0); FDPAR[j + 3u] = f;
  }
}
)S";
}

struct ExtrudeFacesInst : public MeshComputeInst {
  ExtrudeFacesInst(const ExtrudeFacesData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
    _dist   = _floatPlug(this, _d, "distance");
    for (int k = 0; k < kMaxExprParams; k++)
      _exprplugs[k] = typedInputNamed<dflow::Vec4fPlugTraits>(FormatString("exprp%d", k).c_str());
  }
  static const MeshChannel kCh[5];

  // C.4b: FACE mode rebuilds ON the GPU (count -> 3 scans -> emit; 12-byte totals readback);
  // vertex/region mode keeps the CPU build below. ORK_HM_EXTRUDE_CPU=1 = the oracle's reference.
  void rebuildGpuFace(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    auto ci  = ctx->CI();
    int nv = in->_num_verts, nf = in->_num_faces;
    int N  = std::max(1, _d->_segments);
    if (not _cs_xcnt) {
      _cs_xcnt   = fxi->computeShader(fxi->shaderFromShaderText("hm_xgb_cnt", _xgb_cnt_text()), "cs_xcnt");
      auto tsh   = fxi->shaderFromShaderText("hm_xgb_topo", _xgb_topo_text());
      _cs_xtopo  = fxi->computeShader(tsh, "cs_xtopo");
      _cs_xzlift = fxi->computeShader(tsh, "cs_xzlift");
      _cs_xdups  = fxi->computeShader(fxi->shaderFromShaderText("hm_xgb_dups", _xgb_dups_text()), "cs_xdups");
      _xscan.init(ctx);
      _xctl = fxi->createStorageBuffer(32);
    }
    if (meshNextPow2(nf + 1) > _xcap_nf) {                   // scan scratch (grow-only)
      _xcap_nf = meshNextPow2(nf + 1);
      _xdc = env->_pool->acquire(4, nf);     _xcc = env->_pool->acquire(4, nf);     _xfc = env->_pool->acquire(4, nf);
      _xdb = env->_pool->acquire(4, nf + 1); _xcb = env->_pool->acquire(4, nf + 1); _xfb = env->_pool->acquire(4, nf + 1);
      if (not _xs1) { _xs1 = fxi->createStorageBuffer(16); _xs2 = fxi->createStorageBuffer(16); _xs3 = fxi->createStorageBuffer(16); }
    }
    auto tg = in->face("__tags");
    FxShaderStorageBuffer* tags = tg ? tg->_ssbo : in->_vidx->_ssbo;  // no tags -> dummy bind + bit=0 (selects nothing)
    uint32_t bit = tg ? (1u << (_d->_slot & 31)) : 0u;
    auto w4 = [&](FxShaderStorageBuffer* b, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
      uint32_t v[4] = {a0, a1, a2, a3};
      auto m = fxi->mapStorageBuffer(b, 0, sizeof(v), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, v, sizeof(v)); fxi->unmapStorageBuffer(m.get());
    };
    w4(_xs1, uint32_t(nf), 0, 0, 0); w4(_xs2, uint32_t(nf), 0, 0, 0); w4(_xs3, uint32_t(nf), 0, 0, 0);
    {
      uint32_t c[8] = {uint32_t(nf), uint32_t(nv), bit, _d->_keep_base ? 1u : 0u,
                       uint32_t(_d->_segments > 1 ? N : 1), 0u, 0u, 0u};
      auto m = fxi->mapStorageBuffer(_xctl, 0, sizeof(c), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, c, sizeof(c)); fxi->unmapStorageBuffer(m.get());
    }
    // phase 1: counts + the three scans
    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_xcnt, 0, _xctl); ci->bindStorageBuffer(_cs_xcnt, 1, in->_face_offsets->_ssbo);
    ci->bindStorageBuffer(_cs_xcnt, 2, tags);  ci->bindStorageBuffer(_cs_xcnt, 3, _xdc);
    ci->bindStorageBuffer(_cs_xcnt, 4, _xcc);  ci->bindStorageBuffer(_cs_xcnt, 5, _xfc);
    ci->dispatchCompute(_cs_xcnt, (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    _xscan.scan(ctx, _xdc, _xdb, _xs1, nf); ci->storageBarrier();
    _xscan.scan(ctx, _xcc, _xcb, _xs2, nf); ci->storageBarrier();
    _xscan.scan(ctx, _xfc, _xfb, _xs3, nf); ci->storageBarrier();
    ci->endDispatchPhase();
    // the ONLY readback: the 3 totals (dups / corners / faces)
    auto r1 = [&](FxShaderStorageBuffer* b) {
      auto m = fxi->mapStorageBuffer(b, size_t(nf) * 4, 4, BufferMapAccess::READ_ONLY);
      uint32_t v = *reinterpret_cast<const uint32_t*>(m->_mappedaddr);
      fxi->unmapStorageBuffer(m.get());
      return int(v);
    };
    int Vf = r1(_xdb), onc = r1(_xcb), onf = r1(_xfb);
    int FF = (_d->_segments > 1) ? nf * (N + 1) : nf;        // field count (per-(face,ring) | per-face)
    _new_nv = nv + Vf; _new_nc = onc; _new_nf = onf; _nv = nv; _vb = Vf;
    _vidx  = env->_pool->acquireChannel(4, _new_nc);
    _fo    = env->_pool->acquireChannel(4, _new_nf + 1);
    _lift  = env->_pool->acquireChannel(4, nv);
    _dupv  = env->_pool->acquireChannel(4, std::max(1, Vf));
    _fn    = env->_pool->acquireChannel(16, std::max(1, Vf));
    _nrm   = env->_pool->acquireChannel(16, std::max(1, Vf));
    _bin   = env->_pool->acquireChannel(16, std::max(1, Vf));
    _innrm = env->_pool->acquireChannel(16, std::max(1, Vf));
    _dpar  = env->_pool->acquireChannel(4,  std::max(1, Vf));
    _fld0  = env->_pool->acquireChannel(16, std::max(1, FF));
    _fld1  = env->_pool->acquireChannel(16, std::max(1, FF));
    _fld2  = env->_pool->acquireChannel(16, (_d->_segments > 1) ? std::max(1, FF) : 1);
    _otags  = env->_pool->acquireChannel(4, _new_nf);
    _parent = env->_pool->acquireChannel(4, _new_nf);
    _part   = env->_pool->acquireChannel(4, _new_nf);
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (not _params) _params = fxi->createStorageBuffer(16);
    if (not _fctl)   _fctl   = fxi->createStorageBuffer(16);
    if (not _exprp)  _exprp  = fxi->createStorageBuffer(size_t(kMaxExprParams) * 16);
    // phase 2: topology + zero-lift + the per-dup tables
    ci->beginDispatchPhase();
    auto bindT = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _xctl); ci->bindStorageBuffer(cs, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 2, in->_vidx->_ssbo); ci->bindStorageBuffer(cs, 3, tags);
      ci->bindStorageBuffer(cs, 4, _xdb); ci->bindStorageBuffer(cs, 5, _xcb);
      ci->bindStorageBuffer(cs, 6, _xfb); ci->bindStorageBuffer(cs, 7, _vidx->_ssbo);
      ci->bindStorageBuffer(cs, 8, _fo->_ssbo); ci->bindStorageBuffer(cs, 9, _parent->_ssbo);
      ci->bindStorageBuffer(cs, 10, _part->_ssbo); ci->bindStorageBuffer(cs, 11, _lift->_ssbo);
    };
    bindT(_cs_xtopo);  ci->dispatchCompute(_cs_xtopo, (nf + 63) / 64, 1, 1);
    bindT(_cs_xzlift); ci->dispatchCompute(_cs_xzlift, (nv + 63) / 64, 1, 1);
    if (Vf > 0) {
      ci->bindStorageBuffer(_cs_xdups, 0, _xctl); ci->bindStorageBuffer(_cs_xdups, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(_cs_xdups, 2, in->_vidx->_ssbo); ci->bindStorageBuffer(_cs_xdups, 3, tags);
      ci->bindStorageBuffer(_cs_xdups, 4, in->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs_xdups, 5, _xdb); ci->bindStorageBuffer(_cs_xdups, 6, _dupv->_ssbo);
      ci->bindStorageBuffer(_cs_xdups, 7, _fn->_ssbo); ci->bindStorageBuffer(_cs_xdups, 8, _nrm->_ssbo);
      ci->bindStorageBuffer(_cs_xdups, 9, _bin->_ssbo); ci->bindStorageBuffer(_cs_xdups, 10, _innrm->_ssbo);
      ci->bindStorageBuffer(_cs_xdups, 11, _dpar->_ssbo);
      ci->dispatchCompute(_cs_xdups, (nf + 63) / 64, 1, 1);
    }
    ci->storageBarrier();
    ci->endDispatchPhase();
  }

  // CPU boundary build from the CURRENT source (readback vidx/face_offsets + the tags), classify, emit
  // caps + walls, upload + re-pool. Re-runnable (rebuild-on-change); buffers are COW handles -> recycle.
  void rebuild(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    if (_d->_mode == kModeFace and not getenv("ORK_HM_EXTRUDE_CPU")) {
      rebuildGpuFace(ctx, in);                               // C.4b: GPU build; CPU branch = the oracle reference
      return;
    }
    int nv = in->_num_verts, nf = in->_num_faces, nc = in->_num_corners;
    auto rd = [&](FxShaderStorageBuffer* b, int n) {
      std::vector<uint32_t> v(n);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(n) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(n) * 4);
      fxi->unmapStorageBuffer(m.get());
      return v;
    };
    auto vidx = rd(in->_vidx->_ssbo, nc);
    auto fo   = rd(in->_face_offsets->_ssbo, nf + 1);
    std::vector<uint32_t> tags(nf, 0);
    if (auto tg = in->face("__tags")) tags = rd(tg->_ssbo, nf);
    uint32_t bit = 1u << (_d->_slot & 31);
    // source positions + per-vertex normals (vec4/vert) — used only to pick each wall quad's fan diagonal
    // (below) so the non-planar wall doesn't fold a back-facing triangle. read-only, rebuild-time only.
    auto rdf = [&](FxShaderStorageBuffer* b, int nverts) {
      std::vector<float> v(size_t(nverts) * 4);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(nverts) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(nverts) * 16);
      fxi->unmapStorageBuffer(m.get());
      return v;
    };
    auto Pin = rdf(in->channel(MeshChannel::POSITION)->_ssbo, nv);
    auto Nin = rdf(in->channel(MeshChannel::NORMAL)->_ssbo, nv);
    auto P   = [&](uint32_t v) { return fvec3(Pin[4 * v], Pin[4 * v + 1], Pin[4 * v + 2]); };
    auto N   = [&](uint32_t v) { return fvec3(Nin[4 * v], Nin[4 * v + 1], Nin[4 * v + 2]); };

    std::vector<char> selFace(nf);
    for (int f = 0; f < nf; f++) selFace[f] = (tags[f] & bit) ? 1 : 0;

    // ============================ FACE mode (individual) ============================
    // each selected face lifts along its OWN face normal; faces SEPARATE (no shared lifted verts); a PLANAR
    // wall quad per edge (top edge = base edge + faceN*dist) -> the render fan can't fold it (no see-through).
    if (_d->_mode == kModeFace) {
      std::vector<uint32_t> ovidx, ofo, fsrc, oparent, opart, fdpar;
      std::vector<float>    fLDIR, fINPOS, fBIN, fINNRM;               // per-dup vec4 (see sif_fn/nrm/bin/inn)
      int nfo = 0;
      auto face   = [&]() { ofo.push_back(uint32_t(ovidx.size())); nfo++; };
      auto push4  = [](std::vector<float>& v, fvec3 a, float w) { v.push_back(a.x); v.push_back(a.y); v.push_back(a.z); v.push_back(w); };
      // a dedicated output vertex of parent face `pf`. faceN+liftw = baked lift dir & weight; insdir = in-plane
      // shrink toward centroid (gated by per-face inset, runtime); isCap selects the normal model; etan = the
      // tangent/binormal; riminset = the quad-consistent inset dir the in-shader rim normal uses.
      auto add    = [&](uint32_t src, fvec3 faceN, float liftw, fvec3 insdir, float isCap,
                        fvec3 etan, fvec3 riminset, uint32_t pf) -> uint32_t {
        uint32_t idx = uint32_t(nv) + uint32_t(fsrc.size());
        fsrc.push_back(src);
        push4(fLDIR, faceN, liftw); push4(fINPOS, insdir, isCap); push4(fBIN, etan, 0.0f); push4(fINNRM, riminset, 0.0f);
        fdpar.push_back(pf);
        return idx;
      };
      // -------------------------- MULTI-SEGMENT (N rings, welded tube) --------------------------
      // Each selected face becomes a stacked tube of N+1 rings: ring 0 = identity footprint, ring N = cap.
      // Each dup carries DPAR = the per-(face,ring) field index (cs_field_seg fills FA/FB/FC there). Rings are
      // WELDED within a face (smooth tube; smooth_normals reads it as a rounded strand; face_normals re-splits).
      if (_d->_segments > 1) {
        int N = _d->_segments;
        for (int f = 0; f < nf; f++) {
          uint32_t a = fo[f], b = fo[f + 1]; int n = int(b - a);
          if (not selFace[f]) {                                        // unselected -> unchanged (original verts)
            face(); for (uint32_t c = a; c < b; c++) ovidx.push_back(vidx[c]);
            oparent.push_back(uint32_t(f)); opart.push_back(kPartBase); continue;
          }
          fvec3 p0 = P(vidx[a]), p1 = P(vidx[a + 1]);                  // a cap tangent (first edge dir) fallback
          fvec3 ctan = (p1 - p0); float CL = ctan.length();
          ctan = (CL > 1e-12f) ? ctan * (1.0f / CL) : fvec3(1, 0, 0);
          std::vector<std::vector<uint32_t>> ring(N + 1, std::vector<uint32_t>(n));
          for (int r = 0; r <= N; r++) {                               // rings 0..N, n dups each
            float isCap = (r == N) ? 1.0f : 0.0f;
            for (int k = 0; k < n; k++) {
              int k1 = (k + 1) % n;
              fvec3 edge = P(vidx[a + k1]) - P(vidx[a + k]); float EL = edge.length();
              fvec3 etan = (EL > 1e-12f) ? edge * (1.0f / EL) : ctan;  // wall binormal = base edge dir
              ring[r][k] = add(vidx[a + k], fvec3(0, 1, 0), 0.0f, fvec3(0, 0, 0), isCap, etan,
                               fvec3(0, 0, 0), uint32_t(f * (N + 1) + r));   // pf slot carries the FIELD index
            }
          }
          face(); for (int k = 0; k < n; k++) ovidx.push_back(ring[N][k]);   // CAP face = top ring (source winding)
          oparent.push_back(uint32_t(f)); opart.push_back(kPartCap);
          for (int r = 1; r <= N; r++)                                 // wall quads between consecutive rings
            for (int k = 0; k < n; k++) {
              int k1 = (k + 1) % n;
              face();
              ovidx.push_back(ring[r - 1][k]); ovidx.push_back(ring[r - 1][k1]);   // [base a, base b, top b', top a']
              ovidx.push_back(ring[r][k1]);    ovidx.push_back(ring[r][k]);
              oparent.push_back(kFaceNoParent); opart.push_back(kPartWall);
            }
          if (_d->_keep_base) {                                        // re-close the footprint (ring 0, outward)
            face(); for (int k = 0; k < n; k++) ovidx.push_back(ring[0][k]);
            oparent.push_back(uint32_t(f)); opart.push_back(kPartFloor);
          }
        }
        ofo.push_back(uint32_t(ovidx.size()));
        int Vf = int(fsrc.size()); int FF = nf * (N + 1);              // per-(face,ring) field count
        _new_nv = nv + Vf; _new_nc = int(ovidx.size()); _new_nf = nfo; _nv = nv; _vb = Vf;
        _vidx  = env->_pool->acquireChannel(4, _new_nc);
        _fo    = env->_pool->acquireChannel(4, _new_nf + 1);
        _lift  = env->_pool->acquireChannel(4, nv);                    // all-zero: cs_orig just copies originals
        _dupv  = env->_pool->acquireChannel(4, std::max(1, Vf));
        _fn    = env->_pool->acquireChannel(16, std::max(1, Vf));      // unused by cs_face_seg; bound to keep the set valid
        _nrm   = env->_pool->acquireChannel(16, std::max(1, Vf));      // NRMV: .w = isCap (cs_face_seg normal model)
        _bin   = env->_pool->acquireChannel(16, std::max(1, Vf));      // BINV: .xyz = tangent
        _innrm = env->_pool->acquireChannel(16, std::max(1, Vf));
        _dpar  = env->_pool->acquireChannel(4,  std::max(1, Vf));      // DPAR: per-(face,ring) FIELD index
        _fld0  = env->_pool->acquireChannel(16, std::max(1, FF));      // FA=(Cacc,scale)  — cs_field_seg writes
        _fld1  = env->_pool->acquireChannel(16, std::max(1, FF));      // FB=(dir,twist)
        _fld2  = env->_pool->acquireChannel(16, std::max(1, FF));      // FC=(Cbase,inset)
        auto up = [&](FxShaderStorageBuffer* buf, const std::vector<uint32_t>& v) {
          auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(1, v.size()) * 4, BufferMapAccess::WRITE_ONLY);
          if (not v.empty()) std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
          fxi->unmapStorageBuffer(m.get());
        };
        auto upf = [&](FxShaderStorageBuffer* buf, const std::vector<float>& v) {
          auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(4, v.size() * 4), BufferMapAccess::WRITE_ONLY);
          if (not v.empty()) std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
          fxi->unmapStorageBuffer(m.get());
        };
        std::vector<uint32_t> liftz(nv, 0);
        up(_vidx->_ssbo, ovidx); up(_fo->_ssbo, ofo); up(_lift->_ssbo, liftz); up(_dupv->_ssbo, fsrc); up(_dpar->_ssbo, fdpar);
        upf(_fn->_ssbo, fLDIR); upf(_nrm->_ssbo, fINPOS); upf(_bin->_ssbo, fBIN); upf(_innrm->_ssbo, fINNRM);
        _otags  = env->_pool->acquireChannel(4, _new_nf);
        _parent = env->_pool->acquireChannel(4, _new_nf);
        _part   = env->_pool->acquireChannel(4, _new_nf);
        up(_parent->_ssbo, oparent); up(_part->_ssbo, opart);
        for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
        if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
        if (not _params) _params = fxi->createStorageBuffer(16);
        if (not _fctl)   _fctl   = fxi->createStorageBuffer(16);
        if (not _exprp)  _exprp  = fxi->createStorageBuffer(size_t(kMaxExprParams) * 16);
        return;
      }
      for (int f = 0; f < nf; f++) {
        uint32_t a = fo[f], b = fo[f + 1]; int n = int(b - a);
        if (not selFace[f]) {                                          // unselected -> unchanged (original verts)
          face(); for (uint32_t c = a; c < b; c++) ovidx.push_back(vidx[c]);
          oparent.push_back(uint32_t(f)); opart.push_back(kPartBase); continue;
        }
        fvec3 p0 = P(vidx[a]), p1 = P(vidx[a + 1]), p2 = P(vidx[a + 2]);  // face normal (CCW: cross(p1-p0,p2-p0))
        fvec3 fnv = (p1 - p0).crossWith(p2 - p0); float L = fnv.length();
        fnv = (L > 1e-12f) ? fnv * (1.0f / L) : fvec3(0, 1, 0);
        fvec3 ctan = (p1 - p0); float CL = ctan.length();              // a cap tangent (first edge dir)
        ctan = (CL > 1e-12f) ? ctan * (1.0f / CL) : fvec3(1, 0, 0);
        fvec3 C(0, 0, 0);                                              // face centroid (inset pulls toward it)
        for (int k = 0; k < n; k++) C += P(vidx[a + k]);
        C = C * (1.0f / float(n));
        std::vector<uint32_t> du(n);                                   // CAP verts: lifted (+inset), normal = lift dir
        for (int k = 0; k < n; k++) {
          fvec3 vk = P(vidx[a + k]);
          du[k] = add(vidx[a + k], fnv, 1.0f, C - vk, 1.0f, ctan, fvec3(0, 0, 0), uint32_t(f));
        }
        face(); for (int k = 0; k < n; k++) ovidx.push_back(du[k]);    // CAP face (source winding)
        oparent.push_back(uint32_t(f)); opart.push_back(kPartCap);
        for (int k = 0; k < n; k++) {                                  // one wall/rim quad per edge, OWN verts
          int k1 = (k + 1) % n;
          fvec3 Pa = P(vidx[a + k]), Pb = P(vidx[a + k1]);
          fvec3 edge = Pb - Pa; float EL = edge.length();
          fvec3 etan = (EL > 1e-12f) ? edge * (1.0f / EL) : ctan;      // wall tangent (binormal) = base edge dir
          fvec3 insA = C - Pa, insB = C - Pb, insMid = (insA + insB) * 0.5f;   // toward-centroid (inset)
          uint32_t wa  = add(vidx[a + k],  fnv, 0.0f, fvec3(0, 0, 0), 0.0f, etan, insMid, uint32_t(f)); // base a (outer ring)
          uint32_t wb  = add(vidx[a + k1], fnv, 0.0f, fvec3(0, 0, 0), 0.0f, etan, insMid, uint32_t(f)); // base b
          uint32_t wbp = add(vidx[a + k1], fnv, 1.0f, insB,           0.0f, etan, insMid, uint32_t(f)); // top  b' (lift+inset)
          uint32_t wap = add(vidx[a + k],  fnv, 1.0f, insA,           0.0f, etan, insMid, uint32_t(f)); // top  a'
          face(); ovidx.push_back(wa); ovidx.push_back(wb); ovidx.push_back(wbp); ovidx.push_back(wap);
          oparent.push_back(kFaceNoParent); opart.push_back(kPartWall);
        }
        if (_d->_keep_base) {                                         // re-close the footprint (original face, +fN
          face(); for (uint32_t c = a; c < b; c++) ovidx.push_back(vidx[c]);  // outward) so the shell isn't see-through
          oparent.push_back(uint32_t(f)); opart.push_back(kPartFloor);
        }
      }
      ofo.push_back(uint32_t(ovidx.size()));
      int Vf = int(fsrc.size());
      _new_nv = nv + Vf; _new_nc = int(ovidx.size()); _new_nf = nfo; _nv = nv; _vb = Vf;
      _vidx  = env->_pool->acquireChannel(4, _new_nc);
      _fo    = env->_pool->acquireChannel(4, _new_nf + 1);
      _lift  = env->_pool->acquireChannel(4, nv);                      // all-zero: cs_orig just copies originals
      _dupv  = env->_pool->acquireChannel(4, std::max(1, Vf));         // FSRC: source vert per dup
      _fn    = env->_pool->acquireChannel(16, std::max(1, Vf));        // FNV : .xyz lift dir, .w lift weight
      _nrm   = env->_pool->acquireChannel(16, std::max(1, Vf));        // NRMV: .xyz inset dir, .w isCap
      _bin   = env->_pool->acquireChannel(16, std::max(1, Vf));        // BINV: .xyz tangent
      _innrm = env->_pool->acquireChannel(16, std::max(1, Vf));        // INNRM: .xyz rim inset dir (normal)
      _dpar  = env->_pool->acquireChannel(4,  std::max(1, Vf));        // DPAR: parent input face per dup
      _fld0  = env->_pool->acquireChannel(16, std::max(1, nf));        // per-face (dist, inset, _, _) — cs_field writes
      _fld1  = env->_pool->acquireChannel(16, std::max(1, nf));        // per-face (dir.xyz, _)
      _fld2  = env->_pool->acquireChannel(16, 1);                      // unused (single-segment); bound to keep set valid
      auto up = [&](FxShaderStorageBuffer* buf, const std::vector<uint32_t>& v) {
        auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(1, v.size()) * 4, BufferMapAccess::WRITE_ONLY);
        if (not v.empty()) std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
        fxi->unmapStorageBuffer(m.get());
      };
      auto upf = [&](FxShaderStorageBuffer* buf, const std::vector<float>& v) {
        auto m = fxi->mapStorageBuffer(buf, 0, std::max<size_t>(4, v.size() * 4), BufferMapAccess::WRITE_ONLY);
        if (not v.empty()) std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
        fxi->unmapStorageBuffer(m.get());
      };
      std::vector<uint32_t> liftz(nv, 0);
      up(_vidx->_ssbo, ovidx); up(_fo->_ssbo, ofo); up(_lift->_ssbo, liftz); up(_dupv->_ssbo, fsrc); up(_dpar->_ssbo, fdpar);
      upf(_fn->_ssbo, fLDIR); upf(_nrm->_ssbo, fINPOS); upf(_bin->_ssbo, fBIN); upf(_innrm->_ssbo, fINNRM);
      _otags  = env->_pool->acquireChannel(4, _new_nf);
      _parent = env->_pool->acquireChannel(4, _new_nf);
      _part   = env->_pool->acquireChannel(4, _new_nf);
      up(_parent->_ssbo, oparent); up(_part->_ssbo, opart);
      for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
      if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
      if (not _params) _params = fxi->createStorageBuffer(16);
      if (not _fctl)   _fctl   = fxi->createStorageBuffer(16);
      if (not _exprp)  _exprp  = fxi->createStorageBuffer(size_t(kMaxExprParams) * 16);
      return;
    }
    // ============================ VERTEX mode (region, existing) ====================
    std::vector<char> hasSel(nv, 0), hasUnsel(nv, 0);
    for (int f = 0; f < nf; f++)
      for (uint32_t c = fo[f]; c < fo[f + 1]; c++) (selFace[f] ? hasSel : hasUnsel)[vidx[c]] = 1;
    std::vector<int> dupIdx(nv, -1);
    int Vb = 0;
    for (int v = 0; v < nv; v++)
      if (hasSel[v] and hasUnsel[v]) dupIdx[v] = nv + (Vb++);   // boundary vert -> a lifted duplicate
    std::vector<uint32_t> dupv(std::max(1, Vb), 0), lift(nv, 0);
    for (int v = 0; v < nv; v++) {
      if (dupIdx[v] >= 0) dupv[dupIdx[v] - nv] = uint32_t(v);
      else if (hasSel[v]) lift[v] = 1;                          // interior-selected -> lift in place
    }
    auto liftV = [&](uint32_t v) -> uint32_t { return dupIdx[v] >= 0 ? uint32_t(dupIdx[v]) : v; };
    std::unordered_map<uint64_t, std::vector<int>> emap;
    for (int f = 0; f < nf; f++) {
      uint32_t a = fo[f], b = fo[f + 1]; int n = int(b - a);
      for (int k = 0; k < n; k++) {
        uint32_t v0 = vidx[a + k], v1 = vidx[a + ((k + 1) % n)];
        emap[(uint64_t(std::min(v0, v1)) << 32) | std::max(v0, v1)].push_back(f);
      }
    }
    std::vector<uint32_t> ovidx, ofo;
    for (int f = 0; f < nf; f++) {
      ofo.push_back(uint32_t(ovidx.size()));
      for (uint32_t c = fo[f]; c < fo[f + 1]; c++) ovidx.push_back(selFace[f] ? liftV(vidx[c]) : vidx[c]);
    }
    int Eb = 0;
    for (int f = 0; f < nf; f++) {
      if (not selFace[f]) continue;
      uint32_t a = fo[f], b = fo[f + 1]; int n = int(b - a);
      for (int k = 0; k < n; k++) {
        uint32_t v0 = vidx[a + k], v1 = vidx[a + ((k + 1) % n)];
        bool otherSel = false;
        for (int g : emap[(uint64_t(std::min(v0, v1)) << 32) | std::max(v0, v1)])
          if (g != f and selFace[g]) otherSel = true;
        if (otherSel) continue;
        // the wall quad [a,b,b',a'] is NON-PLANAR: a',b' lift along the boundary verts' per-vertex normals,
        // which tilt out of the wall plane (they average selected+unselected face normals), twisting the quad.
        // the renderer fan-triangulates from corner 0 (diagonal corner0..corner2), and for a twisted wall that
        // fixed diagonal can fold one triangle back-facing at high lift -> a see-through triangular hole. pick
        // the diagonal per-wall by cyclically rotating the corner order (same winding) so neither tri flips.
        // choose from the SOURCE normals across a range of lifts -> robust & lift-independent.
        fvec3 Pa = P(v0), Pb = P(v1), Na = N(v0), Nb = N(v1);
        fvec3 refout = (Pb - Pa).crossWith(Nb);    // outward wall normal in the planar limit
        auto worstOut = [&](bool startB) -> float {
          float worst = 1e30f;
          for (float d : {0.5f, 1.0f, 4.0f, 32.0f}) {
            fvec3 A = Pa, B = Pb, Bp = Pb + Nb * d, Ap = Pa + Na * d;
            fvec3 n1, n2;
            if (not startB) { n1 = (B - A).crossWith(Bp - A);  n2 = (Bp - A).crossWith(Ap - A); }  // diag a-b'
            else            { n1 = (Bp - B).crossWith(Ap - B); n2 = (Ap - B).crossWith(A - B);  }  // diag b-a'
            worst = std::min(worst, std::min(n1.dotWith(refout), n2.dotWith(refout)));
          }
          return worst;
        };
        ofo.push_back(uint32_t(ovidx.size()));
        if (worstOut(true) > worstOut(false)) {    // corner0=b -> fan uses the b-a' diagonal
          ovidx.push_back(v1); ovidx.push_back(liftV(v1)); ovidx.push_back(liftV(v0)); ovidx.push_back(v0); // [b,bl,al,a]
        } else {                                   // corner0=a -> fan uses the a-b' diagonal (original)
          ovidx.push_back(v0); ovidx.push_back(v1); ovidx.push_back(liftV(v1)); ovidx.push_back(liftV(v0)); // [a,b,bl,al]
        }
        Eb++;
      }
    }
    ofo.push_back(uint32_t(ovidx.size()));

    _new_nv = nv + Vb; _new_nc = int(ovidx.size()); _new_nf = nf + Eb; _nv = nv; _vb = Vb;
    _vidx = env->_pool->acquireChannel(4, _new_nc);
    _fo   = env->_pool->acquireChannel(4, _new_nf + 1);
    _lift = env->_pool->acquireChannel(4, nv);
    _dupv = env->_pool->acquireChannel(4, std::max(1, Vb));
    _fn    = env->_pool->acquireChannel(16, 1);  // unused in vertex mode (cs_dup ignores them); bound so the
    _nrm   = env->_pool->acquireChannel(16, 1);  // descriptor set stays valid. per-face expression fields are
    _bin   = env->_pool->acquireChannel(16, 1);  // FACE-mode only (vertex/region mode uses the scalar distance).
    _innrm = env->_pool->acquireChannel(16, 1);
    _dpar  = env->_pool->acquireChannel(4, 1);
    _fld0  = env->_pool->acquireChannel(16, 1);
    _fld1  = env->_pool->acquireChannel(16, 1);
    _fld2  = env->_pool->acquireChannel(16, 1);
    if (not _fctl) _fctl = fxi->createStorageBuffer(16);
    if (not _exprp) _exprp = fxi->createStorageBuffer(size_t(kMaxExprParams) * 16);
    auto up = [&](FxShaderStorageBuffer* buf, const std::vector<uint32_t>& v) {
      auto m = fxi->mapStorageBuffer(buf, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    };
    up(_vidx->_ssbo, ovidx); up(_fo->_ssbo, ofo); up(_lift->_ssbo, lift); up(_dupv->_ssbo, dupv);
    // per-output-face (parent, partition) for the FaceTagger: originals [0,nf) parent themselves and
    // are CAP if selected (lifted) else BASE; the appended walls [nf,_new_nf) are new -> WALL, no parent.
    std::vector<uint32_t> oparent(_new_nf), opart(_new_nf);
    for (int f = 0; f < nf; f++) { oparent[f] = uint32_t(f); opart[f] = selFace[f] ? kPartCap : kPartBase; }
    for (int f = nf; f < _new_nf; f++) { oparent[f] = kFaceNoParent; opart[f] = kPartWall; }
    _otags  = env->_pool->acquireChannel(4, _new_nf);
    _parent = env->_pool->acquireChannel(4, _new_nf);
    _part   = env->_pool->acquireChannel(4, _new_nf);
    up(_parent->_ssbo, oparent); up(_part->_ssbo, opart);
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (not _params) _params = fxi->createStorageBuffer(16);
  }

  bool onTopologyReady(Context* ctx) final {    // initial build (the subdivide pattern)
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto sh  = ctx->FXI()->shaderFromShaderText("hypermesh_extrude", _extrude_text());
    _cs_orig = ctx->FXI()->computeShader(sh, "cs_orig");
    _cs_dup  = ctx->FXI()->computeShader(sh, "cs_dup");
    _cs_face = ctx->FXI()->computeShader(sh, "cs_face");
    _cs_face_seg = ctx->FXI()->computeShader(sh, "cs_face_seg");   // multi-segment position kernel (gated at dispatch)
    auto shf  = ctx->FXI()->shaderFromShaderText(                  // per-face field eval (dist/inset/dir predicates)
        "hypermesh_extrude_field", _field_text(_d->_dist_pred, _d->_inset_pred, _d->_dir_pred));
    _cs_field = ctx->FXI()->computeShader(shf, "cs_field");
    if (_d->_segments > 1) {                                       // per-(face,ring) field eval (+twist/scale, prefix path)
      auto shfs = ctx->FXI()->shaderFromShaderText("hypermesh_extrude_field_seg",
          _field_seg_text(_d->_dist_pred, _d->_inset_pred, _d->_dir_pred, _d->_twist_pred, _d->_scale_pred));
      _cs_field_seg = ctx->FXI()->computeShader(shfs, "cs_field_seg");
    }
    _tagger.build(ctx);
    rebuild(ctx, in);
    _built = true; _built_nf = in->_num_faces;
    ackSrcTopo(_input);
    return true;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    int nf   = in->_num_faces;
    bool topoChanged = false;
    _active  = false;
    if (_built and _topoPending) {               // deferred last frame; producer's compute() has filled its buffers -> rebuild now
      rebuild(ctx, in); ackSrcTopo(_input); _built_nf = nf; _pending_nf = -1; _topoPending = false; _active = true; topoChanged = true;
    } else if (_built and srcTopoDirty(_input)) { // producer re-emitted topology THIS frame: its new buffers aren't computed yet ->
      ackSrcTopo(_input); _topoPending = true; _active = true;  // defer one frame, keep the last-built output meanwhile
    } else if (_built and nf == _built_nf) {
      _active = true;                            // steady — use the built topology
    } else if (_built and nf == _pending_nf) {
      rebuild(ctx, in); ackSrcTopo(_input);      // count stable for 1 frame -> tags synced -> rebuild
      _built_nf = nf; _pending_nf = -1; _active = true; topoChanged = true;
    } else if (_built) {
      _pending_nf = nf;                          // source just changed -> pass through this frame
    }
    if (not _active) {                           // PASSTHROUGH (extrude blinks off for the transition frame)
      out->_channels = in->_channels; out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets;
      out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    out->_channels     = _outch;
    out->_faces.clear();
    out->_faces["__tags"] = _otags;            // selection survives the extrude (+ this op's partitions)
    out->_vidx         = _vidx;
    out->_face_offsets = _fo;
    out->_header       = _header;
    out->_capacity     = meshNextPow2(_new_nv);
    out->_num_verts    = _new_nv;
    out->_num_corners  = _new_nc;
    out->_num_faces    = _new_nf;
    _tagger.setMasks(ctx, _new_nf, _d->_part_masks);  // RUNTIME partition masks (no recompile on change)
    uint32_t ctl[4]; ctl[0] = uint32_t(_nv); ctl[1] = uint32_t(_vb);
    float dist = *(_d->typedInputNamed<dflow::FloatPlugTraits>("distance")->_value);
    float usedir = _d->_dir_pred.empty() ? 0.0f : 1.0f;   // per-face direction expr overrides the baked faceN
    std::memcpy(&ctl[2], &dist, 4); std::memcpy(&ctl[3], &usedir, 4);
    auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
    uint32_t hdr[4] = {uint32_t(_new_nv), uint32_t(_new_nc), uint32_t(_new_nf), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr));
    ctx->FXI()->unmapStorageBuffer(mh.get());
    // B.4: snapshot the expression-param PLUGS into the EXPRP SSBO (the defined cross-thread handoff:
    // Python pokes plug values between frames; this pre-phase host write is the render-side snapshot).
    // The S.time slot's .x/.y come from the env clock — declarative time, zero per-frame Python.
    if (_d->_mode == kModeFace and _exprp) {
      auto env = _graphinst->_impl.getShared<MeshEnv>();
      float ep[kMaxExprParams * 4];
      for (int k = 0; k < kMaxExprParams; k++) {
        fvec4 v = _exprplugs[k] ? *(_exprplugs[k]->_value) : fvec4(0, 0, 0, 0);
        ep[k * 4 + 0] = v.x; ep[k * 4 + 1] = v.y; ep[k * 4 + 2] = v.z; ep[k * 4 + 3] = v.w;
      }
      if (_d->_time_slot >= 0 and _d->_time_slot < kMaxExprParams) {
        ep[_d->_time_slot * 4 + 0] = float(env->_abstime);
        ep[_d->_time_slot * 4 + 1] = float(env->_dt);
      }
      auto mp = ctx->FXI()->mapStorageBuffer(_exprp, 0, sizeof(ep), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, ep, sizeof(ep));
      ctx->FXI()->unmapStorageBuffer(mp.get());
    }
    if (topoChanged) out->markTopoChanged(); else out->markChanged();   // propagate dirt downstream
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _active) return;                     // passthrough frame -> output already aliases the source
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
      ci->bindStorageBuffer(cs, 10, _lift->_ssbo);
      ci->bindStorageBuffer(cs, 11, _dupv->_ssbo);
      ci->bindStorageBuffer(cs, 12, _params);
      ci->bindStorageBuffer(cs, 13, _fn->_ssbo);
      ci->bindStorageBuffer(cs, 14, _nrm->_ssbo);
      ci->bindStorageBuffer(cs, 15, _bin->_ssbo);
      ci->bindStorageBuffer(cs, 16, _innrm->_ssbo);
      ci->bindStorageBuffer(cs, 17, _dpar->_ssbo);
      ci->bindStorageBuffer(cs, 18, _fld0->_ssbo);
      ci->bindStorageBuffer(cs, 19, _fld1->_ssbo);
      ci->bindStorageBuffer(cs, 20, _fld2->_ssbo);
    };
    // FACE mode: evaluate the per-INPUT-face fields (dist/inset/dir[/twist/scale]) on GPU each frame, then lift.
    bool seg = (_d->_mode == kModeFace) and (_d->_segments > 1);
    if (_d->_mode == kModeFace) {
      int nf = in->_num_faces;
      uint32_t fct[4] = {uint32_t(nf), 0u, 0u, 0u};
      if (seg) {                                      // seg field shader reads SEGN + the global distance (GDIST)
        float gdist = *(_d->typedInputNamed<dflow::FloatPlugTraits>("distance")->_value);
        fct[1] = uint32_t(_d->_segments);
        std::memcpy(&fct[2], &gdist, 4);
      }
      auto mf = env->_ctx->FXI()->mapStorageBuffer(_fctl, 0, sizeof(fct), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mf->_mappedaddr, fct, sizeof(fct));
      env->_ctx->FXI()->unmapStorageBuffer(mf.get());
      // (EXPRP upload happens in writeParams — the pre-phase host write; see the B.4 snapshot there.)
      auto in_tg = in->face("__tags");
      const FxComputeShader* fld = seg ? _cs_field_seg : _cs_field;
      ci->bindStorageBuffer(fld, 0, s[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(fld, 1, in->_vidx->_ssbo);
      ci->bindStorageBuffer(fld, 2, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(fld, 3, in_tg ? in_tg->_ssbo : in->_vidx->_ssbo);  // no __tags -> dummy (tag-less predicate)
      ci->bindStorageBuffer(fld, 4, _fld0->_ssbo);                             // FLD0 / FA
      ci->bindStorageBuffer(fld, 5, _fld1->_ssbo);                             // FLD1 / FB
      ci->bindStorageBuffer(fld, 6, s[MeshChannel::UV0]->_ssbo);   // input UV0 -> fUV / S.uv
      ci->bindStorageBuffer(fld, 7, _exprp);                       // generic params -> EXPRP / param()
      ci->bindStorageBuffer(fld, 8, _fctl);
      if (seg) {
        ci->bindStorageBuffer(fld, 9, _fld2->_ssbo);                           // FC = (Cbase, inset)
        ci->dispatchCompute(fld, (nf * (_d->_segments + 1) + 63) / 64, 1, 1);
      } else {
        ci->dispatchCompute(fld, (nf + 63) / 64, 1, 1);
      }
      ci->storageBarrier();
    }
    bind(_cs_orig); ci->dispatchCompute(_cs_orig, (_nv + 63) / 64, 1, 1);
    if (_vb > 0) {                                   // VERTEX -> cs_dup (vN lift); FACE -> cs_face[_seg] (faceN lift)
      const FxComputeShader* cs2 = (_d->_mode != kModeFace) ? _cs_dup : (seg ? _cs_face_seg : _cs_face);
      bind(cs2); ci->dispatchCompute(cs2, (_vb + 63) / 64, 1, 1);
    }
    // retag: inherit each output face's parent __tags (input) + apply its partition's MaskOp -> _otags.
    auto in_tags = in->face("__tags");
    _tagger.dispatch(env->_ctx, _otags->_ssbo, in_tags ? in_tags->_ssbo : nullptr,
                     _parent->_ssbo, _part->_ssbo, _new_nf);
    ci->storageBarrier();
  }
  const ExtrudeFacesData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _dist;
  std::shared_ptr<dflow::inpluginst<dflow::Vec4fPlugTraits>> _exprplugs[kMaxExprParams] = {};
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _vidx, _fo, _lift, _dupv, _fn, _nrm, _bin;  // FACE per-dup: _fn=(liftdir,liftw) _nrm=(insetdir,isCap) _bin=tangent
  gpuchannel_ptr_t _innrm, _dpar, _fld0, _fld1, _fld2;       // _innrm=rim inset dir, _dpar=parent face (or seg field idx); _fld0/1/2=fields
  gpuchannel_ptr_t _otags, _parent, _part;   // output __tags + the FaceTagger's per-face (parent,partition)
  FaceTagger _tagger;
  FxShaderStorageBuffer *_header = nullptr, *_params = nullptr, *_fctl = nullptr, *_exprp = nullptr;  // _fctl: count; _exprp: param vec4s
  const FxComputeShader *_cs_orig = nullptr, *_cs_dup = nullptr, *_cs_face = nullptr, *_cs_field = nullptr;
  const FxComputeShader *_cs_face_seg = nullptr, *_cs_field_seg = nullptr;   // multi-segment variants (segments>1)
  int _new_nv = 0, _new_nc = 0, _new_nf = 0, _nv = 0, _vb = 0;
  int _built_nf = -1, _pending_nf = -1;
  bool _topoPending = false;   // producer topology change seen but deferred 1 frame (its compute() must fill its buffers first)
  bool _built = false, _active = false;
  // ---- C.4b GPU face-mode rebuild state ----
  MeshScan _xscan;
  const FxComputeShader *_cs_xcnt = nullptr, *_cs_xtopo = nullptr, *_cs_xzlift = nullptr, *_cs_xdups = nullptr;
  FxShaderStorageBuffer* _xctl = nullptr;                                          // {nf,nv,bit,kb,segs}
  FxShaderStorageBuffer *_xdc = nullptr, *_xcc = nullptr, *_xfc = nullptr;         // per-face counts
  FxShaderStorageBuffer *_xdb = nullptr, *_xcb = nullptr, *_xfb = nullptr;         // scan bases
  FxShaderStorageBuffer *_xs1 = nullptr, *_xs2 = nullptr, *_xs3 = nullptr;         // scan ctls
  int _xcap_nf = 0;
};
const MeshChannel ExtrudeFacesInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeExtrudeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "distance")->setValue(0.5f);
  // B.4: the FIXED expression-param plug set (kMaxExprParams vec4s; unused idle at zero). createInputPlug
  // dedups by name, so the double reshapeIOs on deserialize stays idempotent.
  for (int k = 0; k < kMaxExprParams; k++) {
    auto nm = FormatString("exprp%d", k);
    dflow::ModuleData::createInputPlug<dflow::Vec4fPlugTraits>(data, dflow::EPR_UNIFORM, nm.c_str());
  }
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ExtrudeFacesData::ExtrudeFacesData() {
}
std::shared_ptr<ExtrudeFacesData> ExtrudeFacesData::createShared() {
  auto d = std::make_shared<ExtrudeFacesData>();
  _reshapeExtrudeIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t ExtrudeFacesData::createInstance(dflow::GraphInst* g) const {
  OrkAssert(_predicate_abi == kPredicateABIVersion);  // serialized-predicate ABI gate (see hmdflow.h)
  return std::make_shared<ExtrudeFacesInst>(this, g);
}
void ExtrudeFacesData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ExtrudeFacesData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeExtrudeIOs(m); });
  clazz->directProperty("slot", &ExtrudeFacesData::_slot);
  clazz->directProperty("mode", &ExtrudeFacesData::_mode);
  clazz->directProperty("keep_base", &ExtrudeFacesData::_keep_base);
  clazz->directProperty("segments", &ExtrudeFacesData::_segments);
  // the per-face field expressions ARE the op's authored semantics — post-trace GLSL (the portable
  // artifact, see kPredicateABIVersion); all five were pyext-only and silently dropped on reload.
  clazz->directProperty("predicate_abi", &ExtrudeFacesData::_predicate_abi);
  clazz->directProperty("dist_predicate", &ExtrudeFacesData::_dist_pred);
  clazz->directProperty("inset_predicate", &ExtrudeFacesData::_inset_pred);
  clazz->directProperty("dir_predicate", &ExtrudeFacesData::_dir_pred);
  clazz->directProperty("twist_predicate", &ExtrudeFacesData::_twist_pred);
  clazz->directProperty("scale_predicate", &ExtrudeFacesData::_scale_pred);
  clazz->directProperty("time_slot", &ExtrudeFacesData::_time_slot);   // the S.time EXPRP slot (-1 = none)
  clazz->directVectorProperty("part_masks", &ExtrudeFacesData::_part_masks);
}

} // namespace ork::lev2::hypermesh
