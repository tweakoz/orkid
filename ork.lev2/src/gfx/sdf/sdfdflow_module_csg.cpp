////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"

ImplementReflectionX(ork::lev2::sdf::CsgData, "sdf::CsgData");

namespace ork::lev2::sdf {

///////////////////////////////////////////////////////////////////////////////
// Csg (M2) — see sdfdflow.h. Output rides A's frame; B is sampled TRILINEARLY at
// A's world positions (frames need not match). op/k are runtime (params SSBO).
///////////////////////////////////////////////////////////////////////////////

static const char* _csg_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface cif_a   (descriptor_set 0) { buffer layout(std430) cab { float A[]; }; }
storage_interface cif_b   (descriptor_set 0) { buffer layout(std430) cbb { float B[]; }; }
storage_interface cif_out (descriptor_set 0) { buffer layout(std430) cob { float OUTd[]; }; }
storage_interface cif_par (descriptor_set 0) { buffer layout(std430) cpb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ax; float p_ay; float p_az; float p_avox;
  uint p_bdimx; uint p_bdimy; uint p_bdimz; uint p_op;
  float p_bx; float p_by; float p_bz; float p_bvox;
  float p_k; float p_bbg; float p_pad0; float p_pad1; }; }
compute_interface iface_csg { storage { cif_a cif_b cif_out cif_par } inputs { layout(local_size_x = 64); } }
compute_shader cs_csg : iface_csg {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  vec3 p = vec3(p_ax, p_ay, p_az) + vec3(float(ix), float(iy), float(iz)) * p_avox;
  float dA = A[i];
  // trilinear sample of B at world p (B's index space); outside -> B background
  vec3 bc = (p - vec3(p_bx, p_by, p_bz)) / p_bvox;
  float dB = p_bbg;
  if (bc.x >= 0.0 && bc.y >= 0.0 && bc.z >= 0.0 &&
      bc.x <= float(p_bdimx - 1u) && bc.y <= float(p_bdimy - 1u) && bc.z <= float(p_bdimz - 1u)) {
    uint x0 = uint(floor(bc.x)); uint y0 = uint(floor(bc.y)); uint z0 = uint(floor(bc.z));
    uint x1 = min(x0 + 1u, p_bdimx - 1u);
    uint y1 = min(y0 + 1u, p_bdimy - 1u);
    uint z1 = min(z0 + 1u, p_bdimz - 1u);
    vec3 f = bc - vec3(float(x0), float(y0), float(z0));
    uint sx = 1u; uint sy = p_bdimx; uint sz = p_bdimx * p_bdimy;
    float c000 = B[x0*sx + y0*sy + z0*sz];
    float c100 = B[x1*sx + y0*sy + z0*sz];
    float c010 = B[x0*sx + y1*sy + z0*sz];
    float c110 = B[x1*sx + y1*sy + z0*sz];
    float c001 = B[x0*sx + y0*sy + z1*sz];
    float c101 = B[x1*sx + y0*sy + z1*sz];
    float c011 = B[x0*sx + y1*sy + z1*sz];
    float c111 = B[x1*sx + y1*sy + z1*sz];
    float c00 = mix(c000, c100, f.x);
    float c10 = mix(c010, c110, f.x);
    float c01 = mix(c001, c101, f.x);
    float c11 = mix(c011, c111, f.x);
    dB = mix(mix(c00, c10, f.y), mix(c01, c11, f.y), f.z);
  }
  float r = dA;
  if (p_op == 0u) { r = min(dA, dB); }                 // union
  else if (p_op == 1u) { r = max(dA, dB); }            // intersect
  else if (p_op == 2u) { r = max(dA, -dB); }           // subtract (A minus B)
  else {                                               // smooth-union (iq polynomial smin)
    float k = max(p_k, 1.0e-5);
    float h = clamp(0.5 + 0.5 * (dB - dA) / k, 0.0, 1.0);
    r = mix(dB, dA, h) - k * h * (1.0 - h);
  }
  OUTd[i] = r;
}
)S";
}

struct CsgInst : public SdfComputeInst {
  CsgInst(const CsgData* d, dflow::GraphInst* g)
      : SdfComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    _inA    = typedInputNamed<dflowgfx::SdfGridPlugTraits>("A");
    _inB    = typedInputNamed<dflowgfx::SdfGridPlugTraits>("B");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto e   = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto fxi = e->_ctx->FXI();
    _params  = fxi->createStorageBuffer(80);
    auto sh  = fxi->shaderFromShaderText("sdf_csg", _csg_text());
    _cs      = fxi->computeShader(sh, "cs_csg");
  }
  void writeParams(Context* ctx) final {
    auto A = _srcGrid(_inA);
    auto B = _srcGrid(_inB);
    if (not A or not A->_ssbo)
      return;
    auto e   = env();
    int dim  = A->_dim[0];
    size_t n = size_t(A->_dim[0]) * A->_dim[1] * A->_dim[2];
    if (hypermesh::meshNextPow2(int(n)) != _cap) {
      _brick = e->_pool->acquireChannel(4, int(n));
      _cap   = hypermesh::meshNextPow2(int(n));
    }
    auto out        = _output->_value;
    out->_repr      = dflowgfx::SdfRepr::DENSE;
    out->_ssbo      = _brick->_ssbo;
    out->_dim[0]    = A->_dim[0];
    out->_dim[1]    = A->_dim[1];
    out->_dim[2]    = A->_dim[2];
    out->_origin[0] = A->_origin[0];
    out->_origin[1] = A->_origin[1];
    out->_origin[2] = A->_origin[2];
    out->_voxel      = A->_voxel;
    out->_background = A->_background;
    // B frame (defaults to A's if B absent -> the op degenerates to A op A, harmless)
    int bdx = B ? B->_dim[0] : A->_dim[0];
    int bdy = B ? B->_dim[1] : A->_dim[1];
    int bdz = B ? B->_dim[2] : A->_dim[2];
    struct {
      uint32_t dimx, dimy, dimz, count;
      float ax, ay, az, avox;
      uint32_t bdimx, bdimy, bdimz, op;
      float bx, by, bz, bvox;
      float k, bbg, pad0, pad1;
    } P{uint32_t(A->_dim[0]), uint32_t(A->_dim[1]), uint32_t(A->_dim[2]), uint32_t(n),
        A->_origin[0], A->_origin[1], A->_origin[2], A->_voxel,
        uint32_t(bdx), uint32_t(bdy), uint32_t(bdz), uint32_t(_d->_op),
        B ? B->_origin[0] : A->_origin[0], B ? B->_origin[1] : A->_origin[1], B ? B->_origin[2] : A->_origin[2],
        B ? B->_voxel : A->_voxel,
        std::max(1e-5f, *(_d->typedInputNamed<dflow::FloatPlugTraits>("k")->_value)),
        B ? B->_background : A->_background, 0.0f, 0.0f};
    auto fxi = ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
    _count = int(n);
    (void)dim;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto A = _srcGrid(_inA);
    auto B = _srcGrid(_inB);
    if (not A or not A->_ssbo or not _brick)
      return;
    auto e  = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto ci = e->_ctx->CI();
    ci->bindStorageBuffer(_cs, 0, A->_ssbo);
    ci->bindStorageBuffer(_cs, 1, B and B->_ssbo ? B->_ssbo : A->_ssbo); // B absent -> bind A (op vs self)
    ci->bindStorageBuffer(_cs, 2, _brick->_ssbo);
    ci->bindStorageBuffer(_cs, 3, _params);
    ci->dispatchCompute(_cs, (_count + 63) / 64, 1, 1);
    ci->storageBarrier();
    _output->_value->markChanged();
  }
  const CsgData* _d;
  dflowgfx::sdfgrid_outpluginst_ptr_t _output;
  dflowgfx::sdfgrid_inpluginst_ptr_t _inA, _inB;
  hypermesh::gpuchannel_ptr_t _brick;
  FxShaderStorageBuffer* _params = nullptr;
  const FxComputeShader* _cs     = nullptr;
  int _cap = -1, _count = 0;
};

static void _reshapeCsgIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "A");
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "B");
  dflow::ModuleData::createOutputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "k")->setValue(0.25f);
}
std::shared_ptr<CsgData> CsgData::createShared() {
  auto d = std::make_shared<CsgData>();
  _reshapeCsgIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t CsgData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<CsgInst>(this, g);
}
void CsgData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CsgData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeCsgIOs(m); });
  clazz->directProperty("op", &CsgData::_op);
}

} // namespace ork::lev2::sdf
