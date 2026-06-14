////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::CurvatureModuleData, "terrain::CurvatureModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// CurvatureModule — Out = mode(Laplacian(In)) * scale, clamped to [0,1]. 2 SSBOs.
// Laplacian via the 5-point stencil in UV space (second derivative scales by dim^2),
// so the mask is resolution-independent. Concave (valleys) has positive Laplacian,
// convex (ridges/peaks) negative. Border cells -> 0 (curvature undefined at edges).
///////////////////////////////////////////////////////////////////////////////

static std::string _curvature_text(int dim, float scale, int mode, int radius) {
  // pick the curvature flavor: convex highlights ridges (-lap), concave valleys
  // (+lap), magnitude both (|lap|).
  const char* curv = "abs(lap)";
  switch (CurvatureMode(mode)) {
    case CurvatureMode::CONVEX:    curv = "(-lap)";    break;
    case CurvatureMode::CONCAVE:   curv = "(lap)";     break;
    case CurvatureMode::MAGNITUDE: curv = "abs(lap)";  break;
  }
  int ri = radius / 2;
  if (ri < 1) ri = 1; // inner blur radius (denoise the center end of the band)
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_in } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_curvature : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%);
  uint i  = uint(yi) * %DIMU% + uint(xi);
  // curvature is undefined within `radius` of the border (the box would read OOB) -> 0.
  if (xi < %R% || yi < %R% || xi >= W - %R% || yi >= W - %R%) { odata[i] = 0.0; return; }
  // PRE-BLUR: box averages at the inner (RI) and outer (R) radii in one pass. Their
  // difference is a band-pass (difference-of-box ~ Laplacian-of-Gaussian) curvature
  // that is inherently denoised -> tracks landform ridges, not per-pixel noise.
  float osum = 0.0;
  float isum = 0.0;
  for (int dy = -%R%; dy <= %R%; dy++) {
    for (int dx = -%R%; dx <= %R%; dx++) {
      float s = idata[uint(yi + dy) * %DIMU% + uint(xi + dx)];
      osum += s;
      if (abs(dx) <= %RI% && abs(dy) <= %RI%) { isum += s; }
    }
  }
  float outer = osum / float((2 * %R% + 1) * (2 * %R% + 1));
  float inner = isum / float((2 * %RI% + 1) * (2 * %RI% + 1));
  // normalize to a per-UV 2nd-derivative scale so `scale` stays ~O(1) across dim/radius.
  float lap = (outer - inner) * float(%DIM%) * float(%DIM%) / float(%R% * %R%);
  float c   = %CURV%;                         // convex:(-lap) concave:(lap) magnitude:|lap|
  float m   = max(c, 0.0) * float(%SCALE%);   // wrong-sign -> 0
  odata[i]  = m / (1.0 + m);                  // SOFT (Reinhard) rolloff -> [0,1), magnitude survives
}
)S";
  _shadersub(t, "%CURV%", curv);
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%DIM%", FormatString("%d", dim));
  _shadersub(t, "%RI%", FormatString("%d", ri)); // before %R% (prefix) to avoid clobber
  _shadersub(t, "%R%", FormatString("%d", radius));
  _shadersub(t, "%SCALE%", FormatString("%f", scale));
  return t;
}

struct CurvatureModuleInst : public TerrainComputeInst {
  CurvatureModuleInst(const CurvatureModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _scale  = _floatPlug(this, _d, "scale");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    int rtex = env->radiusTexels(_d->_radius_m); // meters -> texels (resolution-independent)
    auto sh  = env->_ctx->FXI()->shaderFromShaderText(
        "terrain_curvature", _curvature_text(env->_w, _scale->value(), _d->_mode, rtex));
    _cs      = env->_ctx->FXI()->computeShader(sh, "cs_curvature");
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
    h->accumulateString("terrain.curvature.v3"); // v3: meter radius (resolution-independent)
    h->accumulateItem<int>(_d->_mode);
    h->accumulateItem<float>(_d->_radius_m); // meters (the resolution-independent identity)
    h->accumulateItem<float>(_scale->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const CurvatureModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _scale;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeCurvatureIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "scale")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
CurvatureModuleData::CurvatureModuleData() {}
std::shared_ptr<CurvatureModuleData> CurvatureModuleData::createShared() {
  auto d = std::make_shared<CurvatureModuleData>(); _reshapeCurvatureIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t CurvatureModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<CurvatureModuleInst>(this, g);
}
void CurvatureModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CurvatureModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeCurvatureIOs(m); });
  // _mode selects the baked GLSL output (convex/concave/magnitude); _radius is the
  // baked pre-blur/scale. Reflect both so a reloaded graph keeps its curvature flavor.
  clazz->directProperty("mode", &CurvatureModuleData::_mode);
  clazz->directProperty("radius_m", &CurvatureModuleData::_radius_m);
}

} // namespace ork::lev2::terrain
