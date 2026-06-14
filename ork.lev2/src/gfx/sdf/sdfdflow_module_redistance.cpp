////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"

ImplementReflectionX(ork::lev2::sdf::RedistanceData, "sdf::RedistanceData");

namespace ork::lev2::sdf {

///////////////////////////////////////////////////////////////////////////////
// Redistance (M4a) — see sdfdflow.h. GPU eikonal redistance by JUMP FLOODING:
// turn ANY signed brick into a TRUE |grad phi|=1 field while PRESERVING the
// zero-set (sign copied verbatim from the input). Needed because the dense brick
// is the field every M4b consumer samples, and CSG (esp. smooth_union) is not a
// true SDF away from the zero-set.
//
//   init  : near-surface voxels (|phi| <= band*voxel) Newton-project onto the
//           iso-surface (sub-voxel closest point CP = p - phi*gradphi/|gradphi|^2,
//           world-space central-diff gradient); far voxels = INVALID seed (w=0).
//   step  : one JFA pass at `stride` — keep the neighbor CP nearest THIS voxel.
//           ping-pong CP_a/CP_b (host swaps the bind each pass). ceil(log2(N)) passes.
//   adv   : GPU-halve `stride` between steps (no host write mid-dispatch-phase).
//   final : d = ||p - CP||; OUT = sign(phi_in)*d  (zero-set byte-preserved).
//
// Each kernel is its OWN dense-binding shader FILE (Metal rejects sparse sets).
// Frame inherited from the input grid (the Csg-inherits-A pattern).
///////////////////////////////////////////////////////////////////////////////

// init: IN(0) CP(1) par(2)
static const char* _jfa_init_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface jif_in  (descriptor_set 0) { buffer layout(std430) jib { float INd[]; }; }
storage_interface jif_cp  (descriptor_set 0) { buffer layout(std430) jcb { vec4 CP[]; }; }
storage_interface jif_par (descriptor_set 0) { buffer layout(std430) jpb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  uint p_stride; uint p_u0; uint p_u1; uint p_u2;
  float p_band; float p_bg; float p_f0; float p_f1; }; }
compute_interface iface_init { storage { jif_in jif_cp jif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_jfa_init : iface_init {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  float phi  = INd[i];
  vec3 mypos = vec3(p_ox, p_oy, p_oz) + vec3(float(ix), float(iy), float(iz)) * p_voxel;
  uint sx = 1u; uint sy = p_dimx; uint sz = p_dimx * p_dimy;
  // GRADIENT-FREE seed: the nearest zero-crossing on the 6 incident grid edges (t = phi/(phi-phi_nbr),
  // the sub-voxel point where phi=0 along the edge). Robust at union/CSG SEAMS, where the gradient
  // CREASES and a Newton projection (p - phi*grad/|grad|^2) lands on the wrong lobe. Only voxels
  // ADJACENT to the surface (a sign change across an edge) seed; JFA propagates to the rest.
  float bestd = 1.0e30; vec3 bestcp = vec3(0.0); float fnd = 0.0; bool pneg = (phi < 0.0);
  if (ix + 1u < p_dimx) { float pj = INd[i + sx]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos + vec3(t * p_voxel, 0.0, 0.0); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  if (ix > 0u)          { float pj = INd[i - sx]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos - vec3(t * p_voxel, 0.0, 0.0); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  if (iy + 1u < p_dimy) { float pj = INd[i + sy]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos + vec3(0.0, t * p_voxel, 0.0); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  if (iy > 0u)          { float pj = INd[i - sy]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos - vec3(0.0, t * p_voxel, 0.0); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  if (iz + 1u < p_dimz) { float pj = INd[i + sz]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos + vec3(0.0, 0.0, t * p_voxel); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  if (iz > 0u)          { float pj = INd[i - sz]; if ((pj < 0.0) != pneg) { float t = phi / (phi - pj); vec3 cp = mypos - vec3(0.0, 0.0, t * p_voxel); float d = dot(cp - mypos, cp - mypos); if (d < bestd) { bestd = d; bestcp = cp; fnd = 1.0; } } }
  CP[i] = vec4(bestcp, fnd);
}
)S";
}

// step: CPS(0)=src CPD(1)=dst par(2)
static const char* _jfa_step_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface jsf_cps (descriptor_set 0) { buffer layout(std430) jspb { vec4 CPS[]; }; }
storage_interface jsf_cpd (descriptor_set 0) { buffer layout(std430) jdpb { vec4 CPD[]; }; }
storage_interface jsf_par (descriptor_set 0) { buffer layout(std430) jppb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  uint p_stride; uint p_u0; uint p_u1; uint p_u2;
  float p_band; float p_bg; float p_f0; float p_f1; }; }
compute_interface iface_step { storage { jsf_cps jsf_cpd jsf_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_jfa_step : iface_step {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  vec3 mypos = vec3(p_ox, p_oy, p_oz) + vec3(float(ix), float(iy), float(iz)) * p_voxel;
  uint sx = 1u; uint sy = p_dimx; uint sz = p_dimx * p_dimy;
  vec4 best   = CPS[i];
  float bestd = (best.w > 0.0) ? dot(mypos - best.xyz, mypos - best.xyz) : 3.0e38;
  int st = int(p_stride);
  for (int dz = -1; dz <= 1; dz = dz + 1) {
    int nz = int(iz) + dz * st;
    bool zok = (nz >= 0) && (nz < int(p_dimz));
    for (int dy = -1; dy <= 1; dy = dy + 1) {
      int ny = int(iy) + dy * st;
      bool yok = (ny >= 0) && (ny < int(p_dimy));
      for (int dx = -1; dx <= 1; dx = dx + 1) {
        int nx = int(ix) + dx * st;
        bool xok = (nx >= 0) && (nx < int(p_dimx));
        if (zok && yok && xok) {
          uint ni = uint(nx) * sx + uint(ny) * sy + uint(nz) * sz;
          vec4 cand = CPS[ni];
          if (cand.w > 0.0) {
            float d = dot(mypos - cand.xyz, mypos - cand.xyz);
            if (d < bestd) { bestd = d; best = cand; }
          }
        }
      }
    }
  }
  CPD[i] = best;
}
)S";
}

// advance: par(0). GPU-halve the stride (the MeshSort cs_incpass idiom — no host write mid-phase).
static const char* _jfa_adv_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface jaf_par (descriptor_set 0) { buffer layout(std430) japb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  uint p_stride; uint p_u0; uint p_u1; uint p_u2;
  float p_band; float p_bg; float p_f0; float p_f1; }; }
compute_interface iface_adv { storage { jaf_par } inputs { layout(local_size_x = 1); } }
compute_shader cs_jfa_adv : iface_adv {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  p_stride = max(p_stride >> 1u, 1u);
}
)S";
}

// final: IN(0) CP(1) OUT(2) par(3)
static const char* _jfa_final_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface jff_in  (descriptor_set 0) { buffer layout(std430) jfib { float INd[]; }; }
storage_interface jff_cp  (descriptor_set 0) { buffer layout(std430) jfcb { vec4 CP[]; }; }
storage_interface jff_out (descriptor_set 0) { buffer layout(std430) jfob { float OUTd[]; }; }
storage_interface jff_par (descriptor_set 0) { buffer layout(std430) jfpb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  uint p_stride; uint p_u0; uint p_u1; uint p_u2;
  float p_band; float p_bg; float p_f0; float p_f1; }; }
compute_interface iface_final { storage { jff_in jff_cp jff_out jff_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_jfa_final : iface_final {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  vec3 mypos = vec3(p_ox, p_oy, p_oz) + vec3(float(ix), float(iy), float(iz)) * p_voxel;
  vec4 cp = CP[i];
  float d = (cp.w > 0.0) ? length(mypos - cp.xyz) : abs(p_bg);  // no seed reached -> far/background
  float s = (INd[i] < 0.0) ? -1.0 : 1.0;                        // zero-set preserved EXACTLY
  OUTd[i] = s * d;
}
)S";
}

struct RedistanceInst : public SdfComputeInst {
  RedistanceInst(const RedistanceData* d, dflow::GraphInst* g)
      : SdfComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    _in     = typedInputNamed<dflowgfx::SdfGridPlugTraits>("In");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto e   = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto fxi = e->_ctx->FXI();
    _params  = fxi->createStorageBuffer(64);
    _cs_init  = fxi->computeShader(fxi->shaderFromShaderText("sdf_jfa_init",  _jfa_init_text()),  "cs_jfa_init");
    _cs_step  = fxi->computeShader(fxi->shaderFromShaderText("sdf_jfa_step",  _jfa_step_text()),  "cs_jfa_step");
    _cs_adv   = fxi->computeShader(fxi->shaderFromShaderText("sdf_jfa_adv",   _jfa_adv_text()),   "cs_jfa_adv");
    _cs_final = fxi->computeShader(fxi->shaderFromShaderText("sdf_jfa_final", _jfa_final_text()), "cs_jfa_final");
  }
  void writeParams(Context* ctx) final {
    auto src = _srcGrid(_in);
    if (not src or not src->_ssbo)
      return;
    auto e   = env();
    int dim  = src->_dim[0];
    size_t n = size_t(src->_dim[0]) * src->_dim[1] * src->_dim[2];
    if (hypermesh::meshNextPow2(int(n)) != _cap) { // pow2 size-class change -> re-acquire all three
      _brick = e->_pool->acquireChannel(4, int(n));   // OUT (float)
      _cpa   = e->_pool->acquireChannel(16, int(n));  // CP ping (vec4)
      _cpb   = e->_pool->acquireChannel(16, int(n));  // CP pong (vec4)
      _cap   = hypermesh::meshNextPow2(int(n));
    }
    auto out         = _output->_value;
    out->_repr       = dflowgfx::SdfRepr::DENSE;
    out->_ssbo       = _brick->_ssbo;
    out->_dim[0]     = src->_dim[0];
    out->_dim[1]     = src->_dim[1];
    out->_dim[2]     = src->_dim[2];
    out->_origin[0]  = src->_origin[0];
    out->_origin[1]  = src->_origin[1];
    out->_origin[2]  = src->_origin[2];
    out->_voxel      = src->_voxel;
    out->_background = src->_background;
    // JFA pass schedule: ceil(log2(N)) steps, first stride = N/2 (N = next pow2 of the linear dim).
    int N = hypermesh::meshNextPow2(dim);
    int passes = 0;
    while ((1 << passes) < N)
      passes++;                       // ceil(log2 N)
    if (_d->_max_iterations > 0)
      passes = _d->_max_iterations;
    _passes = std::max(1, passes);
    uint32_t first_stride = uint32_t(std::max(1, N / 2));
    struct {
      uint32_t dimx, dimy, dimz, count;
      float ox, oy, oz, voxel;
      uint32_t stride, u0, u1, u2;
      float band, bg, f0, f1;
    } P{uint32_t(src->_dim[0]), uint32_t(src->_dim[1]), uint32_t(src->_dim[2]), uint32_t(n),
        src->_origin[0], src->_origin[1], src->_origin[2], src->_voxel,
        first_stride, 0u, 0u, 0u,
        2.0f, src->_background, 0.0f, 0.0f}; // band = 2 voxels of trusted near-surface seeds
    auto fxi = ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
    _count = int(n);
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto src = _srcGrid(_in);
    if (not src or not src->_ssbo or not _brick)
      return;
    auto e  = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto ci = e->_ctx->CI();
    int groups = (_count + 63) / 64;
    // init: seed CP_a from the input field's near-surface band
    ci->bindStorageBuffer(_cs_init, 0, src->_ssbo);
    ci->bindStorageBuffer(_cs_init, 1, _cpa->_ssbo);
    ci->bindStorageBuffer(_cs_init, 2, _params);
    ci->dispatchCompute(_cs_init, groups, 1, 1);
    ci->storageBarrier();
    // JFA steps — ping-pong CP_src/CP_dst by swapping the bind each pass; halve stride on the GPU.
    auto* csrc = _cpa.get();
    auto* cdst = _cpb.get();
    for (int pass = 0; pass < _passes; pass++) {
      ci->bindStorageBuffer(_cs_step, 0, csrc->_ssbo);
      ci->bindStorageBuffer(_cs_step, 1, cdst->_ssbo);
      ci->bindStorageBuffer(_cs_step, 2, _params);
      ci->dispatchCompute(_cs_step, groups, 1, 1);
      ci->storageBarrier();
      ci->bindStorageBuffer(_cs_adv, 0, _params);
      ci->dispatchCompute(_cs_adv, 1, 1, 1);
      ci->storageBarrier();
      auto* tmp = csrc; csrc = cdst; cdst = tmp; // after the swap, csrc holds the just-written CP
    }
    // finalize: signed distance to the nearest CP (csrc = the final ping-pong result)
    ci->bindStorageBuffer(_cs_final, 0, src->_ssbo);
    ci->bindStorageBuffer(_cs_final, 1, csrc->_ssbo);
    ci->bindStorageBuffer(_cs_final, 2, _brick->_ssbo);
    ci->bindStorageBuffer(_cs_final, 3, _params);
    ci->dispatchCompute(_cs_final, groups, 1, 1);
    ci->storageBarrier();
    _output->_value->markChanged();
  }
  const RedistanceData* _d;
  dflowgfx::sdfgrid_outpluginst_ptr_t _output;
  dflowgfx::sdfgrid_inpluginst_ptr_t _in;
  hypermesh::gpuchannel_ptr_t _brick, _cpa, _cpb; // OUT + CP ping-pong (pool refcount holders)
  FxShaderStorageBuffer* _params = nullptr;
  const FxComputeShader *_cs_init = nullptr, *_cs_step = nullptr, *_cs_adv = nullptr, *_cs_final = nullptr;
  int _cap = -1, _count = 0, _passes = 1;
};

static void _reshapeRedistanceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
std::shared_ptr<RedistanceData> RedistanceData::createShared() {
  auto d = std::make_shared<RedistanceData>();
  _reshapeRedistanceIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t RedistanceData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RedistanceInst>(this, g);
}
void RedistanceData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RedistanceData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeRedistanceIOs(m); });
  clazz->directProperty("max_iterations", &RedistanceData::_max_iterations);
}

} // namespace ork::lev2::sdf
