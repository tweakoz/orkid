////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"
#include "../hypermesh/hmdflow_module.h" // MeshEdges + MeshChannel access (the v1 env contract)

ImplementReflectionX(ork::lev2::sdf::MeshToSdfData, "sdf::MeshToSdfData");

namespace ork::lev2::sdf {

namespace hm = ork::lev2::hypermesh;

///////////////////////////////////////////////////////////////////////////////
// MeshToSdf (M1) — see sdfdflow.h for the contract. Kernel chain per eval:
//   cs_zerovpn  : clear the fixed-point vertex-pseudonormal accumulators
//   cs_fnrm     : per-face UNIT Newell normal
//   cs_vpn      : per-face corner walk -> angle-weighted vertex pseudonormals
//                 (fixed-point int atomics — only the DIRECTION matters for sign)
//   cs_epn      : per-edge pseudonormal from the MeshEdges table (f0+f1 unit
//                 normals; boundary edges bump the BCT counter = the free probe
//                 the in-kernel AUTO sign-mode reads)
//   cs_dist     : per-voxel nearest-feature distance over fan triangles with
//                 barycentric REGION classification -> the matching pseudonormal
//                 sign; or the winding-number sign (solid-angle sum) when the
//                 effective mode is WINDING. Fan diagonals classify as FACE
//                 region (exact for convex faces — the v1 face contract).
///////////////////////////////////////////////////////////////////////////////

// SHADER-FILE-PER-KERNEL: storage_interface BINDING indices are GLOBAL per
// shader FILE (declaration order) — a kernel using a subset gets a SPARSE
// descriptor set, which the Metal pipeline compiler REJECTS. One file per
// kernel keeps every set a dense 0..N matching the host bind calls.

static const char* _m2s_par_decl() { // shared params block (identical in every kernel file)
  return R"S(
storage_interface mif_par  (descriptor_set 0) { buffer layout(std430) mprb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  uint p_nf; uint p_nv; uint p_mode; uint p_pad; }; }
)S";
}

static std::string _m2s_zero_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_vpn  (descriptor_set 0) { buffer layout(std430) mvpb { int VPNd[];  }; }
%PAR%
compute_interface iface_zero { storage { mif_vpn mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_zerovpn : iface_zero {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  VPNd[4u * v + 0u] = 0;
  VPNd[4u * v + 1u] = 0;
  VPNd[4u * v + 2u] = 0;
  VPNd[4u * v + 3u] = 0;
}
)S";
  _shadersub(t, "%PAR%", _m2s_par_decl());
  return t;
}

static std::string _m2s_fnrm_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_P    (descriptor_set 0) { buffer layout(std430) mpb  { vec4 Pd[];   }; }
storage_interface mif_vi   (descriptor_set 0) { buffer layout(std430) mvib { uint VId[];  }; }
storage_interface mif_fo   (descriptor_set 0) { buffer layout(std430) mfob { uint FOd[];  }; }
storage_interface mif_fn   (descriptor_set 0) { buffer layout(std430) mfnb { vec4 FN[];   }; }
%PAR%
compute_interface iface_fnrm { storage { mif_P mif_vi mif_fo mif_fn mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_fnrm : iface_fnrm {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint o0 = FOd[f];
  uint o1 = FOd[f + 1u];
  vec3 acc = vec3(0.0, 0.0, 0.0);
  for (uint c = o0; c < o1; c++) {
    uint cn = (c + 1u == o1) ? o0 : (c + 1u);
    acc = acc + cross(Pd[VId[c]].xyz, Pd[VId[cn]].xyz);
  }
  float l = length(acc);
  vec3 nn = (l > 1.0e-20) ? (acc / l) : vec3(0.0, 0.0, 0.0);
  FN[f] = vec4(nn, 0.0);
}
)S";
  _shadersub(t, "%PAR%", _m2s_par_decl());
  return t;
}

static std::string _m2s_vpn_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_P    (descriptor_set 0) { buffer layout(std430) mpb  { vec4 Pd[];   }; }
storage_interface mif_vi   (descriptor_set 0) { buffer layout(std430) mvib { uint VId[];  }; }
storage_interface mif_fo   (descriptor_set 0) { buffer layout(std430) mfob { uint FOd[];  }; }
storage_interface mif_fn   (descriptor_set 0) { buffer layout(std430) mfnb { vec4 FN[];   }; }
storage_interface mif_vpn  (descriptor_set 0) { buffer layout(std430) mvpb { int VPNd[];  }; }
%PAR%
compute_interface iface_vpn { storage { mif_P mif_vi mif_fo mif_fn mif_vpn mif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_vpn : iface_vpn {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint o0 = FOd[f];
  uint o1 = FOd[f + 1u];
  vec3 fn = FN[f].xyz;
  for (uint c = o0; c < o1; c++) {
    uint cp = (c == o0) ? (o1 - 1u) : (c - 1u);
    uint cn = (c + 1u == o1) ? o0 : (c + 1u);
    vec3 P0 = Pd[VId[c]].xyz;
    vec3 e1 = Pd[VId[cn]].xyz - P0;
    vec3 e2 = Pd[VId[cp]].xyz - P0;
    float l1 = length(e1);
    float l2 = length(e2);
    float ang = 0.0;
    if (l1 > 1.0e-12 && l2 > 1.0e-12) {
      ang = acos(clamp(dot(e1 / l1, e2 / l2), -1.0, 1.0));
    }
    vec3 contrib = fn * (ang * 100000.0);
    uint v = VId[c];
    atomicAdd(VPNd[4u * v + 0u], int(contrib.x));
    atomicAdd(VPNd[4u * v + 1u], int(contrib.y));
    atomicAdd(VPNd[4u * v + 2u], int(contrib.z));
  }
}
)S";
  _shadersub(t, "%PAR%", _m2s_par_decl());
  return t;
}

static std::string _m2s_epn_text() {
  // NB: NO mif_par here — cs_epn references no p_* field; declaring+binding an
  // unused buffer risks DCE dropping it and re-sparsifying the descriptor set.
  return R"S(
fxconfig fxcfg_default {}
storage_interface mif_fn   (descriptor_set 0) { buffer layout(std430) mfnb { vec4 FN[];   }; }
storage_interface mif_epn  (descriptor_set 0) { buffer layout(std430) mepb { vec4 EPN[];  }; }
storage_interface mif_ed   (descriptor_set 0) { buffer layout(std430) medb { uint EDGE[]; }; }
storage_interface mif_ec   (descriptor_set 0) { buffer layout(std430) mecb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface mif_bct  (descriptor_set 0) { buffer layout(std430) mbcb { uint BCT[]; }; }
compute_interface iface_epn { storage { mif_fn mif_epn mif_ed mif_ec mif_bct } inputs { layout(local_size_x = 64); } }
compute_shader cs_epn : iface_epn {
  uint e = gl_GlobalInvocationID.x;
  if (e >= e_ne) { return; }
  uint f0 = EDGE[4u * e + 2u];
  uint f1 = EDGE[4u * e + 3u];
  vec3 pn = FN[f0].xyz;
  if (f1 == 0xFFFFFFFFu) {
    atomicAdd(BCT[0], 1u); // boundary edge -> the AUTO sign-mode probe
  } else {
    pn = pn + FN[f1].xyz;
  }
  EPN[e] = vec4(pn, 0.0);
}
)S";
}

static std::string _m2s_dist_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface mif_P    (descriptor_set 0) { buffer layout(std430) mpb  { vec4 Pd[];   }; }
storage_interface mif_vi   (descriptor_set 0) { buffer layout(std430) mvib { uint VId[];  }; }
storage_interface mif_fo   (descriptor_set 0) { buffer layout(std430) mfob { uint FOd[];  }; }
storage_interface mif_fn   (descriptor_set 0) { buffer layout(std430) mfnb { vec4 FN[];   }; }
storage_interface mif_vpn  (descriptor_set 0) { buffer layout(std430) mvpb { int VPNd[];  }; }
storage_interface mif_epn  (descriptor_set 0) { buffer layout(std430) mepb { vec4 EPN[];  }; }
storage_interface mif_ce   (descriptor_set 0) { buffer layout(std430) mceb { uint CEDGE[]; }; }
storage_interface mif_bct  (descriptor_set 0) { buffer layout(std430) mbcb { uint BCT[]; }; }
%PAR%
storage_interface mif_sdf  (descriptor_set 0) { buffer layout(std430) msdb { float SDF[]; }; }
compute_interface iface_dist { storage { mif_P mif_vi mif_fo mif_fn mif_vpn mif_epn mif_ce mif_bct mif_par mif_sdf }
                               inputs { layout(local_size_x = 64); } }
compute_shader cs_dist : iface_dist {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  vec3 p = vec3(p_ox, p_oy, p_oz) + vec3(float(ix), float(iy), float(iz)) * p_voxel;
  uint mode = p_mode;
  if (mode == 2u) { mode = (BCT[0] > 0u) ? 1u : 0u; } // AUTO: dirty topology -> winding
  float bestd2 = 1.0e30;
  float bsdot  = 1.0;
  float wind   = 0.0;
  for (uint f = 0u; f < p_nf; f++) {
    uint o0 = FOd[f];
    uint o1 = FOd[f + 1u];
    uint n  = o1 - o0;
    vec3 A = Pd[VId[o0]].xyz;
    for (uint k = 1u; k + 1u < n; k++) {
      uint cb = o0 + k;
      uint cc = o0 + k + 1u;
      vec3 B = Pd[VId[cb]].xyz;
      vec3 C = Pd[VId[cc]].xyz;
      // ---- winding (solid angle; Oosterom-Strackee halves) ----
      if (mode == 1u) {
        vec3 wa = A - p;
        vec3 wb = B - p;
        vec3 wc = C - p;
        float la = length(wa);
        float lb = length(wb);
        float lc = length(wc);
        float det = dot(wa, cross(wb, wc));
        float den = la * lb * lc + dot(wa, wb) * lc + dot(wb, wc) * la + dot(wc, wa) * lb;
        wind = wind + atan(det, den);
      }
      // ---- closest point on tri ABC with REGION classification (Ericson) ----
      vec3 ab = B - A;
      vec3 ac = C - A;
      vec3 ap = p - A;
      float d1 = dot(ab, ap);
      float d2 = dot(ac, ap);
      vec3 cand = A;
      int reg  = 1; // vertex A
      int done = 0;
      if (d1 <= 0.0 && d2 <= 0.0) { done = 1; }
      vec3 bp = p - B;
      float d3 = dot(ab, bp);
      float d4 = dot(ac, bp);
      if (done == 0 && d3 >= 0.0 && d4 <= d3) { cand = B; reg = 2; done = 1; }
      float vc = d1 * d4 - d3 * d2;
      if (done == 0 && vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        float t = d1 / (d1 - d3);
        cand = A + ab * t;
        reg  = 4; // edge AB
        done = 1;
      }
      vec3 cp = p - C;
      float d5 = dot(ab, cp);
      float d6 = dot(ac, cp);
      if (done == 0 && d6 >= 0.0 && d5 <= d6) { cand = C; reg = 3; done = 1; }
      float vb = d5 * d2 - d1 * d6;
      if (done == 0 && vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        float t = d2 / (d2 - d6);
        cand = A + ac * t;
        reg  = 6; // edge CA
        done = 1;
      }
      float va = d3 * d6 - d5 * d4;
      if (done == 0 && va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        cand = B + (C - B) * t;
        reg  = 5; // edge BC
        done = 1;
      }
      if (done == 0) {
        float den2 = va + vb + vc;
        float t1 = (abs(den2) > 1.0e-30) ? (vb / den2) : 0.0;
        float t2 = (abs(den2) > 1.0e-30) ? (vc / den2) : 0.0;
        cand = A + ab * t1 + ac * t2;
        reg  = 0; // face interior
      }
      vec3 dv = p - cand;
      float dd = dot(dv, dv);
      if (dd < bestd2) {
        // map the fan-tri region to the POLYGON feature: fan diagonals (AB when
        // k>1; CA when cc is not the last corner) are polygon-INTERIOR -> face.
        vec3 nrm = FN[f].xyz;
        if (reg == 1) {
          uint v = VId[o0];
          nrm = vec3(float(VPNd[4u * v + 0u]), float(VPNd[4u * v + 1u]), float(VPNd[4u * v + 2u]));
        }
        if (reg == 2) {
          uint v = VId[cb];
          nrm = vec3(float(VPNd[4u * v + 0u]), float(VPNd[4u * v + 1u]), float(VPNd[4u * v + 2u]));
        }
        if (reg == 3) {
          uint v = VId[cc];
          nrm = vec3(float(VPNd[4u * v + 0u]), float(VPNd[4u * v + 1u]), float(VPNd[4u * v + 2u]));
        }
        if (reg == 4 && k == 1u)      { nrm = EPN[CEDGE[o0]].xyz; }  // true polygon edge c0->c1
        if (reg == 5)                 { nrm = EPN[CEDGE[cb]].xyz; }  // consecutive corners: always true
        if (reg == 6 && (k + 2u == n)) { nrm = EPN[CEDGE[cc]].xyz; } // closing edge c_last->c0
        bestd2 = dd;
        bsdot  = dot(dv, nrm);
      }
    }
  }
  float sd  = sqrt(bestd2);
  float sgn = 1.0;
  if (mode == 1u) {
    if (abs(wind) > 3.14159265) { sgn = -1.0; } // |sum(omega/2)| = 2pi inside
  } else {
    if (bsdot < 0.0) { sgn = -1.0; }
  }
  SDF[i] = sd * sgn;
}
)S";
  _shadersub(t, "%PAR%", _m2s_par_decl());
  return t;
}

struct MeshToSdfInst : public SdfComputeInst {
  MeshToSdfInst(const MeshToSdfData* d, dflow::GraphInst* g)
      : SdfComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    _input  = typedInputNamed<hm::MeshPlugTraits>("In");
  }
  hm::gpumesh_ptr_t srcMesh() const {
    auto out = std::dynamic_pointer_cast<hm::mesh_outpluginst_t>(_input->_connectedOutput);
    return out ? out->_value : nullptr;
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto e   = inst->_impl.getShared<hm::MeshEnv>();
    auto fxi = e->_ctx->FXI();
    _edges.init(e->_ctx);
    _params = fxi->createStorageBuffer(48);
    _bct    = fxi->createStorageBuffer(16);
    // ONE SHADER FILE PER KERNEL (dense descriptor sets — MoltenVK rejects sparse;
    // storage_interface binding indices are global per FILE in declaration order).
    auto sh_zero = fxi->shaderFromShaderText("sdf_m2s_zero", _m2s_zero_text());
    auto sh_fnrm = fxi->shaderFromShaderText("sdf_m2s_fnrm", _m2s_fnrm_text());
    auto sh_vpn  = fxi->shaderFromShaderText("sdf_m2s_vpn", _m2s_vpn_text());
    auto sh_epn  = fxi->shaderFromShaderText("sdf_m2s_epn", _m2s_epn_text());
    auto sh_dist = fxi->shaderFromShaderText("sdf_m2s_dist", _m2s_dist_text());
    _cs_zero   = fxi->computeShader(sh_zero, "cs_zerovpn");
    _cs_fnrm   = fxi->computeShader(sh_fnrm, "cs_fnrm");
    _cs_vpn    = fxi->computeShader(sh_vpn, "cs_vpn");
    _cs_epn    = fxi->computeShader(sh_epn, "cs_epn");
    _cs_dist   = fxi->computeShader(sh_dist, "cs_dist");
  }
  void writeParams(Context* ctx) final {
    auto in = srcMesh();
    if (not in or not in->_vidx or not in->channel(hm::MeshChannel::POSITION))
      return;
    auto e  = env();
    int dim = std::max(2, *(_d->typedInputNamed<dflow::IntPlugTraits>("dim")->_value));
    float ext = *(_d->typedInputNamed<dflow::FloatPlugTraits>("extent")->_value);
    fvec3 ctr = *(_d->typedInputNamed<dflow::Vec3fPlugTraits>("center")->_value);
    float pad = std::max(0.0f, *(_d->typedInputNamed<dflow::FloatPlugTraits>("pad")->_value));
    // AUTO bound (ext<=0): readback the source positions to bound the brick.
    // ORDERING: writeParams runs in the PRE-dispatch phase, BEFORE the producer's
    // compute on the first eval, so the first read can precede the geometry — we
    // re-read every eval (the live path re-reads each frame, the gate double-
    // evals) and CONVERGE, and NEVER emit a degenerate (zero) extent: a
    // placeholder keeps voxel != 0 until a real fit lands (else openvdb / the
    // kernel divide-by-voxel would choke). An explicit extent>0 skips all this.
    if (ext <= 0.0f) {
      int nv   = in->_num_verts;
      auto pch = in->channel(hm::MeshChannel::POSITION);
      if (nv > 0 and pch and pch->_ssbo) {
        auto fxi = e->_ctx->FXI();
        std::vector<float> P(size_t(nv) * 4);
        auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
        std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
        fxi->unmapStorageBuffer(m.get());
        fvec3 bmin(P[0], P[1], P[2]), bmax = bmin;
        for (int i = 1; i < nv; i++) {
          fvec3 q(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
          bmin = fvec3(std::min(bmin.x, q.x), std::min(bmin.y, q.y), std::min(bmin.z, q.z));
          bmax = fvec3(std::max(bmax.x, q.x), std::max(bmax.y, q.y), std::max(bmax.z, q.z));
        }
        fvec3 span  = bmax - bmin;
        float edge  = std::max(span.x, std::max(span.y, span.z));
        if (edge > 1.0e-5f) { // a real (computed) bound — not the pre-compute empty/zero buffer
          _fitExtent = edge * (1.0f + 2.0f * pad);
          _fitCenter = (bmin + bmax) * 0.5f;
        }
      }
      ext = (_fitExtent > 0.0f) ? _fitExtent : 1.0f; // placeholder until a valid fit converges
      ctr = (_fitExtent > 0.0f) ? _fitCenter : fvec3(0, 0, 0);
    }
    float voxel = ext / float(dim);
    size_t n    = size_t(dim) * dim * dim;
    if (hm::meshNextPow2(int(n)) != _brickCap) {
      _brick    = e->_pool->acquireChannel(4, int(n));
      _brickCap = hm::meshNextPow2(int(n));
    }
    int nf = in->_num_faces, nv = in->_num_verts, nc = in->_num_corners;
    if (hm::meshNextPow2(nf) != _fnCap) { _fnrm = e->_pool->acquireChannel(16, nf); _fnCap = hm::meshNextPow2(nf); }
    if (hm::meshNextPow2(nv) != _vpCap) { _vpn = e->_pool->acquireChannel(16, nv); _vpCap = hm::meshNextPow2(nv); }
    if (hm::meshNextPow2(nc) != _epCap) { _epn = e->_pool->acquireChannel(16, nc); _epCap = hm::meshNextPow2(nc); } // edges <= corners
    _edges.ensure(ctx, nc, nf);
    auto out        = _output->_value;
    out->_repr      = dflowgfx::SdfRepr::DENSE;
    out->_ssbo      = _brick->_ssbo;
    out->_dim[0] = out->_dim[1] = out->_dim[2] = dim;
    out->_origin[0] = ctr.x - ext * 0.5f + voxel * 0.5f;
    out->_origin[1] = ctr.y - ext * 0.5f + voxel * 0.5f;
    out->_origin[2] = ctr.z - ext * 0.5f + voxel * 0.5f;
    out->_voxel      = voxel;
    out->_background = ext;
    struct {
      uint32_t dx, dy, dz, count;
      float ox, oy, oz, voxel;
      uint32_t nf, nv, mode, pad;
    } P{uint32_t(dim), uint32_t(dim), uint32_t(dim), uint32_t(n),
        out->_origin[0], out->_origin[1], out->_origin[2], voxel,
        uint32_t(nf), uint32_t(nv),
        uint32_t(_d->_sign_mode < 0 ? 2 : (_d->_sign_mode & 1)), 0};
    auto fxi = ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
    uint32_t z4[4] = {0, 0, 0, 0}; // boundary-probe counter, zeroed pre-phase
    auto mb        = fxi->mapStorageBuffer(_bct, 0, 16, BufferMapAccess::WRITE_ONLY);
    std::memcpy(mb->_mappedaddr, z4, 16);
    fxi->unmapStorageBuffer(mb.get());
    _count = int(n);
    _nf = nf; _nv = nv;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = srcMesh();
    if (not in or not _brick or not in->_vidx or not in->channel(hm::MeshChannel::POSITION))
      return;
    auto e  = inst->_impl.getShared<hm::MeshEnv>();
    auto ci = e->_ctx->CI();
    _edges.build(e->_ctx, in); // topology-keyed internally; own barriers
    ci->storageBarrier();
    auto P  = in->channel(hm::MeshChannel::POSITION)->_ssbo;
    auto VI = in->_vidx->_ssbo;
    auto FO = in->_face_offsets->_ssbo;
    // zero vpn accumulators + face normals
    ci->bindStorageBuffer(_cs_zero, 0, _vpn->_ssbo);
    ci->bindStorageBuffer(_cs_zero, 1, _params);
    ci->dispatchCompute(_cs_zero, (_nv + 63) / 64, 1, 1);
    ci->bindStorageBuffer(_cs_fnrm, 0, P);
    ci->bindStorageBuffer(_cs_fnrm, 1, VI);
    ci->bindStorageBuffer(_cs_fnrm, 2, FO);
    ci->bindStorageBuffer(_cs_fnrm, 3, _fnrm->_ssbo);
    ci->bindStorageBuffer(_cs_fnrm, 4, _params);
    ci->dispatchCompute(_cs_fnrm, (_nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    // vertex + edge pseudonormals (vpn needs zeroed accum + face normals)
    ci->bindStorageBuffer(_cs_vpn, 0, P);
    ci->bindStorageBuffer(_cs_vpn, 1, VI);
    ci->bindStorageBuffer(_cs_vpn, 2, FO);
    ci->bindStorageBuffer(_cs_vpn, 3, _fnrm->_ssbo);
    ci->bindStorageBuffer(_cs_vpn, 4, _vpn->_ssbo);
    ci->bindStorageBuffer(_cs_vpn, 5, _params);
    ci->dispatchCompute(_cs_vpn, (_nf + 63) / 64, 1, 1);
    ci->bindStorageBuffer(_cs_epn, 0, _fnrm->_ssbo);
    ci->bindStorageBuffer(_cs_epn, 1, _epn->_ssbo);
    ci->bindStorageBuffer(_cs_epn, 2, _edges._edge);
    ci->bindStorageBuffer(_cs_epn, 3, _edges._ectl);
    ci->bindStorageBuffer(_cs_epn, 4, _bct);
    ci->dispatchCompute(_cs_epn, (in->_num_corners + 63) / 64, 1, 1); // edge cap <= corners; kernel early-outs on e_ne
    ci->storageBarrier();
    // the distance + sign kernel
    ci->bindStorageBuffer(_cs_dist, 0, P);
    ci->bindStorageBuffer(_cs_dist, 1, VI);
    ci->bindStorageBuffer(_cs_dist, 2, FO);
    ci->bindStorageBuffer(_cs_dist, 3, _fnrm->_ssbo);
    ci->bindStorageBuffer(_cs_dist, 4, _vpn->_ssbo);
    ci->bindStorageBuffer(_cs_dist, 5, _epn->_ssbo);
    ci->bindStorageBuffer(_cs_dist, 6, _edges._corner_edge);
    ci->bindStorageBuffer(_cs_dist, 7, _bct);
    ci->bindStorageBuffer(_cs_dist, 8, _params);
    ci->bindStorageBuffer(_cs_dist, 9, _brick->_ssbo);
    ci->dispatchCompute(_cs_dist, (_count + 63) / 64, 1, 1);
    ci->storageBarrier();
    _output->_value->markChanged();
  }
  const MeshToSdfData* _d;
  dflowgfx::sdfgrid_outpluginst_ptr_t _output;
  hm::mesh_inpluginst_ptr_t _input;
  hm::MeshEdges _edges;
  hm::gpuchannel_ptr_t _brick, _fnrm, _vpn, _epn;
  FxShaderStorageBuffer *_params = nullptr, *_bct = nullptr;
  const FxComputeShader *_cs_zero = nullptr, *_cs_fnrm = nullptr, *_cs_vpn = nullptr, *_cs_epn = nullptr,
                        *_cs_dist = nullptr;
  int _brickCap = -1, _fnCap = -1, _vpCap = -1, _epCap = -1;
  int _count = 0, _nf = 0, _nv = 0;
  uint64_t _fitSrcV = ~0ull;
  float _fitExtent  = -1.0f;
  fvec3 _fitCenter;
};

static void _reshapeMeshToSdfIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<hm::MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "dim")->setValue(48);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "extent")->setValue(-1.0f); // <=0 AUTO
  dflow::ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "center")->setValue(fvec3(0, 0, 0));
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "pad")->setValue(0.12f);
}
std::shared_ptr<MeshToSdfData> MeshToSdfData::createShared() {
  auto d = std::make_shared<MeshToSdfData>();
  _reshapeMeshToSdfIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t MeshToSdfData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MeshToSdfInst>(this, g);
}
void MeshToSdfData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MeshToSdfData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeMeshToSdfIOs(m); });
  clazz->directProperty("sign_mode", &MeshToSdfData::_sign_mode);
}

} // namespace ork::lev2::sdf
