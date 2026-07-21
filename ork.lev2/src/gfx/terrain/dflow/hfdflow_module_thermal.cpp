////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::ThermalErodeModuleData, "terrain::ThermalErodeModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// ThermalErodeModule — iterative talus relaxation. One step (gather): a cell sheds
// material to lower neighbors wherever the height STEP exceeds the talus threshold T.
// Symmetric pairwise net flow == in(n->C) - out(C->n) summed over 4 neighbors, so it
// is mass-conserving (each pair's flow is +to one, -from the other) AND parallel-safe
// (read old, write new). Border neighbors clamp to self -> no flow leaves the domain.
///////////////////////////////////////////////////////////////////////////////

// dim is RUNTIME data (params SSBO sif_p.p_dimf); the arrays are runtime-sized and the text
// no longer carries dim, so one compile serves every resolution. talus/rate stay baked.
static std::string _thermal_text(float rate) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) ob { float odata[]; }; }
storage_interface sif_in  (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }
storage_interface sif_p   (descriptor_set 0) { buffer layout(std430) pb { float p_dimf; float p_talus; }; }
compute_interface iface { storage { sif_out sif_in sif_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_thermal : iface {
  uint u_dim = uint(p_dimf);
  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(u_dim);
  uint i  = uint(yi) * u_dim + uint(xi);
  int  xl = (xi > 0)     ? (xi - 1) : xi;   // clamp at border -> closed domain (mass conserved)
  int  xr = (xi < W - 1) ? (xi + 1) : xi;
  int  yd = (yi > 0)     ? (yi - 1) : yi;
  int  yu = (yi < W - 1) ? (yi + 1) : yi;
  float hC = idata[i];
  float hL = idata[uint(yi) * u_dim + uint(xl)];
  float hR = idata[uint(yi) * u_dim + uint(xr)];
  float hD = idata[uint(yd) * u_dim + uint(xi)];
  float hU = idata[uint(yu) * u_dim + uint(xi)];
  float T  = p_talus;   // talus threshold: dim-derived (cell_m) -> RUNTIME params
  // net = sum over neighbors of [ inflow(n->C) - outflow(C->n) ], each = max(step - T, 0).
  float net = 0.0;
  net += max((hL - hC) - T, 0.0) - max((hC - hL) - T, 0.0);
  net += max((hR - hC) - T, 0.0) - max((hC - hR) - T, 0.0);
  net += max((hD - hC) - T, 0.0) - max((hC - hD) - T, 0.0);
  net += max((hU - hC) - T, 0.0) - max((hC - hU) - T, 0.0);
  odata[i] = hC + net * float(%RATE%);
}
)S";
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
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    // scratch buffer for the ping-pong (same size as the output field).
    _scratch = env->createStorageBuffer(size_t(env->_w) * size_t(env->_h) * sizeof(float));
    // talus threshold in METERS (heights are natural units): the max stable inter-cell step
    // is tan(repose) * cell_size_m. PHYSICAL -> resolution-independent (finer cells ->
    // proportionally smaller stable step).
    float cell_m = env->_extent_m / float(env->_w);
    float T      = tanf(_talus->value() * 0.017453293f) * cell_m;
    auto fxi     = env->_ctx->FXI();
    auto sh      = fxi->shaderFromShaderText("terrain_thermal", _thermal_text(_rate->value()));
    _cs          = fxi->computeShader(sh, "cs_thermal");
    // dim AND the dim-DERIVED talus threshold (via cell_m) are RUNTIME data now — one
    // compiled shader serves every dim. PARITY: the old literal was `float(%f-text)`,
    // so the uploaded value is %f-ROUNDTRIPPED to match the parsed literal exactly.
    _params      = env->createStorageBuffer(2 * sizeof(float));
    float pm[2]  = {float(env->_w),
                    strtof(FormatString("%f", T).c_str(), nullptr)};
    auto mp      = fxi->mapStorageBuffer(_params, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
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
    ci->bindStorageBuffer(_cs, 2, _params);   // p_dimf (runtime grid dim)
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
      ci->bindStorageBuffer(_cs, 2, _params);   // p_dimf (runtime grid dim)
      ci->dispatchCompute(_cs, g, g, 1);
      cur = nxt;
    }
    // parity gives cur == 0 -> the result is in _output->_value->_ssbo.
  }
  bool cookCacheDefault() const final { return true; } // measured cache-point class (cost-model analysis)
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.thermal.v3"); // v3: heights in meters (talus step in meters; dropped per-op exag plug)
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
  FxShaderStorageBuffer* _params = nullptr; // runtime grid dim (p_dimf), filled in bakeAcquire
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
  // E1-close add-palette (reflection-carried; the hand-curated add-ops tuple is DELETED):
  //   dsl.verb             — the curated DSL wrapper this class inserts as
  //   editor.palette       — appears in the editor add menu (curation marker)
  //   editor.palette.sort  — curated menu position (preserves the old tuple's order)
  // (companion keys on other classes: editor.palette.source = generator/no-input;
  //  editor.palette.recipe = key into the python insertion recipes.)
  clazz->annotateTyped<ConstString>("dsl.verb", "erode_thermal");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 0);
  clazz->directProperty("iterations", &ThermalErodeModuleData::_iterations); // baked step count
}

} // namespace ork::lev2::terrain
