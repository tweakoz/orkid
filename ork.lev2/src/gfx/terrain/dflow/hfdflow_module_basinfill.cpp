////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <queue>

ImplementReflectionX(ork::lev2::terrain::BasinFillModuleData, "terrain::BasinFillModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// BasinFillModule — BASIN (depression / pit / sink) FILL, only. Raises every closed
// depression up to its lowest spill (pour) point so that every cell has a non-increasing
// path to the domain boundary — i.e. no interior local minima remain; filled basins become
// flat lakes at their spill elevation. The rest of the terrain is unchanged.
//
// Implemented with the PRIORITY-FLOOD algorithm (Barnes/Lehman/Mulla 2014): seed a min-heap
// with the boundary cells, repeatedly pop the lowest and raise each un-visited neighbor to
// max(its own height, the pop level). This is an inherently SEQUENTIAL priority-queue flood,
// so — unlike every other terrain op — this is a CPU module: it reads the input field back,
// floods on the CPU (exact, one pass, O(n log n)), and writes the filled field. (A GPU
// Planchon-Darboux relaxation would need ~O(dim) Jacobi iterations and risk under-filling.)
//
// `epsilon` (normalized height units, default 0) adds a tiny per-step gradient so filled
// flats still drain toward the outlet (Priority-Flood+Epsilon); 0 = pure flat fill.
///////////////////////////////////////////////////////////////////////////////

static void _priorityFlood(const float* z, float* out, int W, int H, float eps) {
  const int n = W * H;
  std::vector<char> closed(size_t(n), 0);
  using PE = std::pair<float, int>; // (elevation, index); min-heap
  std::priority_queue<PE, std::vector<PE>, std::greater<PE>> pq;
  auto seed = [&](int idx) {
    if (!closed[idx]) { closed[idx] = 1; out[idx] = z[idx]; pq.push({z[idx], idx}); }
  };
  for (int x = 0; x < W; x++) { seed(x); seed((H - 1) * W + x); }       // top + bottom rows
  for (int y = 0; y < H; y++) { seed(y * W); seed(y * W + (W - 1)); }   // left + right cols
  const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
  const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};                         // 8-connectivity (D8)
  while (!pq.empty()) {
    PE top = pq.top();
    pq.pop();
    float e = top.first;
    int c  = top.second;
    int cx = c % W, cy = c / W;
    for (int k = 0; k < 8; k++) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
      int ni = ny * W + nx;
      if (closed[ni]) continue;
      float nz   = z[ni];
      float fill = (nz > e) ? nz : (e + eps); // raise to spill level (+eps for drainage)
      closed[ni] = 1;
      out[ni]    = fill;
      pq.push({fill, ni});
    }
  }
}

struct BasinFillModuleInst : public TerrainComputeInst {
  BasinFillModuleInst(const BasinFillModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _eps    = _floatPlug(this, _d, "epsilon");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _output->_value); // output SSBO (no shaders — this is a CPU module)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    if (not env->_per_op_sync) { // ops self-defend: the mid-graph readback below needs per-op submit+wait
      printf("terrain basin_fill<%s>: this graph's driver does NOT sync per op — the CPU readback "
             "would map unsubmitted data. Use basin_fill only in a HeightField bake.\n",
             _dgmodule_data->_name.c_str());
      OrkAssert(false);
    }
    auto fxi = env->_ctx->FXI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int W = env->_w, H = env->_h;
    size_t n = size_t(W) * size_t(H);
    // read the input field (valid: the upstream node submitted+waited in its own phase)
    auto im      = fxi->mapStorageBuffer(in->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* z = static_cast<const float*>(im->_mappedaddr);
    std::vector<float> filled(n);
    _priorityFlood(z, filled.data(), W, H, _eps->value());
    fxi->unmapStorageBuffer(im.get());
    // write the filled field to the output (consumed by a later node's phase)
    auto om = fxi->mapStorageBuffer(_output->_value->_ssbo, 0, n * sizeof(float), BufferMapAccess::WRITE_ONLY);
    std::memcpy(om->_mappedaddr, filled.data(), n * sizeof(float));
    fxi->unmapStorageBuffer(om.get());
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.basinfill.v1"); // priority-flood depression fill (CPU)
    h->accumulateItem<float>(_eps->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const BasinFillModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _eps;
};

static void _reshapeBasinFillIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // per-step drainage gradient (normalized height units); 0 = pure flat fill.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "epsilon")->setValue(0.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
BasinFillModuleData::BasinFillModuleData() {}
std::shared_ptr<BasinFillModuleData> BasinFillModuleData::createShared() {
  auto d = std::make_shared<BasinFillModuleData>(); _reshapeBasinFillIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t BasinFillModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<BasinFillModuleInst>(this, g);
}
void BasinFillModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return BasinFillModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeBasinFillIOs(m); });
}

} // namespace ork::lev2::terrain
