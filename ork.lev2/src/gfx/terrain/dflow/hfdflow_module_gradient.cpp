////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::GradientModuleData, "terrain::GradientModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// GradientModule — Out = dot(uv, (dir_x,dir_y))*scale + bias. 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

// DIM is RUNTIME data (params SSBO p_dimf, binding 1) — dim changes never rebuild the
// shader; the output array is runtime-sized. dir/scale/bias stay baked.
// * (1.0/p_dimf), not / p_dimf: the old LITERAL dim divide was compiler-folded to a
// reciprocal multiply — replicate it so runtime-dim output stays bit-identical to the
// baked-dim caches.
static std::string _grad_text(float dx, float dy, float scale, float bias) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_pm  (descriptor_set 0) { buffer layout(std430) pm_in { float p_dimf; }; }
compute_interface iface { storage { sif_out sif_pm } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_grad : iface {
  uint u_dim = uint(p_dimf); // RUNTIME grid dim (params SSBO) — no rebuild on dim change
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  vec2 uv = vec2(float(xi), float(yi)) * (1.0 / p_dimf);
  odata[yi * u_dim + xi] = (uv.x * float(%DX%) + uv.y * float(%DY%)) * float(%SCALE%) + float(%BIAS%);
}
)S";
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
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    _allocOut(env.get(), _output->_value);
    auto dir = _dir->value();
    auto sh = fxi->shaderFromShaderText(
        "terrain_grad", _grad_text(dir.x, dir.y, _sc->value(), _bi->value()));
    _cs = fxi->computeShader(sh, "cs_grad");
    _pm        = env->createStorageBuffer(sizeof(float)); // p_dimf = RUNTIME grid dim
    float dimf = float(env->_w);
    auto mp    = fxi->mapStorageBuffer(_pm, 0, sizeof(dimf), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, &dimf, sizeof(dimf));
    fxi->unmapStorageBuffer(mp.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    int g    = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, _output->_value->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _pm); // p_dimf (RUNTIME grid dim)
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
  FxShaderStorageBuffer* _pm = nullptr;
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

} // namespace ork::lev2::terrain
