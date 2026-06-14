////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"
#include "../hypermesh/hmdflow_module.h" // MeshComputeInst + MeshScan + allocMesh

ImplementReflectionX(ork::lev2::sdf::SdfToMeshData, "sdf::SdfToMeshData");

namespace ork::lev2::sdf {

namespace hm = ork::lev2::hypermesh;

///////////////////////////////////////////////////////////////////////////////
// SdfToMesh (M2) — MARCHING TETRAHEDRA (not cubes): each grid cell splits into 6
// tetrahedra sharing the main diagonal (corner 0 -> 6); each tet has 16 sign
// cases over a TINY, hand-verifiable edge table — no 256-entry MC table, and
// tets are UNAMBIGUOUS (always watertight, no asymptotic decider). Winding is
// made automatic: each emitted triangle is flipped to agree with the SDF
// gradient, so the table needs only correct TOPOLOGY (the watertightness gate
// validates it). v1 emits per-cell SOUP (coincident verts on shared edges); the
// MeshSort position-weld (shared/indexed verts) layers on next.
//
// SIZING is data-dependent: eval 1 = classify + scan the per-cell triangle
// counts; onTopologyReady reads the total back, allocMesh's the exact buffers,
// and requests the re-eval that EMITS. (the subdivide cascade pattern — this is
// why SdfToMesh is a MeshComputeInst: the mesh driver hands it onTopologyReady.)
///////////////////////////////////////////////////////////////////////////////

// the 6-tet decomposition of a cube (corners 0..7, all tets share diagonal 0-6)
static const uint32_t kTetCube[24] = {
    0, 1, 2, 6,  0, 2, 3, 6,  0, 3, 7, 6,
    0, 7, 4, 6,  0, 4, 5, 6,  0, 5, 1, 6};
// tet local edges -> the two local tet-corner indices (0..3)
static const uint32_t kTetEdge[12] = {0, 1,  0, 2,  0, 3,  1, 2,  1, 3,  2, 3};
// per-case triangle list (local edge ids, -1 padded to 6). caseIndex bit i = tet
// corner i INSIDE (sdf < iso). Topology only — winding fixed per-triangle below.
static const int kTetTable[16 * 6] = {
    -1, -1, -1, -1, -1, -1, // 0  ----
     0,  1,  2, -1, -1, -1, // 1  c0
     0,  3,  4, -1, -1, -1, // 2  c1
     1,  3,  4,  1,  4,  2, // 3  c0 c1
     1,  3,  5, -1, -1, -1, // 4  c2
     0,  3,  5,  0,  5,  2, // 5  c0 c2
     0,  1,  5,  0,  5,  4, // 6  c1 c2
     2,  4,  5, -1, -1, -1, // 7  c0 c1 c2  (corner 3 out)
     2,  4,  5, -1, -1, -1, // 8  c3
     0,  1,  5,  0,  5,  4, // 9  c0 c3
     0,  3,  5,  0,  5,  2, // 10 c1 c3
     1,  3,  5, -1, -1, -1, // 11 c0 c1 c3 (corner 2 out)
     1,  3,  4,  1,  4,  2, // 12 c2 c3
     0,  3,  4, -1, -1, -1, // 13 c0 c2 c3 (corner 1 out)
     0,  1,  2, -1, -1, -1, // 14 c1 c2 c3 (corner 0 out)
    -1, -1, -1, -1, -1, -1, // 15 ++++
};

static const char* _s2m_par_decl() {
  return R"S(
storage_interface mif_par (descriptor_set 0) { buffer layout(std430) prb {
  uint p_dim; uint p_cellsx; uint p_cellsy; uint p_cellsz;
  uint p_ncells; uint p_ncap; uint p_fcap; uint p_doweld;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  float p_iso; float p_pad1; float p_pad2; float p_pad3; }; }
)S";
}

static std::string _s2m_count_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_sdf (descriptor_set 0) { buffer layout(std430) sdb { float SDF[]; }; }
storage_interface mif_cnt (descriptor_set 0) { buffer layout(std430) cnb { uint CNT[]; }; }
storage_interface mif_tc  (descriptor_set 0) { buffer layout(std430) tcb { uint TETCUBE[]; }; }
storage_interface mif_ct  (descriptor_set 0) { buffer layout(std430) ctb { uint TRICNT[]; }; }
%PAR%
compute_interface iface_count { storage { mif_sdf mif_cnt mif_tc mif_ct mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_count : iface_count {
  uint cell = gl_GlobalInvocationID.x;
  if (cell >= p_ncells) { return; }
  uint cx = cell % p_cellsx;
  uint cy = (cell / p_cellsx) % p_cellsy;
  uint cz = cell / (p_cellsx * p_cellsy);
  float s[8];
  for (uint i = 0u; i < 8u; i++) {
    uint ox = ((i == 1u) || (i == 2u) || (i == 5u) || (i == 6u)) ? 1u : 0u;
    uint oy = ((i == 2u) || (i == 3u) || (i == 6u) || (i == 7u)) ? 1u : 0u;
    uint oz = (i >= 4u) ? 1u : 0u;
    s[i] = SDF[(cx + ox) + p_dim * ((cy + oy) + p_dim * (cz + oz))];
  }
  uint total = 0u;
  for (uint tt = 0u; tt < 6u; tt++) {
    uint ci = 0u;
    for (uint k = 0u; k < 4u; k++) {
      if (s[TETCUBE[tt * 4u + k]] < p_iso) { ci = ci | (1u << k); }
    }
    total = total + TRICNT[ci];
  }
  CNT[cell] = total;
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

static std::string _s2m_emit_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_sdf (descriptor_set 0) { buffer layout(std430) sdb { float SDF[]; }; }
storage_interface mif_base(descriptor_set 0) { buffer layout(std430) bsb { uint BASE[]; }; }
storage_interface mif_tc  (descriptor_set 0) { buffer layout(std430) tcb { uint TETCUBE[]; }; }
storage_interface mif_te  (descriptor_set 0) { buffer layout(std430) teb { uint TETEDGE[]; }; }
storage_interface mif_tt  (descriptor_set 0) { buffer layout(std430) ttb { int TETTBL[]; }; }
storage_interface mif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[];  }; }
storage_interface mif_N   (descriptor_set 0) { buffer layout(std430) nb  { vec4 Nd[];  }; }
storage_interface mif_B   (descriptor_set 0) { buffer layout(std430) bb2 { vec4 Bd[];  }; }
storage_interface mif_uv  (descriptor_set 0) { buffer layout(std430) ub  { vec4 UVd[]; }; }
storage_interface mif_clr (descriptor_set 0) { buffer layout(std430) clb { vec4 Cd[];  }; }
storage_interface mif_eid (descriptor_set 0) { buffer layout(std430) eib { uint EID[]; }; } // collision-free weld key
%PAR%
compute_interface iface_emit { storage { mif_sdf mif_base mif_tc mif_te mif_tt mif_P mif_N mif_B mif_uv mif_clr mif_eid mif_par }
                               inputs { layout(local_size_x = 64); } }
compute_shader cs_emit : iface_emit {
  uint cell = gl_GlobalInvocationID.x;
  if (cell >= p_ncells) { return; }
  uint cx = cell % p_cellsx;
  uint cy = (cell / p_cellsx) % p_cellsy;
  uint cz = cell / (p_cellsx * p_cellsy);
  // precompute each cube corner's sample, world position, SDF gradient, and global
  // grid index ONCE
  float s[8]; vec3 cpos[8]; vec3 cgrad[8]; uint gidx[8];
  for (uint i = 0u; i < 8u; i++) {
    uint ox = ((i == 1u) || (i == 2u) || (i == 5u) || (i == 6u)) ? 1u : 0u;
    uint oy = ((i == 2u) || (i == 3u) || (i == 6u) || (i == 7u)) ? 1u : 0u;
    uint oz = (i >= 4u) ? 1u : 0u;
    uint gx = cx + ox; uint gy = cy + oy; uint gz = cz + oz;
    gidx[i] = gx + p_dim * (gy + p_dim * gz);
    s[i]    = SDF[gidx[i]];
    cpos[i] = vec3(p_ox, p_oy, p_oz) + vec3(float(gx), float(gy), float(gz)) * p_voxel;
    uint xp = min(gx + 1u, p_dim - 1u); uint xm = (gx > 0u) ? (gx - 1u) : 0u;
    uint yp = min(gy + 1u, p_dim - 1u); uint ym = (gy > 0u) ? (gy - 1u) : 0u;
    uint zp = min(gz + 1u, p_dim - 1u); uint zm = (gz > 0u) ? (gz - 1u) : 0u;
    float dx = SDF[xp + p_dim*(gy + p_dim*gz)] - SDF[xm + p_dim*(gy + p_dim*gz)];
    float dy = SDF[gx + p_dim*(yp + p_dim*gz)] - SDF[gx + p_dim*(ym + p_dim*gz)];
    float dz = SDF[gx + p_dim*(gy + p_dim*zp)] - SDF[gx + p_dim*(gy + p_dim*zm)];
    cgrad[i] = vec3(dx, dy, dz);
  }
  uint face = BASE[cell];
  for (uint tt = 0u; tt < 6u; tt++) {
    uint c0 = TETCUBE[tt*4u + 0u]; uint c1 = TETCUBE[tt*4u + 1u];
    uint c2 = TETCUBE[tt*4u + 2u]; uint c3 = TETCUBE[tt*4u + 3u];
    uint tc[4]; tc[0] = c0; tc[1] = c1; tc[2] = c2; tc[3] = c3;
    uint ci = 0u;
    for (uint k = 0u; k < 4u; k++) { if (s[tc[k]] < p_iso) { ci = ci | (1u << k); } }
    for (uint e = 0u; e < 6u; e = e + 3u) {
      int t0 = TETTBL[ci*6u + e + 0u];
      if (t0 < 0) { break; }
      vec3 vp[3]; vec3 vn[3]; uint eid[3];
      for (uint j = 0u; j < 3u; j++) {
        int edge = TETTBL[ci*6u + e + j];
        uint la = TETEDGE[uint(edge)*2u + 0u];
        uint lb = TETEDGE[uint(edge)*2u + 1u];
        uint ca = tc[la]; uint cb = tc[lb];
        float sa = s[ca]; float sb = s[cb];
        float w = (abs(sb - sa) > 1.0e-12) ? (p_iso - sa) / (sb - sa) : 0.5;
        vp[j] = mix(cpos[ca], cpos[cb], w);
        vn[j] = mix(cgrad[ca], cgrad[cb], w);
        // COLLISION-FREE weld key: the global grid EDGE this vertex sits on =
        // (min endpoint grid index << 5) | direction. Coincident verts (shared
        // edge across tets/cells) get the IDENTICAL key; distinct edges differ.
        uint oax = ((ca==1u)||(ca==2u)||(ca==5u)||(ca==6u))?1u:0u;
        uint oay = ((ca==2u)||(ca==3u)||(ca==6u)||(ca==7u))?1u:0u;
        uint oaz = (ca>=4u)?1u:0u;
        uint obx = ((cb==1u)||(cb==2u)||(cb==5u)||(cb==6u))?1u:0u;
        uint oby = ((cb==2u)||(cb==3u)||(cb==6u)||(cb==7u))?1u:0u;
        uint obz = (cb>=4u)?1u:0u;
        uint mn; int dxx; int dyy; int dzz;
        if (gidx[ca] <= gidx[cb]) { mn = gidx[ca]; dxx = int(obx)-int(oax); dyy = int(oby)-int(oay); dzz = int(obz)-int(oaz); }
        else                       { mn = gidx[cb]; dxx = int(oax)-int(obx); dyy = int(oay)-int(oby); dzz = int(oaz)-int(obz); }
        uint dcode = uint((dxx + 1) + (dyy + 1) * 3 + (dzz + 1) * 9); // [0,26]
        eid[j] = (mn << 5u) | dcode;
      }
      // winding-fix: flip so the geometric face normal agrees with the SDF gradient
      vec3 fn = cross(vp[1] - vp[0], vp[2] - vp[0]);
      vec3 gavg = vn[0] + vn[1] + vn[2];
      uint o0 = 0u; uint o1 = 1u; uint o2 = 2u;
      if (dot(fn, gavg) < 0.0) { o1 = 2u; o2 = 1u; }
      // FALLBACK normal for the CSG SEAM: vn is the COMPOSITE-field central-difference
      // gradient; where two CSG operands' gradients oppose (the seam medial set, e.g.
      // the rim where the sphere pokes the box) it cancels toward zero, so normalize()
      // yields a near-random / NaN normal -> a dark garbage-shaded facet at a fixed
      // spot. Use the winding-corrected geometric FACE normal there instead.
      vec3 ffn       = cross(vp[o1] - vp[o0], vp[o2] - vp[o0]);
      float ffl      = length(ffn);
      vec3 facenrm   = (ffl > 1.0e-12) ? (ffn / ffl) : vec3(0.0, 1.0, 0.0);
      uint base3 = face * 3u;
      if (base3 + 3u > p_ncap) { return; } // capacity backstop (animation overflow): never write OOB
      uint idx[3]; idx[0] = base3 + 0u; idx[1] = base3 + 1u; idx[2] = base3 + 2u;
      uint ord[3]; ord[0] = o0; ord[1] = o1; ord[2] = o2;
      for (uint j = 0u; j < 3u; j++) {
        uint w2 = idx[j];
        uint sj = ord[j];
        float vnl = length(vn[sj]);                          // SEAM guard (see facenrm above):
        vec3 nn = (vnl > 1.0e-6) ? (vn[sj] / vnl) : facenrm; // NaN-length also fails -> facenrm
        Pd[w2]  = vec4(vp[sj], 1.0);
        Nd[w2]  = vec4(nn, 0.0);
        // an arbitrary stable tangent perpendicular to nn
        vec3 ref = (abs(nn.y) < 0.99) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        Bd[w2]  = vec4(normalize(cross(ref, nn)), 0.0);
        UVd[w2] = vec4(0.0, 0.0, 0.0, 0.0);
        Cd[w2]  = vec4(1.0, 1.0, 1.0, 1.0);
        if (p_doweld != 0u) { EID[w2] = eid[sj]; }
      }
      face = face + 1u;
    }
  }
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

///////////////////////////////////////////////////////////////////////////////
// BLOCKY / CUBERILLE — pure voxel-surface extraction (no interpolation, no smoothing). Per voxel
// (a grid sample), for each +x/+y/+z neighbour that flips sign, emit the axis-aligned unit quad on
// the shared face (corners on the half-voxel grid, normal = ±axis). Each internal face is owned by
// the LOWER voxel, so it's emitted exactly once. Reuses the SAME count->scan->alloc->emit->ident
// scaffold as marching-tets — it just produces a different (blocky) soup. Weld is OFF (the hard 90°
// per-face normals must stay distinct). Block size = the brick voxel.
///////////////////////////////////////////////////////////////////////////////
static std::string _s2m_count_blocky_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mbf_sdf (descriptor_set 0) { buffer layout(std430) bsdb { float SDF[]; }; }
storage_interface mbf_cnt (descriptor_set 0) { buffer layout(std430) bcnb { uint CNT[]; }; }
%PAR%
compute_interface iface_bcount { storage { mbf_sdf mbf_cnt mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_count_blocky : iface_bcount {
  uint cell = gl_GlobalInvocationID.x;
  if (cell >= p_ncells) { return; }
  uint cx = cell % p_cellsx;
  uint cy = (cell / p_cellsx) % p_cellsy;
  uint cz = cell / (p_cellsx * p_cellsy);
  bool in0 = SDF[cx + p_dim*(cy + p_dim*cz)] < p_iso;
  uint flips = 0u;
  if ((SDF[(cx+1u) + p_dim*(cy + p_dim*cz)] < p_iso) != in0) { flips = flips + 1u; }  // +x face
  if ((SDF[cx + p_dim*((cy+1u) + p_dim*cz)] < p_iso) != in0) { flips = flips + 1u; }  // +y face
  if ((SDF[cx + p_dim*(cy + p_dim*(cz+1u))] < p_iso) != in0) { flips = flips + 1u; }  // +z face
  CNT[cell] = flips * 2u;   // 2 triangles per quad face (the pipeline is all-triangle soup)
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

static std::string _s2m_emit_blocky_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mbe_sdf (descriptor_set 0) { buffer layout(std430) besdb { float SDF[]; }; }
storage_interface mbe_base(descriptor_set 0) { buffer layout(std430) bebb  { uint BASE[]; }; }
storage_interface mbe_P   (descriptor_set 0) { buffer layout(std430) bepb  { vec4 Pd[];  }; }
storage_interface mbe_N   (descriptor_set 0) { buffer layout(std430) benb  { vec4 Nd[];  }; }
storage_interface mbe_B   (descriptor_set 0) { buffer layout(std430) bebb2 { vec4 Bd[];  }; }
storage_interface mbe_uv  (descriptor_set 0) { buffer layout(std430) beub  { vec4 UVd[]; }; }
storage_interface mbe_clr (descriptor_set 0) { buffer layout(std430) beclb { vec4 Cd[];  }; }
%PAR%
compute_interface iface_bemit { storage { mbe_sdf mbe_base mbe_P mbe_N mbe_B mbe_uv mbe_clr mif_par }
                                inputs { layout(local_size_x = 64); } }
compute_shader cs_emit_blocky : iface_bemit {
  uint cell = gl_GlobalInvocationID.x;
  if (cell >= p_ncells) { return; }
  uint cx = cell % p_cellsx;
  uint cy = (cell / p_cellsx) % p_cellsy;
  uint cz = cell / (p_cellsx * p_cellsy);
  bool in0 = SDF[cx + p_dim*(cy + p_dim*cz)] < p_iso;
  vec3 org = vec3(p_ox, p_oy, p_oz);
  vec3 vc  = org + vec3(float(cx), float(cy), float(cz)) * p_voxel;   // this voxel's center
  float h  = 0.5 * p_voxel;
  uint face = BASE[cell];
  for (uint a = 0u; a < 3u; a = a + 1u) {
    uint nx = cx; uint ny = cy; uint nz = cz;
    if (a == 0u) { nx = cx + 1u; } else if (a == 1u) { ny = cy + 1u; } else { nz = cz + 1u; }
    bool inN = SDF[nx + p_dim*(ny + p_dim*nz)] < p_iso;
    if (inN != in0) {                                                // sign flip -> emit the boundary face
      vec3 ea = (a == 0u) ? vec3(1.0,0.0,0.0) : ((a == 1u) ? vec3(0.0,1.0,0.0) : vec3(0.0,0.0,1.0));
      vec3 eb = (a == 0u) ? vec3(0.0,1.0,0.0) : ((a == 1u) ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0));
      vec3 ec = (a == 0u) ? vec3(0.0,0.0,1.0) : ((a == 1u) ? vec3(1.0,0.0,0.0) : vec3(0.0,1.0,0.0));
      vec3 fc  = vc + ea * h;                                        // face center (half a voxel toward +a)
      vec3 nrm = in0 ? ea : (ea * -1.0);                            // outward: inside -> +a, outside -> -a
      vec3 q[4];
      q[0] = fc - eb*h - ec*h; q[1] = fc + eb*h - ec*h; q[2] = fc + eb*h + ec*h; q[3] = fc - eb*h + ec*h;
      uint tri[6]; tri[0]=0u; tri[1]=1u; tri[2]=2u; tri[3]=0u; tri[4]=2u; tri[5]=3u;
      vec3 ref = (abs(nrm.y) < 0.99) ? vec3(0.0,1.0,0.0) : vec3(1.0,0.0,0.0);
      vec3 tng = normalize(cross(ref, nrm));   // 'tan' is a GLSL builtin -> reserved; use tng
      for (uint tt = 0u; tt < 2u; tt = tt + 1u) {
        vec3 a0 = q[tri[tt*3u+0u]]; vec3 a1 = q[tri[tt*3u+1u]]; vec3 a2 = q[tri[tt*3u+2u]];
        vec3 fn = cross(a1 - a0, a2 - a0);
        if (dot(fn, nrm) < 0.0) { vec3 tmp = a1; a1 = a2; a2 = tmp; }  // winding-fix to the outward normal
        uint base3 = face * 3u;
        if (base3 + 3u > p_ncap) { return; }                        // capacity backstop (animation overflow)
        Pd[base3+0u] = vec4(a0,1.0); Nd[base3+0u] = vec4(nrm,0.0); Bd[base3+0u] = vec4(tng,0.0); UVd[base3+0u] = vec4(0.0); Cd[base3+0u] = vec4(1.0);
        Pd[base3+1u] = vec4(a1,1.0); Nd[base3+1u] = vec4(nrm,0.0); Bd[base3+1u] = vec4(tng,0.0); UVd[base3+1u] = vec4(0.0); Cd[base3+1u] = vec4(1.0);
        Pd[base3+2u] = vec4(a2,1.0); Nd[base3+2u] = vec4(nrm,0.0); Bd[base3+2u] = vec4(tng,0.0); UVd[base3+2u] = vec4(0.0); Cd[base3+2u] = vec4(1.0);
        face = face + 1u;
      }
    }
  }
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

static std::string _s2m_ident_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[]; }; }
storage_interface mif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[]; }; }
storage_interface mif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[]; }; }
storage_interface mif_eid (descriptor_set 0) { buffer layout(std430) eib { uint EID[]; }; }
%PAR%
compute_interface iface_ident { storage { mif_vi mif_fo mif_P mif_eid mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_ident : iface_ident {
  uint i = gl_GlobalInvocationID.x;
  if (i < p_ncap) {
    VId[i] = i;                              // soup: corner i -> its own vertex
    Pd[i]  = vec4(0.0, 0.0, 0.0, 1.0);       // CLEAR to a degenerate point — emit overwrites the LIVE
    // CLEAR the weld key to a SENTINEL too (animated 1-frame-stale count: a slot
    // live last frame but degenerate this frame must NOT keep a real edge-id, or
    // it could collide with a live vertex's edge and steal its (now-origin)
    // representative -> vertex snaps to origin. Sentinel = its own isolated run.
    if (p_doweld != 0u) { EID[i] = 0xFFFFFFFFu; }
  }                                          // verts; any unwritten tail face stays zero-area (invisible)
  if (i <= p_fcap) { FOd[i] = i * 3u; }      // all-tri CSR (fo[nf] = 3*nf terminal included)
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

// M2.5 — publish the LIVE counts to the mesh GPU header (no readback). Runs once
// (1 thread) after the scan: BASE[ncells] is the live triangle total. The render
// triangulator's cs_importcount reads HDR[2] to early-out at the live count, so the
// CPU never sees the count. Writes only nv/nc/nf — leaves flags + bbox (HDR[3..])
// that onTopologyReady set (the brick bound is static; only the count animates).
static std::string _s2m_writehdr_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwh_base (descriptor_set 0) { buffer layout(std430) whbb { uint BASE[]; }; }
storage_interface mwh_hdr  (descriptor_set 0) { buffer layout(std430) whhb { uint HDR[]; }; }
%PAR%
compute_interface iface_writehdr { storage { mwh_base mwh_hdr mif_par } inputs { layout(local_size_x = 1); } }
compute_shader cs_writehdr : iface_writehdr {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint nf = BASE[p_ncells];   // exclusive-scan total = live triangle count
  HDR[0]  = nf * 3u;          // nv  (soup: 3 corners/tri)
  HDR[1]  = nf * 3u;          // nc
  HDR[2]  = nf;               // nf  <- cs_importcount reads this
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

// GPU-HANG FIX — mirror the scan TOTAL into a HOST-VISIBLE buffer (1 thread, runs every eval after the
// scan). onTopologyReady reads the count to size the alloc; reading the DEVICE-LOCAL `_base` directly
// routes through copyToHost -> a single-use staging command buffer on the shared queue, which (with a
// deep/heavy CSG chain whose compute is still in flight) trips MoltenVK "command buffer does not support
// execution more than once" -> GPU reboot. Reading a HOST-VISIBLE buffer instead is a direct vkMapMemory
// (no staging submit) -> no overlap, no hang. (conform never hangs precisely because it does no device readback.)
static std::string _s2m_copytotal_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mcg_base (descriptor_set 0) { buffer layout(std430) cgbb { uint BASE[]; }; }
storage_interface mcg_out  (descriptor_set 0) { buffer layout(std430) cgob { uint COUNTHOST[]; }; }
%PAR%
compute_interface iface_copytotal { storage { mcg_base mcg_out mif_par } inputs { layout(local_size_x = 1); } }
compute_shader cs_copytotal : iface_copytotal {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  COUNTHOST[0] = BASE[p_ncells];   // host-visible mirror of the exclusive-scan total (onTopologyReady reads THIS)
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

///////////////////////////////////////////////////////////////////////////////
// WELD (M2 weld) — turn the SOUP into shared-vertex topology by remapping vidx so
// coincident corners point to ONE representative corner (an INDEX-level dedup —
// no channel compaction/readback). Merge = same QUANTIZED position (coincident
// MC crossings are bit-identical, so they always merge; the run-mark verifies the
// quantized position to defeat spatial-hash collisions). Built on MeshSort (group
// by hash) + MeshScan (run ids). Five tiny kernels, each its own dense-set file.
//   wkey   : KEY[c] = spatialHash(quantize(P[c])) ; PAY[c] = c
//   (MeshSort KEY,PAY -> equal positions adjacent)
//   wmark  : ISTART[i] = 1 at a run start (quantize(P) differs from i-1)
//   (MeshScan ISTART -> PRE = run id per sorted slot)
//   wrep   : at each run start, REP[PRE[i]] = PAY[i]  (the run's representative corner)
//   wremap : REMAP[PAY[i]] = REP[PRE[i]]              (each corner -> its representative)
//   wvidx  : VId[c] = REMAP[c]                        (the shared-vertex index buffer)
///////////////////////////////////////////////////////////////////////////////

static const char* _s2m_wpar_decl() {
  return R"S(
storage_interface mwf_par (descriptor_set 0) { buffer layout(std430) wpb { uint p_nc; uint p_w1; uint p_w2; uint p_w3; }; }
)S";
}
// the weld key is the COLLISION-FREE per-vertex grid-edge id computed in emit
// (EID). Coincident verts share the exact key, so after MeshSort they are
// CONTIGUOUS and a pure adjacent key-compare marks runs correctly (a lossy
// position hash would interleave hash-collisions and shatter the runs).
static std::string _s2m_wkey_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwf_eid (descriptor_set 0) { buffer layout(std430) web { uint EID[]; }; }
storage_interface mwf_key (descriptor_set 0) { buffer layout(std430) wkb { uint KEY[]; }; }
storage_interface mwf_pay (descriptor_set 0) { buffer layout(std430) wyb { uint PAY[]; }; }
%WPAR%
compute_interface iface_wkey { storage { mwf_eid mwf_key mwf_pay mwf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_wkey : iface_wkey {
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  KEY[c] = EID[c];
  PAY[c] = c;
}
)S";
  _shadersub(t, "%WPAR%", _s2m_wpar_decl());
  return t;
}
static std::string _s2m_wmark_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwf_key (descriptor_set 0) { buffer layout(std430) wkb { uint KEY[]; }; }
storage_interface mwf_is  (descriptor_set 0) { buffer layout(std430) wib { uint ISTART[]; }; }
%WPAR%
compute_interface iface_wmark { storage { mwf_key mwf_is mwf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_wmark : iface_wmark {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_nc) { return; }
  ISTART[i] = (i == 0u || KEY[i] != KEY[i - 1u]) ? 1u : 0u; // collision-free key -> contiguous runs
}
)S";
  _shadersub(t, "%WPAR%", _s2m_wpar_decl());
  return t;
}
static std::string _s2m_wrep_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwf_is  (descriptor_set 0) { buffer layout(std430) wib { uint ISTART[]; }; }
storage_interface mwf_pre (descriptor_set 0) { buffer layout(std430) wprb { uint PRE[]; }; }
storage_interface mwf_pay (descriptor_set 0) { buffer layout(std430) wyb { uint PAY[]; }; }
storage_interface mwf_rep (descriptor_set 0) { buffer layout(std430) wrb { uint REP[]; }; }
%WPAR%
compute_interface iface_wrep { storage { mwf_is mwf_pre mwf_pay mwf_rep mwf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_wrep : iface_wrep {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_nc) { return; }
  if (ISTART[i] != 0u) { REP[PRE[i]] = PAY[i]; } // representative corner of this run
}
)S";
  _shadersub(t, "%WPAR%", _s2m_wpar_decl());
  return t;
}
static std::string _s2m_wremap_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwf_pay (descriptor_set 0) { buffer layout(std430) wyb { uint PAY[]; }; }
storage_interface mwf_pre (descriptor_set 0) { buffer layout(std430) wprb { uint PRE[]; }; }
storage_interface mwf_is  (descriptor_set 0) { buffer layout(std430) wib { uint ISTART[]; }; }
storage_interface mwf_rep (descriptor_set 0) { buffer layout(std430) wrb { uint REP[]; }; }
storage_interface mwf_rm  (descriptor_set 0) { buffer layout(std430) wrmb { uint REMAP[]; }; }
%WPAR%
compute_interface iface_wremap { storage { mwf_pay mwf_pre mwf_is mwf_rep mwf_rm mwf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_wremap : iface_wremap {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_nc) { return; }
  // run id = INCLUSIVE prefix - 1 (exclusive PRE[i] equals the run id only at run
  // starts; for interior slots PRE[i] over-counts by one, so add ISTART[i] back).
  uint seg = PRE[i] + ISTART[i] - 1u;
  REMAP[PAY[i]] = REP[seg]; // original corner -> its run's representative corner
}
)S";
  _shadersub(t, "%WPAR%", _s2m_wpar_decl());
  return t;
}
static std::string _s2m_wvidx_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mwf_rm (descriptor_set 0) { buffer layout(std430) wrmb { uint REMAP[]; }; }
storage_interface mwf_vi (descriptor_set 0) { buffer layout(std430) wvib { uint VId[]; }; }
%WPAR%
compute_interface iface_wvidx { storage { mwf_rm mwf_vi mwf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_wvidx : iface_wvidx {
  uint c = gl_GlobalInvocationID.x;
  if (c >= p_nc) { return; }
  VId[c] = REMAP[c]; // coincident corners now share one representative vertex index
}
)S";
  _shadersub(t, "%WPAR%", _s2m_wpar_decl());
  return t;
}

// M2.5 — GPU live-count for the WELD (no readback). Reads the live face count from the
// header (cs_writehdr published it), clamps to capacity (= what emit actually wrote), and
// writes the live CORNER count into: the weld run-id scan's control (p_sn), my weld kernels'
// control (p_pnc), and LIVE[0] (for MeshSort::setCount -> the sort + its internal scan). All
// the weld kernels then early-out at the live count while the scratch stays alloc'd to capacity
// -> the sort WORK shrinks to live with NO host readback. Runs after cs_writehdr, before the sort.
static std::string _s2m_wsetcount_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mws_hdr (descriptor_set 0) { buffer layout(std430) wshb { uint HDR[]; }; }
storage_interface mws_sct (descriptor_set 0) { buffer layout(std430) wssb { uint p_sn; uint sp1; uint sp2; uint sp3; }; }
storage_interface mws_wpr (descriptor_set 0) { buffer layout(std430) wspb { uint p_pnc; uint wp1; uint wp2; uint wp3; }; }
storage_interface mws_liv (descriptor_set 0) { buffer layout(std430) wslb { uint LIVE[]; }; }
%PAR%
compute_interface iface_wsetcount { storage { mws_hdr mws_sct mws_wpr mws_liv mif_par } inputs { layout(local_size_x = 1); } }
compute_shader cs_wsetcount : iface_wsetcount {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint nf = min(HDR[2], p_fcap);   // live face count, clamped to capacity (emit clamps identically)
  uint nc = nf * 3u;               // soup: 3 corners / triangle
  p_sn    = nc;                    // weld run-id scan (MeshScan) p_n
  p_pnc   = nc;                    // cs_wkey/wmark/wrep/wremap/wvidx p_nc
  LIVE[0] = nc;                    // MeshSort::setCount source -> the sort's _ct/_sct p_n
}
)S";
  _shadersub(t, "%PAR%", _s2m_par_decl());
  return t;
}

struct SdfToMeshInst : public hm::MeshComputeInst {
  SdfToMeshInst(const SdfToMeshData* d, dflow::GraphInst* g)
      : hm::MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<hm::MeshPlugTraits>("Out");
    _input  = typedInputNamed<dflowgfx::SdfGridPlugTraits>("In");
  }
  dflowgfx::sdfgrid_inst_ptr_t srcGrid() const {
    auto out = std::dynamic_pointer_cast<dflowgfx::sdfgrid_outpluginst_t>(_input->_connectedOutput);
    return out ? out->_value : nullptr;
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto e   = inst->_impl.getShared<hm::MeshEnv>();
    auto fxi = e->_ctx->FXI();
    _scan.init(e->_ctx);
    _params = fxi->createStorageBuffer(64);
    _sct    = fxi->createStorageBuffer(16); // MeshScan control {p_n,...} (host-written pre-scan)
    // the small constant tables (uploaded once)
    _tetcube = fxi->createStorageBuffer(sizeof(kTetCube));
    _tetedge = fxi->createStorageBuffer(sizeof(kTetEdge));
    _tettbl  = fxi->createStorageBuffer(sizeof(kTetTable));
    _tricnt  = fxi->createStorageBuffer(16 * 4);
    auto up = [&](FxShaderStorageBuffer* b, const void* p, size_t n) {
      auto m = fxi->mapStorageBuffer(b, 0, n, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, p, n);
      fxi->unmapStorageBuffer(m.get());
    };
    up(_tetcube, kTetCube, sizeof(kTetCube));
    up(_tetedge, kTetEdge, sizeof(kTetEdge));
    up(_tettbl, kTetTable, sizeof(kTetTable));
    uint32_t tricnt[16];
    for (int c = 0; c < 16; c++) {
      int n = 0;
      for (int j = 0; j < 6; j += 3)
        if (kTetTable[c * 6 + j] >= 0)
          n++;
      tricnt[c] = uint32_t(n);
    }
    up(_tricnt, tricnt, sizeof(tricnt));
    auto sh   = fxi->shaderFromShaderText("sdf_s2m_count", _s2m_count_text());
    _cs_count = fxi->computeShader(sh, "cs_count");
    auto she  = fxi->shaderFromShaderText("sdf_s2m_emit", _s2m_emit_text());
    _cs_emit  = fxi->computeShader(she, "cs_emit");
    auto shi  = fxi->shaderFromShaderText("sdf_s2m_ident", _s2m_ident_text());
    _cs_ident = fxi->computeShader(shi, "cs_ident");
    auto shh  = fxi->shaderFromShaderText("sdf_s2m_writehdr", _s2m_writehdr_text()); // M2.5 GPU-resident count
    _cs_writehdr = fxi->computeShader(shh, "cs_writehdr");
    // GPU-HANG FIX: a HOST-VISIBLE 1-slot mirror of the scan total + the kernel that fills it, so the
    // onTopologyReady count readback is a direct map (no device-local copyToHost staging submit).
    _countHost   = fxi->createStorageBuffer(16);
    _cs_copytotal = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_copytotal", _s2m_copytotal_text()), "cs_copytotal");
    // BLOCKY / CUBERILLE — alternate count + emit (axis-aligned voxel faces). Reuses scan/ident/writehdr.
    if (_d->_blocky) {
      _cs_count_b = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_count_blocky", _s2m_count_blocky_text()), "cs_count_blocky");
      _cs_emit_b  = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_emit_blocky", _s2m_emit_blocky_text()), "cs_emit_blocky");
    }
    // WELD (M2 weld) — sort/scan + the 5 remap kernels (only used when _weld; NEVER with blocky:
    // welding would merge the hard 90deg per-face normals)
    if (_d->_weld and not _d->_blocky) {
      _wscan.init(e->_ctx);
      _wsort.init(e->_ctx);
      _wsct = fxi->createStorageBuffer(16);
      _wpar = fxi->createStorageBuffer(16);
      _wlive = fxi->createStorageBuffer(16); // M2.5: GPU live corner count -> MeshSort::setCount
      _cs_wsetcount = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wsetcount", _s2m_wsetcount_text()), "cs_wsetcount");
      _cs_wkey   = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wkey", _s2m_wkey_text()), "cs_wkey");
      _cs_wmark  = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wmark", _s2m_wmark_text()), "cs_wmark");
      _cs_wrep   = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wrep", _s2m_wrep_text()), "cs_wrep");
      _cs_wremap = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wremap", _s2m_wremap_text()), "cs_wremap");
      _cs_wvidx  = fxi->computeShader(fxi->shaderFromShaderText("sdf_s2m_wvidx", _s2m_wvidx_text()), "cs_wvidx");
    }
  }
  void writeParams(Context* ctx) final {
    auto in = srcGrid();
    if (not in or not in->_ssbo or in->_dim[0] < 2)
      return;
    auto e        = _graphinst->_impl.getShared<hm::MeshEnv>();
    int dim       = in->_dim[0];
    int cellsx    = dim - 1;
    int ncells    = cellsx * cellsx * cellsx;
    if (hm::meshNextPow2(ncells) != _cntCap) {
      _cnt    = e->_pool->acquireChannel(4, ncells);
      _base   = e->_pool->acquireChannel(4, ncells + 1);
      _cntCap = hm::meshNextPow2(ncells);
    }
    { // MeshScan control buffer: element count
      uint32_t sct[4] = {uint32_t(ncells), 0u, 0u, 0u};
      auto ms = ctx->FXI()->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
      std::memcpy(ms->_mappedaddr, sct, sizeof(sct));
      ctx->FXI()->unmapStorageBuffer(ms.get());
    }
    auto out      = _output->_value;
    bool sized    = (out and not out->_channels.empty());
    // ncap = vertex buffer capacity; fcap = the face_offsets buffer's OWN capacity-1.
    // These are SEPARATE pow2 roundings (verts ~ pow2(6*nf), fo ~ pow2(2*nf+1)) —
    // deriving fcap as ncap/3 can exceed the fo buffer and walk it OOB (device lost).
    int ncap      = sized ? out->_capacity : 0;
    int fcap      = (sized and out->_face_offsets) ? (out->_face_offsets->_capacity - 1) : 0;
    auto fxi      = ctx->FXI();
    // M2.5 FULLY-GPU COUNT — NO per-frame readback. The LIVE face count lives on the
    // GPU (cs_writehdr copies the scan total into _header each frame, in compute()).
    // On the CPU we report the FIXED allocated CAPACITY: it sizes the render
    // triangulator's dispatch, the index-buffer alloc, and the weld corner span. The
    // render early-outs at the live count (its cs_importcount reads _header), so it
    // draws EXACTLY the live primitives — no grow-only, no 1-frame staleness, no
    // degenerate tail, no host sync. facecap = min(fcap, ncap/3) so num_corners never
    // exceeds the vertex buffer (emit's p_ncap backstop already clamps to the same).
    if (sized) {
      int facecap       = std::min(fcap, ncap / 3);
      out->_num_faces   = facecap;
      out->_num_corners = facecap * 3;
      out->_num_verts   = facecap * 3;
    }
    struct {
      uint32_t dim, cellsx, cellsy, cellsz, ncells, ncap, fcap, doweld;
      float ox, oy, oz, voxel, iso, p1, p2, p3;
    } P{uint32_t(dim), uint32_t(cellsx), uint32_t(cellsx), uint32_t(cellsx), uint32_t(ncells),
        uint32_t(ncap), uint32_t(fcap), uint32_t((_d->_weld and not _d->_blocky) ? 1 : 0),
        in->_origin[0], in->_origin[1], in->_origin[2], in->_voxel, 0.0f, 0.0f, 0.0f, 0.0f};
    auto m = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
    _dim = dim; _ncells = ncells;
    // WELD setup (pre-phase): scratch buffers + MeshSort/MeshScan controls. The
    // weld dedups the LIVE corner span [0, num_corners). (STATIC bake = once;
    // per-frame for animated, which is why weld defaults off in the animated DSL.)
    _weld_nc = 0;
    if (_d->_weld and not _d->_blocky and sized and out->_num_corners > 0) {
      int cap = out->_capacity;
      if (cap != _weldCap) {
        _edgeid = e->_pool->acquireChannel(4, cap); // emit's per-vertex grid-edge weld key
        _wkeyb = e->_pool->acquireChannel(4, cap);
        _wpayb = e->_pool->acquireChannel(4, cap);
        _wisb  = e->_pool->acquireChannel(4, cap);
        _wpreb = e->_pool->acquireChannel(4, cap + 1);
        _wrepb = e->_pool->acquireChannel(4, cap);
        _wrmb  = e->_pool->acquireChannel(4, cap);
        _weldCap = cap;
      }
      int nc = out->_num_corners;
      _wsort.ensure(ctx, nc);
      uint32_t sc[4] = {uint32_t(nc), 0u, 0u, 0u};
      auto a = fxi->mapStorageBuffer(_wsct, 0, 16, BufferMapAccess::WRITE_ONLY);
      std::memcpy(a->_mappedaddr, sc, 16);
      fxi->unmapStorageBuffer(a.get());
      auto b = fxi->mapStorageBuffer(_wpar, 0, 16, BufferMapAccess::WRITE_ONLY);
      std::memcpy(b->_mappedaddr, sc, 16); // {p_nc, ...}
      fxi->unmapStorageBuffer(b.get());
      _weld_nc = nc;
    }
  }
  // count + scan (always); emit + ident only once the output mesh is sized
  // (onTopologyReady reads the total back, allocMesh's it, and re-evals to emit).
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = srcGrid();
    if (not in or not in->_ssbo or not _cnt)
      return;
    auto e  = inst->_impl.getShared<hm::MeshEnv>();
    auto ci = e->_ctx->CI();
    if (_d->_blocky) { // CUBERILLE count: 2 tris per sign-flipping +axis voxel face
      ci->bindStorageBuffer(_cs_count_b, 0, in->_ssbo);
      ci->bindStorageBuffer(_cs_count_b, 1, _cnt->_ssbo);
      ci->bindStorageBuffer(_cs_count_b, 2, _params);
      ci->dispatchCompute(_cs_count_b, (_ncells + 63) / 64, 1, 1);
    } else {
      ci->bindStorageBuffer(_cs_count, 0, in->_ssbo);
      ci->bindStorageBuffer(_cs_count, 1, _cnt->_ssbo);
      ci->bindStorageBuffer(_cs_count, 2, _tetcube);
      ci->bindStorageBuffer(_cs_count, 3, _tricnt);
      ci->bindStorageBuffer(_cs_count, 4, _params);
      ci->dispatchCompute(_cs_count, (_ncells + 63) / 64, 1, 1);
    }
    ci->storageBarrier();
    _scan.scan(e->_ctx, _cnt->_ssbo, _base->_ssbo, _sct, _ncells); // BASE[ncells] = total tris
    ci->storageBarrier();
    // GPU-HANG FIX: mirror the scan total into the HOST-VISIBLE _countHost so onTopologyReady can read
    // it with a direct map (no device-local copyToHost staging submit that hangs MoltenVK on deep chains).
    if (_cs_copytotal and _countHost) {
      ci->bindStorageBuffer(_cs_copytotal, 0, _base->_ssbo);
      ci->bindStorageBuffer(_cs_copytotal, 1, _countHost);
      ci->bindStorageBuffer(_cs_copytotal, 2, _params);
      ci->dispatchCompute(_cs_copytotal, 1, 1, 1);
      ci->storageBarrier();
    }
    auto out = _output->_value;
    if (out and not out->_channels.empty()) { // sized (eval 2+): emit
      // M2.5: publish the LIVE count to the GPU header (no readback) — the render
      // triangulator's cs_importcount reads it to early-out at the real primitive count.
      if (out->_header) {
        ci->bindStorageBuffer(_cs_writehdr, 0, _base->_ssbo);
        ci->bindStorageBuffer(_cs_writehdr, 1, out->_header);
        ci->bindStorageBuffer(_cs_writehdr, 2, _params);
        ci->dispatchCompute(_cs_writehdr, 1, 1, 1);
        ci->storageBarrier();
      }
      // cs_ident initializes the WHOLE allocated buffer each frame: vidx identity,
      // fo CSR, and P cleared to a degenerate point (so any tail face the emit
      // doesn't write this frame draws as zero-area / invisible).
      ci->bindStorageBuffer(_cs_ident, 0, out->_vidx->_ssbo);
      ci->bindStorageBuffer(_cs_ident, 1, out->_face_offsets->_ssbo);
      ci->bindStorageBuffer(_cs_ident, 2, out->channel(hm::MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs_ident, 3, _edgeid ? _edgeid->_ssbo : out->_vidx->_ssbo); // weld-key clear (dummy & unwritten when no weld)
      ci->bindStorageBuffer(_cs_ident, 4, _params);
      ci->dispatchCompute(_cs_ident, (out->_capacity + 63) / 64, 1, 1);
      ci->storageBarrier(); // cs_ident CLEARS P/EID; emit then WRITES the live verts — must be ordered
      if (_d->_blocky) { // CUBERILLE emit: axis-aligned voxel quads (2 tris each), per-face normals, no weld
        ci->bindStorageBuffer(_cs_emit_b, 0, in->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 1, _base->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 2, out->channel(hm::MeshChannel::POSITION)->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 3, out->channel(hm::MeshChannel::NORMAL)->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 4, out->channel(hm::MeshChannel::BINORMAL)->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 5, out->channel(hm::MeshChannel::UV0)->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 6, out->channel(hm::MeshChannel::COLOR)->_ssbo);
        ci->bindStorageBuffer(_cs_emit_b, 7, _params);
        ci->dispatchCompute(_cs_emit_b, (_ncells + 63) / 64, 1, 1);
      } else {
        ci->bindStorageBuffer(_cs_emit, 0, in->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 1, _base->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 2, _tetcube);
        ci->bindStorageBuffer(_cs_emit, 3, _tetedge);
        ci->bindStorageBuffer(_cs_emit, 4, _tettbl);
        ci->bindStorageBuffer(_cs_emit, 5, out->channel(hm::MeshChannel::POSITION)->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 6, out->channel(hm::MeshChannel::NORMAL)->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 7, out->channel(hm::MeshChannel::BINORMAL)->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 8, out->channel(hm::MeshChannel::UV0)->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 9, out->channel(hm::MeshChannel::COLOR)->_ssbo);
        ci->bindStorageBuffer(_cs_emit, 10, _edgeid ? _edgeid->_ssbo : out->_vidx->_ssbo); // weld key (dummy & unwritten when no weld)
        ci->bindStorageBuffer(_cs_emit, 11, _params);
        ci->dispatchCompute(_cs_emit, (_ncells + 63) / 64, 1, 1);
      }
      ci->storageBarrier();
      // WELD: dedup coincident corners -> shared-vertex topology (rewrites vidx).
      // The soup attrs stay in place; vidx now points to per-run representatives.
      if (_d->_weld and _weld_nc > 0) {
        int nc      = _weld_nc;            // CAPACITY corner span — sizes the DISPATCH (group counts)
        int wgroups = (nc + 63) / 64;      // + the scratch alloc; the WORK shrinks to live via p_n early-out
        // M2.5: GPU-write the LIVE corner count (from the header) into every weld control buffer, so the
        // sort/scan/remap kernels early-out at the live count — live-sized WORK, capacity-sized dispatch,
        // NO host readback. (out->_header holds the live nf, published by cs_writehdr above.)
        if (out->_header and _cs_wsetcount) {
          ci->bindStorageBuffer(_cs_wsetcount, 0, out->_header);
          ci->bindStorageBuffer(_cs_wsetcount, 1, _wsct);
          ci->bindStorageBuffer(_cs_wsetcount, 2, _wpar);
          ci->bindStorageBuffer(_cs_wsetcount, 3, _wlive);
          ci->bindStorageBuffer(_cs_wsetcount, 4, _params);
          ci->dispatchCompute(_cs_wsetcount, 1, 1, 1);
          ci->storageBarrier();
          _wsort.setCount(e->_ctx, _wlive); // -> the sort's _ct/_sct p_n = live
          ci->storageBarrier();
        }
        ci->bindStorageBuffer(_cs_wkey, 0, _edgeid->_ssbo);
        ci->bindStorageBuffer(_cs_wkey, 1, _wkeyb->_ssbo);
        ci->bindStorageBuffer(_cs_wkey, 2, _wpayb->_ssbo);
        ci->bindStorageBuffer(_cs_wkey, 3, _wpar);
        ci->dispatchCompute(_cs_wkey, wgroups, 1, 1);
        ci->storageBarrier();
        _wsort.sort(e->_ctx, _wkeyb->_ssbo, _wpayb->_ssbo); // group equal edge-ids (stable)
        ci->storageBarrier();
        ci->bindStorageBuffer(_cs_wmark, 0, _wkeyb->_ssbo);
        ci->bindStorageBuffer(_cs_wmark, 1, _wisb->_ssbo);
        ci->bindStorageBuffer(_cs_wmark, 2, _wpar);
        ci->dispatchCompute(_cs_wmark, wgroups, 1, 1);
        ci->storageBarrier();
        _wscan.scan(e->_ctx, _wisb->_ssbo, _wpreb->_ssbo, _wsct, nc); // PRE = run id per sorted slot
        ci->storageBarrier();
        ci->bindStorageBuffer(_cs_wrep, 0, _wisb->_ssbo);
        ci->bindStorageBuffer(_cs_wrep, 1, _wpreb->_ssbo);
        ci->bindStorageBuffer(_cs_wrep, 2, _wpayb->_ssbo);
        ci->bindStorageBuffer(_cs_wrep, 3, _wrepb->_ssbo);
        ci->bindStorageBuffer(_cs_wrep, 4, _wpar);
        ci->dispatchCompute(_cs_wrep, wgroups, 1, 1);
        ci->storageBarrier();
        ci->bindStorageBuffer(_cs_wremap, 0, _wpayb->_ssbo);
        ci->bindStorageBuffer(_cs_wremap, 1, _wpreb->_ssbo);
        ci->bindStorageBuffer(_cs_wremap, 2, _wisb->_ssbo);
        ci->bindStorageBuffer(_cs_wremap, 3, _wrepb->_ssbo);
        ci->bindStorageBuffer(_cs_wremap, 4, _wrmb->_ssbo);
        ci->bindStorageBuffer(_cs_wremap, 5, _wpar);
        ci->dispatchCompute(_cs_wremap, wgroups, 1, 1);
        ci->storageBarrier();
        ci->bindStorageBuffer(_cs_wvidx, 0, _wrmb->_ssbo);
        ci->bindStorageBuffer(_cs_wvidx, 1, out->_vidx->_ssbo);
        ci->bindStorageBuffer(_cs_wvidx, 2, _wpar);
        ci->dispatchCompute(_cs_wvidx, wgroups, 1, 1);
        ci->storageBarrier();
      }
      // CONTENT-DYNAMIC topology: the marching-tets connectivity (vidx, and the
      // packed soup positions) is rebuilt EVERY frame as the SDF animates, but the
      // counts/buffer-handles are STABLE (grow-only _num_faces plateaus). The render
      // triangulator's topoDirty (hmdflow_render.cpp) keys ONLY on _topoVersion /
      // counts / buffer handles — NOT buffer CONTENT — so without an explicit bump it
      // freezes its index buffer at the first plateau frame and thereafter fans STALE
      // vidx -> triangles pull positions from the wrong corner slots (severe with weld,
      // whose representative indices shift per frame). Bump _topoVersion so the
      // triangulator re-runs cs_tri against THIS frame's vidx. (Genuinely correct, not
      // a hack: the connectivity really does change every frame.)
      markOutTopo(_output);
    }
  }
  // post-eval (the mesh driver calls this): read the scan total -> exact alloc ->
  // request the re-eval that emits. Once sized, no more topology change.
  bool onTopologyReady(Context* ctx) final {
    if (not _cnt or not _base)
      return false;
    auto fxi = ctx->FXI();
    uint32_t total = 0;
    { // read the HOST-VISIBLE mirror (direct map) — NOT the device-local _base (which would route
      // through a staging command buffer and hang MoltenVK on deep CSG chains). See cs_copytotal.
      auto m = fxi->mapStorageBuffer(_countHost ? _countHost : _base->_ssbo, 0, 4, BufferMapAccess::READ_ONLY);
      std::memcpy(&total, m->_mappedaddr, 4);
      fxi->unmapStorageBuffer(m.get());
    }
    // HARD MEMORY CAP — the extraction output is data-dependent (surface area / voxel^2). It MUST be
    // bounded so no field / extent / RES / garbage scan-total can allocate GBs and HANG/OOM the GPU.
    // kMaxFaces is a fixed budget (NOT a function of extent or content): a clean blob is tens of K
    // faces, so this never bites normally; a runaway is CLAMPED (mesh gets holes) + logged LOUD, never
    // a hang. `total` is read off the GPU as a uint32 — clamp it as uint BEFORE the *2 so a garbage
    // value (e.g. 0xFFFFFFFF) can't int-overflow the alloc.
    static const uint32_t kMaxFaces = 1u << 20; // ~1M faces; alloc 2x*3v*5ch*16B headroom -> ~480MB ceiling
    bool clamped = (total > kMaxFaces);
    if (clamped)
      printf("[sdf2mesh] WARNING: surface needs %u faces > cap %u — CLAMPING (mesh will have HOLES; "
             "lower RES, shrink extent, or the field is degenerate). NO GPU hang.\n", total, kMaxFaces);
    int nf = int(std::min(total, kMaxFaces));
    // grow-only: re-alloc only when the count exceeds the current capacity (an
    // ANIMATED mesh whose tri count rises past the headroom). The pool's pow2
    // size-classing already gives 1-2x; allocMesh at 2x the first count buys
    // explicit headroom so per-frame growth stays within the buffers.
    auto out = _output->_value;
    int curCapFaces = (out and not out->_channels.empty()) ? (out->_capacity / 3) : 0;
    if (nf <= curCapFaces and curCapFaces > 0)
      return false; // current buffers already hold this count
    auto e        = _graphinst->_impl.getShared<hm::MeshEnv>();
    int alloc_nf  = std::max(1, nf * 2);   // 2x headroom for animation
    int alloc_nv  = alloc_nf * 3;
    printf("[sdf2mesh] %s: live=%u faces -> alloc=%d faces (cap=%dMB) [dim=%d]\n",
           _d->_blocky ? "blocky" : "tets", total, alloc_nf, int(double(alloc_nv) * 5 * 16 / 1e6), _dim);
    allocMesh(e, out, alloc_nv, alloc_nv, alloc_nf,
              {hm::MeshChannel::POSITION, hm::MeshChannel::NORMAL, hm::MeshChannel::BINORMAL,
               hm::MeshChannel::UV0, hm::MeshChannel::COLOR});
    int nv            = std::max(1, nf * 3); // LIVE counts (the render draws these)
    out->_num_verts   = nv;
    out->_num_corners = nv;
    out->_num_faces   = std::max(0, nf);
    _builtFaces       = nf;
    // M2.5 — the live count is GPU-resident from here on: writeParams reports CAPACITY,
    // cs_writehdr publishes the live count to _header, the render reads it (cs_importcount).
    out->_gpuResidentCount = true;
    // GPU header (the triangulator kernels early-out on num_faces); bbox = brick bound
    auto in = srcGrid();
    fvec3 bmin(0, 0, 0), bmax(0, 0, 0);
    if (in) {
      bmin = fvec3(in->_origin[0], in->_origin[1], in->_origin[2]);
      float span = in->_voxel * float(in->_dim[0] - 1);
      bmax = bmin + fvec3(span, span, span);
    }
    struct { uint32_t nv, nc, nf, flags; float bx, by, bz, bw, Bx, By, Bz, Bw; } H{
        uint32_t(nv), uint32_t(nv), uint32_t(std::max(0, nf)), 0,
        bmin.x, bmin.y, bmin.z, 1.0f, bmax.x, bmax.y, bmax.z, 1.0f};
    auto m = fxi->mapStorageBuffer(out->_header, 0, sizeof(H), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &H, sizeof(H));
    fxi->unmapStorageBuffer(m.get());
    return true; // re-eval now emits into the sized buffers
  }
  const SdfToMeshData* _d;
  hm::mesh_outpluginst_ptr_t _output;
  dflowgfx::sdfgrid_inpluginst_ptr_t _input;
  hm::MeshScan _scan;
  hm::gpuchannel_ptr_t _cnt, _base;
  FxShaderStorageBuffer *_params = nullptr, *_sct = nullptr, *_tetcube = nullptr, *_tetedge = nullptr, *_tettbl = nullptr, *_tricnt = nullptr;
  const FxComputeShader *_cs_count = nullptr, *_cs_emit = nullptr, *_cs_ident = nullptr;
  const FxComputeShader *_cs_count_b = nullptr, *_cs_emit_b = nullptr; // CUBERILLE (blocky) count + emit
  const FxComputeShader *_cs_writehdr = nullptr; // M2.5: publish live count to _header (GPU, no readback)
  FxShaderStorageBuffer* _countHost   = nullptr; // HOST-VISIBLE mirror of the scan total (hang-free readback)
  const FxComputeShader *_cs_copytotal = nullptr; // fills _countHost after the scan
  int _cntCap = -1, _dim = 0, _ncells = 0, _builtFaces = -1;
  // WELD (M2 weld) — shared-vertex remap via MeshSort + MeshScan (only when _weld)
  hm::MeshSort _wsort;
  hm::MeshScan _wscan;
  hm::gpuchannel_ptr_t _edgeid, _wkeyb, _wpayb, _wisb, _wpreb, _wrepb, _wrmb;
  FxShaderStorageBuffer *_wsct = nullptr, *_wpar = nullptr, *_wlive = nullptr;
  const FxComputeShader *_cs_wkey = nullptr, *_cs_wmark = nullptr, *_cs_wrep = nullptr, *_cs_wremap = nullptr, *_cs_wvidx = nullptr;
  const FxComputeShader *_cs_wsetcount = nullptr; // M2.5: GPU live-count for the weld (no readback)
  int _weldCap = -1, _weld_nc = 0;
};

static void _reshapeSdfToMeshIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<hm::MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SdfToMeshData::SdfToMeshData() {
}
std::shared_ptr<SdfToMeshData> SdfToMeshData::createShared() {
  auto d = std::make_shared<SdfToMeshData>();
  _reshapeSdfToMeshIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SdfToMeshData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SdfToMeshInst>(this, g);
}
void SdfToMeshData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SdfToMeshData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSdfToMeshIOs(m); });
  clazz->directProperty("weld", &SdfToMeshData::_weld);
  clazz->directProperty("blocky", &SdfToMeshData::_blocky);
}

} // namespace ork::lev2::sdf
