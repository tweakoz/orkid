////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::ExprModuleData, "terrain::ExprModuleData");

namespace ork::lev2::terrain {

// max image inputs an ExprModule can read (In0..In{N-1}). Bind contiguously; the
// expression references them as ctx.input(k). Bump freely — the only cost is the
// (unconnected -> sorter-ignored) plugs on generator-mode ExprModules.
static const int kMaxExprInputs = 8;

///////////////////////////////////////////////////////////////////////////////
// ExprModule — runs an authored GLSL expression body over the grid. The full
// compute shader text is supplied by the ptex3d codegen (emit_compute_field /
// compute_template.py) with placeholders the bake fills per-resolution. This is
// the generic generator behind self.hfbake / self.hfmask (the unified substrate).
///////////////////////////////////////////////////////////////////////////////

struct ExprModuleInst : public TerrainComputeInst {
  ExprModuleInst(const ExprModuleData* d, dflow::GraphInst* g)
      : TerrainComputeInst(d, g)
      , _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _inputs.clear();
    for (int k = 0; k < kMaxExprInputs; k++) {
      std::string nm = FormatString("In%d", k);
      _inputs.push_back(typedInputNamed<HfImagePlugTraits>(nm.c_str()));
    }
  }

  // SETUP (alloc + compile) before the dispatch phase, like FbmModuleInst. The
  // %DIM%/%EXTENT_M%/%HEIGHT_M% holes are filled here from BakeEnv (physical
  // scale) — so the authored body is resolution-independent at trace time and
  // the disk shader cache hits per distinct (dim,extent,height).
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    auto img = _output->_value;
    img->_w        = dim;
    img->_h        = dim;
    img->_channels = 1;
    img->_ssbo     = env->createStorageBuffer(size_t(dim) * size_t(dim) * sizeof(float));

    std::string text = _d->_shadertext;
    auto sub = [&](const std::string& key, const std::string& val) {
      size_t pos = 0;
      while ((pos = text.find(key, pos)) != std::string::npos) {
        text.replace(pos, key.size(), val);
        pos += val.size();
      }
    };
    sub("%DIMSQ%",    FormatString("%d", dim * dim));
    sub("%DIMU%",     FormatString("%du", dim));
    sub("%DIM%",      FormatString("%d", dim));
    sub("%EXTENT_M%", FormatString("%f", env->_extent_m));      // physical XZ span
    sub("%HEIGHT_M%", FormatString("%f", env->_height_scale_m)); // PHYSICAL height (not erosion exag)

    auto shdr = fxi->shaderFromShaderText("terrain_expr", text);
    _cs       = fxi->computeShader(shdr, "cs_expr");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);     // output = slot 0
    // connected inputs bind CONTIGUOUSLY at slots 1.. (Python connects In0..In{n-1}
    // and the shadertext declares exactly that many; stop at the first unconnected).
    int slot = 1;
    for (auto& inp : _inputs) {
      auto in = _srcImg(inp);
      if (!in || !in->_ssbo) break;
      ci->bindStorageBuffer(_cs, slot++, in->_ssbo);
    }
    ci->dispatchCompute(_cs, groups, groups, 1);
    ci->storageBarrier();
  }

  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.expr.v1");
    h->accumulateString(_d->_shadertext); // the authored body IS the identity (params baked in)
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ExprModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  std::vector<hfimg_inpluginst_ptr_t> _inputs;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeExprIOs(dataflow::moduledata_ptr_t data) {
  // In0..In{kMaxExprInputs-1}: optional image inputs (generator-mode leaves them
  // unconnected -> sorter ignores them). hfbake = 0 inputs; hfdisplacement wires the
  // current height to In0 (+ extra fields to In1..). createInputPlug dedups, so the
  // double reshapeIOs on deserialize is idempotent.
  for (int k = 0; k < kMaxExprInputs; k++) {
    std::string nm = FormatString("In%d", k);
    dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, nm.c_str());
  }
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}

ExprModuleData::ExprModuleData() {}
std::shared_ptr<ExprModuleData> ExprModuleData::createShared() {
  auto d = std::make_shared<ExprModuleData>();
  _reshapeExprIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t ExprModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ExprModuleInst>(this, g);
}
void ExprModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ExprModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeExprIOs(mdata); });
  // the authored shader text is the portable, python-decoupled artifact -> reflect it.
  clazz->directProperty("shadertext", &ExprModuleData::_shadertext);
}

} // namespace ork::lev2::terrain
