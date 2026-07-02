////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::SlopeModuleData, "terrain::SlopeModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// SlopeModule — Out = clamp(length(grad(In)) * scale, 0, 1). 2 SSBOs.
// gradient via central differences taken in UV space (per-texel diff * dim/2),
// so the result is resolution-independent (a 45deg ramp reads ~constant slope).
///////////////////////////////////////////////////////////////////////////////

static std::string _slope_text(int dim, float scale, int radius, float slope_factor) {
  int rb = radius / 2;
  if (rb < 1) rb = 1; // box radius at each gradient endpoint (denoise)
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_in } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_slope : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%);
  uint i  = uint(yi) * %DIMU% + uint(xi);
  int  M  = %R% + %RB%; // margin = gradient baseline + box radius
  if (xi < M || yi < M || xi >= W - M || yi >= W - M) { odata[i] = 0.0; return; }
  // PRE-BLUR: gradient at scale R via a difference of box averages offset by +/-R
  // (each box radius RB). Denoised -> tracks landform slope, not per-pixel noise.
  float sL = 0.0;
  float sR = 0.0;
  float sD = 0.0;
  float sU = 0.0;
  for (int dy = -%RB%; dy <= %RB%; dy++) {
    for (int dx = -%RB%; dx <= %RB%; dx++) {
      sL += idata[uint(yi + dy) * %DIMU% + uint(xi - %R% + dx)];
      sR += idata[uint(yi + dy) * %DIMU% + uint(xi + %R% + dx)];
      sD += idata[uint(yi - %R% + dy) * %DIMU% + uint(xi + dx)];
      sU += idata[uint(yi + %R% + dy) * %DIMU% + uint(xi + dx)];
    }
  }
  float n = float((2 * %RB% + 1) * (2 * %RB% + 1));
  // per-UV gradient: delta over baseline 2R texels == 2R/dim in UV.
  float gx = (sR - sL) / n * float(%DIM%) / float(2 * %R%);
  float gy = (sU - sD) / n * float(%DIM%) / float(2 * %R%);
  // per-UV gradient -> REAL rise/run (tan of the terrain angle): * height_scale/extent.
  float m  = length(vec2(gx, gy)) * float(%SLOPEFACTOR%) * float(%SCALE%);
  odata[i] = m / (1.0 + m); // SOFT (Reinhard) rolloff -> [0,1), magnitude survives
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%DIM%", FormatString("%d", dim));
  _shadersub(t, "%RB%", FormatString("%d", rb)); // before %R% (prefix) to avoid clobber
  _shadersub(t, "%R%", FormatString("%d", radius));
  _shadersub(t, "%SLOPEFACTOR%", FormatString("%f", slope_factor));
  _shadersub(t, "%SCALE%", FormatString("%f", scale));
  return t;
}

struct SlopeModuleInst : public TerrainComputeInst {
  SlopeModuleInst(const SlopeModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _scale  = _floatPlug(this, _d, "scale");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    int   rtex   = env->radiusTexels(_d->_radius_m);                 // meters -> texels (res-indep)
    float sfactor = env->_height_scale_m / (env->_extent_m > 0.0f ? env->_extent_m : 1.0f); // -> tan(angle)
    auto sh = env->_ctx->FXI()->shaderFromShaderText(
        "terrain_slope", _slope_text(env->_w, _scale->value(), rtex, sfactor));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_slope");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);              // idata
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.slope.v3"); // v3: meter radius + real-angle (res-independent)
    h->accumulateItem<float>(_d->_radius_m);  // meters (the resolution-independent identity)
    h->accumulateItem<float>(_scale->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const SlopeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _scale;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeSlopeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "scale")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SlopeModuleData::SlopeModuleData() {}
std::shared_ptr<SlopeModuleData> SlopeModuleData::createShared() {
  auto d = std::make_shared<SlopeModuleData>(); _reshapeSlopeIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t SlopeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SlopeModuleInst>(this, g);
}
void SlopeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SlopeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeSlopeIOs(m); });
  clazz->directProperty("radius_m", &SlopeModuleData::_radius_m); // baked pre-blur / scale (meters)
}

} // namespace ork::lev2::terrain
