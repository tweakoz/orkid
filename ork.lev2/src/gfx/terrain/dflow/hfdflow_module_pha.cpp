////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::PhaModuleData, "terrain::PhaModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// PhaModule — PROCEDURAL "phacelle" erosion FILTER (Rune Skovbo Johansen's "fast and
// gorgeous erosion"; https://blog.runevision.com/2026/03/fast-and-gorgeous-erosion-
// filter.html, MPL-2.0). Unlike erox (an iterative Mei SIM) this is a SINGLE-PASS
// analytic filter: it stacks `octaves` of faded gullies built from "phacelle noise"
// (slope-aligned stripe patterns blended over a 4x4 cell neighborhood), carving
// drainage-like gullies along the downslope direction. Because it is a smooth function
// of continuous uv, it is naturally RESOLUTION-INDEPENDENT (like fbm) and SPECKLE-FREE
// (no grid instability), fast (one dispatch), and predictable. 2 SSBOs (out + in) + a
// params SSBO (tweak without recompile). octaves is a baked loop bound; the rest are
// physical-ish float plugs in P[16]:
//   0 strength 1 gully_weight 2 detail 3 scale 4 cell_scale 5 normalization
//   6 lacunarity 7 gain 8 default_height
///////////////////////////////////////////////////////////////////////////////

static std::string _pha_text(int dim, int octaves) {
  std::string t = R"PHA(
fxconfig fxcfg_default {}
storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float P[16]; }; }
compute_interface iface { storage { si_o si_i si_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
libblock lib_pha {
  vec2 phash2(vec2 p) { // -> [-1,1]^2 (Dave Hoskins hash22)
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * vec3(0.1031, 0.1030, 0.0973));
    p3 = p3 + dot(p3, vec3(p3.y, p3.z, p3.x) + 33.33);
    return -1.0 + 2.0 * fract(vec2((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y));
  }
  float clamp01(float t) { return clamp(t, 0.0, 1.0); }
  float pow_inv(float t, float power) { return 1.0 - pow(1.0 - clamp01(t), power); }
  float ease_out(float t) { float v = 1.0 - clamp01(t); return 1.0 - v * v; }
  float smooth_start(float t, float smoothing) {
    if (t >= smoothing) { return t - 0.5 * smoothing; }
    return 0.5 * t * t / max(smoothing, 1e-9);
  }
  vec2 safe_normalize(vec2 n) {
    float l = length(n);
    return (abs(l) > 1e-10) ? (n / l) : n;
  }
  // phacelle noise: slope-aligned stripe pattern blended over a 4x4 cell neighborhood.
  vec4 PhacelleNoise(vec2 p, vec2 normDir, float freq, float offset, float normalization) {
    vec2 sideDir = vec2(-normDir.y, normDir.x) * freq * 6.28318530717959;
    offset = offset * 6.28318530717959;
    vec2 pInt = floor(p);
    vec2 pFrac = fract(p);
    vec2 phaseDir = vec2(0.0);
    float weightSum = 0.0;
    for (int ci = -1; ci <= 2; ci++) {
      for (int cj = -1; cj <= 2; cj++) {
        vec2 gridOffset = vec2(float(ci), float(cj));
        vec2 randomOffset = phash2(pInt + gridOffset) * 0.5;
        vec2 fromCell = pFrac - gridOffset - randomOffset;
        float sqrDist = dot(fromCell, fromCell);
        float weight = max(0.0, exp(-sqrDist * 2.0) - 0.01111);
        weightSum = weightSum + weight;
        float waveInput = dot(fromCell, sideDir) + offset;
        phaseDir = phaseDir + vec2(cos(waveInput), sin(waveInput)) * weight;
      }
    }
    vec2 interpolated = phaseDir / max(weightSum, 1e-9);
    float magnitude = max(1.0 - normalization, sqrt(dot(interpolated, interpolated)));
    return vec4(interpolated / magnitude, sideDir);
  }
  // erosion filter: stack OCT octaves of faded gullies onto heightAndSlope (x=height,
  // yz=slope). Returns delta(height,slope) in xyz, accumulated magnitude in w.
  vec4 ErosionFilter(vec2 p, vec3 heightAndSlope, float fadeTarget,
                     float strength, float gullyWeight, float detail,
                     vec4 rounding, vec4 onset, vec2 assumedSlope,
                     float scale, float lacunarity, float gain, float cellScale, float normalization) {
    strength = strength * scale;
    fadeTarget = clamp(fadeTarget, -1.0, 1.0);
    vec3 inputHAS = heightAndSlope;
    float freq = 1.0 / (scale * cellScale);
    float slopeLength = max(length(vec2(heightAndSlope.y, heightAndSlope.z)), 1e-10);
    float magnitude = 0.0;
    float roundingMult = 1.0;
    float roundingForInput = mix(rounding.y, rounding.x, clamp01(fadeTarget + 0.5)) * rounding.z;
    float combiMask = ease_out(smooth_start(slopeLength * onset.x, roundingForInput * onset.x));
    float ridgeMapCombiMask = ease_out(slopeLength * onset.z);
    float ridgeMapFadeTarget = fadeTarget;
    vec2 hslope = vec2(heightAndSlope.y, heightAndSlope.z);
    vec2 gullySlope = mix(hslope, hslope / slopeLength * assumedSlope.x, assumedSlope.y);
    for (int oi = 0; oi < %OCT%; oi++) {
      vec4 phacelle = PhacelleNoise(p * freq, safe_normalize(gullySlope), cellScale, 0.25, normalization);
      vec2 pzw = vec2(phacelle.z, phacelle.w) * (-freq); // multiply by freq (p was scaled); negate -> downslope
      float sloping = abs(phacelle.y);
      gullySlope = gullySlope + sign(phacelle.y) * pzw * strength * gullyWeight;
      vec3 gullies = vec3(phacelle.x, phacelle.y * pzw.x, phacelle.y * pzw.y);
      vec3 fadedGullies = mix(vec3(fadeTarget, 0.0, 0.0), gullies * gullyWeight, combiMask);
      heightAndSlope = heightAndSlope + fadedGullies * strength;
      magnitude = magnitude + strength;
      fadeTarget = fadedGullies.x;
      float roundingForOctave = mix(rounding.y, rounding.x, clamp01(phacelle.x + 0.5)) * roundingMult;
      float newMask = ease_out(smooth_start(sloping * onset.y, roundingForOctave * onset.y));
      combiMask = pow_inv(combiMask, detail) * newMask;
      ridgeMapFadeTarget = mix(ridgeMapFadeTarget, gullies.x, ridgeMapCombiMask);
      ridgeMapCombiMask = ridgeMapCombiMask * ease_out(sloping * onset.w);
      strength = strength * gain;
      freq = freq * lacunarity;
      roundingMult = roundingMult * rounding.w;
    }
    return vec4(heightAndSlope - inputHAS, magnitude);
  }
}
compute_shader cs_pha : iface : lib_pha {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%);
  uint i  = uint(yi) * %DIMU% + uint(xi);
  float STRENGTH=P[0]; float GULLYW=P[1]; float DETAIL=P[2]; float SCALE=P[3];
  float CELLSCALE=P[4]; float NORMALIZ=P[5]; float LAC=P[6]; float GAIN=P[7]; float DEFH=P[8];
  float h  = idata[i];
  float hL = (xi>0)   ? idata[i-1u] : h;
  float hR = (xi<W-1) ? idata[i+1u] : h;
  float hD = (yi>0)   ? idata[i-uint(W)] : h;
  float hU = (yi<W-1) ? idata[i+uint(W)] : h;
  float gx = (hR - hL) * 0.5 * float(%DIM%); // d height / d uv
  float gy = (hU - hD) * 0.5 * float(%DIM%);
  vec3  has = vec3(h, gx, gy);
  float fadeTarget = clamp((h - DEFH) / 0.15, -1.0, 1.0);
  vec4  ROUND  = vec4(0.1, 0.0, 0.1, 2.0); // demo defaults (ridge,crease,init-mult,octave-mult)
  vec4  ONSET  = vec4(0.7, 1.25, 2.8, 1.5);
  vec2  ASLOPE = vec2(0.7, 1.0);
  vec2  p = vec2(float(xi), float(yi)) / float(%DIM%);
  vec4  hd = ErosionFilter(p, has, fadeTarget, STRENGTH, GULLYW, DETAIL, ROUND, ONSET, ASLOPE, SCALE, LAC, GAIN, CELLSCALE, NORMALIZ);
  odata[i] = h + hd.x;
}
)PHA";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%DIM%", FormatString("%d", dim));
  _shadersub(t, "%OCT%", FormatString("%d", octaves));
  return t;
}

struct PhaModuleInst : public TerrainComputeInst {
  PhaModuleInst(const PhaModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output   = typedOutputNamed<HfImagePlugTraits>("Out");
    _input    = typedInputNamed<HfImagePlugTraits>("In");
    _strength = _floatPlug(this, _d, "strength");
    _gully    = _floatPlug(this, _d, "gully_weight");
    _detail   = _floatPlug(this, _d, "detail");
    _scale    = _floatPlug(this, _d, "scale");
    _cell     = _floatPlug(this, _d, "cell_scale");
    _norm     = _floatPlug(this, _d, "normalization");
    _lac      = _floatPlug(this, _d, "lacunarity");
    _gain     = _floatPlug(this, _d, "gain");
    _defh     = _floatPlug(this, _d, "default_height");
  }
  void _fillParams(BakeEnv* env) {
    float P[16] = {0};
    P[0] = _strength->value();
    P[1] = _gully->value();
    P[2] = _detail->value();
    P[3] = _scale->value();
    P[4] = _cell->value();
    P[5] = _norm->value();
    P[6] = _lac->value();
    P[7] = _gain->value();
    P[8] = _defh->value();
    auto fxi = env->_ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    _params = env->createStorageBuffer(16 * sizeof(float));
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_pha", _pha_text(env->_w, _d->_octaves));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_pha");
    _fillParams(env.get()); // pre-dispatch-phase fill (a host map mid-phase is not visible)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);              // idata
    ci->bindStorageBuffer(_cs, 2, _params);                // P
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.pha.v1"); // procedural phacelle erosion filter
    h->accumulateItem<int>(_d->_octaves);
    h->accumulateItem<float>(_strength->value());
    h->accumulateItem<float>(_gully->value());
    h->accumulateItem<float>(_detail->value());
    h->accumulateItem<float>(_scale->value());
    h->accumulateItem<float>(_cell->value());
    h->accumulateItem<float>(_norm->value());
    h->accumulateItem<float>(_lac->value());
    h->accumulateItem<float>(_gain->value());
    h->accumulateItem<float>(_defh->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const PhaModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _strength, _gully, _detail, _scale, _cell, _norm, _lac, _gain, _defh;
  FxShaderStorageBuffer* _params = nullptr;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapePhaIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "strength")->setValue(0.22f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "gully_weight")->setValue(0.5f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "detail")->setValue(1.5f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "scale")->setValue(0.15f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "cell_scale")->setValue(0.7f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "normalization")->setValue(0.5f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "lacunarity")->setValue(2.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "gain")->setValue(0.5f);
  // mid-height reference: height that maps to fadeTarget 0 (valley=-1 .. peak=+1 over +/-0.15)
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "default_height")->setValue(0.5f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
PhaModuleData::PhaModuleData() {}
std::shared_ptr<PhaModuleData> PhaModuleData::createShared() {
  auto d = std::make_shared<PhaModuleData>(); _reshapePhaIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t PhaModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<PhaModuleInst>(this, g);
}
void PhaModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return PhaModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapePhaIOs(m); });
  clazz->directProperty("octaves", &PhaModuleData::_octaves); // baked loop bound
}

} // namespace ork::lev2::terrain
