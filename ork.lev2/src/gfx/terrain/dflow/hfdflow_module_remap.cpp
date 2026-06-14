////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::RemapModuleData, "terrain::RemapModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// RemapModule — Out = clamp(In*scale + bias, lo, hi). Two SSBOs in one storage
// interface: odata at binding 0, idata at binding 1 (declaration order).
///////////////////////////////////////////////////////////////////////////////

static std::string _remap_compute_text(int dim, float scale, float bias, float lo, float hi) {
  std::string tmpl = R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) {
  buffer layout(std430) ob { float odata[%DIMSQ%]; };
}
storage_interface sif_in (descriptor_set 0) {
  buffer layout(std430) ib { float idata[%DIMSQ%]; };
}
compute_interface iface_remap {
  storage { sif_out sif_in }
  inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); }
}
compute_shader cs_remap : iface_remap {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  uint i  = yi * %DIMU% + xi;
  float v = idata[i] * float(%SCALE%) + float(%BIAS%);
  odata[i] = clamp(v, float(%LO%), float(%HI%));
}
)SHADER";
  auto sub = [&](const std::string& key, const std::string& val) {
    size_t pos = 0;
    while ((pos = tmpl.find(key, pos)) != std::string::npos) {
      tmpl.replace(pos, key.size(), val);
      pos += val.size();
    }
  };
  sub("%DIMSQ%", FormatString("%d", dim * dim));
  sub("%DIMU%", FormatString("%du", dim));
  sub("%SCALE%", FormatString("%f", scale));
  sub("%BIAS%", FormatString("%f", bias));
  sub("%LO%", FormatString("%f", lo));
  sub("%HI%", FormatString("%f", hi));
  return tmpl;
}

struct RemapModuleInst : public TerrainComputeInst {
  RemapModuleInst(const RemapModuleData* data, dflow::GraphInst* ginst)
      : TerrainComputeInst(data, ginst)
      , _rmd(data) {
  }
  void onLink(dflow::GraphInst* inst) final {
    _output  = typedOutputNamed<HfImagePlugTraits>("Out");
    _input   = typedInputNamed<HfImagePlugTraits>("In");
    _inScale = typedInputNamed<dflow::FloatPlugTraits>("scale");
    _inBias  = typedInputNamed<dflow::FloatPlugTraits>("bias");
    _inLo    = typedInputNamed<dflow::FloatPlugTraits>("lo");
    _inHi    = typedInputNamed<dflow::FloatPlugTraits>("hi");
    // copy data-plug defaults into the inst plugs (see Fbm onLink note)
    _inScale->_value = _rmd->typedInputNamed<dflow::FloatPlugTraits>("scale")->_value;
    _inBias->_value  = _rmd->typedInputNamed<dflow::FloatPlugTraits>("bias")->_value;
    _inLo->_value    = _rmd->typedInputNamed<dflow::FloatPlugTraits>("lo")->_value;
    _inHi->_value    = _rmd->typedInputNamed<dflow::FloatPlugTraits>("hi")->_value;
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<BakeEnv>();
    auto fxi  = env->_ctx->FXI();
    int dim   = env->_w;
    auto img  = _output->_value;
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = fxi->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    auto text = _remap_compute_text(dim, _inScale->value(), _inBias->value(), _inLo->value(), _inHi->value());
    auto shdr = fxi->shaderFromShaderText("terrain_remap", text);
    _cs       = fxi->computeShader(shdr, "cs_remap");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto out   = _output->_value;
    auto in    = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, out->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);  // idata
    ci->dispatchCompute(_cs, groups, groups, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.remap.v1");
    h->accumulateItem<float>(_inScale->value());
    h->accumulateItem<float>(_inBias->value());
    h->accumulateItem<float>(_inLo->value());
    h->accumulateItem<float>(_inHi->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const RemapModuleData* _rmd;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _inScale, _inBias, _inLo, _inHi;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeRemapIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  auto sc = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "scale");
  auto bi = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "bias");
  auto lo = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "lo");
  auto hi = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "hi");
  sc->setValue(1.0f);
  bi->setValue(0.0f);
  lo->setValue(0.0f);
  hi->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}

RemapModuleData::RemapModuleData() {
}
std::shared_ptr<RemapModuleData> RemapModuleData::createShared() {
  auto data = std::make_shared<RemapModuleData>();
  _reshapeRemapIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t RemapModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<RemapModuleInst>(this, ginst);
}
void RemapModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RemapModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeRemapIOs(mdata); });
}

} // namespace ork::lev2::terrain
