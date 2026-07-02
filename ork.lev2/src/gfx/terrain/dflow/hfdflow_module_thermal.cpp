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
    _exag   = _floatPlug(this, _d, "exaggerated_height_m");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value);
    // scratch buffer for the ping-pong (same size as the output field).
    _scratch = env->createStorageBuffer(size_t(env->_w) * size_t(env->_h) * sizeof(float));
    // talus threshold in NORMALIZED height units: the max stable inter-cell step is
    // tan(angle) * cell_size_m, divided by height_scale to renormalize. PHYSICAL ->
    // resolution-independent (finer cells -> proportionally smaller stable step).
    float cell_m = env->_extent_m / float(env->_w);
    // erosion runs at its OWN (exaggerated) vertical scale, scoped to this op; the env's
    // physical height (_height_scale_m) is for MEASUREMENTS. 0 -> no exaggeration (physical).
    float h_m    = (_exag->value() > 0.0f) ? _exag->value() : env->_height_scale_m;
    float T      = tanf(_talus->value() * 0.017453293f) * cell_m / h_m;
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
    h->accumulateString("terrain.thermal.v2"); // v2: erosion_exaggerated_height_m plug
    h->accumulateItem<int>(_d->_iterations);
    h->accumulateItem<float>(_talus->value());
    h->accumulateItem<float>(_rate->value());
    h->accumulateItem<float>(_exag->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ThermalErodeModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _talus, _rate, _exag;
  FxShaderStorageBuffer* _scratch = nullptr;
  const FxComputeShader* _cs = nullptr;
};

static void _reshapeThermalIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "talus_deg")->setValue(33.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "rate")->setValue(0.15f);
  // erosion vertical exaggeration (meters that normalized 1.0 is during EROSION only);
  // 0 -> use the env physical height. Scoped to this op, NOT a bake-wide scale.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "exaggerated_height_m")->setValue(0.0f);
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

} // namespace ork::lev2::terrain
