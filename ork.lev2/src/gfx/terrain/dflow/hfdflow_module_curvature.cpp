////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <ork/reflect/enum_serializer.inl>

ImplementReflectionX(ork::lev2::terrain::CurvatureModuleData, "terrain::CurvatureModuleData");
ImplementEnumSerializer(ork::lev2::terrain::CurvatureMode);

namespace ork::lev2::terrain {

// EnumSerializer registration (E1) — lowercase to match the T.curvature(mode="...")
// DSL spelling; values match ops._CURV_MODE.
BeginEnumRegistration(CurvatureMode);
  enumtype->addEnum("convex", CurvatureMode::CONVEX);
  enumtype->addEnum("concave", CurvatureMode::CONCAVE);
  enumtype->addEnum("magnitude", CurvatureMode::MAGNITUDE);
EndEnumRegistration();

///////////////////////////////////////////////////////////////////////////////
// CurvatureModule — Out = mode(Laplacian(In)) * scale, clamped to [0,1]. 2 SSBOs. In is in
// METERS (natural units), so the Laplacian is a meter-scale 2nd derivative; `scale` retunes it.
// Laplacian via the 5-point stencil in UV space (second derivative scales by dim^2),
// so the mask is resolution-independent. Concave (valleys) has positive Laplacian,
// convex (ridges/peaks) negative. Border cells -> 0 (curvature undefined at edges).
///////////////////////////////////////////////////////////////////////////////

static std::string _curvature_text(float scale, int mode, int radius) {
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
  // DIM is RUNTIME data (params SSBO p_dimf, binding 2) — dim changes never rebuild the
  // shader; the field arrays are runtime-sized. The 2nd-derivative normalization uses
  // p_dimf as a MULTIPLY (no reciprocal). Radius/mode/scale stay baked.
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }
storage_interface sif_pm  (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; }; }
compute_interface iface { storage { sif_out sif_in sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_curvature : iface {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(u_dim);
  uint i  = uint(yi) * u_dim + uint(xi);
  // curvature is undefined within `radius` of the border (the box would read OOB) -> 0.
  if (xi < %R% || yi < %R% || xi >= W - %R% || yi >= W - %R%) { odata[i] = 0.0; return; }
  // PRE-BLUR: box averages at the inner (RI) and outer (R) radii in one pass. Their
  // difference is a band-pass (difference-of-box ~ Laplacian-of-Gaussian) curvature
  // that is inherently denoised -> tracks landform ridges, not per-pixel noise.
  float osum = 0.0;
  float isum = 0.0;
  for (int dy = -%R%; dy <= %R%; dy++) {
    for (int dx = -%R%; dx <= %R%; dx++) {
      float s = idata[uint(yi + dy) * u_dim + uint(xi + dx)];
      osum += s;
      if (abs(dx) <= %RI% && abs(dy) <= %RI%) { isum += s; }
    }
  }
  float outer = osum / float((2 * %R% + 1) * (2 * %R% + 1));
  float inner = isum / float((2 * %RI% + 1) * (2 * %RI% + 1));
  // normalize to a per-UV 2nd-derivative scale so `scale` stays ~O(1) across dim/radius.
  float lap = (outer - inner) * p_dimf * p_dimf / float(%R% * %R%);
  float c   = %CURV%;                         // convex:(-lap) concave:(lap) magnitude:|lap|
  float m   = max(c, 0.0) * float(%SCALE%);   // wrong-sign -> 0
  odata[i]  = m / (1.0 + m);                  // SOFT (Reinhard) rolloff -> [0,1), magnitude survives
}
)S";
  _shadersub(t, "%CURV%", curv);
  _shadersub(t, "%RI%", FormatString("%d", ri)); // before %R% (prefix) to avoid clobber
  _shadersub(t, "%R%", FormatString("%d", radius));
  _shadersub(t, "%SCALE%", FormatString("%f", scale));
  return t;
}

struct CurvatureModuleInst : public TerrainComputeInst {
  CurvatureModuleInst(const CurvatureModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  // S4: a curvature MASK is analysis, not surface — never publish it as live display heights.
  bool viewableDefault() const override { return false; }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _scale  = _floatPlug(this, _d, "scale");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    int rtex = env->radiusTexels(_d->_radius_m); // meters -> texels (resolution-independent)
    auto sh  = fxi->shaderFromShaderText(
        "terrain_curvature", _curvature_text(_scale->value(), int(_d->_mode), rtex));
    _cs      = fxi->computeShader(sh, "cs_curvature");
    _pm        = env->createStorageBuffer(sizeof(float)); // p_dimf = RUNTIME grid dim
    float dimf = float(env->_w);
    auto mp    = fxi->mapStorageBuffer(_pm, 0, sizeof(dimf), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, &dimf, sizeof(dimf));
    fxi->unmapStorageBuffer(mp.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);              // idata
    ci->bindStorageBuffer(_cs, 2, _pm);                    // p_dimf (RUNTIME grid dim)
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.curvature.v4"); // v4: heights in meters (Laplacian now in meter units); v3: meter radius
    h->accumulateItem<int>(int(_d->_mode)); // hash the CODE (shader identity unchanged by the enum migration)
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
  FxShaderStorageBuffer* _pm = nullptr;
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
  // _mode selects the baked GLSL output (convex/concave/magnitude) — a real reflected
  // enum (serializes by NAME, exposes its choice list to the propsheet, E1); _radius is
  // the baked pre-blur/scale.
  InvokeEnumRegistration(CurvatureMode);
  clazz->directEnumProperty("mode", &CurvatureModuleData::_mode);
  clazz->directProperty("radius_m", &CurvatureModuleData::_radius_m);
}

} // namespace ork::lev2::terrain
