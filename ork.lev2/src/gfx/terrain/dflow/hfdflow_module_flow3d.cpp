////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <utility>   // std::swap
#include <algorithm> // std::min

ImplementReflectionX(ork::lev2::terrain::Flow3DModuleData, "terrain::Flow3DModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// Flow3DModule — CONTINUOUS FLOW FIELD. Emits a per-cell flow DIRECTION (unit −∇z, any
// angle — NOT D8-quantised) + slope as an RGBA display field, and the MFD drainage
// DISCHARGE as a mono field. Direction/slope come from a cheap central-difference gradient;
// discharge is an MFD (Holmgren) weights + Jacobi gather. Two outputs:
//   "Out"       RGBA32F : R,G = (-∇z normalised)*0.5+0.5 ; B = clamp(slope*slope_scale,0,1) ; A=1
//   "Discharge" R32F    : log(1+area) (or raw), MFD drainage area in m²
// This is the substrate for continuous transport-limited erosion+deposition (advect sediment
// along the flow vector) — no SFD tree needed.
///////////////////////////////////////////////////////////////////////////////

static constexpr int kAutoCap     = 1024;
static constexpr int kSubmitChunk = 128;

// 8 normalized MFD out-weights / cell. si_w=weights[8N] si_i=dem.
static std::string _f3_weights_text(int dim, float p) {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float wdata[%WSQ%]; }; }\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }\n"
    "compute_interface iface { storage { si_w si_i } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_f3_weights : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);\n"
    "  float zc = idata[i];\n"
    "  float d0 = (xi<W-1) ? (zc-idata[i+1u]) : -1.0;\n"
    "  float d1 = (xi>0)   ? (zc-idata[i-1u]) : -1.0;\n"
    "  float d2 = (yi<W-1) ? (zc-idata[i+Wu]) : -1.0;\n"
    "  float d3 = (yi>0)   ? (zc-idata[i-Wu]) : -1.0;\n"
    "  float d4 = (xi<W-1&&yi<W-1) ? (zc-idata[i+Wu+1u])*0.70710678 : -1.0;\n"
    "  float d5 = (xi<W-1&&yi>0)   ? (zc-idata[i-Wu+1u])*0.70710678 : -1.0;\n"
    "  float d6 = (xi>0&&yi<W-1)   ? (zc-idata[i+Wu-1u])*0.70710678 : -1.0;\n"
    "  float d7 = (xi>0&&yi>0)     ? (zc-idata[i-Wu-1u])*0.70710678 : -1.0;\n"
    "  float w0 = d0>0.0 ? pow(d0,float(%P%)) : 0.0; float w1 = d1>0.0 ? pow(d1,float(%P%)) : 0.0;\n"
    "  float w2 = d2>0.0 ? pow(d2,float(%P%)) : 0.0; float w3 = d3>0.0 ? pow(d3,float(%P%)) : 0.0;\n"
    "  float w4 = d4>0.0 ? pow(d4,float(%P%)) : 0.0; float w5 = d5>0.0 ? pow(d5,float(%P%)) : 0.0;\n"
    "  float w6 = d6>0.0 ? pow(d6,float(%P%)) : 0.0; float w7 = d7>0.0 ? pow(d7,float(%P%)) : 0.0;\n"
    "  float sw = w0+w1+w2+w3+w4+w5+w6+w7; float inv = (sw>0.0) ? (1.0/sw) : 0.0;\n"
    "  uint b = 8u*i;\n"
    "  wdata[b+0u]=w0*inv; wdata[b+1u]=w1*inv; wdata[b+2u]=w2*inv; wdata[b+3u]=w3*inv;\n"
    "  wdata[b+4u]=w4*inv; wdata[b+5u]=w5*inv; wdata[b+6u]=w6*inv; wdata[b+7u]=w7*inv;\n"
    "}\n");
  _shadersub(t, "%WSQ%", FormatString("%d", 8 * dim * dim));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%P%", FormatString("%g", p));
  return t;
}

static std::string _f3_init_text(int dim) {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }\n"
    "compute_interface iface { storage { si_o } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_f3_init : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  uint i = gl_GlobalInvocationID.y*%DIMU% + gl_GlobalInvocationID.x; odata[i] = 1.0;\n"
    "}\n");
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  return t;
}

static std::string _f3_accum_text(int dim) {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }\n"
    "storage_interface si_a (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }\n"
    "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float wdata[%WSQ%]; }; }\n"
    "compute_interface iface { storage { si_o si_a si_w } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_f3_accum : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);\n"
    "  float a = 1.0;\n"
    "  if (xi<W-1)          { uint nb=i+1u;     a += wdata[8u*nb+1u]*adata[nb]; }\n"
    "  if (xi>0)            { uint nb=i-1u;     a += wdata[8u*nb+0u]*adata[nb]; }\n"
    "  if (yi<W-1)          { uint nb=i+Wu;     a += wdata[8u*nb+3u]*adata[nb]; }\n"
    "  if (yi>0)            { uint nb=i-Wu;     a += wdata[8u*nb+2u]*adata[nb]; }\n"
    "  if (xi<W-1&&yi<W-1)  { uint nb=i+Wu+1u;  a += wdata[8u*nb+7u]*adata[nb]; }\n"
    "  if (xi<W-1&&yi>0)    { uint nb=i-Wu+1u;  a += wdata[8u*nb+6u]*adata[nb]; }\n"
    "  if (xi>0&&yi<W-1)    { uint nb=i+Wu-1u;  a += wdata[8u*nb+5u]*adata[nb]; }\n"
    "  if (xi>0&&yi>0)      { uint nb=i-Wu-1u;  a += wdata[8u*nb+4u]*adata[nb]; }\n"
    "  odata[i] = a;\n"
    "}\n");
  _shadersub(t, "%WSQ%", FormatString("%d", 8 * dim * dim));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  return t;
}

// gradient -> RGBA flow field (dir,slope), mono discharge, AND a metrics RGBA (flatness, curvature,
// wetness). central-difference -∇z + Laplacian, CLAMP_TO_EDGE. Slope/curvature are PHYSICAL
// (resolution-INVARIANT): slope = grad * aspect (height_m/texel_m); curvature = lap * aspect2
// (height_m/texel_m²) ≈ ∇²z in 1/m. si_i=dem si_a=acc si_o=Out(RGBA) si_d=discharge(mono)
// si_m=Metrics(RGBA: flatness, curvature, wetness, 1).
static std::string _f3_field_text(int dim, float cell_area, float aspect, float aspect2,
                                  float slope_scale, float flat_scale, float curv_scale,
                                  float twi_scale, bool log_compress) {
  std::string t = std::string(
    "\nfxconfig fxcfg_default {}\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }\n"
    "storage_interface si_a (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }\n"
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[%RGBASQ%]; }; }\n"
    "storage_interface si_d (descriptor_set 0) { buffer layout(std430) db { float ddata[%DIMSQ%]; }; }\n"
    "storage_interface si_m (descriptor_set 0) { buffer layout(std430) mb { float mdata[%RGBASQ%]; }; }\n"
    "compute_interface iface { storage { si_i si_a si_o si_d si_m } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_f3_field : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);\n"
    "  float zc = idata[i];\n"
    "  float zl = idata[(xi>0)   ? i-1u : i];\n"
    "  float zr = idata[(xi<W-1) ? i+1u : i];\n"
    "  float zd = idata[(yi>0)   ? i-Wu : i];\n"
    "  float zu = idata[(yi<W-1) ? i+Wu : i];\n"
    "  float gx = (zr - zl) * 0.5; float gy = (zu - zd) * 0.5;\n"   // ∇z (normalized-height, per-texel)
    "  float gm = sqrt(gx*gx + gy*gy);\n"
    "  float il = (gm > 1e-12) ? (1.0/gm) : 0.0;\n"
    "  float dxn = -gx * il; float dyn = -gy * il;\n"               // unit downhill flow dir (scale-free)
    "  float slope = gm * float(%ASPECT%);\n"                       // PHYSICAL slope = rise/run
    "  uint b = 4u*i;\n"
    "  odata[b+0u] = dxn*0.5 + 0.5;\n"
    "  odata[b+1u] = dyn*0.5 + 0.5;\n"
    "  odata[b+2u] = clamp(slope * float(%SLOPESCALE%), 0.0, 1.0);\n"
    "  odata[b+3u] = 1.0;\n"
    "  float area = adata[i] * float(%CELLAREA%);\n"
    "  ddata[i] = %OUTEXPR%;\n"
    // ---- metrics ----
    "  float flatv = 1.0 / (1.0 + slope * float(%FLATSCALE%));\n"             // flat areas -> 1 (flat is a GLSL keyword)
    "  float curv = ((zl+zr+zd+zu) - 4.0*zc) * float(%ASPECT2%);\n"          // physical ∇²z (1/m); >0 concave (valley)
    "  float curvd = clamp(0.5 + curv * float(%CURVSCALE%), 0.0, 1.0);\n"    // 0.5=flat, >0.5 valley, <0.5 ridge
    "  float twi = log(max(area,1.0)) - log(max(slope,1e-4));\n"            // wetness index ln(A/slope)
    "  float wet = clamp(twi / float(%TWISCALE%), 0.0, 1.0);\n"
    "  mdata[b+0u] = flatv; mdata[b+1u] = curvd; mdata[b+2u] = wet; mdata[b+3u] = 1.0;\n"  // R=flatness G=curvature B=wetness
    "}\n");
  _shadersub(t, "%OUTEXPR%", log_compress ? "log(1.0 + area)" : "area");
  _shadersub(t, "%CELLAREA%", FormatString("%g", cell_area));
  _shadersub(t, "%ASPECT%", FormatString("%g", aspect));
  _shadersub(t, "%ASPECT2%", FormatString("%g", aspect2));
  _shadersub(t, "%SLOPESCALE%", FormatString("%g", slope_scale));
  _shadersub(t, "%FLATSCALE%", FormatString("%g", flat_scale));
  _shadersub(t, "%CURVSCALE%", FormatString("%g", curv_scale));
  _shadersub(t, "%TWISCALE%", FormatString("%g", twi_scale));
  _shadersub(t, "%RGBASQ%", FormatString("%d", 4 * dim * dim));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  return t;
}

struct Flow3DModuleInst : public TerrainComputeInst {
  Flow3DModuleInst(const Flow3DModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _outField   = typedOutputNamed<HfImagePlugTraits>("Out");
    _outDis     = typedOutputNamed<HfImagePlugTraits>("Discharge");
    _outMetrics = typedOutputNamed<HfImagePlugTraits>("Metrics");
    _input      = typedInputNamed<HfImagePlugTraits>("In");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    size_t n = size_t(dim) * size_t(dim);
    // "Out" + "Metrics" are RGBA32F (4/cell) — allocate manually (not the mono _allocOut).
    _outField->_value->_w = dim; _outField->_value->_h = dim; _outField->_value->_channels = 4;
    _outField->_value->_ssbo = env->createStorageBuffer(n * 4 * sizeof(float));
    _outMetrics->_value->_w = dim; _outMetrics->_value->_h = dim; _outMetrics->_value->_channels = 4;
    _outMetrics->_value->_ssbo = env->createStorageBuffer(n * 4 * sizeof(float));
    _allocOut(env.get(), _outDis->_value); // mono discharge
    _wt   = env->createStorageBuffer(n * 8 * sizeof(float));
    _accA = env->createStorageBuffer(n * sizeof(float));
    _accB = env->createStorageBuffer(n * sizeof(float));
    float cell    = (dim > 0) ? (env->_extent_m / float(dim)) : 1.0f;
    float aspect  = env->_height_scale_m / cell;          // slope:     grad  * height_m/texel_m
    float aspect2 = env->_height_scale_m / (cell * cell); // curvature: lap   * height_m/texel_m²
    _csWeights = fxi->computeShader(fxi->shaderFromShaderText("terrain_f3_weights", _f3_weights_text(dim, _d->_exponent)), "cs_f3_weights");
    _csInit    = fxi->computeShader(fxi->shaderFromShaderText("terrain_f3_init",    _f3_init_text(dim)), "cs_f3_init");
    _csAccum   = fxi->computeShader(fxi->shaderFromShaderText("terrain_f3_accum",   _f3_accum_text(dim)), "cs_f3_accum");
    _csField   = fxi->computeShader(fxi->shaderFromShaderText("terrain_f3_field",
                     _f3_field_text(dim, cell * cell, aspect, aspect2, _d->_slope_scale,
                                    _d->_flat_scale, _d->_curv_scale, _d->_twi_scale, _d->_log_compress)), "cs_f3_field");
    _iters = (_d->_iterations > 0) ? _d->_iterations : std::min(dim, kAutoCap);
    if (_iters < 1) _iters = 1;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    // 1) MFD weights + 2) acc=1 + 3) Jacobi gather (discharge)
    ci->bindStorageBuffer(_csWeights, 0, _wt); ci->bindStorageBuffer(_csWeights, 1, in->_ssbo);
    ci->dispatchCompute(_csWeights, g, g, 1); ci->storageBarrier();
    ci->bindStorageBuffer(_csInit, 0, _accA);
    ci->dispatchCompute(_csInit, g, g, 1); ci->storageBarrier();
    FxShaderStorageBuffer* cur = _accA; FxShaderStorageBuffer* nxt = _accB;
    for (int it = 0; it < _iters; it++) {
      ci->bindStorageBuffer(_csAccum, 0, nxt); ci->bindStorageBuffer(_csAccum, 1, cur);
      ci->bindStorageBuffer(_csAccum, 2, _wt);
      ci->dispatchCompute(_csAccum, g, g, 1); std::swap(cur, nxt);
      if (((it + 1) % kSubmitChunk) == 0) { ci->endDispatchPhase(); ci->beginDispatchPhase(); }
      else ci->storageBarrier();
    }
    // 4) gradient -> RGBA flow field + mono discharge + RGBA metrics
    ci->bindStorageBuffer(_csField, 0, in->_ssbo); ci->bindStorageBuffer(_csField, 1, cur);
    ci->bindStorageBuffer(_csField, 2, _outField->_value->_ssbo);
    ci->bindStorageBuffer(_csField, 3, _outDis->_value->_ssbo);
    ci->bindStorageBuffer(_csField, 4, _outMetrics->_value->_ssbo);
    ci->dispatchCompute(_csField, g, g, 1); ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.flow3d.v3"); // v3: + Metrics output (R=flatness, G=curvature, B=wetness)
    h->accumulateItem<float>(_d->_exponent);
    h->accumulateItem<int>(_d->_iterations);
    h->accumulateItem<int>(_d->_log_compress ? 1 : 0);
    h->accumulateItem<float>(_d->_slope_scale);
    h->accumulateItem<float>(_d->_flat_scale);
    h->accumulateItem<float>(_d->_curv_scale);
    h->accumulateItem<float>(_d->_twi_scale);
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const Flow3DModuleData* _d;
  hfimg_outpluginst_ptr_t _outField, _outDis, _outMetrics;
  hfimg_inpluginst_ptr_t _input;
  FxShaderStorageBuffer *_wt = nullptr, *_accA = nullptr, *_accB = nullptr;
  const FxComputeShader *_csWeights = nullptr, *_csInit = nullptr, *_csAccum = nullptr, *_csField = nullptr;
  int _iters = 1;
};

static void _reshapeFlow3DIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Discharge");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Metrics");
}
Flow3DModuleData::Flow3DModuleData() {}
std::shared_ptr<Flow3DModuleData> Flow3DModuleData::createShared() {
  auto d = std::make_shared<Flow3DModuleData>(); _reshapeFlow3DIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t Flow3DModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<Flow3DModuleInst>(this, g);
}
void Flow3DModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return Flow3DModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeFlow3DIOs(m); });
  clazz->directProperty("exponent", &Flow3DModuleData::_exponent);
  clazz->directProperty("iterations", &Flow3DModuleData::_iterations);
  clazz->directProperty("log_compress", &Flow3DModuleData::_log_compress);
  clazz->directProperty("slope_scale", &Flow3DModuleData::_slope_scale);
  clazz->directProperty("flat_scale", &Flow3DModuleData::_flat_scale);
  clazz->directProperty("curv_scale", &Flow3DModuleData::_curv_scale);
  clazz->directProperty("twi_scale", &Flow3DModuleData::_twi_scale);
}

} // namespace ork::lev2::terrain
