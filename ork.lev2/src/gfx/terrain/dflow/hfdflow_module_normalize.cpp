////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::NormalizeModuleData, "terrain::NormalizeModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// NormalizeModule — rescale a field's [min,max] to [out_lo,out_hi] (default
// [0,1]). The EXPLICIT, controllable counterpart to the bake flush's unconditional
// auto-exposure: put it where you WANT the renormalization (and pre_expose-skip the
// flush later). Three GPU passes per compute(): (1) init the min/max accumulator,
// (2) atomic-reduce min/max over the field (float keys encoded to an order-
// preserving uint), (3) rescale. Each pass is its own submission (descriptor-set
// gotcha + the reduce must complete before the rescale reads it).
///////////////////////////////////////////////////////////////////////////////

// order-preserving float<->uint key so atomicMin/atomicMax on uint == float min/max.
// shadlang requires free functions to live in a libblock the shader INHERITS (free
// functions at file scope are not in scope in the compute_shader) — mirrors lib_pha.
static const char* _NRM_KEY = R"S(
libblock lib_nrm {
  uint f2u(float f){ uint u = floatBitsToUint(f); return (u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u); }
  float u2f(uint e){ return uintBitsToFloat((e & 0x80000000u) != 0u ? (e & 0x7fffffffu) : ~e); }
}
)S";

// DIM is RUNTIME data (params SSBO p_dimf) — dim changes never rebuild the shader; the
// field arrays are runtime-sized. The reduce/rescale passes bind it (init has no field).
static std::string _nrm_init_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_mm (descriptor_set 0) { buffer layout(std430) mb { uint mmdata[2]; }; }
compute_interface iface { storage { sif_mm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_nrm_init : iface {
  if (gl_GlobalInvocationID.x == 0u && gl_GlobalInvocationID.y == 0u) {
    mmdata[0] = 0xffffffffu; // min accumulator: atomicMin pulls it DOWN
    mmdata[1] = 0u;          // max accumulator: atomicMax pushes it UP
  }
}
)S";
  return t;
}

static std::string _nrm_reduce_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_in (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }
storage_interface sif_mm (descriptor_set 0) { buffer layout(std430) mb { uint  mmdata[2]; }; }
storage_interface sif_pm (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; }; }
compute_interface iface { storage { sif_in sif_mm sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
%KEY%
compute_shader cs_nrm_reduce : iface : lib_nrm {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint i = gl_GlobalInvocationID.y * u_dim + gl_GlobalInvocationID.x;
  uint e = f2u(idata[i]);
  atomicMin(mmdata[0], e);
  atomicMax(mmdata[1], e);
}
)S";
  return t;
}

static std::string _nrm_rescale_text(float lo, float hi) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }
storage_interface sif_mm  (descriptor_set 0) { buffer layout(std430) mb { uint  mmdata[2]; }; }
storage_interface sif_pm  (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; }; }
compute_interface iface { storage { sif_out sif_in sif_mm sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
%KEY%
compute_shader cs_nrm_rescale : iface : lib_nrm {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint i = gl_GlobalInvocationID.y * u_dim + gl_GlobalInvocationID.x;
  float mn = u2f(mmdata[0]);
  float mx = u2f(mmdata[1]);
  float d  = mx - mn;
  float t  = (d > 1e-12) ? clamp((idata[i] - mn) / d, 0.0, 1.0) : 0.0;
  odata[i] = mix(float(%LO%), float(%HI%), t);
}
)S";
  return t;
}

static void _nrm_sub(std::string& t, const std::string& k, const std::string& v) {
  size_t pos = 0;
  while ((pos = t.find(k, pos)) != std::string::npos) { t.replace(pos, k.size(), v); pos += v.size(); }
}

struct NormalizeModuleInst : public TerrainComputeInst {
  NormalizeModuleInst(const NormalizeModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _lo     = _floatPlug(this, _d, "out_lo");
    _hi     = _floatPlug(this, _d, "out_hi");
  }

  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    _minmax = env->createStorageBuffer(2 * sizeof(uint32_t));

    auto build = [&](const char* entry, std::string text) -> const FxComputeShader* {
      _nrm_sub(text, "%KEY%", _NRM_KEY);
      _nrm_sub(text, "%LO%", FormatString("%f", _lo->value()));
      _nrm_sub(text, "%HI%", FormatString("%f", _hi->value()));
      return fxi->computeShader(fxi->shaderFromShaderText(entry, text), entry);
    };
    _csInit    = build("cs_nrm_init",    _nrm_init_text());
    _csReduce  = build("cs_nrm_reduce",  _nrm_reduce_text());
    _csRescale = build("cs_nrm_rescale", _nrm_rescale_text(_lo->value(), _hi->value()));

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
    auto out = _output->_value->_ssbo;
    int g    = (env->_w + 7) / 8;
    auto next = [&]() { ci->endDispatchPhase(); ci->beginDispatchPhase(); }; // submit+wait between passes

    // pass 1: init the accumulator (driver already opened the first phase)
    ci->bindStorageBuffer(_csInit, 0, _minmax);
    ci->dispatchCompute(_csInit, 1, 1, 1);
    next();
    // pass 2: atomic min/max reduce over the field
    ci->bindStorageBuffer(_csReduce, 0, in->_ssbo);
    ci->bindStorageBuffer(_csReduce, 1, _minmax);
    ci->bindStorageBuffer(_csReduce, 2, _pm);      // p_dimf (RUNTIME grid dim)
    ci->dispatchCompute(_csReduce, g, g, 1);
    next();
    // pass 3: rescale [min,max] -> [lo,hi]
    ci->bindStorageBuffer(_csRescale, 0, out);
    ci->bindStorageBuffer(_csRescale, 1, in->_ssbo);
    ci->bindStorageBuffer(_csRescale, 2, _minmax);
    ci->bindStorageBuffer(_csRescale, 3, _pm);     // p_dimf (RUNTIME grid dim)
    ci->dispatchCompute(_csRescale, g, g, 1);
    ci->storageBarrier();
  }

  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.normalize.v1");
    h->accumulateItem<float>(_lo->value());
    h->accumulateItem<float>(_hi->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const NormalizeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _lo, _hi;
  FxShaderStorageBuffer* _minmax = nullptr;
  FxShaderStorageBuffer* _pm = nullptr;
  const FxComputeShader *_csInit = nullptr, *_csReduce = nullptr, *_csRescale = nullptr;
};

static void _reshapeNormalizeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "out_lo")->setValue(0.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "out_hi")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
NormalizeModuleData::NormalizeModuleData() {}
std::shared_ptr<NormalizeModuleData> NormalizeModuleData::createShared() {
  auto d = std::make_shared<NormalizeModuleData>(); _reshapeNormalizeIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t NormalizeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<NormalizeModuleInst>(this, g);
}
void NormalizeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return NormalizeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeNormalizeIOs(m); });
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the vocabulary).
  clazz->annotateTyped<ConstString>("dsl.verb", "normalize");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 8);
}

} // namespace ork::lev2::terrain
