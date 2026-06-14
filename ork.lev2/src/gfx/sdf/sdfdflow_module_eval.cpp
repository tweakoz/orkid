////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"

ImplementReflectionX(ork::lev2::sdf::SdfEvalData, "sdf::SdfEvalData");

namespace ork::lev2::sdf {

///////////////////////////////////////////////////////////////////////////////
// SdfEval — see sdfdflow.h. The authored GLSL distance expression (vec3 p ->
// float) BAKES into the kernel (the Select-predicate pattern); dim/extent/
// center are RUNTIME plugs re-read every eval (params SSBO — pokes are live,
// a dim change reallocs the brick from the pow2 pool, no recompile).
///////////////////////////////////////////////////////////////////////////////

static const char* _sdfeval_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_sdf (descriptor_set 0) { buffer layout(std430) sdfb { float SDF[]; }; }
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) prb {
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_count;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  float p_offx; float p_offy; float p_offz; float p_pad; }; }
compute_interface iface { storage { sif_sdf sif_par }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_sdfeval : iface {
  uint i = gl_GlobalInvocationID.x;
  if (i >= p_count) { return; }
  uint ix = i % p_dimx;
  uint iy = (i / p_dimx) % p_dimy;
  uint iz = i / (p_dimx * p_dimy);
  // `offset` (a runtime plug) TRANSLATES the shape: the expression at p=0 appears
  // at world `offset`. Animate it to move an analytic SDF without recompiling.
  vec3 p = vec3(p_ox, p_oy, p_oz) + vec3(float(ix), float(iy), float(iz)) * p_voxel - vec3(p_offx, p_offy, p_offz);
  float d = (%EXPR%);
  SDF[i] = d;
}
)S";
}

struct SdfEvalInst : public SdfComputeInst {
  SdfEvalInst(const SdfEvalData* d, dflow::GraphInst* g)
      : SdfComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto e   = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto fxi = e->_ctx->FXI();
    _params  = fxi->createStorageBuffer(48); // dim xyz + count + origin xyz + voxel + offset xyz + pad
    std::string text = _sdfeval_text();
    _shadersub(text, "%EXPR%", _d->_expression);
    auto sh = fxi->shaderFromShaderText("sdf_eval", text);
    _cs     = fxi->computeShader(sh, "cs_sdfeval");
  }
  void writeParams(Context* ctx) final {
    auto e      = env();
    int dim     = std::max(2, *(_d->typedInputNamed<dflow::IntPlugTraits>("dim")->_value));
    float ext   = std::max(1e-4f, *(_d->typedInputNamed<dflow::FloatPlugTraits>("extent")->_value));
    fvec3 ctr   = *(_d->typedInputNamed<dflow::Vec3fPlugTraits>("center")->_value);
    fvec3 off   = *(_d->typedInputNamed<dflow::Vec3fPlugTraits>("offset")->_value);
    float voxel = ext / float(dim);
    size_t n    = size_t(dim) * dim * dim;
    if (hypermesh::meshNextPow2(int(n)) != _cap) { // pow2 size-class change -> re-acquire
      _brick = e->_pool->acquireChannel(4, int(n));
      _cap   = hypermesh::meshNextPow2(int(n));
    }
    auto out        = _output->_value;
    out->_repr      = dflowgfx::SdfRepr::DENSE;
    out->_ssbo      = _brick->_ssbo;
    out->_dim[0] = out->_dim[1] = out->_dim[2] = dim;
    // voxel CENTER convention: voxel (0,0,0) sits half a voxel inside the cube's min corner
    out->_origin[0] = ctr.x - ext * 0.5f + voxel * 0.5f;
    out->_origin[1] = ctr.y - ext * 0.5f + voxel * 0.5f;
    out->_origin[2] = ctr.z - ext * 0.5f + voxel * 0.5f;
    out->_voxel     = voxel;
    struct {
      uint32_t dx, dy, dz, count;
      float ox, oy, oz, voxel;
      float offx, offy, offz, pad;
    } P{uint32_t(dim), uint32_t(dim), uint32_t(dim), uint32_t(n),
        out->_origin[0], out->_origin[1], out->_origin[2], voxel,
        off.x, off.y, off.z, 0.0f};
    auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &P, sizeof(P));
    ctx->FXI()->unmapStorageBuffer(m.get());
    _count = int(n);
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto e  = inst->_impl.getShared<hypermesh::MeshEnv>();
    auto ci = e->_ctx->CI();
    ci->bindStorageBuffer(_cs, 0, _brick->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _params);
    ci->dispatchCompute(_cs, (_count + 63) / 64, 1, 1);
    ci->storageBarrier();
    _output->_value->markChanged();
  }
  const SdfEvalData* _d;
  dflowgfx::sdfgrid_outpluginst_ptr_t _output;
  hypermesh::gpuchannel_ptr_t _brick; // pool refcount holder; the plug carries the raw ssbo
  FxShaderStorageBuffer* _params = nullptr;
  const FxComputeShader* _cs     = nullptr;
  int _cap = -1, _count = 0;
};

static void _reshapeSdfEvalIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createOutputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "dim")->setValue(64);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "extent")->setValue(4.0f);
  dflow::ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "center")->setValue(fvec3(0, 0, 0));
  dflow::ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "offset")->setValue(fvec3(0, 0, 0)); // runtime shape translate
}
std::shared_ptr<SdfEvalData> SdfEvalData::createShared() {
  auto d = std::make_shared<SdfEvalData>();
  _reshapeSdfEvalIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SdfEvalData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SdfEvalInst>(this, g);
}
void SdfEvalData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SdfEvalData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSdfEvalIOs(m); });
  clazz->directProperty("expression", &SdfEvalData::_expression);
}

} // namespace ork::lev2::sdf
