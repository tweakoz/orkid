////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <queue>
#include <cstring>

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

static void _priorityFloodStdHeap(const float* z, float* out, int W, int H, float eps) {
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

// float -> order-preserving uint32 key (flip sign bit for non-negatives, all bits for
// negatives — the standard radix-sort float trick; total order matches operator<).
static inline uint32_t _floodKey(float f) {
  uint32_t u;
  std::memcpy(&u, &f, sizeof(u));
  return (u & 0x80000000u) ? ~u : (u | 0x80000000u);
}

// MONOTONE RADIX HEAP variant: priority-flood only ever pushes keys >= the last popped
// key (fill = max(nz, e[+eps]) >= e), the textbook monotone-PQ case, so a radix heap
// pops in EXACT non-decreasing elevation order — the same semantics as the binary heap
// (ties are benign: equal keys mean equal pop level mean identical fills), for O(1)
// amortized ops instead of O(log n) comparisons over ~33M heap ops per 4096^2 node.
// The std::priority_queue version above was 40% of the whole cold forest cook
// (perf: __adjust_heap + compute, ~3.1s per basin node); ORKID_BASINFILL_STDHEAP=1
// selects it for A/B. The pop level e is recovered from out[] (always written before
// push), so entries carry only (key, index).
static void _priorityFloodRadix(const float* z, float* out, int W, int H, float eps) {
  const int n = W * H;
  std::vector<char> closed(size_t(n), 0);
  struct Ent { uint32_t k; int i; };
  std::vector<Ent> buckets[33];
  uint32_t last = 0; // floor: max key popped so far (monotone invariant: pushes >= last)
  size_t count = 0;
  auto bidx = [](uint32_t k, uint32_t floor) -> int {
    return (k == floor) ? 0 : (32 - __builtin_clz(k ^ floor));
  };
  auto push = [&](uint32_t k, int i) { buckets[bidx(k, last)].push_back({k, i}); count++; };
  auto seed = [&](int idx) {
    if (!closed[idx]) { closed[idx] = 1; out[idx] = z[idx]; push(_floodKey(z[idx]), idx); }
  };
  for (int x = 0; x < W; x++) { seed(x); seed((H - 1) * W + x); }       // top + bottom rows
  for (int y = 0; y < H; y++) { seed(y * W); seed(y * W + (W - 1)); }   // left + right cols
  const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
  const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};                         // 8-connectivity (D8)
  while (count) {
    if (buckets[0].empty()) {
      // advance the floor: smallest nonempty bucket holds the next minimum; entries
      // redistribute to strictly lower buckets relative to the new floor (amortized
      // O(1) moves per element over the whole flood).
      int j = 1;
      while (buckets[j].empty()) j++;
      uint32_t mn = buckets[j][0].k;
      for (auto& e : buckets[j]) mn = std::min(mn, e.k);
      last = mn;
      for (auto& e : buckets[j]) buckets[bidx(e.k, last)].push_back(e);
      buckets[j].clear();
    }
    Ent top = buckets[0].back(); // bucket 0 holds keys == last exactly; any order is fine
    buckets[0].pop_back();
    count--;
    int c   = top.i;
    float e = out[c];
    int cx = c % W, cy = c / W;
    for (int k8 = 0; k8 < 8; k8++) {
      int nx = cx + dx[k8], ny = cy + dy[k8];
      if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
      int ni = ny * W + nx;
      if (closed[ni]) continue;
      float nz   = z[ni];
      float fill = (nz > e) ? nz : (e + eps); // raise to spill level (+eps for drainage)
      closed[ni] = 1;
      out[ni]    = fill;
      push(_floodKey(fill), ni);
    }
  }
}

static void _priorityFlood(const float* z, float* out, int W, int H, float eps) {
  static const bool s_stdheap = (getenv("ORKID_BASINFILL_STDHEAP") != nullptr);
  if (s_stdheap)
    _priorityFloodStdHeap(z, out, W, H, eps);
  else
    _priorityFloodRadix(z, out, W, H, eps);
}

struct BasinFillModuleInst : public TerrainComputeInst {
  BasinFillModuleInst(const BasinFillModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<HfImagePlugTraits>("Out");
    _input  = typedInputNamed<HfImagePlugTraits>("In");
    _eps    = _floatPlug(this, _d, "epsilon");
    _blend  = _floatPlug(this, _d, "blend");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
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
    // `blend` crossfades filled vs the ORIGINAL input (the lpf idiom; CPU op -> CPU mix):
    // 0 = passthrough, 1 (default) = fully filled — the ==1.0 branch is a bit-exact no-op.
    float blend = std::min(std::max(_blend->value(), 0.0f), 1.0f);
    if (blend < 1.0f)
      for (size_t i = 0; i < n; i++)
        filled[i] = z[i] + (filled[i] - z[i]) * blend;
    fxi->unmapStorageBuffer(im.get());
    // write the filled field to the output (consumed by a later node's phase)
    auto om = fxi->mapStorageBuffer(_output->_value->_ssbo, 0, n * sizeof(float), BufferMapAccess::WRITE_ONLY);
    std::memcpy(om->_mappedaddr, filled.data(), n * sizeof(float));
    fxi->unmapStorageBuffer(om.get());
  }
  bool cookCacheDefault() const final { return true; } // measured cache-point class (cost-model analysis)
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.basinfill.v2"); // v2: runtime blend crossfade (CPU mix)
    h->accumulateItem<float>(_eps->value());
    h->accumulateItem<float>(_blend->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const BasinFillModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _eps, _blend;
};

static void _reshapeBasinFillIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // per-step drainage gradient (normalized height units); 0 = pure flat fill.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "epsilon")->setValue(0.0f);
  // crossfade filled vs original: 0 = passthrough, 1 = fully filled (default).
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "blend")->setValue(1.0f);
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
