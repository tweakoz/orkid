////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
// NoiseModule — generator. One module, four NOISE-BASIS PRIMITIVES selected by a baked
// `_basis` enum (0 perlin / 1 simplex / 2 worley-F1 / 3 voronoi). Each is a pointwise
// function of the texel's domain position — no neighbour coupling, no iteration state —
// so this is the simplest module class (cf. FbmModule). `octaves` (default 1 = pure
// primitive) fBm-stacks the chosen basis the same way FbmModule does; `frequency`/
// `amplitude` are float plugs. The four basis functions live in a libblock the compute
// shader inherits (the shadlang way to share free functions in a compute kernel).
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::NoiseModuleData, "terrain::NoiseModuleData");

namespace ork::lev2::terrain {

// basis enum -> the libblock call substituted into the octave loop.
static const char* _noise_basis_call(int basis) {
  switch (basis) {
    case 1:  return "_nsimplex(p)";
    case 2:  return "_nworley(p)";
    case 3:  return "_nvoronoi(p)";
    default: return "_nperlin(p)";   // 0
  }
}

///////////////////////////////////////////////////////////////////////////////
// compute-shader source: DIM / FREQ / OCT / AMP / BASISCALL substituted in.
///////////////////////////////////////////////////////////////////////////////

// E.1b REALTIME PARAMS (mirrors FbmModule): frequency / amplitude / offset / warp_amt
// live in a params SSBO (binding 1) re-written from the DATA plugs every pre-phase —
// live-editable + offset_vel time pan. Only STRUCTURE bakes: dims, octave bound, basis,
// warp presence. When `warped`, wx/wy ride bindings 2/3.
static std::string _noise_compute_text(int dim, int octaves, int basis, bool warped) {
  std::string warp_sif  = warped
      ? "storage_interface sif_wx (descriptor_set 0) { buffer layout(std430) wx_in { float wxdata[%DIMSQ%]; }; }\n"
        "storage_interface sif_wy (descriptor_set 0) { buffer layout(std430) wy_in { float wydata[%DIMSQ%]; }; }\n"
      : "";
  std::string warp_list = warped ? " sif_wx sif_wy" : "";
  std::string warp_add  = warped
      ? " + p_wamt * vec2(wxdata[yi * %DIMU% + xi], wydata[yi * %DIMU% + xi])"
      : "";
  std::string tmpl = R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_hf (descriptor_set 0) {
  buffer layout(std430) hf_out { float heights[%DIMSQ%]; };
}
storage_interface sif_pm (descriptor_set 0) {
  buffer layout(std430) pm_in { float p_freq; float p_amp; float p_obx; float p_oby;
                                float p_wamt; float p_r0; float p_r1; float p_r2; };
}
%WARPSIF%compute_interface iface_hf {
  storage { sif_hf sif_pm%WARPLIST% }
  inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); }
}
libblock lib_tnoise {
  // signed 2D hash -> gradient / feature-point offset in [-1,1].
  vec2 _nhash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
  }
  vec3 _npermute(vec3 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
  // PERLIN — gradient (lattice) noise, quintic-smoothed; remapped to ~[0,1].
  float _nperlin(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = dot(_nhash2(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float b = dot(_nhash2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float c = dot(_nhash2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float d = dot(_nhash2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));
    float n = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    return 0.5 + 0.5 * n;
  }
  // SIMPLEX — Ashima/McEwan 2D simplex (webgl-noise); remapped to ~[0,1].
  float _nsimplex(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439);
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz; x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = _npermute(_npermute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m; m = m * m;
    vec3 x  = 2.0 * fract(p * C.www) - 1.0;
    vec3 h  = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 0.5 + 0.5 * (130.0 * dot(m, g));
  }
  // WORLEY (cellular F1) — distance to the nearest scattered feature point.
  float _nworley(vec2 p) {
    vec2 ip = floor(p); vec2 fp = fract(p);
    float f1 = 8.0;
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec2 g = vec2(float(i), float(j));
        vec2 o = 0.5 + 0.5 * _nhash2(ip + g);
        vec2 r = g + o - fp;
        f1 = min(f1, dot(r, r));
      }
    }
    return sqrt(f1);
  }
  // VORONOI — value of the nearest feature point's cell (flat random per cell).
  float _nvoronoi(vec2 p) {
    vec2 ip = floor(p); vec2 fp = fract(p);
    float f1 = 8.0; vec2 cell = vec2(0.0);
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec2 g = vec2(float(i), float(j));
        vec2 o = 0.5 + 0.5 * _nhash2(ip + g);
        vec2 r = g + o - fp;
        float d = dot(r, r);
        if (d < f1) { f1 = d; cell = ip + g; }
      }
    }
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
  }
}
compute_shader cs_noise : iface_hf : lib_tnoise {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  // domain offset (as FbmModule): keep texel (0,0) off the lattice origin so no fixed
  // point is reinforced across octaves (would be a degenerate sink for erosion). The user
  // `offset` folds in; the optional WARP term bends the base domain per-texel (every octave
  // inherits it through p *= 2.0).
  vec2 p = vec2(float(xi), float(yi)) / float(%DIM%) * p_freq + vec2(p_obx, p_oby)%WARPADD%;
  float sum = 0.0, ampl = 1.0, nrm = 0.0;
  for (int o = 0; o < %OCT%; o++) {     // octaves==1 -> pure primitive; >1 -> fBm-stacked
    sum += ampl * %BASISCALL%;
    nrm += ampl;
    ampl *= 0.5;
    p *= 2.0;
  }
  heights[yi * %DIMU% + xi] = (sum / nrm) * p_amp;
}
)SHADER";
  auto sub = [&](const std::string& key, const std::string& val) {
    size_t pos = 0;
    while ((pos = tmpl.find(key, pos)) != std::string::npos) {
      tmpl.replace(pos, key.size(), val);
      pos += val.size();
    }
  };
  // inject the optional-warp fragments FIRST (they contain %DIMSQ%/%DIMU% themselves).
  sub("%WARPSIF%", warp_sif);
  sub("%WARPLIST%", warp_list);
  sub("%WARPADD%", warp_add);
  sub("%DIMSQ%", FormatString("%d", dim * dim));
  sub("%DIMU%", FormatString("%du", dim));
  sub("%DIM%", FormatString("%d", dim));
  sub("%OCT%", FormatString("%d", octaves));
  sub("%BASISCALL%", _noise_basis_call(basis));
  return tmpl;
}

///////////////////////////////////////////////////////////////////////////////
// NoiseModule
///////////////////////////////////////////////////////////////////////////////

struct NoiseModuleInst : public TerrainComputeInst {
  NoiseModuleInst(const NoiseModuleData* data, dflow::GraphInst* ginst)
      : TerrainComputeInst(data, ginst)
      , _nmd(data) {
  }

  void onLink(dflow::GraphInst* inst) final {
    _output  = typedOutputNamed<HfImagePlugTraits>("Out");
    _inWarpX = typedInputNamed<HfImagePlugTraits>("warp_x");
    _inWarpY = typedInputNamed<HfImagePlugTraits>("warp_y");
  }

  // runtime params re-read from the DATA plugs each pre-phase (mirrors FbmModuleInst —
  // live-editable; offset_vel * env abstime = the clock-driven pan; bake stays t=0).
  void _fillParams(BakeEnv* env) {
    auto fxi      = env->_ctx->FXI();
    float freq    = *(_nmd->typedInputNamed<dflow::FloatPlugTraits>("frequency")->_value);
    float amp     = *(_nmd->typedInputNamed<dflow::FloatPlugTraits>("amplitude")->_value);
    fvec2 off     = *(_nmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset")->_value);
    fvec2 vel     = *(_nmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset_vel")->_value);
    float wamt    = *(_nmd->typedInputNamed<dflow::FloatPlugTraits>("warp_amt")->_value);
    float t       = float(env->_abstime);
    float pm[8]   = {freq, amp, 11.7f + off.x + vel.x * t, 31.3f + off.y + vel.y * t,
                     wamt, 0.0f, 0.0f, 0.0f};
    auto mp = fxi->mapStorageBuffer(_pm, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
  }

  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<BakeEnv>();
    auto fxi  = env->_ctx->FXI();
    int dim   = env->_w;
    auto img  = _output->_value;
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = env->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    // warp active only when BOTH displacement fields are connected (resolved pre-activate).
    // only STRUCTURE bakes into the text; scalars ride the params SSBO (seeded at t=0 here).
    _warped   = (_srcImg(_inWarpX) != nullptr) and (_srcImg(_inWarpY) != nullptr);
    auto text = _noise_compute_text(dim, _nmd->_octaves, _nmd->_basis, _warped);
    auto shdr = fxi->shaderFromShaderText("terrain_noise", text);
    _cs       = fxi->computeShader(shdr, "cs_noise");
    _pm       = env->createStorageBuffer(8 * sizeof(float));
    _fillParams(env.get());
  }

  // IPrePhaseParams — see FbmModuleInst.
  void writeParams(Context* ctx) final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    if (env and _pm)
      _fillParams(env.get());
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _pm);
    if (_warped) { // bindings 2/3 — per-texel domain displacement fields (wx/wy)
      ci->bindStorageBuffer(_cs, 2, _srcImg(_inWarpX)->_ssbo);
      ci->bindStorageBuffer(_cs, 3, _srcImg(_inWarpY)->_ssbo);
    }
    ci->dispatchCompute(_cs, groups, groups, 1);
    ci->storageBarrier();
  }

  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.noise.v3"); // v3: runtime params SSBO (E.1b)
    h->accumulateItem<int>(_nmd->_basis);
    h->accumulateItem<int>(_nmd->_octaves);
    h->accumulateItem<float>(*(_nmd->typedInputNamed<dflow::FloatPlugTraits>("frequency")->_value));
    h->accumulateItem<float>(*(_nmd->typedInputNamed<dflow::FloatPlugTraits>("amplitude")->_value));
    auto off = *(_nmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset")->_value);
    h->accumulateItem<float>(off.x);
    h->accumulateItem<float>(off.y);
    // offset_vel deliberately NOT hashed (t=0 invariant; see FbmModuleInst).
    h->accumulateItem<int>(_warped ? 1 : 0);
    if (_warped)
      h->accumulateItem<float>(*(_nmd->typedInputNamed<dflow::FloatPlugTraits>("warp_amt")->_value));
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const NoiseModuleData* _nmd;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _inWarpX, _inWarpY;
  FxShaderStorageBuffer* _pm = nullptr;
  bool _warped = false;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeNoiseIOs(dataflow::moduledata_ptr_t data) {
  auto freq = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "frequency");
  auto amp  = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amplitude");
  freq->setValue(4.0f);
  amp->setValue(1.0f);
  // constant domain offset (lattice-cell units) — pan / reseed the field.
  dflow::ModuleData::createInputPlug<dflow::Vec2fPlugTraits>(data, dflow::EPR_UNIFORM, "offset")->setValue(fvec2(0.0f, 0.0f));
  // E.1b: domain pan VELOCITY (cells/sec) — effective offset = offset + offset_vel * abstime (env clock).
  dflow::ModuleData::createInputPlug<dflow::Vec2fPlugTraits>(data, dflow::EPR_UNIFORM, "offset_vel")->setValue(fvec2(0.0f, 0.0f));
  // fused domain warp: per-texel (wx,wy) displacement fields + scalar amount. Both image
  // inputs unconnected -> plain noise (no warp storage interfaces emitted).
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "warp_x");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "warp_y");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "warp_amt")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}

NoiseModuleData::NoiseModuleData() {
}
std::shared_ptr<NoiseModuleData> NoiseModuleData::createShared() {
  auto data = std::make_shared<NoiseModuleData>();
  _reshapeNoiseIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t NoiseModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<NoiseModuleInst>(this, ginst);
}
void NoiseModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return NoiseModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeNoiseIOs(mdata); });
  // _basis (noise primitive) + _octaves are BAKED scalars (not plugs) -> reflect them so
  // the serialized graph self-describes (the JSON is the portable, python-decoupled artifact).
  clazz->directProperty("basis", &NoiseModuleData::_basis);
  clazz->directProperty("octaves", &NoiseModuleData::_octaves);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::terrain
