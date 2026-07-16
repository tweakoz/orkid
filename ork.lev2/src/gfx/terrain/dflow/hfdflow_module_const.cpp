////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::ConstModuleData, "terrain::ConstModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// ConstModule — Out = level. 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

// DIM is RUNTIME data (params SSBO p_dimf) — dim changes never rebuild the shader;
// the output array is runtime-sized. Only `level` stays baked into the text.
static std::string _const_text(float level) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_pm (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; }; }
compute_interface iface { storage { sif_out sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_const : iface {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint i = gl_GlobalInvocationID.y * u_dim + gl_GlobalInvocationID.x;
  odata[i] = float(%LEVEL%);
}
)S";
  _shadersub(t, "%LEVEL%", FormatString("%f", level));
  return t;
}

struct ConstModuleInst : public TerrainComputeInst {
  ConstModuleInst(const ConstModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _level  = _floatPlug(this, _d, "level");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    auto sh = fxi->shaderFromShaderText("terrain_const", _const_text(_level->value()));
    _cs     = fxi->computeShader(sh, "cs_const");
    _pm        = env->createStorageBuffer(sizeof(float)); // p_dimf = RUNTIME grid dim
    float dimf = float(env->_w);
    auto mp    = fxi->mapStorageBuffer(_pm, 0, sizeof(dimf), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, &dimf, sizeof(dimf));
    fxi->unmapStorageBuffer(mp.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    int g    = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _pm);
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.const.v1");
    h->accumulateItem<float>(_level->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ConstModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _level;
  FxShaderStorageBuffer* _pm = nullptr;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeConstIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "level")->setValue(0.5f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ConstModuleData::ConstModuleData() {}
std::shared_ptr<ConstModuleData> ConstModuleData::createShared() {
  auto d = std::make_shared<ConstModuleData>(); _reshapeConstIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t ConstModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ConstModuleInst>(this, g);
}
void ConstModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ConstModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeConstIOs(m); });
}

} // namespace ork::lev2::terrain
