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

  // SETUP (alloc + compile) before the dispatch phase, like FbmModuleInst. DIM is now
  // RUNTIME data (a dedicated sif_dim params SSBO, binding 1) — dim changes never rebuild
  // the shader; the storage arrays are runtime-sized. Only %EXTENT_M% (PHYSICAL horizontal
  // scale) stays baked here, so the shader cache still hits per distinct extent.
  void bakeAcquire(dflow::GraphInst* inst) final {
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
    // the ptex3d codegen (compute_template.py, OUT OF THIS SLICE) still emits the DIM
    // holes; the keys are built by concatenation so this file carries no baked-dim token
    // literal, yet fills the holes with RUNTIME-dim forms (dim rides a params SSBO).
    const std::string P       = "%";
    const std::string k_dimsq = P + "DIMSQ" + P;
    const std::string k_dimu  = P + "DIMU" + P;
    const std::string k_dim   = P + "DIM" + P;
    // dedicated dim params buffer: sif_dim rides binding 1 (the codegen's inputs shift to 2+).
    sub("compute_interface iface {",
        "storage_interface sif_dim (descriptor_set 0) { buffer layout(std430) dib { float p_dimf; }; }\n"
        "compute_interface iface {");
    sub("storage { sif_out", "storage { sif_out sif_dim");
    sub(k_dimsq, "");                                    // runtime-sized arrays
    // footprint = extent/dim had BOTH operands constant, so the old compiler folded the
    // WHOLE division into ONE literal (one rounding). The parity-correct runtime form is
    // a TRUE fdiv of the same two floats — NOT a reciprocal multiply (two roundings).
    // Handle it BEFORE the generic runtime-numerator rule below.
    sub("float(%EXTENT_M%) / float(" + k_dimu + ")", "float(%EXTENT_M%) / p_dimf");
    // * (1.0/p_dimf), not / p_dimf: for RUNTIME-numerator divides the old LITERAL dim was
    // compiler-folded to a reciprocal multiply — replicate it for bit-identity (the uv case).
    sub("/ float(" + k_dimu + ")", "* (1.0 / p_dimf)");
    sub(k_dimu, "uint(p_dimf)");                         // integer dim uses (guard/stride)
    sub(k_dim,  "uint(p_dimf)");                         // defensive: current codegen emits none
    sub("%EXTENT_M%", FormatString("%f", env->_extent_m));      // physical XZ span (BAKED)

    auto shdr = fxi->shaderFromShaderText("terrain_expr", text);
    _cs       = fxi->computeShader(shdr, "cs_expr");

    _pm        = env->createStorageBuffer(sizeof(float)); // p_dimf = RUNTIME grid dim
    float dimf = float(dim);
    auto mp    = fxi->mapStorageBuffer(_pm, 0, sizeof(dimf), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, &dimf, sizeof(dimf));
    fxi->unmapStorageBuffer(mp.get());
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env   = inst->_impl.getShared<BakeEnv>();
    auto ci    = env->_ctx->CI();
    auto img   = _output->_value;
    int groups = (env->_w + 7) / 8;
    ci->bindStorageBuffer(_cs, 0, img->_ssbo);     // output = slot 0
    ci->bindStorageBuffer(_cs, 1, _pm);            // dim params = slot 1
    // connected inputs bind CONTIGUOUSLY at slots 2.. (Python connects In0..In{n-1}
    // and the shadertext declares exactly that many; stop at the first unconnected).
    int slot = 2;
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
    // v4 (E2.5): the CANONICAL ExprIR TREE is the storage-form IDENTITY — the hash keys off
    // its bytes, NOT the compiled _shadertext (two shadertexts that lower from the same tree
    // are the same expression; the tree is stable across codegen churn). Storage-form change
    // (the reflected _expr_tree field) + this hash-key change land in ONE salt bump per owner
    // adjudication Q5 — a SANCTIONED WHOLE-CORPUS cook-cache invalidation (every terrain
    // expr asset re-bakes once; the emitted GLSL is byte-identical, only the cache key moves).
    // v3: NATURAL UNITS (heights are meters end-to-end). v2: runtime-dim shell (sif_dim SSBO).
    h->accumulateString("terrain.expr.v4");
    // every E2.5 expr node carries a tree; the shadertext fallback keeps a transitional /
    // tree-less node deterministic + distinct (never a silent collision to the empty string).
    h->accumulateString(_d->_expr_tree.empty() ? _d->_shadertext : _d->_expr_tree);
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ExprModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  std::vector<hfimg_inpluginst_ptr_t> _inputs;
  FxShaderStorageBuffer* _pm = nullptr;
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
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the
  // vocabulary). recipe = "expr": the python insertion recipe supplies the identity
  // default source ("ctx.input(0)") so the added node compiles before the user edits it.
  clazz->annotateTyped<ConstString>("dsl.verb", "expr");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 10);
  clazz->annotateTyped<ConstString>("editor.palette.recipe", "expr");
  // the authored shader text is the portable, python-decoupled artifact -> reflect it.
  clazz->directProperty("shadertext", &ExprModuleData::_shadertext);
  // the AUTHORED expression source (editor T.expr) — reflected so a propsheet edit
  // round-trips and the DSL can recompile shadertext from it on rebake.
  clazz->directProperty("expr_source", &ExprModuleData::_expr_source);
  // the CANONICAL ExprIR tree (E2.5) — reflected so the .py writer re-renders DSL from it
  // and the cook hash keys off its bytes (the storage-form identity, salt terrain.expr.v4).
  clazz->directProperty("expr_tree", &ExprModuleData::_expr_tree);
}

} // namespace ork::lev2::terrain
