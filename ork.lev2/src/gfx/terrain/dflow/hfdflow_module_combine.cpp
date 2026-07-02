////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::CombineModuleData, "terrain::CombineModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// CombineModule — Out = op(A, B). 3 SSBOs (out=0, a=1, b=2). For MIX, the blend
// factor `t` is a RUNTIME param (params SSBO si_p, binding 3) — NOT baked — so
// T.mix(a, b, scalar) does NOT compile a shader per blend value (same pattern as
// lpf/erox). That makes "blend any operator vs its input" — T.mix(z, T.op(z), t) —
// the general, recompile-free way to crossfade erosion/basin/etc. at partial strength.
///////////////////////////////////////////////////////////////////////////////

static std::string _combine_text(int dim, int op) {
  const char* expr = "adata[i] + bdata[i]";
  bool is_mix = (CombineOp(op) == CombineOp::MIX);
  switch (CombineOp(op)) {
    case CombineOp::ADD: expr = "adata[i] + bdata[i]"; break;
    case CombineOp::SUB: expr = "adata[i] - bdata[i]"; break;
    case CombineOp::MUL: expr = "adata[i] * bdata[i]"; break;
    case CombineOp::MIN: expr = "min(adata[i], bdata[i])"; break;
    case CombineOp::MAX: expr = "max(adata[i], bdata[i])"; break;
    case CombineOp::MIX: expr = "mix(adata[i], bdata[i], P[0])"; break;   // P[0]=t, RUNTIME (params SSBO)
  }
  std::string storages =
    "storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }\n"
    "storage_interface sif_a   (descriptor_set 0) { buffer layout(std430) ab { float adata[%DIMSQ%]; }; }\n"
    "storage_interface sif_b   (descriptor_set 0) { buffer layout(std430) bb { float bdata[%DIMSQ%]; }; }\n";
  std::string silist = "sif_out sif_a sif_b";                 // bindings: 0=out 1=a 2=b
  if (is_mix) {
    storages += "storage_interface sif_p (descriptor_set 0) { buffer layout(std430) pb { float P[4]; }; }\n";
    silist = "sif_out sif_a sif_b sif_p";                     // + binding 3 = params (t)
  }
  std::string tmpl = std::string("\nfxconfig fxcfg_default {}\n") + storages +
    "compute_interface iface { storage { " + silist + " } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader cs_combine : iface {\n"
    "  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }\n"
    "  uint i = gl_GlobalInvocationID.y * %DIMU% + gl_GlobalInvocationID.x;\n"
    "  odata[i] = %EXPR%;\n"
    "}\n";
  _shadersub(tmpl, "%EXPR%", expr);
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
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    _isMix = (CombineOp(_d->_op) == CombineOp::MIX);
    auto sh = fxi->shaderFromShaderText("terrain_combine", _combine_text(env->_w, _d->_op));
    _cs     = fxi->computeShader(sh, "cs_combine");
    if (_isMix) {
      // blend factor t is RUNTIME (so a varying t doesn't recompile). Fill here, in bakeAcquire
      // (pre-dispatch-phase: a host map mid-phase is not visible).
      _params = env->createStorageBuffer(4 * sizeof(float));
      float P[4] = {_t->value(), 0.0f, 0.0f, 0.0f};
      auto m = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, P, sizeof(P));
      fxi->unmapStorageBuffer(m.get());
    }
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
    if (_isMix) ci->bindStorageBuffer(_cs, 3, _params);    // P[0] = t (MIX only)
    ci->dispatchCompute(_cs, g, g, 1);
    ci->storageBarrier();
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.combine.v2"); // MIX t now runtime (params SSBO); value still hashed
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
  bool _isMix = false;
  FxShaderStorageBuffer* _params = nullptr;
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

} // namespace ork::lev2::terrain
