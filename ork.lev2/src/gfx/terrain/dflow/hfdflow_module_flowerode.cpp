////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <utility> // std::swap

ImplementReflectionX(ork::lev2::terrain::FlowErodeModuleData, "terrain::FlowErodeModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// FlowErodeModule — continuous (MFD/vector-field) erosion+deposition step driven by a flow map.
// Inputs: "In" = z, "Discharge" = drainage area A (T.flow / T.flow3d.discharge). Each step:
//   slope S = |∇z|*aspect (physical, recomputed from the CURRENT z); flatness = 1/(1+S*flat_k);
//   A from discharge (exp() if log). erode = k_erode·Aᵐ·Sⁿ ; deposit = k_deposit·flatness·A^dep_m ;
//   dz = dt·(deposit - erode), CLAMPED to ±clamp_frac·(local relief) so a cell can never drop below
//   its lowest neighbour or rise above its highest -> unconditionally bounded (no implicit solve).
// Boundary cells are held fixed (base level). niter internal steps hold A fixed; chain
// T.flow3d -> T.flow_erode in the DSL to recompute A as z evolves.
///////////////////////////////////////////////////////////////////////////////

static constexpr int kSubmitChunk = 64;

// one continuous erosion+deposition step, in PHYSICAL meters (z is in meters). slope = |grad z|
// / texel_m (true rise/run); erode/deposit are in meters; the clamp bounds change to a fraction of the
// physical local relief -> dt/k_* are now meaningful magnitudes, clamp_frac is a true safety guard.
// si_i=z_in si_d=discharge si_o=z_out. inv_texel = 1/texel_m.
static std::string _fe_step_text(float dt, float k_erode, float k_deposit,
                                 float m, float n, float dep_m, float flat_k, float clamp_frac,
                                 bool disch_log) {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float zin[]; }; }\n"
    "storage_interface si_d (descriptor_set 0) { buffer layout(std430) db { float disch[]; }; }\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float zout[]; }; }\n"
    "storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float p_dimf; float p_invtexel; }; }\n"
    "compute_interface iface { storage { si_i si_d si_o si_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_fe_step : iface {\n"
    "  uint u_dim = uint(p_dimf);\n"
    "  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(u_dim); uint Wu = u_dim; uint i = uint(yi)*Wu + uint(xi);\n"
    "  float zc = zin[i];\n"
    "  if (xi==0 || yi==0 || xi==W-1 || yi==W-1) { zout[i] = zc; return; }\n"   // boundary = base level
    "  float zl = zin[i-1u]; float zr = zin[i+1u]; float zd = zin[i-Wu]; float zu = zin[i+Wu];\n"
    "  float gx = (zr - zl) * 0.5; float gy = (zu - zd) * 0.5;\n"               // grad of PHYSICAL z (meters)
    "  float slope = sqrt(gx*gx + gy*gy) * p_invtexel;\n"                       // physical rise/run (dim-derived -> RUNTIME)
    "  float flatv = 1.0 / (1.0 + slope * float(%FLATK%));\n"                   // flat is a GLSL keyword
    "  float d = disch[i];\n"
    "  float A = %AEXPR%;\n"                                                    // raw drainage area
    "  A = max(A, 1.0);\n"
    "  float ero = float(%DT%) * float(%KERODE%)   * pow(A, float(%M%))    * pow(slope, float(%N%));\n"
    "  float dep = float(%DT%) * float(%KDEPOSIT%) * flatv * pow(A, float(%DEPM%));\n"
    "  float dz  = dep - ero;\n"
    "  float zmin = min(min(zl,zr), min(zd,zu));\n"
    "  float zmax = max(max(zl,zr), max(zd,zu));\n"
    "  float down = max(zc - zmin, 0.0); float up = max(zmax - zc, 0.0);\n"
    "  dz = clamp(dz, -float(%CLAMPF%)*down, float(%CLAMPF%)*up);\n"            // bound by local relief
    "  zout[i] = zc + dz;\n"
    "}\n");
  _shadersub(t, "%AEXPR%", disch_log ? "exp(min(d,30.0)) - 1.0" : "d");
  _shadersub(t, "%DT%",      FormatString("%g", dt));
  _shadersub(t, "%KERODE%",  FormatString("%g", k_erode));
  _shadersub(t, "%KDEPOSIT%",FormatString("%g", k_deposit));
  _shadersub(t, "%M%",       FormatString("%g", m));
  _shadersub(t, "%N%",       FormatString("%g", n));
  _shadersub(t, "%DEPM%",    FormatString("%g", dep_m));
  _shadersub(t, "%FLATK%",   FormatString("%g", flat_k));
  _shadersub(t, "%CLAMPF%",  FormatString("%g", clamp_frac));
  return t;
}

// z_in copy into the ping-pong (heights are METERS already): o = i_.  si_o=z si_i=in.
static std::string _fe_zin_text() {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float o[]; }; }\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float i_[]; }; }\n"
    "storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float p_dimf; }; }\n"
    "compute_interface iface { storage { si_o si_i si_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_fe_zin : iface {\n"
    "  uint u_dim = uint(p_dimf);\n"
    "  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }\n"
    "  uint k = gl_GlobalInvocationID.y*u_dim + gl_GlobalInvocationID.x; o[k] = i_[k];\n"
    "}\n");
  return t;
}

// meters output, crossfaded vs the ORIGINAL input (the lpf `blend` idiom; RUNTIME param —
// mix(x,y,1.0) is NOT bit-exact y, so full blend selects the eroded value EXACTLY).
// si_o=out si_i=z(eroded) si_g=original.
static std::string _fe_zout_text() {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float o[]; }; }\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float i_[]; }; }\n"
    "storage_interface si_g (descriptor_set 0) { buffer layout(std430) gb { float g_[]; }; }\n"
    "storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float p_dimf; float p_invtexel; float p_blend; }; }\n"
    "compute_interface iface { storage { si_o si_i si_g si_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_fe_zout : iface {\n"
    "  uint u_dim = uint(p_dimf);\n"
    "  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }\n"
    "  uint k = gl_GlobalInvocationID.y*u_dim + gl_GlobalInvocationID.x;\n"
    "  // full blend selects i_ EXACTLY (mix lowers to x+(y-x)*a — NOT bit-exact y at a=1)\n"
    "  o[k] = (p_blend >= 1.0) ? i_[k] : mix(g_[k], i_[k], p_blend);\n"
    "}\n");
  return t;
}

struct FlowErodeModuleInst : public TerrainComputeInst {
  FlowErodeModuleInst(const FlowErodeModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _disch  = typedInputNamed<HfImagePlugTraits>("Discharge");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    _allocOut(env.get(), _output->_value);
    size_t n = size_t(dim) * size_t(dim);
    _zA = env->createStorageBuffer(n * sizeof(float));
    _zB = env->createStorageBuffer(n * sizeof(float));
    // dim AND the dim-DERIVED physical scalar (inv_texel) are RUNTIME data now — one
    // compiled step shader serves every dim. PARITY: the old literal was %g-formatted,
    // so the uploaded float is %g-ROUNDTRIPPED to match the parsed literal exactly.
    float cell      = (dim > 0) ? (env->_extent_m / float(dim)) : 1.0f;
    float inv_texel = 1.0f / cell;                  // slope = |grad z| / texel_m (z in meters)
    float blend = std::min(std::max(_d->_blend, 0.0f), 1.0f);
    _params = env->createStorageBuffer(3 * sizeof(float));
    { float pm[3] = {float(dim),
                     strtof(FormatString("%g", inv_texel).c_str(), nullptr),
                     blend};
      auto mp = fxi->mapStorageBuffer(_params, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
      fxi->unmapStorageBuffer(mp.get()); }
    _csStep = fxi->computeShader(fxi->shaderFromShaderText("terrain_fe_step",
                  _fe_step_text(_d->_dt, _d->_k_erode, _d->_k_deposit, _d->_m, _d->_n,
                                _d->_dep_m, _d->_flat_k, _d->_clamp_frac, _d->_disch_log)), "cs_fe_step");
    _csZin  = fxi->computeShader(fxi->shaderFromShaderText("terrain_fe_zin",  _fe_zin_text()),  "cs_fe_zin");
    _csZout = fxi->computeShader(fxi->shaderFromShaderText("terrain_fe_zout", _fe_zout_text()), "cs_fe_zout");
    _niter = (_d->_niter < 1) ? 1 : _d->_niter;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    auto dis = _srcImg(_disch);
    OrkAssert(in && in->_ssbo && dis && dis->_ssbo);
    int g = (env->_w + 7) / 8;
    // z (meters) = copy of input
    ci->bindStorageBuffer(_csZin, 0, _zA); ci->bindStorageBuffer(_csZin, 1, in->_ssbo);
    ci->bindStorageBuffer(_csZin, 2, _params);
    ci->dispatchCompute(_csZin, g, g, 1); ci->storageBarrier();
    FxShaderStorageBuffer* cur = _zA; FxShaderStorageBuffer* nxt = _zB;
    for (int it = 0; it < _niter; it++) {
      ci->bindStorageBuffer(_csStep, 0, cur); ci->bindStorageBuffer(_csStep, 1, dis->_ssbo);
      ci->bindStorageBuffer(_csStep, 2, nxt);
      ci->bindStorageBuffer(_csStep, 3, _params);
      ci->dispatchCompute(_csStep, g, g, 1); std::swap(cur, nxt);
      if (((it + 1) % kSubmitChunk) == 0) { ci->endDispatchPhase(); ci->beginDispatchPhase(); }
      else ci->storageBarrier();
    }
    // output (meters) = mix(original, eroded z, blend)
    ci->bindStorageBuffer(_csZout, 0, _output->_value->_ssbo); ci->bindStorageBuffer(_csZout, 1, cur);
    ci->bindStorageBuffer(_csZout, 2, in->_ssbo);
    ci->bindStorageBuffer(_csZout, 3, _params);
    ci->dispatchCompute(_csZout, g, g, 1); ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.flowerode.v4.meters"); // v4: heights in meters (zin/zout are copies, no height_scale)
    h->accumulateItem<int>(_d->_niter);
    h->accumulateItem<float>(_d->_dt);
    h->accumulateItem<float>(_d->_k_erode);
    h->accumulateItem<float>(_d->_k_deposit);
    h->accumulateItem<float>(_d->_m);
    h->accumulateItem<float>(_d->_n);
    h->accumulateItem<float>(_d->_dep_m);
    h->accumulateItem<float>(_d->_flat_k);
    h->accumulateItem<float>(_d->_clamp_frac);
    h->accumulateItem<float>(_d->_blend);
    h->accumulateItem<int>(_d->_disch_log ? 1 : 0);
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const FlowErodeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input, _disch;
  FxShaderStorageBuffer *_zA = nullptr, *_zB = nullptr;
  FxShaderStorageBuffer *_params = nullptr; // runtime grid dim (p_dimf), filled in bakeAcquire
  const FxComputeShader *_csStep = nullptr, *_csZin = nullptr, *_csZout = nullptr;
  int _niter = 1;
};

static void _reshapeFlowErodeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Discharge");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
FlowErodeModuleData::FlowErodeModuleData() {}
std::shared_ptr<FlowErodeModuleData> FlowErodeModuleData::createShared() {
  auto d = std::make_shared<FlowErodeModuleData>(); _reshapeFlowErodeIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t FlowErodeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<FlowErodeModuleInst>(this, g);
}
void FlowErodeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return FlowErodeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeFlowErodeIOs(m); });
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the
  // vocabulary). recipe = "flow_erode": the python insertion recipe auto-inserts the
  // canonical flow3d companion and wires its discharge.
  clazz->annotateTyped<ConstString>("dsl.verb", "flow_erode");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 3);
  clazz->annotateTyped<ConstString>("editor.palette.recipe", "flow_erode");
  clazz->directProperty("niter", &FlowErodeModuleData::_niter);
  clazz->directProperty("dt", &FlowErodeModuleData::_dt);
  clazz->directProperty("k_erode", &FlowErodeModuleData::_k_erode);
  clazz->directProperty("k_deposit", &FlowErodeModuleData::_k_deposit);
  clazz->directProperty("m", &FlowErodeModuleData::_m);
  clazz->directProperty("n", &FlowErodeModuleData::_n);
  clazz->directProperty("dep_m", &FlowErodeModuleData::_dep_m);
  clazz->directProperty("flat_k", &FlowErodeModuleData::_flat_k);
  clazz->directProperty("clamp_frac", &FlowErodeModuleData::_clamp_frac);
  // _blend is a reflected module PROPERTY (not a plug) — its slider range rides the
  // PROPERTY annotation seam (editor.range.min/max), flowing through moduleClasses()
  // property metadata into the propsheet exactly as plug ranges flow via plugSpec
  // (the params-lane carve-out; this establishes the module-prop range pattern).
  clazz->directProperty("blend", &FlowErodeModuleData::_blend)
      ->annotate<float>("editor.range.min", 0.0f)
      ->annotate<float>("editor.range.max", 1.0f);
  clazz->directProperty("disch_log", &FlowErodeModuleData::_disch_log);
}

} // namespace ork::lev2::terrain
