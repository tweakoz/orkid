////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::TerraceModuleData, "terrain::TerraceModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// TerraceModule — quantize to `steps` plateaus with a `sharpness` riser. 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

static std::string _terrace_text(int dim, float steps, float sharp) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_in } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_terrace : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i  = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  float v = idata[i];
  float s = v * float(%STEPS%);
  float fl = floor(s);
  float fr = s - fl;
  float w  = max((1.0 - float(%SHARP%)) * 0.5, 0.001);
  float k  = smoothstep(0.5 - w, 0.5 + w, fr);
  odata[i] = (fl + k) / float(%STEPS%);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%STEPS%", FormatString("%f", steps));
  _shadersub(t, "%SHARP%", FormatString("%f", sharp));
  return t;
}

struct TerraceModuleInst : public TerrainComputeInst {
  TerraceModuleInst(const TerraceModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _steps  = _floatPlug(this, _d, "steps");
    _sharp  = _floatPlug(this, _d, "sharpness");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_terrace", _terrace_text(env->_w, _steps->value(), _sharp->value()));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_terrace");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.terrace.v1");
    h->accumulateItem<float>(_steps->value());
    h->accumulateItem<float>(_sharp->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const TerraceModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _steps, _sharp;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeTerraceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "steps")->setValue(4.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "sharpness")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
TerraceModuleData::TerraceModuleData() {}
std::shared_ptr<TerraceModuleData> TerraceModuleData::createShared() {
  auto d = std::make_shared<TerraceModuleData>(); _reshapeTerraceIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t TerraceModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<TerraceModuleInst>(this, g);
}
void TerraceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TerraceModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeTerraceIOs(m); });
}

} // namespace ork::lev2::terrain
