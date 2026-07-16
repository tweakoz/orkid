////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::LpfModuleData, "terrain::LpfModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// LpfModule — separable GAUSSIAN low-pass filter. `cutoff_texels` (or `cutoff_m`,
// resolution-independent) is the spatial cutoff scale: features finer than that
// wavelength are attenuated (Gaussian sigma = cutoff/6; soft rolloff, no ringing).
// Two passes (horizontal then vertical), edge-clamped. `blend` (0..1, default 1.0)
// crossfades the filtered result against the ORIGINAL input (0 = passthrough,
// 1 = fully filtered), applied ONCE in the vertical pass.
//
// RUNTIME PARAMS: sigma, blend, AND grid dim live in a small params SSBO (si_p) read at
// compute, NOT baked into the shader text (mirrors erox/pha). Only the loop RADIUS must
// bake (GLSL loop bounds are compile-time), so it's BUCKETED to the next power of two:
// every cutoff that maps to the same radius bucket shares one compiled shader. A loop
// that sweeps cutoff (e.g. cutoff_m = base - i*step) therefore compiles ONE shader for
// the whole sweep instead of one per value.
///////////////////////////////////////////////////////////////////////////////

static std::string _lpf_text(const char* name, bool horizontal, int rmax) {
  // one separable axis: out[i] = sum_{d=-R..R} in[clamped neighbor] * gauss(d), normalized.
  // gauss weight uses a RUNTIME inv2s2 = 1/(2*sigma^2) from the params SSBO; rmax is baked.
  // DIM is RUNTIME data too (params SSBO P[2]) — dim changes never rebuild the shader; the
  // field arrays are runtime-sized.
  const char* idx = horizontal ? "uint(yi)*u_dim + uint(clamp(xi+d,0,W-1))"
                               : "uint(clamp(yi+d,0,W-1))*u_dim + uint(xi)";
  std::string storages =
    "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }\n"
    "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }\n"
    "storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float P[4]; }; }\n";
  std::string silist = "si_o si_i si_p";                    // bindings: 0=o 1=i 2=p
  if (!horizontal) {
    // vertical pass also reads the ORIGINAL input (gdata) so it can blend once at the end.
    storages +=
      "storage_interface si_g (descriptor_set 0) { buffer layout(std430) gb { float gdata[]; }; }\n";
    silist = "si_o si_i si_g si_p";                         // bindings: 0=o 1=i 2=g 3=p
  }
  std::string t = std::string("\nfxconfig fxcfg_default {}\n") + storages +
    "compute_interface iface { storage { " + silist + " } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader " + name + " : iface {\n"
    "  uint u_dim = uint(P[2]);\n"                                                // RUNTIME grid dim (params SSBO) — no rebuild on dim change
    "  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(u_dim);\n"
    "  uint i  = uint(yi) * u_dim + uint(xi);\n"
    "  float inv2s2 = P[0];\n"                                                    // runtime gaussian: 1/(2*sigma^2)
    "  float sum = 0.0; float wsum = 0.0;\n"
    "  for (int d = -%RMAX%; d <= %RMAX%; d++) {\n"                               // RMAX baked (loop bound); weight is runtime
    "    float w = exp(-float(d*d) * inv2s2);\n"
    "    sum  = sum + idata[" + idx + "] * w;\n"
    "    wsum = wsum + w;\n"
    "  }\n"
    "  float filt = sum / wsum;\n";
  if (horizontal)
    t += "  odata[i] = filt;\n";
  else
    t += "  float blend = P[1];\n"
         "  odata[i] = mix(gdata[i], filt, blend);\n";                           // crossfade vs ORIGINAL, once
  t += "}\n";
  _shadersub(t, "%RMAX%", FormatString("%d", rmax)); // last (no prefix collision)
  return t;
}

struct LpfModuleInst : public TerrainComputeInst {
  LpfModuleInst(const LpfModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output  = typedOutputNamed<HfImagePlugTraits>("Out");
    _input   = typedInputNamed<HfImagePlugTraits>("In");
    _cutoff  = _floatPlug(this, _d, "cutoff_texels");
    _cutoffM = _floatPlug(this, _d, "cutoff_m");
    _blend   = _floatPlug(this, _d, "blend");
  }
  static int _nextPow2(int v) { int p = 1; while (p < v) p <<= 1; return p; }
  // cutoff_m (meters) takes precedence when > 0 -> RESOLUTION-INDEPENDENT (converted to texels
  // per-bake from dim/extent). Else cutoff_texels. The wavelength -> gaussian sigma (texels).
  float _sigmaTexels(BakeEnv* env) const {
    float cutoff_texels = (_cutoffM->value() > 0.0f) ? (_cutoffM->value() * env->texelsPerMeter())
                                                     : _cutoff->value();
    return std::max(cutoff_texels / 6.0f, 0.25f);
  }
  // sigma + blend -> params SSBO. Filled in bakeAcquire (pre-dispatch-phase: a host map mid-phase
  // is not visible). sigma clamped so 3*sigma stays inside the baked loop bound.
  void _fillParams(BakeEnv* env) {
    float sigma  = std::min(_sigmaTexels(env), float(_rmax) / 3.0f);
    float inv2s2 = 1.0f / (2.0f * sigma * sigma);
    float blend  = std::min(std::max(_blend->value(), 0.0f), 1.0f);
    float P[4]   = {inv2s2, blend, float(env->_w), 0.0f}; // P[2] = RUNTIME grid dim
    auto fxi = env->_ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    _allocOut(env.get(), _output->_value);
    _tmp    = env->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));
    _params = env->createStorageBuffer(4 * sizeof(float)); // {inv2s2, blend, dim, _}; filled below
    // RADIUS must bake (loop bound), so BUCKET it to the next power of two (min 4): every cutoff
    // in a bucket shares one shader -> a cutoff sweep compiles one shader, not one per value.
    float sigma = _sigmaTexels(env.get());
    int rmaxGrid = std::max(1, dim / 2 - 1);
    int R = std::max(1, int(std::ceil(3.0f * sigma)));
    _rmax = std::min(std::max(_nextPow2(R), 4), rmaxGrid);
    _csH = fxi->computeShader(fxi->shaderFromShaderText("terrain_lpf_h", _lpf_text("cs_lpf_h", true,  _rmax)), "cs_lpf_h");
    _csV = fxi->computeShader(fxi->shaderFromShaderText("terrain_lpf_v", _lpf_text("cs_lpf_v", false, _rmax)), "cs_lpf_v");
    _fillParams(env.get()); // pre-dispatch-phase host write (visible to the GPU)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    // pass 1: horizontal blur in->tmp
    ci->bindStorageBuffer(_csH, 0, _tmp);
    ci->bindStorageBuffer(_csH, 1, in->_ssbo);
    ci->bindStorageBuffer(_csH, 2, _params);
    ci->dispatchCompute(_csH, g, g, 1);
    ci->endDispatchPhase(); ci->beginDispatchPhase(); // submit (tmp must be complete before V reads it)
    // pass 2: vertical blur tmp->out, crossfaded against the ORIGINAL input (blend)
    ci->bindStorageBuffer(_csV, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_csV, 1, _tmp);
    ci->bindStorageBuffer(_csV, 2, in->_ssbo);
    ci->bindStorageBuffer(_csV, 3, _params);
    ci->dispatchCompute(_csV, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.lpf.v3"); // runtime sigma+blend (params SSBO), bucketed radius
    h->accumulateItem<float>(_cutoff->value());
    h->accumulateItem<float>(_cutoffM->value()); // meters (resolution-independent identity)
    h->accumulateItem<float>(_blend->value());   // crossfade vs original
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const LpfModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _cutoff, _cutoffM, _blend;
  FxShaderStorageBuffer* _tmp = nullptr;
  FxShaderStorageBuffer* _params = nullptr;
  int _rmax = 4;
  const FxComputeShader *_csH = nullptr, *_csV = nullptr;
};

static void _reshapeLpfIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // cutoff scale in TEXELS (features finer than ~this wavelength are attenuated).
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "cutoff_texels")->setValue(8.0f);
  // cutoff scale in METERS — when > 0 it OVERRIDES cutoff_texels and is resolution-INDEPENDENT
  // (converted to texels per-bake from dim/extent). Default 0 = use cutoff_texels.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "cutoff_m")->setValue(0.0f);
  // crossfade filtered vs original: 0 = passthrough, 1 = fully filtered (default).
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "blend")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LpfModuleData::LpfModuleData() {}
std::shared_ptr<LpfModuleData> LpfModuleData::createShared() {
  auto d = std::make_shared<LpfModuleData>(); _reshapeLpfIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t LpfModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LpfModuleInst>(this, g);
}
void LpfModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LpfModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeLpfIOs(m); });
}

} // namespace ork::lev2::terrain
