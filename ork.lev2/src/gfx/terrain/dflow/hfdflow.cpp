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
    _dx = _floatPlug(this, _d, "dir_x");
    _dy = _floatPlug(this, _d, "dir_y");
    _sc = _floatPlug(this, _d, "scale");
    _bi = _floatPlug(this, _d, "bias");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    auto sh = env->_ctx->FXI()->shaderFromShaderText(
        "terrain_grad", _grad_text(env->_w, _dx->value(), _dy->value(), _sc->value(), _bi->value()));
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
    h->accumulateString("terrain.gradient.v1");
    h->accumulateItem<float>(_dx->value());
    h->accumulateItem<float>(_dy->value());
    h->accumulateItem<float>(_sc->value());
    h->accumulateItem<float>(_bi->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const GradientModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _dx, _dy, _sc, _bi;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeGradIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "dir_x")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "dir_y")->setValue(0.0f);
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

std::vector<fieldstats_ptr_t> bakeHeightfield(dflow::graphdata_ptr_t graph, Context* ctx, int dim) {
  // topo-sort. The sorter allocates a register per connected output plug from a
  // per-type pool, so the GpuComputeImage2D type MUST have a register block
  // (keyed by the out-plug's data_type_t = GpuComputeImage2DData) or the sort
  // asserts. (Distinct per-format pools would need distinct C++ types — later.)
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuComputeImage2DData>("hf_img", 8);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);

  // instantiate
  auto ginst = dflow::GraphData::createGraphInst(graph);

  auto env  = std::make_shared<BakeEnv>();
  env->_ctx = ctx;
  env->_w   = dim;
  env->_h   = dim;
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
    ginst->_cookContextHash = uint64_t(dim); // context = bake dimension (W=H)
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

  // flush captures: readback each source SSBO -> RGBA32F EXR (h,h,h,1)
  std::vector<fieldstats_ptr_t> stats;
  auto fxi = ctx->FXI();
  for (auto& req : env->_captures) {
    auto img  = req._img; // GpuComputeImage2DInst (the producer's output value)
    int w     = img->_w;
    int h     = img->_h;
    size_t n  = size_t(w) * size_t(h);
    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* src = (const float*)mapping->_mappedaddr;

    std::vector<float> rgba(n * 4);
    float vmin = 1e30f, vmax = -1e30f;
    double vsum = 0.0;
    for (size_t i = 0; i < n; i++) {
      float v       = src[i];
      rgba[i * 4 + 0] = v;
      rgba[i * 4 + 1] = v;
      rgba[i * 4 + 2] = v;
      rgba[i * 4 + 3] = 1.0f;
      vmin = (v < vmin) ? v : vmin;
      vmax = (v > vmax) ? v : vmax;
      vsum += v;
    }
    fxi->unmapStorageBuffer(mapping.get());
    float vmean = float(vsum / double(n));
    printf("[terrain bake] field stats: min<%g> max<%g> mean<%g>\n", vmin, vmax, vmean);
    auto fs   = std::make_shared<FieldStats>();
    fs->_min  = vmin;
    fs->_max  = vmax;
    fs->_mean = vmean;
    stats.push_back(fs);

    Image oimg;
    oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
    // engine pattern (vulkan_fbi_capture): write into the Image's datablock via a
    // const-cast, then OIIO encodes by file extension (.exr -> float EXR).
    memcpy((void*)oimg._data->data(), rgba.data(), rgba.size() * sizeof(float));
    oimg.writeToFile(req._path);
    printf("[terrain bake] wrote <%s> (%dx%d)\n", req._path.c_str(), w, h);
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
    gr->typedInputNamed<dflow::FloatPlugTraits>("dir_x")->setValue(dx);
    gr->typedInputNamed<dflow::FloatPlugTraits>("dir_y")->setValue(dy);
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
    auto cap  = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "cst", cst);
    dflow::GraphData::addModule(g, "comb", comb);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(comb->inputNamed("A"), fbm->outputNamed("Out"));
    g->safeConnect(comb->inputNamed("B"), cst->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), comb->outputNamed("Out"));
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
