////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Heightfield compute-dataflow — first slice. fbm -> SSBO -> readback -> EXR,
// authored as a serializable ork::dataflow GraphData of compute modules.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>
#include <ork/lev2/gfx/image.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <ork/kernel/string/string.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/kernel/datacache.h> // DataBlockCache — per-node cook cache

ImplementReflectionX(ork::lev2::terrain::TerrainModuleData, "terrain::TerrainModuleData");
ImplementReflectionX(ork::lev2::terrain::FbmModuleData, "terrain::FbmModuleData");
ImplementReflectionX(ork::lev2::terrain::RemapModuleData, "terrain::RemapModuleData");
ImplementReflectionX(ork::lev2::terrain::ConstModuleData, "terrain::ConstModuleData");
ImplementReflectionX(ork::lev2::terrain::GradientModuleData, "terrain::GradientModuleData");
ImplementReflectionX(ork::lev2::terrain::CombineModuleData, "terrain::CombineModuleData");
ImplementReflectionX(ork::lev2::terrain::TerraceModuleData, "terrain::TerraceModuleData");
ImplementReflectionX(ork::lev2::terrain::SlopeModuleData, "terrain::SlopeModuleData");
ImplementReflectionX(ork::lev2::terrain::CurvatureModuleData, "terrain::CurvatureModuleData");
ImplementReflectionX(ork::lev2::terrain::MaskBlendModuleData, "terrain::MaskBlendModuleData");
ImplementReflectionX(ork::lev2::terrain::ThermalErodeModuleData, "terrain::ThermalErodeModuleData");
ImplementReflectionX(ork::lev2::terrain::HydroErodeModuleData, "terrain::HydroErodeModuleData");
ImplementReflectionX(ork::lev2::terrain::CaptureModuleData, "terrain::CaptureModuleData");

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////

gpucomputeimage2d_inst_ptr_t HfImagePlugTraits::data_to_inst(gpucomputeimage2d_data_ptr_t inp) {
  return std::make_shared<GpuComputeImage2DInst>(inp);
}

///////////////////////////////////////////////////////////////////////////////
// bases
///////////////////////////////////////////////////////////////////////////////

void TerrainModuleData::describeX(class_t* clazz) {
}
TerrainModuleData::TerrainModuleData() {
}

///////////////////////////////////////////////////////////////////////////////
// TerrainComputeInst — shared base for the GPU compute ops (everything but the
// Capture sink). Holds the output image plug `_output` and implements the cook-
// cache hooks: a node's output is the W*H float field in its SSBO, (de)serialized
// to a datablock. The bake driver syncs per op, so cookStore reads valid data.
// cookLoad uploads a cached field so downstream ops consume it without recompute.
///////////////////////////////////////////////////////////////////////////////

struct TerrainComputeInst : public dflow::DgModuleInst {
  TerrainComputeInst(const dflow::DgModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g) {
  }
  // the node's output field (resolved by name so we don't shadow each derived
  // inst's own _output member).
  gpucomputeimage2d_inst_ptr_t _outImg() const {
    auto self = const_cast<TerrainComputeInst*>(this);
    auto outp = self->typedOutputNamed<HfImagePlugTraits>("Out");
    return outp ? outp->_value : nullptr;
  }

  datablock_ptr_t cookStore() const final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    auto img = _outImg();
    if (not(img and img->_ssbo))
      return nullptr;
    size_t n     = size_t(img->_w) * size_t(img->_h);
    auto fxi     = env->_ctx->FXI();
    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    auto db      = std::make_shared<DataBlock>();
    db->addItem<int>(img->_w);
    db->addItem<int>(img->_h);
    db->addItem<int>(img->_channels);
    db->addData(mapping->_mappedaddr, n * sizeof(float));
    fxi->unmapStorageBuffer(mapping.get());
    return db;
  }

  bool cookLoad(datablock_constptr_t db) final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    auto img = _outImg();
    if (not(img and img->_ssbo))
      return false;
    DataBlockInputStream istr(db);
    int w  = istr.getItem<int>();
    int h  = istr.getItem<int>();
    int ch = istr.getItem<int>();
    (void)ch;
    if (w != img->_w or h != img->_h)
      return false; // dimension changed -> recompute
    size_t n     = size_t(w) * size_t(h);
    auto fxi     = env->_ctx->FXI();
    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mapping->_mappedaddr, istr.current(), n * sizeof(float));
    fxi->unmapStorageBuffer(mapping.get());
    return true;
  }

  // shared tail for every op's cookComputeHash: mix in the bake context (dim)
  // and the upstream node hashes. Each op prepends its own version salt + params.
  static void _mixTail(DataBlock::hasher_t h, uint64_t ctx, const std::vector<uint64_t>& ih) {
    h->accumulateItem<uint64_t>(ctx);
    for (auto x : ih)
      h->accumulateItem<uint64_t>(x);
  }
};

///////////////////////////////////////////////////////////////////////////////
// the compute-shader source for fbm, with DIM / FREQ / OCT / AMP substituted in.
///////////////////////////////////////////////////////////////////////////////

static std::string _fbm_compute_text(int dim, float freq, int octaves, float amp) {
  std::string tmpl = R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_hf (descriptor_set 0) {
  buffer layout(std430) hf_out { float heights[%DIMSQ%]; };
}
compute_interface iface_hf {
  storage { sif_hf }
  inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); }
}
compute_shader cs_fbm : iface_hf {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  vec2 p = vec2(float(xi), float(yi)) / float(%DIM%) * float(%FREQ%);
  float sum = 0.0, ampl = 1.0, nrm = 0.0;
  for (int o = 0; o < %OCT%; o++) {
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    vec2 u  = fp * fp * (3.0 - 2.0 * fp);
    float a = fract(sin(dot(ip + vec2(0.0, 0.0), vec2(127.1, 311.7))) * 43758.5453);
    float b = fract(sin(dot(ip + vec2(1.0, 0.0), vec2(127.1, 311.7))) * 43758.5453);
    float c = fract(sin(dot(ip + vec2(0.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);
    float d = fract(sin(dot(ip + vec2(1.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);
    float n = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    sum += ampl * n;
    nrm += ampl;
    ampl *= 0.5;
    p *= 2.0;
  }
  heights[yi * %DIMU% + xi] = (sum / nrm) * float(%AMP%);
}
)SHADER";
  // sized SSBO array (shadlang wants a concrete length, not a runtime array)
  auto sub = [&](const std::string& key, const std::string& val) {
    size_t pos = 0;
    while ((pos = tmpl.find(key, pos)) != std::string::npos) {
      tmpl.replace(pos, key.size(), val);
      pos += val.size();
    }
  };
  sub("%DIMSQ%", FormatString("%d", dim * dim));
  sub("%DIMU%", FormatString("%du", dim));
  sub("%DIM%", FormatString("%d", dim));
  sub("%FREQ%", FormatString("%f", freq));
  sub("%OCT%", FormatString("%d", octaves));
  sub("%AMP%", FormatString("%f", amp));
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
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _inFreq = typedInputNamed<dflow::FloatPlugTraits>("frequency");
    _inAmp  = typedInputNamed<dflow::FloatPlugTraits>("amplitude");
    // copy the data plug's set value into the inst plug — an unconnected inst plug
    // is NOT auto-populated from its data default (mirrors GlobalModuleInst::onLink).
    _inFreq->_value = _fmd->typedInputNamed<dflow::FloatPlugTraits>("frequency")->_value;
    _inAmp->_value  = _fmd->typedInputNamed<dflow::FloatPlugTraits>("amplitude")->_value;
  }

  // SETUP (SSBO alloc + shader compile) happens here — onActivate runs during
  // updateTopology, BEFORE the bake's beginFrame/dispatch-phase. Doing it inside
  // the dispatch phase corrupts GPU state (test_compute_basic sets up first too).
  // The scalar plug values are read here and baked into the shader text (a one-
  // shot bake); a live-CONNECTED scalar would need a UBO read at compute() time.
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<BakeEnv>();
    auto fxi  = env->_ctx->FXI();
    int dim   = env->_w;
    auto img  = _output->_value; // GpuComputeImage2DInst (created via data_to_inst)
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = fxi->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    auto text = _fbm_compute_text(dim, _inFreq->value(), _fmd->_octaves, _inAmp->value());
    auto shdr = fxi->shaderFromShaderText("terrain_fbm", text);
    _cs       = fxi->computeShader(shdr, "cs_fbm");
  }

  // DISPATCH only (inside the dispatch phase). A barrier after each dispatch makes
  // this module's writes visible to downstream modules' reads.
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);
    ci->dispatchCompute(_cs, groups, groups, 1);
    ci->storageBarrier();
  }

  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.fbm.v1");
    h->accumulateItem<int>(_fmd->_octaves);
    h->accumulateItem<float>(_inFreq->value());
    h->accumulateItem<float>(_inAmp->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const FbmModuleData* _fmd;
  hfimg_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _inFreq;
  dflow::float_inp_pluginst_ptr_t _inAmp;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeFbmIOs(dataflow::moduledata_ptr_t data) {
  auto freq = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "frequency");
  auto amp  = dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amplitude");
  freq->setValue(4.0f);
  amp->setValue(1.0f);
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
  // _octaves is a BAKED loop bound (not a plug) — reflect it so the serialized
  // graph self-describes (the JSON is the portable, python-decoupled artifact).
  clazz->directProperty("octaves", &FbmModuleData::_octaves);
}

///////////////////////////////////////////////////////////////////////////////
// the SOURCE field for an image input = the CONNECTED output's value.
///////////////////////////////////////////////////////////////////////////////

static gpucomputeimage2d_inst_ptr_t _srcImg(hfimg_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

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

///////////////////////////////////////////////////////////////////////////////
// shared helpers for the elementwise ops
///////////////////////////////////////////////////////////////////////////////

// grab a float input pluginst AND copy its data-plug default into the inst (an
// unconnected inst plug is not auto-populated — see the onLink notes above).
static dflow::float_inp_pluginst_ptr_t
_floatPlug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::FloatPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::FloatPlugTraits>(name)->_value;
  return p;
}

// same, for a vec2 input plug (e.g. a packed 2D direction/offset).
static dflow::fvec2_inp_pluginst_ptr_t
_vec2Plug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::Vec2fPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::Vec2fPlugTraits>(name)->_value;
  return p;
}

// substitute %KEY% -> val in a shader template
static void _shadersub(std::string& s, const std::string& key, const std::string& val) {
  size_t pos = 0;
  while ((pos = s.find(key, pos)) != std::string::npos) {
    s.replace(pos, key.size(), val);
    pos += val.size();
  }
}

// every op's output buffer is W*H R32F; allocate it once (onActivate, pre-frame)
static void _allocOut(BakeEnv* env, gpucomputeimage2d_inst_ptr_t img) {
  img->_w        = env->_w;
  img->_h        = env->_h;
  img->_channels = 1;
  img->_ssbo     = env->_ctx->FXI()->createStorageBuffer(size_t(env->_w) * size_t(env->_h) * sizeof(float));
}

///////////////////////////////////////////////////////////////////////////////
// ConstModule — Out = level. 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

static std::string _const_text(int dim, float level) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_const : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  odata[i] = float(%LEVEL%);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%LEVEL%", FormatString("%f", level));
  return t;
}

struct ConstModuleInst : public TerrainComputeInst {
  ConstModuleInst(const ConstModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _level  = _floatPlug(this, _d, "level");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_const", _const_text(env->_w, _level->value()));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_const");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    int g    = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.const.v1");
    h->accumulateItem<float>(_level->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ConstModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _level;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeConstIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "level")->setValue(0.5f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ConstModuleData::ConstModuleData() {}
std::shared_ptr<ConstModuleData> ConstModuleData::createShared() {
  auto d = std::make_shared<ConstModuleData>(); _reshapeConstIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t ConstModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ConstModuleInst>(this, g);
}
void ConstModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ConstModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeConstIOs(m); });
}

///////////////////////////////////////////////////////////////////////////////
// GradientModule — Out = dot(uv, (dir_x,dir_y))*scale + bias. 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

static std::string _grad_text(int dim, float dx, float dy, float scale, float bias) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_grad : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  vec2 uv = vec2(float(xi), float(yi)) / float(%DIM%);
  odata[yi * %DIMU% + xi] = (uv.x * float(%DX%) + uv.y * float(%DY%)) * float(%SCALE%) + float(%BIAS%);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%DIM%", FormatString("%d", dim));
  _shadersub(t, "%DX%", FormatString("%f", dx));
  _shadersub(t, "%DY%", FormatString("%f", dy));
  _shadersub(t, "%SCALE%", FormatString("%f", scale));
  _shadersub(t, "%BIAS%", FormatString("%f", bias));
  return t;
}

struct GradientModuleInst : public TerrainComputeInst {
  GradientModuleInst(const GradientModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _dir = _vec2Plug(this, _d, "dir"); // packed (dir_x, dir_y)
    _sc = _floatPlug(this, _d, "scale");
    _bi = _floatPlug(this, _d, "bias");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto dir = _dir->value();
    auto sh = env->_ctx->FXI()->shaderFromShaderText(
        "terrain_grad", _grad_text(env->_w, dir.x, dir.y, _sc->value(), _bi->value()));
    _cs = env->_ctx->FXI()->computeShader(sh, "cs_grad");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    int g    = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.gradient.v2"); // v2: dir_x/dir_y -> single vec2 "dir"
    auto dir = _dir->value();
    h->accumulateItem<float>(dir.x);
    h->accumulateItem<float>(dir.y);
    h->accumulateItem<float>(_sc->value());
    h->accumulateItem<float>(_bi->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const GradientModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  dflow::fvec2_inp_pluginst_ptr_t _dir;
  dflow::float_inp_pluginst_ptr_t _sc, _bi;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeGradIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::Vec2fPlugTraits>(data, dflow::EPR_UNIFORM, "dir")->setValue(fvec2(1.0f, 0.0f));
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "scale")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "bias")->setValue(0.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
GradientModuleData::GradientModuleData() {}
std::shared_ptr<GradientModuleData> GradientModuleData::createShared() {
  auto d = std::make_shared<GradientModuleData>(); _reshapeGradIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t GradientModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<GradientModuleInst>(this, g);
}
void GradientModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return GradientModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeGradIOs(m); });
}

///////////////////////////////////////////////////////////////////////////////
// CombineModule — Out = op(A, B). 3 SSBOs (out=0, a=1, b=2).
///////////////////////////////////////////////////////////////////////////////

static std::string _combine_text(int dim, int op, float t) {
  const char* expr = "adata[i] + bdata[i]";
  switch (CombineOp(op)) {
    case CombineOp::ADD: expr = "adata[i] + bdata[i]"; break;
    case CombineOp::SUB: expr = "adata[i] - bdata[i]"; break;
    case CombineOp::MUL: expr = "adata[i] * bdata[i]"; break;
    case CombineOp::MIN: expr = "min(adata[i], bdata[i])"; break;
    case CombineOp::MAX: expr = "max(adata[i], bdata[i])"; break;
    case CombineOp::MIX: expr = "mix(adata[i], bdata[i], float(%T%))"; break;
  }
  std::string tmpl = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_a   (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }
storage_interface sif_b   (descriptor_set 0) { buffer layout(std430) bb { float bdata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_a sif_b } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_combine : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  odata[i] = %EXPR%;
}
)S";
  _shadersub(tmpl, "%EXPR%", expr);
  _shadersub(tmpl, "%T%", FormatString("%f", t));
  _shadersub(tmpl, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(tmpl, "%DIMU%", FormatString("%du", dim));
  return tmpl;
}

struct CombineModuleInst : public TerrainComputeInst {
  CombineModuleInst(const CombineModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _inA = typedInputNamed<HfImagePlugTraits>("A");
    _inB = typedInputNamed<HfImagePlugTraits>("B");
    _t   = _floatPlug(this, _d, "t");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_combine", _combine_text(env->_w, _d->_op, _t->value()));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_combine");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto a   = _srcImg(_inA);
    auto b   = _srcImg(_inB);
    OrkAssert(a && a->_ssbo && b && b->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, a->_ssbo);               // adata
    ci->bindStorageBuffer(_cs, 2, b->_ssbo);               // bdata
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.combine.v1");
    h->accumulateItem<int>(_d->_op);
    h->accumulateItem<float>(_t->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const CombineModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _inA, _inB;
  dflow::float_inp_pluginst_ptr_t _t;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeCombineIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "A");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "B");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "t")->setValue(0.5f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
CombineModuleData::CombineModuleData() {}
std::shared_ptr<CombineModuleData> CombineModuleData::createShared() {
  auto d = std::make_shared<CombineModuleData>(); _reshapeCombineIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t CombineModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<CombineModuleInst>(this, g);
}
void CombineModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CombineModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeCombineIOs(m); });
  // _op selects the baked GLSL expression (add/sub/mul/min/max/mix) — reflect it
  // so the op survives serialize/deserialize (else a reloaded graph reverts to ADD).
  clazz->directProperty("op", &CombineModuleData::_op);
}

///////////////////////////////////////////////////////////////////////////////
// TerraceModule — quantize to `steps` plateaus with a `sharpness` riser. 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

static std::string _terrace_text(int dim, float steps, float sharp) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_in } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_terrace : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i  = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  float v = idata[i];
  float s = v * float(%STEPS%);
  float fl = floor(s);
  float fr = s - fl;
  float w  = max((1.0 - float(%SHARP%)) * 0.5, 0.001);
  float k  = smoothstep(0.5 - w, 0.5 + w, fr);
  odata[i] = (fl + k) / float(%STEPS%);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%STEPS%", FormatString("%f", steps));
  _shadersub(t, "%SHARP%", FormatString("%f", sharp));
  return t;
}

struct TerraceModuleInst : public TerrainComputeInst {
  TerraceModuleInst(const TerraceModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _steps  = _floatPlug(this, _d, "steps");
    _sharp  = _floatPlug(this, _d, "sharpness");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_terrace", _terrace_text(env->_w, _steps->value(), _sharp->value()));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_terrace");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->_ssbo);
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.terrace.v1");
    h->accumulateItem<float>(_steps->value());
    h->accumulateItem<float>(_sharp->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const TerraceModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _steps, _sharp;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeTerraceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "steps")->setValue(4.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "sharpness")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
TerraceModuleData::TerraceModuleData() {}
std::shared_ptr<TerraceModuleData> TerraceModuleData::createShared() {
  auto d = std::make_shared<TerraceModuleData>(); _reshapeTerraceIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t TerraceModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<TerraceModuleInst>(this, g);
}
void TerraceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TerraceModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeTerraceIOs(m); });
}

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
  void onActivate(dflow::GraphInst* inst) final {
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

///////////////////////////////////////////////////////////////////////////////
// MaskBlendModule — Out = mix(A, B, M). 4 SSBOs (out=0, a=1, b=2, m=3). The
// masking primitive: B replaces A where the [0,1] mask field M is high.
///////////////////////////////////////////////////////////////////////////////

static std::string _maskblend_text(int dim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_a   (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }
storage_interface sif_b   (descriptor_set 0) { buffer layout(std430) bb { float bdata[%DIMSQ%]; }; }
storage_interface sif_m   (descriptor_set 0) { buffer layout(std430) mb { float mdata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_a sif_b sif_m } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_maskblend : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;
  odata[i] = mix(adata[i], bdata[i], clamp(mdata[i], 0.0, 1.0));
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  return t;
}

struct MaskBlendModuleInst : public TerrainComputeInst {
  MaskBlendModuleInst(const MaskBlendModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _inA = typedInputNamed<HfImagePlugTraits>("A");
    _inB = typedInputNamed<HfImagePlugTraits>("B");
    _inM = typedInputNamed<HfImagePlugTraits>("M");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText("terrain_maskblend", _maskblend_text(env->_w));
    _cs     = env->_ctx->FXI()->computeShader(sh, "cs_maskblend");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto a   = _srcImg(_inA);
    auto b   = _srcImg(_inB);
    auto m   = _srcImg(_inM);
    OrkAssert(a && a->_ssbo && b && b->_ssbo && m && m->_ssbo);
    int g = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo); // odata
    ci->bindStorageBuffer(_cs, 1, a->_ssbo);               // adata
    ci->bindStorageBuffer(_cs, 2, b->_ssbo);               // bdata
    ci->bindStorageBuffer(_cs, 3, m->_ssbo);               // mdata
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.maskblend.v1");
    _mixTail(h, ctx, ih); // identity is fully determined by the 3 input hashes
    h->finish();
    return h->result();
  }

  const MaskBlendModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _inA, _inB, _inM;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeMaskBlendIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "A");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "B");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "M");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
MaskBlendModuleData::MaskBlendModuleData() {}
std::shared_ptr<MaskBlendModuleData> MaskBlendModuleData::createShared() {
  auto d = std::make_shared<MaskBlendModuleData>(); _reshapeMaskBlendIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t MaskBlendModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MaskBlendModuleInst>(this, g);
}
void MaskBlendModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MaskBlendModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeMaskBlendIOs(m); });
}

///////////////////////////////////////////////////////////////////////////////
// ThermalErodeModule — iterative talus relaxation. One step (gather): a cell sheds
// material to lower neighbors wherever the height STEP exceeds the talus threshold T.
// Symmetric pairwise net flow == in(n->C) - out(C->n) summed over 4 neighbors, so it
// is mass-conserving (each pair's flow is +to one, -from the other) AND parallel-safe
// (read old, write new). Border neighbors clamp to self -> no flow leaves the domain.
///////////////////////////////////////////////////////////////////////////////

static std::string _thermal_text(int dim, float talus_thresh, float rate) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
compute_interface iface { storage { sif_out sif_in } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_thermal : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%);
  uint i  = uint(yi) * %DIMU% + uint(xi);
  int  xl = (xi > 0)     ? (xi - 1) : xi;   // clamp at border -> closed domain (mass conserved)
  int  xr = (xi < W - 1) ? (xi + 1) : xi;
  int  yd = (yi > 0)     ? (yi - 1) : yi;
  int  yu = (yi < W - 1) ? (yi + 1) : yi;
  float hC = idata[i];
  float hL = idata[uint(yi) * %DIMU% + uint(xl)];
  float hR = idata[uint(yi) * %DIMU% + uint(xr)];
  float hD = idata[uint(yd) * %DIMU% + uint(xi)];
  float hU = idata[uint(yu) * %DIMU% + uint(xi)];
  float T  = float(%T%);
  // net = sum over neighbors of [ inflow(n->C) - outflow(C->n) ], each = max(step - T, 0).
  float net = 0.0;
  net += max((hL - hC) - T, 0.0) - max((hC - hL) - T, 0.0);
  net += max((hR - hC) - T, 0.0) - max((hC - hR) - T, 0.0);
  net += max((hD - hC) - T, 0.0) - max((hC - hD) - T, 0.0);
  net += max((hU - hC) - T, 0.0) - max((hC - hU) - T, 0.0);
  odata[i] = hC + net * float(%RATE%);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%T%", FormatString("%f", talus_thresh));
  _shadersub(t, "%RATE%", FormatString("%f", rate));
  return t;
}

struct ThermalErodeModuleInst : public TerrainComputeInst {
  ThermalErodeModuleInst(const ThermalErodeModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _talus  = _floatPlug(this, _d, "talus_deg");
    _rate   = _floatPlug(this, _d, "rate");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    // scratch buffer for the ping-pong (same size as the output field).
    _scratch = env->_ctx->FXI()->createStorageBuffer(size_t(env->_w) * size_t(env->_h) * sizeof(float));
    // talus threshold in NORMALIZED height units: the max stable inter-cell step is
    // tan(angle) * cell_size_m, divided by height_scale to renormalize. PHYSICAL ->
    // resolution-independent (finer cells -> proportionally smaller stable step).
    float cell_m = env->_extent_m / float(env->_w);
    float T      = tanf(_talus->value() * 0.017453293f) * cell_m / env->_height_scale_m;
    auto sh      = env->_ctx->FXI()->shaderFromShaderText("terrain_thermal", _thermal_text(env->_w, T, _rate->value()));
    _cs          = env->_ctx->FXI()->computeShader(sh, "cs_thermal");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int K = _d->_iterations;
    if (K < 1) K = 1;
    int g = (env->_w + 7) / 8;
    FxShaderStorageBuffer* bufs[2] = {_output->_value->_ssbo, _scratch};
    int cur = (K - 1) & 1; // parity so the FINAL write lands in bufs[0] (=output)
    // step 0: read the upstream input, write bufs[cur].
    ci->bindStorageBuffer(_cs, 0, bufs[cur]); // odata (write)
    ci->bindStorageBuffer(_cs, 1, in->_ssbo); // idata (read)
    ci->dispatchCompute(_cs, g, g, 1);
    for (int it = 1; it < K; it++) {
      // Each iteration must be its OWN submission: this CI keeps a single descriptor
      // set per pipeline (vulkan_compute.cpp updateDescriptorSet), so a 2nd bind+
      // dispatch in the SAME command buffer would clobber the 1st (all dispatches
      // would read the last-bound buffers). endDispatchPhase = submit+WAIT; the
      // driver opened the first phase and closes the last one after compute() returns.
      ci->endDispatchPhase();
      ci->beginDispatchPhase();
      int nxt = 1 - cur;
      ci->bindStorageBuffer(_cs, 0, bufs[nxt]); // write
      ci->bindStorageBuffer(_cs, 1, bufs[cur]); // read previous step
      ci->dispatchCompute(_cs, g, g, 1);
      cur = nxt;
    }
    // parity gives cur == 0 -> the result is in _output->_value->_ssbo.
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.thermal.v1");
    h->accumulateItem<int>(_d->_iterations);
    h->accumulateItem<float>(_talus->value());
    h->accumulateItem<float>(_rate->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ThermalErodeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _talus, _rate;
  FxShaderStorageBuffer* _scratch = nullptr;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeThermalIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "talus_deg")->setValue(33.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "rate")->setValue(0.15f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ThermalErodeModuleData::ThermalErodeModuleData() {}
std::shared_ptr<ThermalErodeModuleData> ThermalErodeModuleData::createShared() {
  auto d = std::make_shared<ThermalErodeModuleData>(); _reshapeThermalIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t ThermalErodeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ThermalErodeModuleInst>(this, g);
}
void ThermalErodeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ThermalErodeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeThermalIOs(m); });
  clazz->directProperty("iterations", &ThermalErodeModuleData::_iterations); // baked step count
}

///////////////////////////////////////////////////////////////////////////////
// HydroErodeModule — Mei et al. virtual-pipes hydraulic erosion. 4 shaders run
// per step: (init once) zero water/sed/flux, copy In->terr; then per iteration
// (1) FLUX: outflow to 4 neighbors from (terrain+water) height diffs, scaled to
//     available water; (2) WATER+ERODE: update water depth from flux divergence,
//     derive velocity, capacity C = Kc*sin(slope)*|v|, erode (C>s) or deposit
//     (C<s) terrain<->sediment, add rain, evaporate; (3) TRANSPORT: semi-Lagrangian
//     advect sediment by velocity. Constants A/g/l/dt are baked sensible defaults.
///////////////////////////////////////////////////////////////////////////////

// fmt the common GLSL header for a hydro pass. orkid allows only ONE buffer per
// `storage_interface`, so each SSBO is its own interface; `sifaces` is the block of
// those declarations and `siflist` the space-separated names for `storage { ... }`
// (their ORDER == the binding indices). `body` is the shader body.
static std::string _hydro_text(int dim, const char* name, const char* sifaces, const char* siflist,
                               const char* body, //
                               float rain, float evap, float capacity, float erosion, float deposition) {
  std::string t = std::string("\nfxconfig fxcfg_default {}\n") + sifaces +
    "compute_interface iface { storage { " + siflist + " } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader " + name + " : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(%DIMU%);\n"
    "  uint i  = uint(yi) * %DIMU% + uint(xi);\n"
    "  const float DT = 0.02; const float A = 1.0; const float G = 9.81; const float L = 1.0;\n"
    "  const float RAIN = float(%RAIN%); const float KE = float(%EVAP%);\n"
    "  const float KC = float(%CAP%); const float KS = float(%EROS%); const float KD = float(%DEPO%);\n"
    + body + "\n}\n";
  _shadersub(t, "%DIMSQ4%", FormatString("%d", dim * dim * 4)); // before %DIMSQ% (prefix)
  _shadersub(t, "%DIMSQ2%", FormatString("%d", dim * dim * 2));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%RAIN%", FormatString("%f", rain));
  _shadersub(t, "%EVAP%", FormatString("%f", evap));
  _shadersub(t, "%CAP%", FormatString("%f", capacity));
  _shadersub(t, "%EROS%", FormatString("%f", erosion));
  _shadersub(t, "%DEPO%", FormatString("%f", deposition));
  return t;
}

struct HydroErodeModuleInst : public TerrainComputeInst {
  HydroErodeModuleInst(const HydroErodeModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _rain   = _floatPlug(this, _d, "rain");
    _evap   = _floatPlug(this, _d, "evaporation");
    _cap    = _floatPlug(this, _d, "capacity");
    _eros   = _floatPlug(this, _d, "erosion");
    _depo   = _floatPlug(this, _d, "deposition");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    _allocOut(env.get(), _output->_value); // terr A (= output)
    size_t nf = size_t(dim) * size_t(dim);
    _terrB = fxi->createStorageBuffer(nf * sizeof(float));
    _water = fxi->createStorageBuffer(nf * sizeof(float));
    _sedA  = fxi->createStorageBuffer(nf * sizeof(float));
    _sedB  = fxi->createStorageBuffer(nf * sizeof(float));
    _flux  = fxi->createStorageBuffer(nf * 4 * sizeof(float));
    _vel   = fxi->createStorageBuffer(nf * 2 * sizeof(float));
    float rn = _rain->value(), ev = _evap->value(), kc = _cap->value(), ks = _eros->value(), kd = _depo->value();
    // --- INIT: terr = In, water/sed/flux = 0. binds: 0 terr(w) 1 in(r) 2 water(w) 3 sed(w) 4 flux(w)
    {
      const char* ifc =
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr[%DIMSQ%]; }; }\n"
        "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[%DIMSQ%]; }; }\n"
        "storage_interface si_s (descriptor_set 0) { buffer layout(std430) sb { float sed[%DIMSQ%]; }; }\n"
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[%DIMSQ4%]; }; }\n";
      const char* body =
        "  terr[i] = idata[i];\n  water[i] = 0.0;\n  sed[i] = 0.0;\n"
        "  flux[4u*i+0u]=0.0; flux[4u*i+1u]=0.0; flux[4u*i+2u]=0.0; flux[4u*i+3u]=0.0;";
      _csInit = fxi->computeShader(fxi->shaderFromShaderText("hydro_init",
          _hydro_text(dim, "cs_hydro_init", ifc, "si_t si_i si_w si_s si_f", body, rn, ev, kc, ks, kd)), "cs_hydro_init");
    }
    // --- FLUX: binds 0 flux(rw) 1 terr(r) 2 water(r)
    {
      const char* ifc =
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[%DIMSQ4%]; }; }\n"
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr[%DIMSQ%]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[%DIMSQ%]; }; }\n";
      const char* body =
        "  float hC = terr[i] + water[i];\n"
        "  float hL = (xi>0)   ? (terr[i-1u]+water[i-1u]) : hC;\n"
        "  float hR = (xi<W-1) ? (terr[i+1u]+water[i+1u]) : hC;\n"
        "  float hD = (yi>0)   ? (terr[i-uint(W)]+water[i-uint(W)]) : hC;\n"
        "  float hU = (yi<W-1) ? (terr[i+uint(W)]+water[i+uint(W)]) : hC;\n"
        "  float k = DT*A*G/L;\n"
        "  float fL = max(0.0, flux[4u*i+0u] + k*(hC-hL));\n"
        "  float fR = max(0.0, flux[4u*i+1u] + k*(hC-hR));\n"
        "  float fD = max(0.0, flux[4u*i+2u] + k*(hC-hD));\n"
        "  float fU = max(0.0, flux[4u*i+3u] + k*(hC-hU));\n"
        "  float sum = (fL+fR+fD+fU)*DT;\n"
        "  float kk = (sum>1e-9) ? min(1.0, water[i]*L*L/sum) : 1.0;\n"
        "  flux[4u*i+0u]=fL*kk; flux[4u*i+1u]=fR*kk; flux[4u*i+2u]=fD*kk; flux[4u*i+3u]=fU*kk;";
      _csFlux = fxi->computeShader(fxi->shaderFromShaderText("hydro_flux",
          _hydro_text(dim, "cs_hydro_flux", ifc, "si_f si_t si_w", body, rn, ev, kc, ks, kd)), "cs_hydro_flux");
    }
    // --- WATER+ERODE: binds 0 terrOut(w) 1 terrIn(r) 2 water(rw) 3 flux(r) 4 sed(rw) 5 vel(w)
    {
      const char* ifc =
        "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float terr_o[%DIMSQ%]; }; }\n"
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr_i[%DIMSQ%]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[%DIMSQ%]; }; }\n"
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[%DIMSQ4%]; }; }\n"
        "storage_interface si_s (descriptor_set 0) { buffer layout(std430) sb { float sed[%DIMSQ%]; }; }\n"
        "storage_interface si_v (descriptor_set 0) { buffer layout(std430) vb { float vel[%DIMSQ2%]; }; }\n";
      const char* body =
        "  float b = terr_i[i];\n  float d = water[i] + RAIN;\n"
        "  float inL = (xi>0)   ? flux[4u*(i-1u)+1u] : 0.0;\n"   // left's R
        "  float inR = (xi<W-1) ? flux[4u*(i+1u)+0u] : 0.0;\n"   // right's L
        "  float inD = (yi>0)   ? flux[4u*(i-uint(W))+3u] : 0.0;\n"  // down's U
        "  float inU = (yi<W-1) ? flux[4u*(i+uint(W))+2u] : 0.0;\n"  // up's D
        "  float fL=flux[4u*i+0u], fR=flux[4u*i+1u], fD=flux[4u*i+2u], fU=flux[4u*i+3u];\n"
        "  float outflow = fL+fR+fD+fU;\n"
        "  float d2 = d + DT*((inL+inR+inD+inU) - outflow)/(L*L);\n"
        "  if (d2 < 0.0) d2 = 0.0;\n"
        "  float dWx = ((inL - fL) + (fR - inR))*0.5;\n"
        "  float dWy = ((inD - fD) + (fU - inU))*0.5;\n"
        "  float davg = clamp((d+d2)*0.5, 1e-3, 0.5);\n" // CAP the denominator (deep ponds shouldn't kill velocity)
        "  float u = dWx/(L*davg);\n  float v = dWy/(L*davg);\n"
        "  float vmag = min(sqrt(u*u+v*v), 4.0);\n"      // clamp velocity for the CAPACITY term only
        "  float bL=(xi>0)?terr_i[i-1u]:b, bR=(xi<W-1)?terr_i[i+1u]:b;\n"
        "  float bD=(yi>0)?terr_i[i-uint(W)]:b, bU=(yi<W-1)?terr_i[i+uint(W)]:b;\n"
        "  float gx=(bR-bL)*0.5*64.0, gy=(bU-bD)*0.5*64.0;\n" // scale normalized [0,1] slope -> real tilt
        "  float sina = sqrt(gx*gx+gy*gy)/sqrt(gx*gx+gy*gy+1.0);\n"
        "  sina = max(sina, 0.001);\n"                   // floor far below real slopes (was 0.05 == no slope signal)
        "  float C = min(KC * sina * vmag, 2.0);\n"      // capacity (KC now has headroom)
        "  float s = sed[i];\n  float bnew = b; float snew = s;\n"
        "  if (C > s) { float amt = KS*(C-s);          bnew = b - amt; snew = max(s + amt, 0.0); }\n"  // erode (KS scales; no magic cap)
        "  else       { float amt = min(KD*(s-C), s);  bnew = b + amt; snew = max(s - amt, 0.0); }\n"  // deposit (<= available sediment)
        "  terr_o[i] = clamp(bnew, -1.0, 2.0);\n  water[i] = d2*(1.0-KE);\n  sed[i] = snew;\n"
        "  vel[2u*i+0u]=u; vel[2u*i+1u]=v;";             // store RAW (unclamped) velocity for transport
      _csWater = fxi->computeShader(fxi->shaderFromShaderText("hydro_water",
          _hydro_text(dim, "cs_hydro_water", ifc, "si_o si_t si_w si_f si_s si_v", body, rn, ev, kc, ks, kd)), "cs_hydro_water");
    }
    // --- TRANSPORT: binds 0 sedOut(w) 1 sedIn(r) 2 vel(r)
    {
      const char* ifc =
        "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float sed_o[%DIMSQ%]; }; }\n"
        "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float sed_i[%DIMSQ%]; }; }\n"
        "storage_interface si_v (descriptor_set 0) { buffer layout(std430) vb { float vel[%DIMSQ2%]; }; }\n";
      const char* body =
        "  float u=vel[2u*i+0u], v=vel[2u*i+1u];\n"
        "  float ADV = 100.0;\n"  // advect sediment a MEANINGFUL number of cells/step (u*DT alone was sub-cell)
        "  float sx = clamp(float(xi) - u*DT*ADV, 0.0, float(W-1));\n"
        "  float sy = clamp(float(yi) - v*DT*ADV, 0.0, float(W-1));\n"
        "  int x0=int(floor(sx)), y0=int(floor(sy));\n"
        "  int x1=min(x0+1,W-1), y1=min(y0+1,W-1);\n"
        "  float fx=sx-float(x0), fy=sy-float(y0);\n"
        "  float s00=sed_i[uint(y0*W+x0)], s10=sed_i[uint(y0*W+x1)];\n"
        "  float s01=sed_i[uint(y1*W+x0)], s11=sed_i[uint(y1*W+x1)];\n"
        "  sed_o[i] = mix(mix(s00,s10,fx), mix(s01,s11,fx), fy);";
      _csXport = fxi->computeShader(fxi->shaderFromShaderText("hydro_xport",
          _hydro_text(dim, "cs_hydro_xport", ifc, "si_o si_i si_v", body, rn, ev, kc, ks, kd)), "cs_hydro_xport");
    }
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int K = _d->_iterations;
    if (K < 1) K = 1;
    int g = (env->_w + 7) / 8;
    FxShaderStorageBuffer* terr[2] = {_output->_value->_ssbo, _terrB};
    FxShaderStorageBuffer* sed[2]  = {_sedA, _sedB};
    auto submitNext = [&]() { ci->endDispatchPhase(); ci->beginDispatchPhase(); }; // descriptor-set gotcha
    // init (the driver already opened the first phase)
    ci->bindStorageBuffer(_csInit, 0, terr[0]);
    ci->bindStorageBuffer(_csInit, 1, in->_ssbo);
    ci->bindStorageBuffer(_csInit, 2, _water);
    ci->bindStorageBuffer(_csInit, 3, sed[0]);
    ci->bindStorageBuffer(_csInit, 4, _flux);
    ci->dispatchCompute(_csInit, g, g, 1);
    int tc = 0, sc = 0; // current terr / sed buffer index
    for (int it = 0; it < K; it++) {
      submitNext();
      // pass 1: flux (in place)
      ci->bindStorageBuffer(_csFlux, 0, _flux);
      ci->bindStorageBuffer(_csFlux, 1, terr[tc]);
      ci->bindStorageBuffer(_csFlux, 2, _water);
      ci->dispatchCompute(_csFlux, g, g, 1);
      submitNext();
      // pass 2: water + erode/deposit (terr ping-pong, sed in place, vel out)
      ci->bindStorageBuffer(_csWater, 0, terr[1 - tc]);
      ci->bindStorageBuffer(_csWater, 1, terr[tc]);
      ci->bindStorageBuffer(_csWater, 2, _water);
      ci->bindStorageBuffer(_csWater, 3, _flux);
      ci->bindStorageBuffer(_csWater, 4, sed[sc]);
      ci->bindStorageBuffer(_csWater, 5, _vel);
      ci->dispatchCompute(_csWater, g, g, 1);
      tc = 1 - tc;
      submitNext();
      // pass 3: sediment transport (sed ping-pong)
      ci->bindStorageBuffer(_csXport, 0, sed[1 - sc]);
      ci->bindStorageBuffer(_csXport, 1, sed[sc]);
      ci->bindStorageBuffer(_csXport, 2, _vel);
      ci->dispatchCompute(_csXport, g, g, 1);
      sc = 1 - sc;
    }
    // the eroded terrain is in terr[tc]; point the output image at it.
    _output->_value->_ssbo = terr[tc];
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.hydro.v2"); // v2: real sediment transport + slope scale, caps removed
    h->accumulateItem<int>(_d->_iterations);
    h->accumulateItem<float>(_rain->value());
    h->accumulateItem<float>(_evap->value());
    h->accumulateItem<float>(_cap->value());
    h->accumulateItem<float>(_eros->value());
    h->accumulateItem<float>(_depo->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const HydroErodeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _rain, _evap, _cap, _eros, _depo;
  FxShaderStorageBuffer *_terrB = nullptr, *_water = nullptr, *_sedA = nullptr, *_sedB = nullptr, *_flux = nullptr, *_vel = nullptr;
  const FxComputeShader *_csInit = nullptr, *_csFlux = nullptr, *_csWater = nullptr, *_csXport = nullptr;
};

static void _reshapeHydroIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "rain")->setValue(0.012f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "evaporation")->setValue(0.015f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "capacity")->setValue(0.30f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "erosion")->setValue(0.30f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "deposition")->setValue(0.30f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
HydroErodeModuleData::HydroErodeModuleData() {}
std::shared_ptr<HydroErodeModuleData> HydroErodeModuleData::createShared() {
  auto d = std::make_shared<HydroErodeModuleData>(); _reshapeHydroIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t HydroErodeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<HydroErodeModuleInst>(this, g);
}
void HydroErodeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return HydroErodeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeHydroIOs(m); });
  clazz->directProperty("iterations", &HydroErodeModuleData::_iterations);
}

///////////////////////////////////////////////////////////////////////////////
// CaptureModule — records the request; the driver flushes after GPU submit.
///////////////////////////////////////////////////////////////////////////////

struct CaptureModuleInst : public dflow::DgModuleInst {
  CaptureModuleInst(const CaptureModuleData* data, dflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst)
      , _cmd(data) {
  }
  void onLink(dflow::GraphInst* inst) final {
    _input = typedInputNamed<HfImagePlugTraits>("In");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    // resolve the SOURCE field from the CONNECTED output (not our input's own
    // _value, which is an empty default) — that's where the producer set _ssbo.
    auto out = std::dynamic_pointer_cast<hfimg_outpluginst_t>(_input->_connectedOutput);
    OrkAssert(out);
    env->_captures.push_back(CaptureRequest{out->_value, _cmd->_path});
  }
  const CaptureModuleData* _cmd;
  hfimg_inpluginst_ptr_t _input;
};

static void _reshapeCaptureIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
}

CaptureModuleData::CaptureModuleData() {
}
std::shared_ptr<CaptureModuleData> CaptureModuleData::createShared() {
  auto data = std::make_shared<CaptureModuleData>();
  _reshapeCaptureIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t CaptureModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<CaptureModuleInst>(this, ginst);
}
void CaptureModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CaptureModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeCaptureIOs(mdata); });
  // serialize the channel identity so the graph self-describes its sinks (_path
  // is machine-specific and stays unreflected — derived from _channel at bake).
  clazz->directProperty("channel", &CaptureModuleData::_channel);
}

///////////////////////////////////////////////////////////////////////////////
// driver
///////////////////////////////////////////////////////////////////////////////

// cache-hit count from the most recent cacheable bake (for terrainCacheTest).
static int s_lastCookHits = -1;

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m, float height_scale_m) {
  // topo-sort. The sorter allocates a register per connected output plug from a
  // per-type pool, so the GpuComputeImage2D type MUST have a register block
  // (keyed by the out-plug's data_type_t = GpuComputeImage2DData) or the sort
  // asserts. (Distinct per-format pools would need distinct C++ types — later.)
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuComputeImage2DData>("hf_img", 8);
  // register pools for the scalar plug types a terrain graph can carry on a
  // CONNECTED edge. Today every dataflow edge is a GpuComputeImage2D and the
  // vec2/vec4/float plugs are UNIFORM inputs (which the sorter never allocates a
  // register for) — but a future module that OUTPUTS a vec2/vec4/float field
  // would have its output plug keyed here, so register the pools up front (the
  // sort asserts a non-null register for any connected output, dataflow_sorter
  // line ~204). Cheap (a small pool each) and keeps the machine complete.
  dgctx->createRegisters<float>("hf_float", 8);
  dgctx->createRegisters<fvec2>("hf_vec2", 8);
  dgctx->createRegisters<fvec4>("hf_vec4", 8);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);

  // instantiate
  auto ginst = dflow::GraphData::createGraphInst(graph);

  auto env             = std::make_shared<BakeEnv>();
  env->_ctx            = ctx;
  env->_w              = dim;
  env->_h              = dim;
  env->_extent_m       = extent_m;       // world units -> resolution-independent meter params
  env->_height_scale_m = height_scale_m;
  ginst->_impl.setShared<BakeEnv>(env);

  ginst->updateTopology(topo);

  // run the compute (modules dispatch; capture modules record requests)
  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0f;
  updata->_dt      = 0.0f;

  ctx->beginFrame();
  auto ci = ctx->CI();
  if (graph->_cacheable) {
    // per-node cook cache: Merkle-hash every node (cheap scalars), then per node
    // either upload its cached field (hit) or dispatch it IN ITS OWN dispatch
    // phase + read the result back to cache (miss). Per-op submit+wait makes the
    // inline readback valid. The Capture sink has no cookStore, so it just runs.
    // cook context = everything outside the graph that changes a node's OUTPUT:
    // the bake resolution AND the world units (meters->texels depends on dim/extent;
    // real slope depends on height_scale). Node hashes carry the resolution-independent
    // meter params; this context folds in the per-bake resolution + scale.
    {
      auto ch = DataBlock::createHasher();
      ch->accumulateItem<int>(dim);
      ch->accumulateItem<float>(extent_m);
      ch->accumulateItem<float>(height_scale_m);
      ch->finish();
      ginst->_cookContextHash = ch->result();
    }
    ginst->computeNodeHashes();
    int cook_hits = 0, cook_computes = 0;
    for (auto inst : ginst->_ordered_module_insts) {
      auto db = DataBlockCache::findDataBlock(inst->_cookHash);
      if (db and inst->cookLoad(db)) {
        // cache HIT — cached field uploaded to the node's SSBO; no GPU dispatch
        cook_hits++;
      } else {
        ci->beginDispatchPhase();
        inst->compute(ginst.get(), updata);
        ci->endDispatchPhase(); // submit + WAIT -> this node's output is now valid
        if (auto store = inst->cookStore())
          DataBlockCache::setDataBlock(inst->_cookHash, store);
        cook_computes++;
      }
    }
    printf("[cook] cacheable bake: %d cache-hits, %d computed\n", cook_hits, cook_computes);
    s_lastCookHits = cook_hits;
  } else {
    ci->beginDispatchPhase();
    ginst->compute(updata);
    ci->endDispatchPhase();
  }
  ctx->endFrame();

  // flush captures: readback each source SSBO and encode by file extension.
  //   .exr -> RGBA32F float (h,h,h,1), the canonical (lossless) artifact — keeps the
  //           TRUE field values (so physical height = value * height_scale_m).
  //   .png -> single-channel 16-bit grayscale, NORMALIZED to the field's [min,max]
  //           across the full 16-bit range (auto-exposed for max contrast/precision;
  //           great for masks/curv that live in a sub-range). PNG can't hold float so
  //           OIIO would silently drop to 8-bit; R16UI forces the full 16-bit depth.
  //           The absolute scale is recoverable from the printed/returned FieldStats
  //           (and the EXR), since the PNG is field-relative.
  std::vector<fieldstats_ptr_t> stats;
  auto fxi = ctx->FXI();
  for (auto& req : env->_captures) {
    auto img  = req._img; // GpuComputeImage2DInst (the producer's output value)
    int w     = img->_w;
    int h     = img->_h;
    size_t n  = size_t(w) * size_t(h);
    std::string ps(req._path.c_str());
    bool as_png = ps.size() >= 4 && (ps.compare(ps.size() - 4, 4, ".png") == 0);

    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* src = (const float*)mapping->_mappedaddr;

    // pass 1: stats (+ build the RGBA32F EXR buffer inline; the EXR keeps true values).
    std::vector<float> rgba; // .exr path
    if (not as_png) rgba.resize(n * 4);
    float vmin = 1e30f, vmax = -1e30f;
    double vsum = 0.0;
    for (size_t i = 0; i < n; i++) {
      float v = src[i];
      vmin = (v < vmin) ? v : vmin;
      vmax = (v > vmax) ? v : vmax;
      vsum += v;
      if (not as_png) {
        rgba[i * 4 + 0] = v;
        rgba[i * 4 + 1] = v;
        rgba[i * 4 + 2] = v;
        rgba[i * 4 + 3] = 1.0f;
      }
    }
    float vmean = float(vsum / double(n));

    // pass 2 (PNG only): NORMALIZE [min,max] -> [0,65535] (full-range contrast). A
    // flat field (range ~ 0) -> all 0.
    std::vector<uint16_t> g16; // .png path
    if (as_png) {
      g16.resize(n);
      float range = vmax - vmin;
      float inv   = (range > 1e-12f) ? (1.0f / range) : 0.0f;
      for (size_t i = 0; i < n; i++) {
        float t = (src[i] - vmin) * inv;
        t       = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        g16[i]  = uint16_t(t * 65535.0f + 0.5f);
      }
    }
    fxi->unmapStorageBuffer(mapping.get());
    printf("[terrain bake] field stats: min<%g> max<%g> mean<%g>%s\n", vmin, vmax, vmean,
           as_png ? "  (png16 normalized to [min,max])" : "");
    auto fs   = std::make_shared<FieldStats>();
    fs->_min  = vmin;
    fs->_max  = vmax;
    fs->_mean = vmean;
    stats.push_back(fs);

    // engine pattern (vulkan_fbi_capture): write into the Image's datablock via a
    // const-cast, then OIIO encodes by file extension.
    Image oimg;
    if (as_png) {
      oimg.initWithFormat(w, h, EBufferFormat::R16UI);
      memcpy((void*)oimg._data->data(), g16.data(), n * sizeof(uint16_t));
    } else {
      oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
      memcpy((void*)oimg._data->data(), rgba.data(), rgba.size() * sizeof(float));
    }
    // heightfields/masks are LINEAR data — tag the PNG linear so it isn't read back
    // through an sRGB curve (the engine-wide PNG default is sRGB; this is opt-in).
    oimg.writeToFile(req._path, /*linear_colorspace=*/ as_png);
    printf("[terrain bake] wrote <%s> (%dx%d, %s)\n", req._path.c_str(), w, h, as_png ? "png16/linear/normalized" : "exr32f");
  }
  return stats;
}

void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim) {
  // fbm -> remap -> capture. remap doubles + clamps, so the field stats shift
  // visibly (proves the input-reading module + multi-SSBO bind + barrier).
  auto graph = std::make_shared<dflow::GraphData>();
  auto fbm   = FbmModuleData::createShared();
  auto remap = RemapModuleData::createShared();
  auto cap   = CaptureModuleData::createShared();
  cap->_path = outpath;
  remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(2.0f);
  dflow::GraphData::addModule(graph, "fbm", fbm);
  dflow::GraphData::addModule(graph, "remap", remap);
  dflow::GraphData::addModule(graph, "capture", cap);
  graph->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
  graph->safeConnect(cap->inputNamed("In"), remap->outputNamed("Out"));
  bakeHeightfield(graph, ctx, dim);
}

///////////////////////////////////////////////////////////////////////////////
// op self-test — every case is driven by Const inputs (or a closed-form
// Gradient), so the expected min/max/mean is known analytically. This exercises:
//   Const     1 SSBO  generator
//   Gradient  1 SSBO  generator w/ spatial addressing (both axes)
//   Combine   3 SSBO  out=0,a=1,b=2  (the strongest multi-SSBO-bind test)
//   Terrace   2 SSBO  quantize math
///////////////////////////////////////////////////////////////////////////////

int terrainOpsSelfTest(Context* ctx, int dim) {
  int fails    = 0;
  const float D = float(dim);

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  auto check = [&](const char* name, fieldstats_ptr_t s, float emin, float emax, float emean, float tol) {
    bool ok = aeq(s->_min, emin, tol) && aeq(s->_max, emax, tol) && aeq(s->_mean, emean, tol);
    printf(
        "[selftest] %-26s min<%.5f|%.5f> max<%.5f|%.5f> mean<%.5f|%.5f> : %s\n",
        name, s->_min, emin, s->_max, emax, s->_mean, emean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  };

  // --- builders ----------------------------------------------------------------
  auto mkConst = [&](float lvl) -> constmoduledata_ptr_t {
    auto m = ConstModuleData::createShared();
    m->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(lvl);
    return m;
  };
  auto capTo = [&](dflow::graphdata_ptr_t g, dflow::moduledata_ptr_t producer, const std::string& path) {
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(path.c_str());
    dflow::GraphData::addModule(g, "capture", cap);
    g->safeConnect(cap->inputNamed("In"), producer->outputNamed("Out"));
  };
  auto path = [](const char* nm) { return FormatString("/tmp/terrain_selftest_%s.exr", nm); };

  // --- Const -------------------------------------------------------------------
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.5f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_half"));
    check("Const(0.5)", bakeHeightfield(g, ctx, dim)[0], 0.5f, 0.5f, 0.5f, 1e-4f);
  }
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.25f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_quarter"));
    check("Const(0.25)", bakeHeightfield(g, ctx, dim)[0], 0.25f, 0.25f, 0.25f, 1e-4f);
  }

  // --- Gradient (closed form: linear ramp along an axis) -----------------------
  // value = uv.axis ; over xi=0..D-1 : min=0, max=(D-1)/D, mean=(D-1)/(2D)
  auto runGrad = [&](const char* nm, float dx, float dy) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(dx, dy));
    dflow::GraphData::addModule(g, "grad", gr);
    capTo(g, gr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Gradient(x)", runGrad("grad_x", 1.0f, 0.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);
  check("Gradient(y)", runGrad("grad_y", 0.0f, 1.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);

  // --- Combine (3 SSBO: out,a,b) ----------------------------------------------
  auto runCombine = [&](const char* nm, int op, float a, float b, float t) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = CombineModuleData::createShared();
    cm->_op = op;
    cm->typedInputNamed<dflow::FloatPlugTraits>("t")->setValue(t);
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    g->safeConnect(cm->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(cm->inputNamed("B"), cb->outputNamed("Out"));
    capTo(g, cm, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Combine(ADD,.3,.2)", runCombine("comb_add", int(CombineOp::ADD), 0.3f, 0.2f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(SUB,.8,.3)", runCombine("comb_sub", int(CombineOp::SUB), 0.8f, 0.3f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MUL,.5,.5)", runCombine("comb_mul", int(CombineOp::MUL), 0.5f, 0.5f, 0.0f), 0.25f, 0.25f, 0.25f, 1e-4f);
  check("Combine(MIN,.3,.7)", runCombine("comb_min", int(CombineOp::MIN), 0.3f, 0.7f, 0.0f), 0.3f, 0.3f, 0.3f, 1e-4f);
  check("Combine(MAX,.3,.7)", runCombine("comb_max", int(CombineOp::MAX), 0.3f, 0.7f, 0.0f), 0.7f, 0.7f, 0.7f, 1e-4f);
  check("Combine(MIX,.2,.8,.5)", runCombine("comb_mix5", int(CombineOp::MIX), 0.2f, 0.8f, 0.5f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MIX,.2,.8,.25)", runCombine("comb_mix25", int(CombineOp::MIX), 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Terrace (2 SSBO) : quantize a constant to the nearest plateau -----------
  auto runTerrace = [&](const char* nm, float in, float steps, float sharp) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ci = mkConst(in);
    auto tr = TerraceModuleData::createShared();
    tr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(steps);
    tr->typedInputNamed<dflow::FloatPlugTraits>("sharpness")->setValue(sharp);
    dflow::GraphData::addModule(g, "c", ci);
    dflow::GraphData::addModule(g, "t", tr);
    g->safeConnect(tr->inputNamed("In"), ci->outputNamed("Out"));
    capTo(g, tr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  // 0.6*4=2.4 -> plateau 2 -> 2/4=0.5 ; 0.7*4=2.8 -> riser -> 3/4=0.75 ; 0.9*2=1.8 -> 2/2=1.0
  check("Terrace(.6,4,1)", runTerrace("terr_06", 0.6f, 4.0f, 1.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Terrace(.7,4,1)", runTerrace("terr_07", 0.7f, 4.0f, 1.0f), 0.75f, 0.75f, 0.75f, 1e-4f);
  check("Terrace(.9,2,1)", runTerrace("terr_09", 0.9f, 2.0f, 1.0f), 1.0f, 1.0f, 1.0f, 1e-4f);

  // --- Slope (pre-blurred gradient + soft rolloff, Mask by Feature) -----------
  // flat field -> slope 0 everywhere. A unit ramp has gradient magnitude exactly 1
  // (box-averaging a linear field is exact), so slope = soft_rolloff(1*scale) =
  // 1/(1+1) = 0.5 uniform in the interior; the border ring (margin radius+box) is 0.
  // bake the mask-generator cases with extent==height_scale==dim, so 1 texel == 1 m
  // (radius_m == radius_texels) and the slope factor height_scale/extent == 1.
  const float E = float(dim);
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f; // -> 2 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, sl, path("slope_flat"));
    check("Slope(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f;
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), gr->outputNamed("Out"));
    capTo(g, sl, path("slope_ramp"));
    auto s  = bakeHeightfield(g, ctx, dim, E, E)[0];
    bool ok = (s->_min < 1e-4f) && aeq(s->_max, 0.5f, 2e-3f) && (s->_mean > 0.3f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Slope(ramp,x)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // --- MaskBlend (4 SSBO): per-texel mix(A,B,M) -------------------------------
  auto runBlend = [&](const char* nm, float a, float b, float mk) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = mkConst(mk);
    auto mb = MaskBlendModuleData::createShared();
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    dflow::GraphData::addModule(g, "mb", mb);
    g->safeConnect(mb->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("B"), cb->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("M"), cm->outputNamed("Out"));
    capTo(g, mb, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("MaskBlend(.2,.8,0)",   runBlend("mask_0",  0.2f, 0.8f, 0.0f),  0.2f, 0.2f, 0.2f, 1e-4f);
  check("MaskBlend(.2,.8,1)",   runBlend("mask_1",  0.2f, 0.8f, 1.0f),  0.8f, 0.8f, 0.8f, 1e-4f);
  check("MaskBlend(.2,.8,.25)", runBlend("mask_25", 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Curvature (band-pass diff-of-box + soft rolloff, Mask by Feature) -------
  // a flat field has zero curvature (diff-of-box of a constant is 0, soft rolloff
  // of 0 is 0). A squared ramp uv.x^2 is concave everywhere (positive curvature),
  // so CONVEX (ridge) -> 0 everywhere and CONCAVE responds (>0, soft-rolled into (0,1)).
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = int(CurvatureMode::MAGNITUDE);
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(cv->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, cv, path("curv_flat"));
    check("Curvature(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  auto runCurv = [&](const char* nm, int mode, float scale) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared(); // value = uv.x
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sq = CombineModuleData::createShared(); // uv.x * uv.x = uv.x^2
    sq->_op = int(CombineOp::MUL);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = mode;
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    cv->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(scale);
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "sq", sq);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(sq->inputNamed("A"), gr->outputNamed("Out"));
    g->safeConnect(sq->inputNamed("B"), gr->outputNamed("Out"));
    g->safeConnect(cv->inputNamed("In"), sq->outputNamed("Out"));
    capTo(g, cv, path(nm));
    return bakeHeightfield(g, ctx, dim, E, E)[0];
  };
  // convex of a concave (valley-shaped) field -> 0 everywhere (no ridges); border zeroed.
  check("Curvature(x^2,convex)", runCurv("curv_convex", int(CurvatureMode::CONVEX), 4.0f), 0.0f, 0.0f, 0.0f, 1e-4f);
  // concave responds: min 0 (border ring), max in (0,1) (soft rolloff never saturates),
  // interior ~uniform for a quadratic. Robust property check (not an exact pin).
  {
    auto s  = runCurv("curv_concave", int(CurvatureMode::CONCAVE), 4.0f);
    bool ok = (s->_min < 1e-4f) && (s->_max > 0.05f) && (s->_max < 0.999f) && (s->_mean > 0.04f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Curvature(x^2,concave)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  printf("[selftest] %d case(s) FAILED\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// round-trip gate — the serialized GraphData is the portable, python-decoupled
// artifact (the future dflow UI editor loads exactly this JSON). This proves a
// terrain graph survives serialize -> deserialize -> bake with NO python and NO
// loss: build fbm(octaves=7) * const(0.5) via Combine(MUL), bake (stats A); JSON
// round-trip to a fresh clone; re-supply the capture path (wrapper-owned, not in
// the graph); assert the clone's _octaves/_op survived AND it bakes identical
// stats (so the baked scalars, float plug values, and connections all round-trip).
///////////////////////////////////////////////////////////////////////////////

int terrainRoundTripTest(Context* ctx, int dim) {
  int fails = 0;
  auto aeq  = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g    = std::make_shared<dflow::GraphData>();
    auto fbm  = FbmModuleData::createShared();
    fbm->_octaves = 7; // non-default baked scalar (default is 5)
    auto cst  = ConstModuleData::createShared();
    cst->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(0.5f);
    auto comb = CombineModuleData::createShared();
    comb->_op = int(CombineOp::MUL); // non-default baked scalar (default is ADD)
    // Gradient with a NON-default vec2 "dir" — exercises the vec2 plug VALUE
    // surviving the JSON round-trip (the inplugdata<Vec2fPlugTraits> reflection).
    auto grad = GradientModuleData::createShared();
    grad->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(0.6f, 0.8f)); // default is (1,0)
    grad->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    auto comb2 = CombineModuleData::createShared();
    comb2->_op = int(CombineOp::MUL);
    auto cap  = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "cst", cst);
    dflow::GraphData::addModule(g, "comb", comb);
    dflow::GraphData::addModule(g, "grad", grad);
    dflow::GraphData::addModule(g, "comb2", comb2);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(comb->inputNamed("A"), fbm->outputNamed("Out"));
    g->safeConnect(comb->inputNamed("B"), cst->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("A"), comb->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("B"), grad->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), comb2->outputNamed("Out"));
    return g;
  };

  // (1) original bake
  auto g0 = build("/tmp/terrain_rt_orig.exr");
  auto s0 = bakeHeightfield(g0, ctx, dim);

  // (2) serialize -> JSON -> deserialize a fresh clone
  ork::reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(g0);
  std::string json = ser.output();
  ork::object_ptr_t out;
  ork::reflect::serdes::JsonDeserializer deser(json.c_str());
  deser.deserializeTop(out);
  auto g1 = std::dynamic_pointer_cast<dflow::GraphData>(out);
  if (not g1) {
    printf("[roundtrip] deserialize -> GraphData FAILED\n");
    return 1;
  }

  // (3) direct assertions: the baked scalars survived the round-trip
  auto fbm1  = std::dynamic_pointer_cast<FbmModuleData>(g1->module("fbm"));
  auto comb1 = std::dynamic_pointer_cast<CombineModuleData>(g1->module("comb"));
  if (not fbm1 or fbm1->_octaves != 7) {
    printf("[roundtrip] _octaves LOST (got %d, want 7)\n", fbm1 ? fbm1->_octaves : -1);
    fails++;
  }
  if (not comb1 or comb1->_op != int(CombineOp::MUL)) {
    printf("[roundtrip] _op LOST (got %d, want %d=MUL)\n", comb1 ? comb1->_op : -1, int(CombineOp::MUL));
    fails++;
  }
  // the vec2 "dir" plug VALUE must survive JSON (inplugdata<Vec2fPlugTraits> reflection)
  auto grad1 = std::dynamic_pointer_cast<GradientModuleData>(g1->module("grad"));
  if (not grad1) {
    printf("[roundtrip] clone missing 'grad' module\n");
    fails++;
  } else {
    auto dir = grad1->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->value();
    bool ok  = aeq(dir.x, 0.6f, 1e-6f) and aeq(dir.y, 0.8f, 1e-6f);
    printf("[roundtrip] gradient vec2 'dir' got(%.3f, %.3f) want(0.600, 0.800) : %s\n", dir.x, dir.y, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // (4) re-supply the capture path (wrapper-owned, deliberately NOT serialized),
  // then bake the clone and require byte-identical field stats.
  auto cap1 = std::dynamic_pointer_cast<CaptureModuleData>(g1->module("cap"));
  if (not cap1) {
    printf("[roundtrip] clone missing 'cap' module\n");
    return fails + 1;
  }
  cap1->_path = ork::file::Path("/tmp/terrain_rt_clone.exr");
  auto s1 = bakeHeightfield(g1, ctx, dim);

  if (s0.size() == 1 and s1.size() == 1) {
    bool ok = aeq(s0[0]->_min, s1[0]->_min, 1e-6f)   //
              and aeq(s0[0]->_max, s1[0]->_max, 1e-6f) //
              and aeq(s0[0]->_mean, s1[0]->_mean, 1e-6f);
    printf(
        "[roundtrip] bake-equivalence orig(min %.6f max %.6f mean %.6f) vs clone(min %.6f max %.6f mean %.6f) : %s\n",
        s0[0]->_min, s0[0]->_max, s0[0]->_mean, s1[0]->_min, s1[0]->_max, s1[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[roundtrip] capture-count mismatch s0=%zu s1=%zu\n", s0.size(), s1.size());
    fails++;
  }

  printf("[roundtrip] %d failure(s)\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// per-node cook cache gate — bake a cacheable graph twice. The cold bake
// computes + stores every node's field (anonymously, by content hash) in the
// DataBlockCache; the warm bake (same graph -> same node hashes) loads them, so
// it must (a) produce a byte-identical field and (b) report cache hits for the
// compute nodes (only the Capture sink recomputes).
///////////////////////////////////////////////////////////////////////////////

int terrainCacheTest(Context* ctx, int dim) {
  int fails = 0;

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g        = std::make_shared<dflow::GraphData>();
    g->_cacheable = true; // opt in to the per-node cook cache
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remap    = RemapModuleData::createShared();
    remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    remap->typedInputNamed<dflow::FloatPlugTraits>("bias")->setValue(0.5f);
    auto terr = TerraceModuleData::createShared();
    terr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(6.0f);
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remap", remap);
    dflow::GraphData::addModule(g, "terr", terr);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(terr->inputNamed("In"), remap->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), terr->outputNamed("Out"));
    return g;
  };

  printf("[cachetest] COLD bake:\n");
  auto s1 = bakeHeightfield(build("/tmp/terrain_cache_cold.exr"), ctx, dim);
  int cold_hits = s_lastCookHits;

  printf("[cachetest] WARM bake (same graph -> expect cache hits):\n");
  auto s2 = bakeHeightfield(build("/tmp/terrain_cache_warm.exr"), ctx, dim);
  int warm_hits = s_lastCookHits;

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  if (s1.size() == 1 and s2.size() == 1) {
    bool ok = aeq(s1[0]->_min, s2[0]->_min, 1e-6f)   //
              and aeq(s1[0]->_max, s2[0]->_max, 1e-6f) //
              and aeq(s1[0]->_mean, s2[0]->_mean, 1e-6f);
    printf("[cachetest] field cold(mean %.6f) vs warm(mean %.6f) : %s\n", s1[0]->_mean, s2[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[cachetest] capture-count mismatch\n");
    fails++;
  }
  // the warm bake must actually have hit the cache for the 3 compute nodes
  // (fbm/remap/terr); the Capture sink always recomputes.
  printf("[cachetest] cook hits: cold=%d warm=%d\n", cold_hits, warm_hits);
  if (warm_hits < 3) {
    printf("[cachetest] FAIL: warm bake hit cache only %d times (expected >= 3)\n", warm_hits);
    fails++;
  }

  printf("[cachetest] %d failure(s)\n", fails);
  return fails;
}

} // namespace ork::lev2::terrain

///////////////////////////////////////////////////////////////////////////////
// plug template instantiations (mirrors particle_plugs.cpp — custom plug types
// need explicit describeX/createInstance specializations + reflection or the
// vtables don't link).
///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;
namespace trn   = ork::lev2::terrain;

template <> //
void trn::hfimg_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void trn::hfimg_inplugdata_t::describeX(class_t* clazz) {
}

template <> //
dflow::inpluginst_ptr_t trn::hfimg_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<trn::hfimg_inpluginst_t>(this, minst);
}
template <> //
dflow::outpluginst_ptr_t trn::hfimg_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<trn::hfimg_outpluginst_t>(this, minst);
}

ImplementTemplateReflectionX(trn::hfimg_outplugdata_t, "terrain::hfimgoutplug");
ImplementTemplateReflectionX(trn::hfimg_inplugdata_t, "terrain::hfimginpplug");
