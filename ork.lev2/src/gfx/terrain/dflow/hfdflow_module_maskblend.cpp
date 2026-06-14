////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::MaskBlendModuleData, "terrain::MaskBlendModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// MaskBlendModule — Out = mix(A, B, M). 4 SSBOs (out=0, a=1, b=2, m=3). The
// masking primitive: B replaces A where the [0,1] mask field M is high.
///////////////////////////////////////////////////////////////////////////////

static std::string _maskblend_text(int dim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_a   (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }
storage_interface sif_b   (descriptor_set 0) { buffer layout(std430) bb { float bdata[%DIMSQ%]; }; }
storage_interface sif_m   (descriptor_set 0) { buffer layout(std430) mb { float mdata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_a sif_b sif_m } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_maskblend : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  odata[i] = mix(adata[i], bdata[i], clamp(mdata[i], 0.0, 1.0));
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  return t;
}

struct MaskBlendModuleInst : public TerrainComputeInst {
  MaskBlendModuleInst(const MaskBlendModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _inA = typedInputNamed<HfImagePlugTraits>("A");
    _inB = typedInputNamed<HfImagePlugTraits>("B");
    _inM = typedInputNamed<HfImagePlugTraits>("M");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_maskblend", _maskblend_text(env->_w));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_maskblend");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto a   = _srcImg(_inA);
    auto b   = _srcImg(_inB);
    auto m   = _srcImg(_inM);
    OrkAssert(a && a->_ssbo && b && b->_ssbo && m && m->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, a->_ssbo);               // adata
    ci->bindStorageBuffer(_cs, 2, b->_ssbo);               // bdata
    ci->bindStorageBuffer(_cs, 3, m->_ssbo);               // mdata
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.maskblend.v1");
    _mixTail(h, ctx, ih); // identity is fully determined by the 3 input hashes
    h->finish();
    return h->result();
  }

  const MaskBlendModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _inA, _inB, _inM;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeMaskBlendIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "A");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "B");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "M");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
MaskBlendModuleData::MaskBlendModuleData() {}
std::shared_ptr<MaskBlendModuleData> MaskBlendModuleData::createShared() {
  auto d = std::make_shared<MaskBlendModuleData>(); _reshapeMaskBlendIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t MaskBlendModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MaskBlendModuleInst>(this, g);
}
void MaskBlendModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MaskBlendModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeMaskBlendIOs(m); });
}

} // namespace ork::lev2::terrain
