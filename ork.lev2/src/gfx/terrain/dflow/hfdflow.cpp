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

ImplementReflectionX(ork::lev2::terrain::TerrainModuleData, "terrain::TerrainModuleData");
ImplementReflectionX(ork::lev2::terrain::FbmModuleData, "terrain::FbmModuleData");
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

struct FbmModuleInst : public dflow::DgModuleInst {
  FbmModuleInst(const FbmModuleData* data, dflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst)
      , _fmd(data) {
  }

  void onLink(dflow::GraphInst* inst) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
  }

  // SETUP (SSBO alloc + shader compile) happens here — onActivate runs during
  // updateTopology, BEFORE the bake's beginFrame/dispatch-phase. Doing it inside
  // the dispatch phase corrupts GPU state (test_compute_basic sets up first too).
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<BakeEnv>();
    auto fxi  = env->_ctx->FXI();
    int dim   = env->_w;
    auto img  = _output->_value; // GpuComputeImage2DInst (created via data_to_inst)
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = fxi->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    auto text = _fbm_compute_text(dim, _fmd->_frequency, _fmd->_octaves, _fmd->_amplitude);
    auto shdr = fxi->shaderFromShaderText("terrain_fbm", text);
    _cs       = fxi->computeShader(shdr, "cs_fbm");
  }

  // DISPATCH only (inside the dispatch phase).
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);
    ci->dispatchCompute(_cs, groups, groups, 1);
  }

  const FbmModuleData* _fmd;
  hfimg_outpluginst_ptr_t _output;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeFbmIOs(dataflow::moduledata_ptr_t data) {
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
}

///////////////////////////////////////////////////////////////////////////////
// driver
///////////////////////////////////////////////////////////////////////////////

void bakeHeightfield(dflow::graphdata_ptr_t graph, Context* ctx, int dim) {
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
  ci->beginDispatchPhase();
  ginst->compute(updata);
  ci->endDispatchPhase();
  ctx->endFrame();

  // flush captures: readback each source SSBO -> RGBA32F EXR (h,h,h,1)
  auto fxi = ctx->FXI();
  for (auto& req : env->_captures) {
    auto img  = req._img; // GpuComputeImage2DInst (the producer's output value)
    int w     = img->_w;
    int h     = img->_h;
    size_t n  = size_t(w) * size_t(h);
    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* src = (const float*)mapping->_mappedaddr;

    std::vector<float> rgba(n * 4);
    for (size_t i = 0; i < n; i++) {
      float v       = src[i];
      rgba[i * 4 + 0] = v;
      rgba[i * 4 + 1] = v;
      rgba[i * 4 + 2] = v;
      rgba[i * 4 + 3] = 1.0f;
    }
    fxi->unmapStorageBuffer(mapping.get());

    Image oimg;
    oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
    // engine pattern (vulkan_fbi_capture): write into the Image's datablock via a
    // const-cast, then OIIO encodes by file extension (.exr -> float EXR).
    memcpy((void*)oimg._data->data(), rgba.data(), rgba.size() * sizeof(float));
    oimg.writeToFile(req._path);
    printf("[terrain bake] wrote <%s> (%dx%d)\n", req._path.c_str(), w, h);
  }
}

void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim) {
  auto graph = std::make_shared<dflow::GraphData>();
  auto fbm   = FbmModuleData::createShared();
  auto cap   = CaptureModuleData::createShared();
  cap->_path = outpath;
  dflow::GraphData::addModule(graph, "fbm", fbm);
  dflow::GraphData::addModule(graph, "capture", cap);
  graph->safeConnect(cap->inputNamed("In"), fbm->outputNamed("Out"));
  bakeHeightfield(graph, ctx, dim);
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
