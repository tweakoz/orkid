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
// TerraceModule — quantize to plateaus `step_m` METERS apart with a `sharpness` riser. 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

// DIM and BLEND are RUNTIME data (params SSBO, binding 2) — dim/blend changes never
// rebuild the shader; the field arrays are runtime-sized. `step_m`/`sharpness` stay baked.
// `blend` crossfades the terraced result vs the ORIGINAL input (the lpf idiom): 0 =
// passthrough, 1 (default) = full op — mix(x,y,1.0) is bit-exact y for finite inputs.
static std::string _terrace_text(float step_m, float sharp) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }
storage_interface sif_pm  (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; float p_blend; }; }
compute_interface iface { storage { sif_out sif_in sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_terrace : iface {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint i  = gl_GlobalInvocationID.y * u_dim + gl_GlobalInvocationID.x;
  float v = idata[i];
  float s = v / float(%STEPM%);
  float fl = floor(s);
  float fr = s - fl;
  float w  = max((1.0 - float(%SHARP%)) * 0.5, 0.001);
  float k  = smoothstep(0.5 - w, 0.5 + w, fr);
  float r  = (fl + k) * float(%STEPM%);
  // full blend selects r EXACTLY (mix lowers to x+(y-x)*a — NOT bit-exact y at a=1)
  odata[i] = (p_blend >= 1.0) ? r : mix(v, r, p_blend);
}
)S";
  _shadersub(t, "%STEPM%", FormatString("%f", step_m));
  _shadersub(t, "%SHARP%", FormatString("%f", sharp));
  return t;
}

struct TerraceModuleInst : public TerrainComputeInst {
  TerraceModuleInst(const TerraceModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _stepM  = _floatPlug(this, _d, "step_m");
    _sharp  = _floatPlug(this, _d, "sharpness");
    _blend  = _floatPlug(this, _d, "blend");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    auto sh = fxi->shaderFromShaderText("terrain_terrace", _terrace_text(_stepM->value(), _sharp->value()));
    _cs     = fxi->computeShader(sh, "cs_terrace");
    _pm     = env->createStorageBuffer(2 * sizeof(float)); // {p_dimf, p_blend} = RUNTIME
    float blend = std::min(std::max(_blend->value(), 0.0f), 1.0f);
    float pm[2] = {float(env->_w), blend};
    auto mp = fxi->mapStorageBuffer(_pm, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);
    ci->bindStorageBuffer(_cs, 2, _pm); // p_dimf (RUNTIME grid dim)
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.terrace.v3"); // v3: step_m meters (was steps count); heights in meters
    h->accumulateItem<float>(_stepM->value());
    h->accumulateItem<float>(_sharp->value());
    h->accumulateItem<float>(_blend->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const TerraceModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _stepM, _sharp, _blend;
  FxShaderStorageBuffer* _pm = nullptr;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeTerraceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "step_m")->setValue(100.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "sharpness")->setValue(1.0f);
  // crossfade terraced vs original: 0 = passthrough, 1 = fully terraced (default).
  auto blend = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "blend");
  blend->setValue(1.0f);
  blend->annotateRange(0.0f, 1.0f);
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
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the vocabulary).
  clazz->annotateTyped<ConstString>("dsl.verb", "terrace");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 6);
}

} // namespace ork::lev2::terrain
