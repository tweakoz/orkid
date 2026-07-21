////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::FbmModuleData, "terrain::FbmModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// the compute-shader source for fbm. E.1b REALTIME PARAMS: frequency / amplitude /
// domain offset / warp amount are NO LONGER baked into the text — they live in a small
// params SSBO (binding 1) re-written from the DATA plugs every pre-phase (writeParams),
// so a plug poke (editor / Python / the clock-driven offset_vel pan) lands next eval
// with no recompile. Only STRUCTURE stays baked: the octave loop bound and
// whether the warp inputs exist.
///////////////////////////////////////////////////////////////////////////////

static std::string _fbm_compute_text(int octaves, bool warped) {
  // optional warp: extra storage interfaces + the per-texel displacement term on `p`.
  // DIM is RUNTIME data (params SSBO p_dimf) — dim changes never rebuild the shader;
  // the storage arrays are runtime-sized.
  std::string warp_sif  = warped
      ? "storage_interface sif_wx (descriptor_set 0) { buffer layout(std430) wx_in { float wxdata[]; }; }\n"
        "storage_interface sif_wy (descriptor_set 0) { buffer layout(std430) wy_in { float wydata[]; }; }\n"
      : "";
  std::string warp_list = warped ? " sif_wx sif_wy" : "";
  std::string warp_add  = warped
      ? " + p_wamt * vec2(wxdata[yi * u_dim + xi], wydata[yi * u_dim + xi])"
      : "";
  std::string tmpl = R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_hf (descriptor_set 0) {
  buffer layout(std430) hf_out { float heights[]; };
}
storage_interface sif_pm (descriptor_set 0) {
  buffer layout(std430) pm_in { float p_freq; float p_amp; float p_obx; float p_oby;
                                float p_wamt; float p_r0; float p_dimf; float p_r2; };
}
%WARPSIF%compute_interface iface_hf {
  storage { sif_hf sif_pm%WARPLIST% }
  inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); }
}
libblock lib_fbmhash {
  // DETERMINISTIC integer lattice hash (lowbias32 mix) — replaces
  // fract(sin(dot(..))*43758..), whose vendor-approximated sin() made mac and linux
  // generate entirely different noise from identical params (the cross-platform
  // terrain divergence). The seed is RUNTIME data (params SSBO p_r0);
  // seed changes never rebuild the shader.
  float _fbmhash21(vec2 cell, uint seed) { // [0,1)
    uvec2 q = uvec2(ivec2(cell)) * uvec2(1597334673u, 3812015801u);
    uint n = q.x ^ q.y ^ (seed * 0x9e3779b9u);
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; n ^= n >> 16;
    return float(n >> 8) * (1.0 / 16777216.0);
  }
}
compute_shader cs_fbm : iface_hf : lib_fbmhash {
  // CROSS-PLATFORM BIT-PARITY (the precise-qualifier experiment): every float in the
  // octave chain is `precise` -> glslang decorates the contributing ops
  // NoContraction in SPIR-V, forcing NVIDIA and Metal to the same fma/contraction
  // behavior. Without it the two backends drift ~3.6e-07 on fbm_0 and downstream
  // threshold/erode nodes amplify the ULPs into boundary flips.
  uint u_dim = uint(p_dimf);   // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  // domain offset: keep texel (0,0) off the lattice origin so no texel maps to a FIXED
  // lattice point across octaves (a reinforced value there becomes a degenerate sink
  // erosion deepens into a crater — originally sin(0)=0 pinned it to the global min;
  // the integer hash un-pins the VALUE but the offset stays: fixed-point reinforcement
  // is basis-independent).
  // p_obx/p_oby = the anti-degeneracy base + user offset + offset_vel*time (HOST-composed);
  // the optional WARP term bends the base domain per-texel BEFORE the octave loop.
  // * (1.0/p_dimf), not / p_dimf: with the old LITERAL dim the compiler folded the
  // divide into a reciprocal multiply — replicate it so runtime-dim output stays
  // bit-identical to the v6 baked-dim caches.
  precise vec2 p = vec2(float(xi), float(yi)) * (1.0 / p_dimf) * p_freq + vec2(p_obx, p_oby)%WARPADD%;
  uint sd = uint(p_r0); // lattice-hash seed — runtime data, no shader rebuild on change
  precise float sum = 0.0;
  precise float ampl = 1.0;
  precise float nrm = 0.0;
  for (int o = 0; o < %OCT%; o++) {
    precise vec2 ip = floor(p);
    precise vec2 fp = fract(p);
    precise vec2 u  = fp * fp * (3.0 - 2.0 * fp);
    precise float a = _fbmhash21(ip + vec2(0.0, 0.0), sd);
    precise float b = _fbmhash21(ip + vec2(1.0, 0.0), sd);
    precise float c = _fbmhash21(ip + vec2(0.0, 1.0), sd);
    precise float d = _fbmhash21(ip + vec2(1.0, 1.0), sd);
    precise float n = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    sum += ampl * n;
    nrm += ampl;
    ampl *= 0.5;
    p *= 2.0;
  }
  precise float outh = (sum / nrm) * p_amp;
  heights[yi * u_dim + xi] = outh;
}
)SHADER";
  auto sub = [&](const std::string& key, const std::string& val) {
    size_t pos = 0;
    while ((pos = tmpl.find(key, pos)) != std::string::npos) {
      tmpl.replace(pos, key.size(), val);
      pos += val.size();
    }
  };
  sub("%WARPSIF%", warp_sif);
  sub("%WARPLIST%", warp_list);
  sub("%WARPADD%", warp_add);
  sub("%OCT%", FormatString("%d", octaves));
  return tmpl;
}

///////////////////////////////////////////////////////////////////////////////
// FbmModule
///////////////////////////////////////////////////////////////////////////////

struct FbmModuleInst : public TerrainComputeInst {
  FbmModuleInst(const FbmModuleData* data, dflow::GraphInst* ginst)
      : TerrainComputeInst(data, ginst)
      , _fmd(data) {
  }

  void onLink(dflow::GraphInst* inst) final {
    _output  = typedOutputNamed<HfImagePlugTraits>("Out");
    _inWarpX = typedInputNamed<HfImagePlugTraits>("warp_x");
    _inWarpY = typedInputNamed<HfImagePlugTraits>("warp_y");
  }

  // runtime params are re-read from the DATA plugs (the pokeable channel — m.inputs.* sets
  // the moduledata; the inst snapshots here, the inset/extrude idiom). offset_vel * abstime
  // composes the clock-driven pan (env clock: 0 in a bake -> deterministic t=0 snapshot).
  void _fillParams(BakeEnv* env) {
    auto fxi      = env->_ctx->FXI();
    float freq    = *(_fmd->typedInputNamed<dflow::FloatPlugTraits>("frequency")->_value);
    float amp     = *(_fmd->typedInputNamed<dflow::FloatPlugTraits>("amplitude")->_value);
    fvec2 off     = *(_fmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset")->_value);
    fvec2 vel     = *(_fmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset_vel")->_value);
    float wamt    = *(_fmd->typedInputNamed<dflow::FloatPlugTraits>("warp_amt")->_value);
    float t       = float(env->_abstime);
    float pm[8]   = {freq, amp,
                     11.7f + off.x + vel.x * t,  // anti-degeneracy base + user offset + pan
                     31.3f + off.y + vel.y * t,
                     wamt, float(_fmd->_seed),   // p_r0 = lattice-hash seed
                     float(env->_w), 0.0f};      // p_dimf = RUNTIME grid dim
    auto mp = fxi->mapStorageBuffer(_pm, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
  }

  // SETUP (SSBO alloc + shader compile) happens here — under a legacy/live driver the
  // onActivate shim runs it during updateTopology, BEFORE the bake's beginFrame; under
  // the frontier bake driver it runs just before this node's dispatch phase (either
  // way, never mid-phase). Only STRUCTURE bakes into the text (dims, octave loop
  // bound, warp presence); dim + the scalar params go to the params SSBO, seeded here at
  // t=0 and re-written per eval by writeParams.
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<BakeEnv>();
    auto fxi  = env->_ctx->FXI();
    int dim   = env->_w;
    auto img  = _output->_value; // GpuComputeImage2DInst (created via data_to_inst)
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = env->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    // connections are resolved in updateTopology BEFORE activate(), so the warp inputs
    // are known here: warp is active only when BOTH displacement fields are connected.
    _warped   = (_srcImg(_inWarpX) != nullptr) and (_srcImg(_inWarpY) != nullptr);
    auto text = _fbm_compute_text(_fmd->_octaves, _warped);
    auto shdr = fxi->shaderFromShaderText("terrain_fbm", text);
    _cs       = fxi->computeShader(shdr, "cs_fbm");
    _pm       = env->createStorageBuffer(8 * sizeof(float));
    _fillParams(env.get());
  }

  // IPrePhaseParams — the live host (and the bake driver, uniformly) calls this before
  // each dispatch phase: params re-read from the plugs -> live-editable + time-driven.
  void writeParams(Context* ctx) final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    if (env and _pm)
      _fillParams(env.get());
  }

  // DISPATCH only (inside the dispatch phase). A barrier after each dispatch makes
  // this module's writes visible to downstream modules' reads.
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _pm);
    if (_warped) { // bindings 2/3 — the per-texel domain displacement fields (wx/wy)
      ci->bindStorageBuffer(_cs, 2, _srcImg(_inWarpX)->_ssbo);
      ci->bindStorageBuffer(_cs, 3, _srcImg(_inWarpY)->_ssbo);
    }
    ci->dispatchCompute(_cs, groups, groups, 1);
    ci->storageBarrier();
  }

  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.fbm.v6"); // v6: precise/NoContraction octave chain (cross-platform fma parity); v5: integer lattice hash
    h->accumulateItem<int>(_fmd->_octaves);
    h->accumulateItem<int>(_fmd->_seed);
    h->accumulateItem<float>(*(_fmd->typedInputNamed<dflow::FloatPlugTraits>("frequency")->_value));
    h->accumulateItem<float>(*(_fmd->typedInputNamed<dflow::FloatPlugTraits>("amplitude")->_value));
    auto off = *(_fmd->typedInputNamed<dflow::Vec2fPlugTraits>("offset")->_value);
    h->accumulateItem<float>(off.x);
    h->accumulateItem<float>(off.y);
    // offset_vel deliberately NOT hashed: a bake is the t=0 snapshot, where the output is
    // velocity-invariant — hashing it would only split otherwise-identical cache entries.
    h->accumulateItem<int>(_warped ? 1 : 0);
    if (_warped)
      h->accumulateItem<float>(*(_fmd->typedInputNamed<dflow::FloatPlugTraits>("warp_amt")->_value));
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const FbmModuleData* _fmd;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _inWarpX, _inWarpY;
  FxShaderStorageBuffer* _pm = nullptr;
  bool _warped = false;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeFbmIOs(dataflow::moduledata_ptr_t data) {
  auto freq = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "frequency");
  auto amp  = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amplitude");
  freq->setValue(4.0f);
  amp->setValue(1.0f);
  // constant domain offset (lattice-cell units) — pan / reseed the field.
  dflow::ModuleData::createInputPlug<dflow::Vec2fPlugTraits>(data, dflow::EPR_UNIFORM, "offset")->setValue(fvec2(0.0f, 0.0f));
  // E.1b: domain pan VELOCITY (lattice-cells / second) — effective offset = offset + offset_vel * abstime,
  // fed by the family env clock (B.4). Zero = static. Serializes as a plug value (declarative animation).
  // DISPLAY-ONLY / bake-inert: a bake is the t=0 snapshot (offset_vel deliberately un-hashed) — the editor
  // marks the row honestly distinct (drives the LIVE pan only, never the baked field).
  auto offset_vel = dflow::ModuleData::createInputPlug<dflow::Vec2fPlugTraits>(data, dflow::EPR_UNIFORM, "offset_vel");
  offset_vel->setValue(fvec2(0.0f, 0.0f));
  offset_vel->markDisplayOnly();
  // fused domain warp: per-texel (wx,wy) displacement fields + scalar amount. Both image
  // inputs unconnected -> plain fbm (no warp storage interfaces emitted).
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "warp_x");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "warp_y");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "warp_amt")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}

FbmModuleData::FbmModuleData() {
}
std::shared_ptr<FbmModuleData> FbmModuleData::createShared() {
  auto data = std::make_shared<FbmModuleData>();
  _reshapeFbmIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t FbmModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<FbmModuleInst>(this, ginst);
}
void FbmModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return FbmModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeFbmIOs(mdata); });
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the
  // vocabulary). source = a GENERATOR: inserted with no input, starting a new branch.
  clazz->annotateTyped<ConstString>("dsl.verb", "fbm");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 11);
  clazz->annotateTyped<bool>("editor.palette.source", true);
  // _octaves is a BAKED loop bound (not a plug) — reflect it so the serialized
  // graph self-describes (the JSON is the portable, python-decoupled artifact).
  clazz->directProperty("octaves", &FbmModuleData::_octaves);
  clazz->directProperty("seed", &FbmModuleData::_seed);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::terrain
