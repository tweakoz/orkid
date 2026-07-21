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
#include <ork/lev2/gfx/dflow_gpuupdate.h> // family-neutral gpuUpdate seam (bake stamps its params)
#include <ork/lev2/gfx/image.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <ork/kernel/string/string.h>
#include <chrono>
#include <map>
#include <algorithm>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/kernel/datacache.h> // DataBlockCache — per-node cook cache
#include <ork/kernel/opq.h>       // concurrent queue — parallel capture encodes (WS7)
#include <ork/kernel/semaphore.h>
#include <ork/lev2/gfx/gpumicrotask.h> // MT3 — SOFT_DEADLINE re-bake slicing
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cinttypes>
#include <atomic>
#include <functional>
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::TerrainModuleData, "terrain::TerrainModuleData");
ImplementReflectionX(ork::lev2::terrain::CaptureModuleData, "terrain::CaptureModuleData");
ImplementReflectionX(ork::lev2::terrain::TerrainSubGraphModuleData, "terrain::TerrainSubGraphModuleData");
ImplementReflectionX(ork::lev2::terrain::TerrainLoopModuleData, "terrain::TerrainLoopModuleData");

// member of the INTERCHANGE namespace (B.3) — must be defined in an enclosing namespace of the class.
namespace ork::lev2::dflowgfx {
gpucomputeimage2d_inst_ptr_t HfImagePlugTraits::data_to_inst(gpucomputeimage2d_data_ptr_t inp) {
  return std::make_shared<GpuComputeImage2DInst>(inp);
}
} // namespace ork::lev2::dflowgfx

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// bases
///////////////////////////////////////////////////////////////////////////////

void TerrainModuleData::describeX(class_t* clazz) {
}
TerrainModuleData::TerrainModuleData() {
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
    if (not env->_flushes_captures) { // ops self-defend: a non-bake host never flushes captures
      printf(
          "terrain Capture<%s>: this graph's driver does NOT flush captures (a capture in a "
          "cross-family/live graph would silently write nothing). Bake captures with a "
          "HeightField asset (bakeHeightfield).\n",
          _cmd->_name.c_str());
      OrkAssert(false);
    }
    // SELECT-AS-OUTPUT drop: an output-node bake disconnects every non-display capture's
    // "In" (see bakeHeightfield). A disconnected sink records nothing and the flush skips
    // it — self-defend loudly rather than assert on the (deliberately) missing edge.
    if (not _input->_connectedOutput) {
      printf(
          "terrain Capture<%s>: input 'In' is disconnected (select-as-output dropped this "
          "sink) — skipping capture.\n",
          _cmd->_name.c_str());
      return;
    }
    // resolve the SOURCE field from the CONNECTED output (not our input's own
    // _value, which is an empty default) — that's where the producer set _ssbo.
    auto out = std::dynamic_pointer_cast<hfimg_outpluginst_t>(_input->_connectedOutput);
    OrkAssert(out);
    env->_captures.push_back(CaptureRequest{out->_value, _cmd->_channel, _cmd->_path, _cookKey});
  }
  const CaptureModuleData* _cmd;
  hfimg_inpluginst_ptr_t _input;
  uint64_t _cookKey = 0; // set by the bake driver's currency pass (0 = cache disabled)
};

///////////////////////////////////////////////////////////////////////////////
// Capture-currency sidecar — "<output-file>.cookhash", one text line carrying the
// capture key (producer cook-hash mix) AND the channel's FieldStats. The stats ride
// along because the caller's manifest (asset_gen) needs min/max/mean even when the
// capture is skipped — a currency hit must return the SAME stats the original flush
// did. Version-prefixed; any parse failure = not current = re-bake (never trust).
///////////////////////////////////////////////////////////////////////////////

static void _writeCaptureSidecar(
    const std::string& imgpath, uint64_t key, const FieldStats& fs, float extent_m, int dim) {
  std::ofstream out(imgpath + ".cookhash", std::ios::trunc);
  if (out) // hfcap2 = natural-units flush (EXR raw); the version bump invalidates every
           // pre-migration normalized product without touching any cache. The trailing
           // units/extent/texel fields are SELF-DESCRIPTION for bare-EXR consumers (the
           // reader's 5-field parse ignores them; the manifest stays the full contract).
    out << "hfcap2 hash<" << std::hex << key << std::dec //
        << "> channel<" << fs._channel                   //
        << "> min<" << std::setprecision(9) << fs._min   //
        << "> max<" << fs._max                           //
        << "> mean<" << fs._mean                         //
        << "> units<meters> extent_m<" << extent_m       //
        << "> texel_m<" << ((dim > 0) ? (extent_m / float(dim)) : 0.0f) << ">\n";
}

static bool _readCaptureSidecar(const std::string& imgpath, uint64_t expect_key, fieldstats_ptr_t& out_stats) {
  std::error_code ec;
  if (not std::filesystem::exists(imgpath, ec)) // the image itself must exist too
    return false;
  std::ifstream in(imgpath + ".cookhash");
  if (not in)
    return false;
  std::string line;
  std::getline(in, line);
  uint64_t key = 0;
  char chname[128] = {0};
  float mn = 0.0f, mx = 0.0f, me = 0.0f;
  if (sscanf(line.c_str(), "hfcap2 hash<%" SCNx64 "> channel<%127[^>]> min<%g> max<%g> mean<%g>", //
             &key, chname, &mn, &mx, &me) != 5)
    return false;
  if (key != expect_key)
    return false;
  out_stats           = std::make_shared<FieldStats>();
  out_stats->_min     = mn;
  out_stats->_max     = mx;
  out_stats->_mean    = me;
  out_stats->_channel = chname;
  return true;
}

// shared channel-list splitter (flush + currency pass must agree exactly)
static std::vector<std::string> _splitCaptureChannels(const std::string& cs, const char* fallback) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= cs.size()) {
    size_t comma   = cs.find(',', start);
    size_t len     = (comma == std::string::npos) ? std::string::npos : (comma - start);
    std::string tk = cs.substr(start, len);
    if (not tk.empty())
      out.push_back(tk);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  if (out.empty())
    out.push_back(fallback);
  return out;
}

// per-channel output path: substitute "{channel}" in the template if present
static std::string _capturePathForChannel(const ork::file::Path& tmpl, const std::string& ch) {
  std::string path = ork::file::Path::expandPathString(std::string(tmpl.c_str()));
  const std::string mark = "{channel}";
  size_t pos = path.find(mark);
  if (pos != std::string::npos)
    path.replace(pos, mark.size(), ch);
  return path;
}

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
  // serialize the channel identity so the graph self-describes its sinks (_path
  // is machine-specific and stays unreflected — derived from _channel at bake).
  clazz->directProperty("channel", &CaptureModuleData::_channel);
  // per-bake cook-cache opt-out (round-trips with the graph).
  clazz->directProperty("cache", &CaptureModuleData::_cache);
  // S4 progressive display: on_complete (default) / on_checkpoint (see hfdflow.h).
  clazz->directProperty("visual_update_mode", &CaptureModuleData::_visual_update_mode);
}

///////////////////////////////////////////////////////////////////////////////
// BakeEnv allocation arena — see the declaration notes in hfdflow.h.
///////////////////////////////////////////////////////////////////////////////

// Bake-buffer residency policy for DISCRETE GPUs: iterative bake
// computes (eflow-class flow sims) against HOST planes run GPU-100% but PCIe-BUS-bound
// (measured 8.8 GB/s rx + 2.8 GB/s tx for 29+ min) — VRAM-resident the same compute is
// O(minutes→seconds). So big planes go DEVICE_LOCAL under a live-bytes budget; beyond
// the budget they degrade to HOST (forest-class multi-10GB transient peaks must never
// OOM the 24GB card — §4 sequencing trap). Small allocations (per-eval params SSBOs)
// ALWAYS stay HOST: on DEVICE every CPU map/write is a synchronous staged submit+wait
// (see ork.dox/vk_deferred_updates_postmortem.md — the deferral that made those cheap
// was removed). CPU touches of DEVICE planes (cook-cache load/store, capture EXR reads,
// mid-graph readback modules) route through the staged copy paths, which the per-op
// submit+WAIT bake driver makes safe. On UMA (apple) the budget is 0 = pure-HOST as
// before: a DL-only request there would force the staged map path for zero benefit.
// ORKID_BAKE_DEVICE_MB overrides the budget (0 disables the policy entirely).
static size_t _bakeDeviceBudgetBytes(Context* ctx) {
  static size_t s_budget = [ctx]() -> size_t {
    if (const char* e = getenv("ORKID_BAKE_DEVICE_MB"))
      return size_t(atoll(e)) << 20;
#if defined(__APPLE__)
    return 0; // UMA: HOST already is device-local
#else
    // VRAM-SCALED default: the fixed 8GB spilled eflow's ~22GB cold frontier to HOST
    // (per-op profile 2026-07-03: 708s of bus-bound GPU stall at rx ~10GB/s). Leave a
    // fixed render-path headroom instead — bakes run at load time when render demand
    // is small, and the arena frees entirely at bake end.
    size_t heap = ctx ? ctx->deviceLocalHeapBytes() : 0;
    if (heap > (size_t(10) << 30))
      return heap - (size_t(6) << 30); // 3090: 24GB -> 18GB budget
    return size_t(8192) << 20; // small/unknown cards keep the conservative 8GB
#endif
  }();
  return s_budget;
}
static constexpr size_t kBakeDeviceMinBytes = size_t(1) << 20; // planes only; params stay HOST

FxShaderStorageBuffer* BakeEnv::createStorageBuffer(size_t length) {
  // Residency class for the pool key: 0 = HOST, 1 = DEVICE. The budgeted-DEVICE policy
  // (comment above) picks DEVICE for big planes while the live-bytes budget holds; the
  // (size, class) pool key guarantees reuse never crosses residency classes.
  bool want_device = (length >= kBakeDeviceMinBytes)                     //
                 and ((_device_bytes + length) <= _bakeDeviceBudgetBytes(_ctx));
  const int res_class = want_device ? 1 : 0;
  const poolkey_t key{length, res_class};
  if (_lazy_acquire) {
    // frontier mode: serve from the (size, residency)-classed free-list when possible.
    auto it = _pool_free.find(key);
    if (it != _pool_free.end() and not it->second.empty()) {
      auto buf = it->second.back();
      it->second.pop_back();
      _pool_free_set.erase(buf);
      _pool_reuses++;
      if (_in_scope)
        _scope.push_back(buf);
      return buf;
    }
  }
  auto fxi = _ctx->FXI();
  auto buf = want_device //
      ? fxi->createStorageBuffer(length, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE)
      : fxi->createStorageBuffer(length);
  if (want_device)
    _device_bytes += length;
  _allocs.push_back(buf);
  _alloc_size[buf] = key;
  _arena_bytes += length;
  if (_arena_bytes > _peak_arena_bytes)
    _peak_arena_bytes = _arena_bytes;
  if (_in_scope)
    _scope.push_back(buf);
  return buf;
}

void BakeEnv::releaseToPool(FxShaderStorageBuffer* buf) {
  if (nullptr == buf)
    return;
  if (not _pool_free_set.insert(buf).second)
    return; // already free (plug aliasing) — releasing twice would double-lend it
  auto it = _alloc_size.find(buf);
  OrkAssert(it != _alloc_size.end()); // released a buffer this arena never created
  _pool_free[it->second].push_back(buf);
}

void BakeEnv::beginNodeScope() {
  OrkAssert(not _in_scope);
  _scope.clear();
  _in_scope = true;
}

void BakeEnv::endNodeScope(const std::unordered_set<FxShaderStorageBuffer*>& keep) {
  OrkAssert(_in_scope);
  _in_scope = false;
  for (auto b : _scope)
    if (keep.find(b) == keep.end())
      releaseToPool(b);
  _scope.clear();
}

void BakeEnv::freeAllocs() {
  auto fxi = _ctx->FXI();
  std::unordered_set<FxShaderStorageBuffer*> freed;
  for (auto b : _allocs)
    if (b and freed.insert(b).second)
      fxi->destroyStorageBuffer(b);
  _allocs.clear();
  _pool_free.clear();
  _pool_free_set.clear();
  _alloc_size.clear();
  _arena_bytes  = 0;
  _device_bytes = 0;
}

///////////////////////////////////////////////////////////////////////////////
// driver
///////////////////////////////////////////////////////////////////////////////

// cache-hit count from the most recent cacheable bake (for terrainCacheTest).
static int s_lastCookHits = -1;
// SubGraph/Loop nested-cook accounting from the most recent bake (for terrainSubgraphTest):
// summed compute + cache-loaded nested-node counts across every composite iteration. Reset
// per bake at the top of bakeHeightfield; the composite insts accumulate into them.
static int s_lastSubgraphComputes = -1;
static int s_lastSubgraphLoads    = -1;

///////////////////////////////////////////////////////////////////////////////
// runCookedGraph — the per-node Merkle cook loop, factored out of bakeHeightfield
// so a SubGraphModuleInst can run its NESTED GraphInst through the SAME machinery
// (per-iteration cache granularity must survive: computeNodeHashes, per-node
// cook-cache find/store against "dflowcache", per-op dispatch-phase submit+WAIT,
// demand planning / skip logic, capture-currency, pooled release schedule).
//
// PRECONDITIONS the caller owns (bakeHeightfield today; a module inst later):
//   * ginst is instantiated + updateTopology'd and carries `env` on its _impl
//     (the modules resolve BakeEnv via their own _graphinst->_impl).
//   * a frame is open (ctx->beginFrame) — the dispatch phases nest inside it here.
// It does NOT stock env, flush captures, free allocs, or begin/end the frame; the
// bake-driver-only concerns (the s_lastCookHits stat, the capture flush) stay with
// the caller and consume the returned counts + sidecar-recovered stats.
///////////////////////////////////////////////////////////////////////////////

struct CookedGraphResult {
  int _cook_loaded   = 0; // nodes served from the cook cache (uploaded a cached plane)
  int _cook_skipped  = 0; // nodes the demand plan never dispatched (cache-satisfied)
  int _cook_computes = 0; // nodes that dispatched (+ maybe stored)
  // FieldStats recovered from capture-currency sidecars (sinks that never ran) — the
  // caller appends these to the flush's stats so its manifest contract holds either way.
  std::vector<fieldstats_ptr_t> _current_stats;
};

///////////////////////////////////////////////////////////////////////////////
// MT3 — the `_cursor`-over-topo-list resume state (JUL13_DFLOW §2.6/§E5, line 255).
// runCookedGraph plans ONCE (into this state), then advances the SAME per-node
// loop `max_nodes` at a time; each partial call saves `_cursor` and returns via
// `out_more=true`. A null CookSliceState (the burst/blocking bakeHeightfield path)
// uses a function-local one at unlimited budget and runs to completion in one call
// — byte-identical to HEAD (the plan + loop statements are unchanged, only their
// storage moved to references into this struct and the loop bound became `_cursor`).
// T10: while a cook is paused between slices this state (and env's pooled SSBOs)
// stays live across frames — 100s of MB of VRAM for a big frontier is ACCEPTABLE.
///////////////////////////////////////////////////////////////////////////////

struct CookSliceState {
  bool   _planned = false;
  size_t _cursor  = 0; // next node index in the topo order
  size_t _N       = 0;
  bool   do_disk_cache = true;
  // demand-plan vectors (filled once by the planning block below)
  std::vector<bool>        hit, needed, sink, capcurrent;
  std::vector<const char*> missreason;
  std::vector<std::vector<dflow::outpluginst_ptr_t>> release_at;
  // S4: nodes whose completed "Out" plane is checkpoint-published to env->_live_out
  // (viewable + mono + an ancestor of the on_checkpoint height sink). Empty unless armed.
  std::vector<bool>        livepub;
  // running counters (persist across slices)
  int    cook_loaded = 0, cook_computes = 0, cook_skipped = 0;
  size_t ncap_readback = 0;
  CookedGraphResult _result; // accumulates _current_stats; finalized counts written at end
  // (the T9 cacheStore gate lives on BakeEnv::_allow_store — it must reach nested cooks too)
};

// SubGraphModule support (STEP 2): a composite inst runs its NESTED GraphInst through
// this same driver.
//   * context_salt/salted — a LoopModuleInst folds a per-iteration salt into the nested
//     cook-context hash so each iteration's nodes hash DISTINCTLY (per-iteration Merkle
//     granularity: the crux N->N+k oracle). salted=false reproduces the pre-subgraph
//     context hash EXACTLY (byte-identity for the top-level bake).
//   * forced_roots — a nested body has no Capture sink, so nothing would be demanded;
//     the composite forces its promoted-output inner insts to be demanded (their
//     upstream is pulled normally) AND pinned (never released to the pool), so the
//     composite can read the result out after the nested run returns.
static CookedGraphResult runCookedGraph(
    dflow::graphinst_ptr_t ginst, const bakeenv_ptr_t& env, ui::updatedata_ptr_t updata,
    uint64_t context_salt = 0, bool salted = false,
    const std::vector<dflow::dgmoduleinst_ptr_t>& forced_roots = {},
    bool allow_disk_cache = true,
    // MT3 slicing (default nullptr/-1 == the burst path, one call to completion):
    CookSliceState* slice_state = nullptr, // persistent plan + `_cursor`; nullptr -> function-local
    int64_t max_nodes = -1,                // nodes to advance THIS call; <0 == unbounded (burst)
    bool* out_more = nullptr) {            // set true when nodes remain (yield); false at completion
  // shared BakeEnv gives the bake grid/world units the cook-context hash + probe key on
  // (W==H==dim); graph is ginst's own graphdata (the do_disk_cache scan reads its
  // CaptureModules). The SubGraphModuleInst passes the parent env + its nested inst.
  CookSliceState  _local_slice;
  CookSliceState& S    = slice_state ? *slice_state : _local_slice;
  // Alias every persistent plan datum + counter to the slice state — the burst path's
  // function-local S makes this byte-identical to HEAD; a sliced call resumes from S.
  auto& current_stats  = S._result._current_stats; // sidecar-recovered stats (currency pass)
  auto& do_disk_cache  = S.do_disk_cache;
  auto& hit            = S.hit;
  auto& needed         = S.needed;
  auto& sink           = S.sink;
  auto& capcurrent     = S.capcurrent;
  auto& missreason     = S.missreason;
  auto& release_at     = S.release_at;
  int&  cook_loaded    = S.cook_loaded;
  int&  cook_computes  = S.cook_computes;
  int&  cook_skipped   = S.cook_skipped;
  size_t& ncap_readback = S.ncap_readback;
  Context* ctx         = env->_ctx;
  auto ci              = ctx->CI();
  const int dim        = env->_w;
  const float extent_m = env->_extent_m;
  auto graph           = ginst->_graphdata;
    // per-node cook cache: Merkle-hash every node (cheap scalars), then per node
    // either upload its cached field (hit) or dispatch it IN ITS OWN dispatch
    // phase + read the result back to cache (miss). Per-op submit+wait makes the
    // inline readback valid. The Capture sink has no cookStore, so it just runs.
    //
    // The DISK cook cache is gated separately from this per-op synced loop: any
    // CaptureModule with cache=false (self.capture(..., cache=False)) turns off
    // the find/store for the WHOLE bake (the cache is whole-bake, per-node), while
    // the synced compute below — which terrain needs for cross-node SSBO ordering
    // — still runs. So cache=False = recompute every run, never touch dflowcache.
    // allow_disk_cache=false (a composite nested run under a NON-cacheable host) forces
    // recompute-every-run: a captureless nested body would otherwise disk-cache with a
    // salt that (host non-cacheable => no computeNodeHashes => salt base 0) is NOT
    // sensitive to the external input, risking a stale cross-input hit. Keeping nested
    // disk-caching iff the host is cacheable makes the salt-valid <=> disk-cache invariant hold.
    auto&        order = ginst->_ordered_module_insts; // (loop-invariant; the loop reads it too)
    const size_t N     = order.size();
    S._N               = N;
    // debug flags read by BOTH the plan block and the loop — function scope so a sliced
    // re-entry (plan skipped) still sees them (see ORKID_COOK_DEBUG / BAKE_DEFERRED_FLUSH).
    static const bool s_cookdbg        = (getenv("ORKID_COOK_DEBUG") != nullptr);
    static const bool s_deferred_flush = (getenv("ORKID_BAKE_DEFERRED_FLUSH") != nullptr);
    // MT3 — PLAN ONCE. A sliced cook re-enters runCookedGraph every slice; this whole
    // demand-plan block runs on the FIRST call only, its results persisting in S. The
    // burst path (S == the function-local) runs it exactly as HEAD did.
    if (not S._planned) {
    do_disk_cache = allow_disk_cache;
    for (size_t i = 0; do_disk_cache and i < graph->numModules(); i++) {
      if (auto cap = std::dynamic_pointer_cast<CaptureModuleData>(graph->module(i))) {
        if (not cap->_cache) {
          do_disk_cache = false;
          break;
        }
      }
    }
    // cook context = everything outside the graph that changes a node's OUTPUT:
    // the bake resolution AND the world units (meters->texels depends on dim/extent).
    // Heights are TRUE METERS on the plugs — no vertical scale exists to fold in.
    if (do_disk_cache) {
      {
        auto ch = DataBlock::createHasher();
        ch->accumulateItem<int>(dim);
        ch->accumulateItem<float>(extent_m);
        ch->accumulateString("terrain.naturalunits.v1"); // units migration epoch
        // per-iteration salt (loop nested runs only) — folded AFTER the epoch so the
        // unsalted top-level bake's context hash is bit-identical to pre-subgraph HEAD.
        if (salted)
          ch->accumulateItem<uint64_t>(context_salt);
        ch->finish();
        ginst->_cookContextHash = ch->result();
      }
      ginst->computeNodeHashes();
    }
    ////////////////////////////////////////////////////////////////////////
    // WS4 FRONTIER — demand-driven plan over the topo order, THEN the loop.
    //
    // A cache-HIT node never reads its inputs (compute is skipped), so demand
    // propagates upstream only through nodes that will DISPATCH. Consequences:
    //  * a node nobody (transitively) dispatches against is SKIPPED outright —
    //    no buffer, no disk read, no upload. A fully-warm bake touches only the
    //    capture sources.
    //  * each surviving buffer is released back to the env pool at its LAST
    //    dispatching reader (capture sources stay pinned — the flush below
    //    reads them after the loop; freeAllocs reclaims them at bake end).
    // Per-op submit+WAIT makes mid-loop reuse GPU-safe (see BakeEnv notes).
    ////////////////////////////////////////////////////////////////////////

    // forced demand roots (composite nested runs): a set for O(1) membership.
    std::unordered_set<const void*> forced_root_set;
    for (auto& r : forced_roots)
      forced_root_set.insert(r.get());

    // --- classify: sink / cache-hit. Probe via PREFIX reads (~256B of header) —
    // the full plane is fetched from disk only when a needed hit actually LOADS
    // (below), and released right after upload. Reading whole entries here held
    // ~33GB of planes for a warm 4096 bake (the 40+GB load-RSS incident).
    // ORKID_COOK_DEBUG=1: record WHY a node isn't a hit (printed if it dispatches:
    // no-cache-entry = hash not on disk; probe-reject = entry fails validation;
    // not-cacheable-type = not a TerrainComputeInst).
    hit.assign(N, false); needed.assign(N, false); sink.assign(N, false);
    missreason.assign(N, nullptr); // cookdbg: why not a cache hit
    for (size_t i = 0; i < N; i++) {
      auto inst = order[i];
      sink[i]   = (inst->numOutputs() == 0); // Capture — always runs, reads at flush
      if (s_cookdbg) // full topo map (node -> cook hash) for offline blob forensics
        printf("[cookdbg] PLAN i<%zu> node<%s> hash<0x%zx>%s\n", i,
               inst->_abstract_module_data->_name.c_str(), size_t(inst->_cookHash),
               sink[i] ? " (sink)" : "");
      if (do_disk_cache and not sink[i]) {
        if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst)) {
          // STRATEGIC CACHE POINTS: only cache-point classes probe (and store, below).
          // A non-cache-point node is never a hit, so warm demand walks through it to
          // the deepest clean cached cut on each fork and recomputes the cheap segment.
          if (not tci->cookIsCachePoint()) {
            missreason[i] = "not-a-cache-point";
          } else {
            auto hdr = DataBlockCache::findDataBlockPrefix("dflowcache", inst->_cookHash, 256);
            if (hdr and tci->cookProbe(hdr, dim, dim))
              hit[i] = true;
            else
              missreason[i] = hdr ? "probe-reject" : "no-cache-entry";
          }
        } else {
          missreason[i] = "not-cacheable-type";
        }
      }
    }

    // --- producer map: output-pluginst -> node index (same walk computeNodeHashes uses)
    std::unordered_map<const void*, size_t> producer_of;
    for (size_t i = 0; i < N; i++)
      for (int o = 0; o < order[i]->numOutputs(); o++)
        producer_of[order[i]->output(o).get()] = i;

    // --- capture-currency: a Capture sink whose on-disk outputs carry a sidecar
    // matching its CURRENT key (producer cook-hash + channels + path template + a
    // format salt) does not need to run — and (demand below) pulls NOTHING upstream.
    // A fully-current bake therefore computes zero nodes, reads zero cache blobs and
    // re-encodes zero files. The sidecar also returns the channel FieldStats (the
    // caller's manifest contract). Any missing/mismatched file → the sink runs and the
    // flush rewrites images + sidecars.
    capcurrent.assign(N, false);
    for (size_t i = 0; i < N; i++) {
      if (not sink[i] or not do_disk_cache)
        continue;
      auto cap = std::dynamic_pointer_cast<CaptureModuleInst>(order[i]);
      if (not cap)
        continue;
      // producer hash: the sink's single input's connected producer node
      uint64_t prod_hash = 0;
      for (auto inp : cap->_inputs)
        if (inp->_connectedOutput) {
          auto it = producer_of.find(inp->_connectedOutput.get());
          if (it != producer_of.end())
            prod_hash = order[it->second]->_cookHash;
        }
      auto ch = DataBlock::createHasher();
      ch->accumulateItem<int>(0x6f0a0001); // capture-currency format salt
      ch->accumulateItem<uint64_t>(prod_hash);
      ch->accumulateString(cap->_cmd->_channel);
      ch->accumulateString(std::string(cap->_cmd->_path.c_str()));
      ch->finish();
      cap->_cookKey = ch->result(); // flush writes sidecars with this when the sink runs
      // current only if EVERY emitted channel's image + sidecar match the key. An EMPTY
      // channel list never claims currency: the flush's fallback channel name differs by
      // branch (mono "height" vs multi "field") and the plan can't know which applies.
      if (cap->_cmd->_channel.empty())
        continue;
      auto channels = _splitCaptureChannels(cap->_cmd->_channel, "field");
      bool all_ok   = true;
      std::vector<fieldstats_ptr_t> sstats;
      for (auto& chname : channels) {
        fieldstats_ptr_t fs;
        if (_readCaptureSidecar(_capturePathForChannel(cap->_cmd->_path, chname), cap->_cookKey, fs))
          sstats.push_back(fs);
        else {
          all_ok = false;
          break;
        }
      }
      if (all_ok) {
        capcurrent[i] = true;
        for (auto& fs : sstats)
          current_stats.push_back(fs);
        if (s_cookdbg)
          printf("[cookdbg] CAPTURE-CURRENT sink<%s> key<0x%zx> (%zu files)\n", //
                 cap->_abstract_module_data->_name.c_str(), size_t(cap->_cookKey), channels.size());
      }
    }

    // --- demand (reverse-topo): non-current sinks run; a dispatching node pulls its producers.
    // A forced root (composite promoted-output inner inst) is demanded like a sink.
    for (size_t ri = N; ri > 0; ri--) {
      size_t j = ri - 1;
      if (sink[j] and not capcurrent[j])
        needed[j] = true;
      if (forced_root_set.count(order[j].get()))
        needed[j] = true;
      if (not(needed[j] and not hit[j]))
        continue; // hit or unneeded -> reads nothing
      for (auto inp : order[j]->_inputs)
        if (inp->_connectedOutput) {
          auto it = producer_of.find(inp->_connectedOutput.get());
          if (it != producer_of.end())
            needed[it->second] = true;
        }
    }

    // --- S4 progressive display (JUL13 §E5/S4): a Capture sink requesting
    // visual_update_mode == "on_checkpoint" for the HEIGHT channel arms the named
    // LiveOutput artifact (BakeEnv-owned, OUTSIDE the register pool) and marks every
    // publishable node: VIEWABLE (class default + reflected per-node override) AND an
    // ANCESTOR of the sink (a mask/side-branch plane must never flash as the display
    // heights). This block lives on the per-node cook branch ONLY — the structural
    // fork's checkpoint-capable side; the one-phase non-cacheable driver degrades
    // loudly at its call site (bakeHeightfield). ORKID_S4_DISABLE=1 kills it here.
    S.livepub.assign(N, false);
    if (not s4ProgressiveDisabled()) {
      for (size_t i = 0; i < N; i++) {
        if (not sink[i])
          continue;
        auto cap = std::dynamic_pointer_cast<CaptureModuleInst>(order[i]);
        if (not cap or cap->_cmd->_visual_update_mode != "on_checkpoint")
          continue;
        auto channels   = _splitCaptureChannels(cap->_cmd->_channel, "height");
        bool has_height = false;
        for (auto& ch : channels)
          has_height |= (ch == "height");
        if (not has_height)
          continue; // v1: the live artifact IS the height plane
        // ancestor closure of this sink (reverse-topo over the producer map)
        std::vector<bool> anc(N, false);
        for (auto inp : cap->_inputs)
          if (inp->_connectedOutput) {
            auto it = producer_of.find(inp->_connectedOutput.get());
            if (it != producer_of.end())
              anc[it->second] = true;
          }
        for (size_t ri = N; ri > 0; ri--) {
          size_t j = ri - 1;
          if (not anc[j])
            continue;
          for (auto inp : order[j]->_inputs)
            if (inp->_connectedOutput) {
              auto it = producer_of.find(inp->_connectedOutput.get());
              if (it != producer_of.end())
                anc[it->second] = true;
            }
        }
        size_t npub = 0;
        for (size_t j = 0; j < N; j++)
          if (anc[j])
            if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(order[j]))
              if (tci->isViewable()) {
                S.livepub[j] = true;
                npub++;
              }
        env->_live_key = liveFieldCanonicalKey(_capturePathForChannel(cap->_cmd->_path, "height"));
        env->_live_out = liveFieldAcquire(env->_live_key);
        env->_live_out->beginBake();
        printf("[s4-live] ARMED key<%s> sink<%s> publishable<%zu/%zu nodes>\n",
               env->_live_key.c_str(), cap->_abstract_module_data->_name.c_str(), npub, N);
        break; // v1: ONE live artifact per bake (the height plane)
      }
    } else {
      for (size_t i = 0; i < N; i++) {
        if (not sink[i])
          continue;
        auto cap = std::dynamic_pointer_cast<CaptureModuleInst>(order[i]);
        if (cap and cap->_cmd->_visual_update_mode == "on_checkpoint") {
          printf("[s4-live] DISABLED (ORKID_S4_DISABLE) — sink<%s> degrades to on_complete\n",
                 cap->_abstract_module_data->_name.c_str());
          break;
        }
      }
    }

    // --- release schedule: for each producer OUTPUT PLUG, the last dispatching
    // reader's index. INCREMENTAL FLUSH (default): a sink counts as a normal last
    // reader — its field is read back to host the moment it runs (below), so the
    // plane needs no post-loop pinning. ORKID_BAKE_DEFERRED_FLUSH=1 restores the
    // old behavior (pin sink-read plugs; the post-loop flush maps the live SSBOs) —
    // costs O(#sinks) planes of VRAM for the whole bake, which is what spilled
    // eflow's frontier past the DEVICE budget (2026-07-03 profile).
    // Buffers resolve from the plug AT RELEASE TIME (erox aliases its output to a
    // ping-pong buffer during compute, so the pointer is only final after it runs).
    std::unordered_map<const void*, size_t> last_reader; // outpluginst -> index
    std::unordered_set<const void*> pinned;
    // forced roots (composite promoted outputs): pin every output plug so it survives
    // the nested run — the composite reads the field out (into a carry buffer) and then
    // releases it back to the pool itself. Without this the zero-reader release below
    // would reclaim the result before the composite could read it.
    for (auto& r : forced_roots)
      for (int o = 0; o < r->numOutputs(); o++)
        pinned.insert((const void*)r->output(o).get());
    for (size_t j = 0; j < N; j++) {
      for (auto inp : order[j]->_inputs) {
        if (not inp->_connectedOutput)
          continue;
        auto key = (const void*)inp->_connectedOutput.get();
        if (sink[j] and s_deferred_flush)
          pinned.insert(key);
        else if (needed[j] and not hit[j])
          last_reader[key] = j; // ascending j -> ends at the LAST reader
      }
    }
    release_at.assign(N, {});
    for (size_t j = 0; j < N; j++)
      for (int o = 0; o < order[j]->numOutputs(); o++) {
        auto op  = order[j]->output(o);
        auto key = (const void*)op.get();
        if (pinned.count(key))
          continue;
        auto it = last_reader.find(key);
        if (it != last_reader.end())
          release_at[it->second].push_back(op);
        else if (needed[j])
          // ZERO-READER output: no dispatching consumer anywhere downstream —
          // either unconnected (the eflow loop's 37 flow3d nodes publish Out +
          // Metrics that nothing reads: 2×256MiB × 37 ≈ 18.5GB of the measured
          // 21.3GB frontier) or every reader is a hit/skip, which reads nothing.
          // Release at the producer's own index (fires right after it computes
          // or cookLoads — hit uploads leak these planes identically). Without
          // this the plane squats in the pool until bake end: "free at last
          // reader" never triggers when there is no reader. releaseToPool's
          // free-set dedup keeps plug aliasing (erox pingpong) safe.
          release_at[j].push_back(op);
      }
    S._planned = true;
    } // end MT3 plan-once (do_disk_cache/hit/needed/sink/capcurrent/release_at now in S)

    // --- the loop (MT3: resumes at S._cursor; counters live in S; see aliases above)
    // ORKID_BAKE_PROFILE=1: wall-time attribution of the serial cook loop, per
    // computed node: acquire / params / dispatch(record+submit+WAIT) /
    // store(staged readback+serialize) / disk(fwrite) / other(loop residual).
    static const bool s_bakeprof = (getenv("ORKID_BAKE_PROFILE") != nullptr);
    auto bp_now = []() -> double {
      return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    double bp_acq = 0, bp_par = 0, bp_dsp = 0, bp_sto = 0, bp_dsk = 0;
    double bp_wait = 0; // pure vkWaitForFences inside dispatch ~= real GPU execution
    double bp_max_dsp = 0, bp_max_sto = 0;
    // per-module-CLASS rollup — the cost model for the strategic cache-point set
    // (cache a class iff its recompute cost beats its blob load cost).
    struct BpClass { double dsp = 0, wait = 0, sto = 0; size_t bytes = 0; int n = 0; };
    std::map<std::string, BpClass> bp_class;
    size_t bp_bytes = 0;
    std::string bp_max_dsp_n, bp_max_sto_n;
    const double bp_loop0 = bp_now();
    int64_t nodes_this_call = 0;
    for (size_t i = S._cursor; i < N; i++) {
      // MT3 yield point: node-boundary granularity (T12 — a slice never splits a node's
      // dispatch triplet, so slice boundaries can't change results). max_nodes<0 is the
      // burst path (never yields). max_nodes==0 plans-only (yields before any node runs).
      if (max_nodes >= 0 and nodes_this_call >= max_nodes) {
        if (out_more)
          *out_more = true;
        return S._result; // partial — counters not yet finalized; caller ignores until !more
      }
      S._cursor = i; // committed only past the yield gate (a yielded node re-runs cleanly)
      auto inst = order[i];
      if (not needed[i]) {
        cook_skipped++;
        S._cursor = i + 1;
        continue; // no acquire, no disk read, no upload
      }
      nodes_this_call++;
      double bp_t0 = s_bakeprof ? bp_now() : 0.0;
      // node scope brackets a TerrainComputeInst's pooled scratch. A COMPOSITE node
      // (SubGraph/Loop inst) instead runs a NESTED runCookedGraph whose per-node
      // scopes would trip beginNodeScope's not-in-scope assert — and it manages its
      // own buffers — so composites take no host node scope.
      const bool node_scoped = (std::dynamic_pointer_cast<TerrainComputeInst>(inst) != nullptr);
      if (node_scoped)
        env->beginNodeScope();
      if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst))
        tci->bakeAcquire(ginst.get()); // outputs + scratch, pool-served (lazy mode)
      // ORKID_TERRAIN_DIMLOG: per-node dim-flow trace (the editor turns this on) —
      // every output register's dims + buffer identity, so a stale-size/stale-dim
      // register is visible in the log instead of as a mesh discontinuity.
      static const bool s_dimlog = (getenv("ORKID_TERRAIN_DIMLOG") != nullptr);
      if (s_dimlog) {
        for (int o = 0; o < inst->numOutputs(); o++) {
          if (auto op = std::dynamic_pointer_cast<hfimg_outpluginst_t>(inst->output(o))) {
            auto img = op->_value;
            if (img)
              printf("[terrain-dim] node<%s> out<%d> %dx%d ch=%d ssbo=%p %s\n",
                     inst->_abstract_module_data->_name.c_str(), o, img->_w, img->_h,
                     img->_channels, (void*)img->_ssbo, hit[i] ? "CACHE-LOAD" : "compute");
          }
        }
      }
      if (s_bakeprof) { double t = bp_now(); bp_acq += t - bp_t0; bp_t0 = t; }
      if (hit[i]) {
        // cache HIT — fetch the FULL entry now (probe read only a header prefix),
        // upload to the node's (just-acquired) SSBO, release the host copy at
        // scope end. Namespaced cache entries are not retained in RAM (RSS fix).
        auto db     = DataBlockCache::findDataBlock("dflowcache", inst->_cookHash);
        bool loaded = db and inst->cookLoad(db);
        OrkAssert(loaded); // probe passed; a failure here is a probe/load bug, not a recompute case
        cook_loaded++;
      } else {
        // family-neutral pre-phase host write (E.1b realtime-params mechanism; at the
        // bake's t=0 this equals the onActivate fill — uniform, not behavioral).
        if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
          pp->writeParams(ctx);
        if (s_bakeprof) { double t = bp_now(); bp_par += t - bp_t0; bp_t0 = t; }
        double bp_w0 = s_bakeprof ? ci->_gpuWaitAccum : 0.0;
        ci->beginDispatchPhase();
        inst->compute(ginst.get(), updata);
        ci->endDispatchPhase(); // submit + WAIT -> this node's output is now valid
        if (s_bakeprof) {
          double t = bp_now(), d = t - bp_t0;
          bp_dsp += d; bp_t0 = t;
          double w = ci->_gpuWaitAccum - bp_w0;
          bp_wait += w;
          if (d > bp_max_dsp) { bp_max_dsp = d; bp_max_dsp_n = inst->_abstract_module_data->_name; }
          auto& bc = bp_class[inst->_abstract_module_data->GetClass()->Name().c_str()];
          bc.dsp += d; bc.wait += w; bc.n++;
        }
        // T9 (MANDATORY): a SLICED cook may NOT populate the content-addressed dflowcache
        // until sliced-vs-blocking byte-identity is proven (cache-poison risk). env->_allow_store
        // is true on the burst path AND propagates to nested composite cooks (shared env);
        // the microtask holds it false per the oracle-gated policy.
        bool store_this = do_disk_cache and env->_allow_store;
        if (store_this and not sink[i])
          if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst))
            store_this = tci->cookIsCachePoint(); // strategic cache points: store only at cuts
        if (store_this) {
          if (auto store = inst->cookStore()) {
            if (s_bakeprof) {
              double t = bp_now(), d = t - bp_t0;
              bp_sto += d; bp_t0 = t;
              bp_bytes += store->length();
              if (d > bp_max_sto) { bp_max_sto = d; bp_max_sto_n = inst->_abstract_module_data->_name; }
              auto& bc = bp_class[inst->_abstract_module_data->GetClass()->Name().c_str()];
              bc.sto += d; bc.bytes += store->length();
            }
            DataBlockCache::setDataBlock("dflowcache", inst->_cookHash, store);
            if (s_bakeprof) { double t = bp_now(); bp_dsk += t - bp_t0; bp_t0 = t; }
          }
        }
        if (s_cookdbg) // name every node that actually DISPATCHES + why it wasn't a hit
          printf("[cookdbg] COMPUTED node<%s> hash<0x%zx> reason<%s>\n", //
                 inst->_abstract_module_data->_name.c_str(), size_t(inst->_cookHash),
                 missreason[i] ? missreason[i] : "?");
        cook_computes++;
      }
      // S4 CHECKPOINT PUBLISH (spec anchor: after this node's endDispatchPhase, before
      // endNodeScope): the node's "Out" plane is VALID here — a cache hit just uploaded
      // it, or the dispatch above submitted + WAITed — and nothing has been released
      // yet. Read the whole plane straight into the LiveOutput's back plane and flip:
      // frame-coherent (a consumer never sees a partial plane), and GPU-safe by
      // construction (this is the SAME post-sync readback point the incremental capture
      // flush uses — no mid-phase readback, no GPU read-while-write, so the MoltenVK-
      // class double-buffer hazard never exists on this path).
      if (env->_live_out and i < S.livepub.size() and S.livepub[i]) {
        if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst)) {
          auto img = tci->_outImg();
          int  nch = (img and img->_channels >= 1) ? img->_channels : 1;
          if (img and img->_ssbo and nch == 1 and img->_w == env->_w and img->_h == env->_h) {
            float* dst = env->_live_out->beginPublish(img->_w, img->_h);
            ctx->FXI()->readStorageBuffer(
                img->_ssbo, 0, size_t(img->_w) * size_t(img->_h) * sizeof(float), dst);
            uint64_t gen = env->_live_out->endPublish(img->_w, img->_h);
            printf("[s4-live] publish key<%s> gen<%" PRIu64 "> node<%s> coverage<%zu/%zu>\n",
                   env->_live_key.c_str(), gen,
                   inst->_abstract_module_data->_name.c_str(), i + 1, N);
          }
        }
      }
      // INCREMENTAL FLUSH readback: this sink's request was just recorded and per-op
      // sync makes the source field valid — copy it to host NOW so the source plane
      // releases below (release_at) instead of pinning until the post-loop flush.
      // Encode/stats/sidecars still happen in the flush, from the host copy.
      if (sink[i] and not s_deferred_flush) {
        auto fxi_rb = ctx->FXI();
        for (size_t ri = ncap_readback; ri < env->_captures.size(); ri++) {
          auto& req = env->_captures[ri];
          auto img  = req._img;
          if (not(img and img->_ssbo))
            continue;
          int rch    = (img->_channels < 1) ? 1 : img->_channels;
          size_t cnt = size_t(img->_w) * size_t(img->_h) * size_t(rch);
          static const bool s_dimlog2 = (getenv("ORKID_TERRAIN_DIMLOG") != nullptr);
          if (s_dimlog2)
            printf("[terrain-dim] CAPTURE channel<%s> readback %dx%d ch=%d (%zu floats) env=%dx%d ssbo=%p\n",
                   req._channels.c_str(), img->_w, img->_h, rch, cnt, env->_w, env->_h,
                   (void*)img->_ssbo);
          req._hostcopy = std::make_shared<std::vector<float>>(cnt);
          fxi_rb->readStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), req._hostcopy->data());
        }
        ncap_readback = env->_captures.size();
      }
      // scratch back to the pool — everything this node acquired EXCEPT what its
      // output plugs publish (read NOW, post-compute: erox finalized its alias).
      // (composite nodes took no scope above — their buffers are managed by the inst.)
      if (node_scoped) {
        std::unordered_set<FxShaderStorageBuffer*> keep;
        for (int o = 0; o < inst->numOutputs(); o++)
          if (auto op = std::dynamic_pointer_cast<hfimg_outpluginst_t>(inst->output(o)))
            if (op->_value and op->_value->_ssbo)
              keep.insert(op->_value->_ssbo);
        env->endNodeScope(keep);
      }
      // upstream buffers whose last dispatching reader was this node
      for (auto& op : release_at[i])
        if (auto hop = std::dynamic_pointer_cast<hfimg_outpluginst_t>(op))
          if (hop->_value)
            env->releaseToPool(hop->_value->_ssbo);
      S._cursor = i + 1; // MT3: this node fully committed (submit+WAIT done) — resume past it
    }
    // reached here == the topo loop completed (S._cursor == N) — finalize + report.
    if (out_more)
      *out_more = false;
    if (s_bakeprof and cook_computes > 0) {
      double bp_loop = bp_now() - bp_loop0;
      double bp_oth  = bp_loop - (bp_acq + bp_par + bp_dsp + bp_sto + bp_dsk);
      printf(
          "[cookprof] loop %.1fs over %d computed | acquire %.1fs | params %.1fs | dispatch %.1fs "
          "(max %.3fs @%s) | store %.1fs (max %.3fs @%s, %.1f MB read back) | disk %.1fs | other %.1fs\n",
          bp_loop, cook_computes, bp_acq, bp_par, bp_dsp, bp_max_dsp, bp_max_dsp_n.c_str(), bp_sto,
          bp_max_sto, bp_max_sto_n.c_str(), double(bp_bytes) / (1024.0 * 1024.0), bp_dsk, bp_oth);
      printf(
          "[cookprof] dispatch split: fence-wait %.1fs (~real GPU) | record+submit %.1fs (CPU-side)\n",
          bp_wait, bp_dsp - bp_wait);
      printf(
          "[cookprof] per-node avg: dispatch %.3fs (wait %.3fs), store %.3fs, disk %.3fs\n",
          bp_dsp / cook_computes, bp_wait / cook_computes, bp_sto / cook_computes, bp_dsk / cook_computes);
      // per-class cost model (sorted by compute cost): cache-point candidates are the
      // classes whose per-node recompute exceeds a blob load (~0.1s-class).
      std::vector<std::pair<std::string, BpClass>> bcl(bp_class.begin(), bp_class.end());
      std::sort(bcl.begin(), bcl.end(),
                [](auto& a, auto& b) { return (a.second.dsp + a.second.sto) > (b.second.dsp + b.second.sto); });
      for (auto& item : bcl)
        printf("[cookprof] class %-40s n<%3d> dispatch %7.2fs (wait %7.2fs) avg %.3fs | store %6.2fs %6.1fMB\n",
               item.first.c_str(), item.second.n, item.second.dsp, item.second.wait,
               item.second.dsp / std::max(1, item.second.n), item.second.sto,
               double(item.second.bytes) / (1024.0 * 1024.0));
    }
    if (do_disk_cache)
      printf(
          "[cook] cacheable bake: %d cache-loaded, %d demand-skipped, %d computed | arena peak %.1f MB (%.1f MB device), %d pool reuses\n",
          cook_loaded, cook_skipped, cook_computes, double(env->_peak_arena_bytes) / (1024.0 * 1024.0),
          double(env->_device_bytes) / (1024.0 * 1024.0), env->_pool_reuses);
    else
      printf(
          "[cook] cache DISABLED (capture cache=False): %d computed, %d demand-skipped, 0 disk I/O | arena peak %.1f MB (%.1f MB device), %d pool reuses\n",
          cook_computes, cook_skipped, double(env->_peak_arena_bytes) / (1024.0 * 1024.0),
          double(env->_device_bytes) / (1024.0 * 1024.0), env->_pool_reuses);
  S._result._cook_loaded   = cook_loaded;
  S._result._cook_skipped  = cook_skipped;
  S._result._cook_computes = cook_computes;
  return S._result;
}

// stock the terrain CookGraphDriver on a GraphInst _impl (beside its BakeEnv) — the
// generic composite runtime resolves it from there. Defined below TerrainCookDriver.
static void _stockTerrainCookDriver(dflow::graphinst_ptr_t ginst, const bakeenv_ptr_t& env);

///////////////////////////////////////////////////////////////////////////////
// MT3 — bakeHeightfield factored into { setup, cook, flush } so BOTH the burst
// driver (bakeHeightfield) and the SLICED driver (TerrainCookMicrotask) share ONE
// setup + ONE flush (byte-identity by construction; only the cook's DRIVING differs
// — one call vs `_cursor`-paced slices). The E4 skeleton extraction stays PARKED;
// this is the minimal factoring that driving-layer slicing needs.
///////////////////////////////////////////////////////////////////////////////

struct TerrainBakeSetup {
  dflow::graphdata_ptr_t _graph;
  dflow::graphinst_ptr_t _ginst;
  bakeenv_ptr_t          _env;
  ui::updatedata_ptr_t   _updata;
  std::vector<std::pair<dflow::inplugdata_ptr_t, dflow::outplugdata_ptr_t>> _marker_restore;
  int   _dim      = 0;
  float _extent_m = 0.0f;
};

// SETUP — select-as-output re-point + topo sort + instantiate + BakeEnv + stock the
// cook driver + a frozen (t=0) updata. Runs OUTSIDE any open frame (matches HEAD's
// updateTopology-before-beginFrame ordering); no dispatch here (T12: inputs frozen
// at bake start — the cook only reads this snapshot).
static TerrainBakeSetup _terrainBakeSetup(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m) {
  TerrainBakeSetup S;
  S._graph    = graph;
  S._dim      = dim;
  S._extent_m = extent_m;
  auto& marker_restore = S._marker_restore;
  if (not graph->_output_node.empty()) {
    auto marked = graph->module(graph->_output_node);
    dflow::outplugdata_ptr_t marked_out = marked ? marked->outputNamed("Out") : nullptr;
    if (not marked_out) {
      // ops self-defend: a missing/output-less marker is loud but non-fatal — the graph
      // bakes with its authored capture wiring untouched.
      printf(
          "terrain output-node<%s> %s — ignoring select-as-output marker.\n",
          graph->_output_node.c_str(),
          marked ? "has no 'Out' output" : "not found in graph");
    } else {
      for (size_t i = 0; i < graph->numModules(); i++) {
        auto cap = std::dynamic_pointer_cast<CaptureModuleData>(graph->module(i));
        if (not cap)
          continue;
        auto in = cap->inputNamed("In");
        if (not in)
          continue;
        auto channels   = _splitCaptureChannels(cap->_channel, "");
        bool is_display = false;
        for (auto& ch : channels)
          if (ch == "height" or ch == "normal") {
            is_display = true;
            break;
          }
        marker_restore.emplace_back(in, in->_connectedOutput); // authored edge, restored at return
        graph->disconnect(in);          // drop the authored edge either way
        if (is_display)                 // display capture -> the marked node's output
          graph->safeConnect(in, marked_out);
        // else: left disconnected -> CaptureModuleInst::compute skips it (loud, once).
      }
    }
  }
  // topo-sort. The sorter allocates a register per connected output plug from a
  // per-type pool, so the GpuComputeImage2D type MUST have a register block
  // (keyed by the out-plug's data_type_t = GpuComputeImage2DData) or the sort
  // asserts. (Distinct per-format pools would need distinct C++ types — later.)
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuComputeImage2DData>("hf_img", 64);
  // register pools for the scalar plug types a terrain graph can carry on a
  // CONNECTED edge. Today every dataflow edge is a GpuComputeImage2D and the
  // vec2/vec4/float plugs are UNIFORM inputs (which the sorter never allocates a
  // register for) — but a future module that OUTPUTS a vec2/vec4/float field
  // would have its output plug keyed here, so register the pools up front (the
  // sort asserts a non-null register for any connected output, dataflow_sorter
  // line ~204). Cheap (a small pool each) and keeps the machine complete.
  dgctx->createRegisters<float>("hf_float", 256);
  dgctx->createRegisters<fvec2>("hf_vec2", 256);
  dgctx->createRegisters<fvec4>("hf_vec4", 256);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);

  // instantiate
  auto ginst = dflow::GraphData::createGraphInst(graph);

  auto env             = std::make_shared<BakeEnv>();
  env->_ctx            = ctx;
  env->_w              = dim;
  env->_h              = dim;
  env->_extent_m       = extent_m;       // world units -> resolution-independent meter params
  env->_per_op_sync      = true; // THIS driver syncs per op (the cook loop / capture flush below)
  env->_flushes_captures = true;
  // WS4 FRONTIER: only the cacheable path runs the per-op-synced cook loop below,
  // which is what makes lazy acquire + pooled release GPU-safe. The non-cacheable
  // path records the whole graph in ONE dispatch phase, so it keeps eager
  // activate-time allocation (the onActivate shim honors this flag).
  env->_lazy_acquire = graph->_cacheable;
  ginst->_impl.setShared<BakeEnv>(env);
  // the GPU cook driver the generic composite runtime (SubGraph/Loop insts) resolves —
  // stocked ONLY by this per-op-synced bake path (a cross-family / live host stocks the
  // BakeEnv but NOT this driver, so a composite there self-defends loudly, as before).
  _stockTerrainCookDriver(ginst, env);

  ginst->updateTopology(topo);

  // frozen updata (T12: t=0 — the cook reads no wall-clock / RCFD_TIME inputs mid-bake).
  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0f;
  updata->_dt      = 0.0f;

  S._ginst  = ginst;
  S._env    = env;
  S._updata = updata;
  return S;
}

// FLUSH — readback + encode every capture (WS7 concurrent tails), free the arena,
// restore the select-as-output edges, stamp the gpuUpdate seam. Identical for burst
// + sliced (the sliced final slice calls it once). Defined after bakeHeightfield.
static std::vector<fieldstats_ptr_t> _terrainBakeFlush(
    TerrainBakeSetup& S, Context* ctx, std::vector<fieldstats_ptr_t>& current_stats);

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m) {
  // BURST driver (initial loads / headless oracle): setup -> cook-to-completion -> flush,
  // all inside ONE frame. The editor RE-BAKE path uses the sliced driver instead (MT3).
  auto S     = _terrainBakeSetup(graph, ctx, dim, extent_m);
  auto ginst = S._ginst;
  auto env   = S._env;
  auto updata = S._updata;

  ctx->beginFrame();
  auto ci = ctx->CI();
  // FieldStats recovered from capture-currency sidecars (skipped sinks) — appended to
  // the flush's stats before return so the caller's manifest contract holds either way.
  std::vector<fieldstats_ptr_t> current_stats;
  // reset the composite nested-cook accounting for this bake (the loop insts add to it).
  s_lastSubgraphComputes = 0;
  s_lastSubgraphLoads    = 0;
  if (graph->_cacheable) {
    auto _cooked  = runCookedGraph(ginst, env, updata);
    current_stats = std::move(_cooked._current_stats);
    // cache-SATISFIED count: loaded + demand-skipped (a skipped node is satisfied
    // by the cache too — nothing dispatched against it needed recomputing). Keeps
    // terrainCacheTest's invariant: warm bake recomputes nothing but the sink.
    s_lastCookHits = _cooked._cook_loaded + _cooked._cook_skipped;
  } else {
    // S4 STRUCTURAL FORK (loud degrade): checkpoints exist ONLY on the cacheable
    // per-node branch above — this path records the WHOLE graph in ONE dispatch phase,
    // so an on_checkpoint request cannot publish mid-bake. Degrade to on_complete,
    // LOUDLY (never silently drop a requested behavior).
    for (size_t i = 0; i < graph->numModules(); i++)
      if (auto cap = std::dynamic_pointer_cast<CaptureModuleData>(graph->module(i)))
        if (cap->_visual_update_mode == "on_checkpoint") {
          printf("[s4-live] DEGRADE sink<%s>: visual_update_mode=on_checkpoint requires the "
                 "cacheable per-node cook; this graph is NON-CACHEABLE — degrading to "
                 "on_complete (no progressive display this bake)\n",
                 cap->_name.c_str());
          break;
        }
    for (auto inst : ginst->_ordered_module_insts) // family-neutral pre-phase (see above)
      if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
        pp->writeParams(ctx);
    ci->beginDispatchPhase();
    ginst->compute(updata);
    ci->endDispatchPhase();
  }
  ctx->endFrame();
  return _terrainBakeFlush(S, ctx, current_stats);
}

static std::vector<fieldstats_ptr_t> _terrainBakeFlush(
    TerrainBakeSetup& S, Context* ctx, std::vector<fieldstats_ptr_t>& current_stats) {
  auto  env            = S._env;
  auto  graph          = S._graph;
  const int   dim      = S._dim;
  const float extent_m = S._extent_m;
  auto& marker_restore = S._marker_restore;

  // flush captures: readback each source SSBO and encode by file extension. BOTH
  // paths are SINGLE-CHANNEL and NORMALIZED to the field's [min,max] (auto-exposed
  // for max contrast/precision; great for masks/curv that live in a sub-range):
  //   .exr -> R32F float (lossless, no quantization — preferred source for normals).
  //   .png -> 16-bit grayscale (R16UI; PNG can't hold float, R16UI forces full depth).
  // The absolute scale is recoverable from the printed/returned FieldStats (min/max),
  // since both encodings are field-relative.
  std::vector<fieldstats_ptr_t> stats;
  auto fxi = ctx->FXI();
  // WS7 (LOADX): SSBO readbacks stay SERIAL on the bound context, but each capture's
  // CPU tail — stats, normalize, Scharr normal synthesis, EXR/PNG encode, file write,
  // sidecar — fans out to the concurrent queue and joins before return. The encodes
  // were the dominant cold-bake cost after the cook itself (67s of serial 4096² EXR
  // work in the linux scn_forest profile); they are per-file independent. Per-request
  // stats land in their own slot so the returned FLUSH order is unchanged.
  const size_t nreq = env->_captures.size();
  std::vector<std::vector<fieldstats_ptr_t>> req_stats(nreq);
  ork::semaphore flush_sema("terra_capflush");
  int njobs = 0;

  // S4: bake FINAL — publish the height sink's captured plane to the live artifact (if
  // one was armed) so live consumers converge EXACTLY to the on-disk product, then mark
  // FINAL so held-back consumers (physics hold-last-final) rebind exactly once, NOW.
  // markFinal runs even when nothing published this bake (capture-current / all-skipped)
  // — the artifact must never be left "live" past its bake. Runs BEFORE the flush loop
  // below consumes the _hostcopy planes.
  if (env->_live_out) {
    for (auto& req : env->_captures) {
      auto img = req._img;
      if (not(img and req._hostcopy))
        continue;
      if (img->_channels > 1)
        continue; // the live artifact is the mono height plane
      bool is_live_src = false;
      for (auto& ch : _splitCaptureChannels(req._channels, "height"))
        if (ch == "height" and
            liveFieldCanonicalKey(_capturePathForChannel(req._path, "height")) == env->_live_key)
          is_live_src = true;
      if (not is_live_src)
        continue;
      env->_live_out->publish(img->_w, img->_h, req._hostcopy->data());
      printf("[s4-live] publish key<%s> gen<%" PRIu64 "> node<flush> coverage<final>\n",
             env->_live_key.c_str(), env->_live_out->generation());
      break;
    }
    env->_live_out->markFinal();
    printf("[s4-live] FINAL key<%s> gen<%" PRIu64 ">\n",
           env->_live_key.c_str(), env->_live_out->generation());
  }

  // split a comma-joined channel list ("height,normal") into trimmed tokens.
  auto split_channels = [](const std::string& cs, const char* fallback) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= cs.size()) {
      size_t comma   = cs.find(',', start);
      size_t len     = (comma == std::string::npos) ? std::string::npos : (comma - start);
      std::string tk = cs.substr(start, len);
      while (not tk.empty() and tk.front() == ' ') tk.erase(tk.begin());
      while (not tk.empty() and tk.back() == ' ') tk.pop_back();
      if (not tk.empty()) out.push_back(tk);
      if (comma == std::string::npos) break;
      start = comma + 1;
    }
    if (out.empty()) out.push_back(fallback);
    return out;
  };

  for (size_t ri = 0; ri < nreq; ri++) {
    auto& req = env->_captures[ri];
    auto img  = req._img; // GpuComputeImage2DInst (the producer's output value)
    int w     = img->_w;
    int h     = img->_h;
    size_t n  = size_t(w) * size_t(h);
    std::string path_tpl = std::string(req._path.c_str());
    uint64_t cookkey     = req._cookkey;
    auto slot            = &req_stats[ri];

    // MULTI-CHANNEL capture (e.g. flow3d's RGBA flow field): the producer wrote `_channels`
    // interleaved floats/cell, already display-ready. Pass through to an RGBA32F EXR verbatim
    // (no [min,max] normalize, no synthesized normal). EXR only (float); PNG would quantize.
    if (img->_channels >= 2) {
      int nch = img->_channels;
      auto rgba = std::make_shared<std::vector<float>>(n * 4, 0.0f);
      if (req._hostcopy) { // incremental flush: field already read back at sink-run time
        const float* mc = req._hostcopy->data();
        for (size_t i = 0; i < n; i++)
          for (int c = 0; c < 4; c++)
            (*rgba)[i * 4 + c] = (c < nch) ? mc[i * size_t(nch) + size_t(c)] : (c == 3 ? 1.0f : 0.0f);
        req._hostcopy.reset();
      } else { // deferred mode: the plane stayed pinned; map the live SSBO
        auto mcmap = fxi->mapStorageBuffer(img->_ssbo, 0, n * size_t(nch) * sizeof(float), BufferMapAccess::READ_ONLY);
        const float* mc = (const float*)mcmap->_mappedaddr;
        for (size_t i = 0; i < n; i++)
          for (int c = 0; c < 4; c++)
            (*rgba)[i * 4 + c] = (c < nch) ? mc[i * size_t(nch) + size_t(c)] : (c == 3 ? 1.0f : 0.0f);
        fxi->unmapStorageBuffer(mcmap.get());
      }
      // one job per capture: emit the SAME RGBA under each name (the list form of
      // capture() — one readback, one image per name).
      auto mchans = split_channels(req._channels, "field");
      opq::concurrentQueue()->enqueue([=, &flush_sema]() {
        for (auto& ch : mchans) {
          std::string path = ork::file::Path::expandPathString(path_tpl);
          { const std::string mark = "{channel}"; size_t pos = path.find(mark);
            if (pos != std::string::npos) path.replace(pos, mark.size(), ch); }
          Image oimg; oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
          memcpy((void*)oimg._data->data(), rgba->data(), rgba->size() * sizeof(float));
          oimg.writeToFile(ork::file::Path(path.c_str()), /*linear=*/false);
          printf("[terrain bake] wrote <%s> (%dx%d, RGBA32F, %dch passthrough)\n", path.c_str(), w, h, nch);
          auto fs = std::make_shared<FieldStats>(); fs->_min = 0.0f; fs->_max = 1.0f; fs->_mean = 0.0f;
          fs->_channel = ch;
          if (cookkey) // capture-currency: next unchanged bake skips this file
            _writeCaptureSidecar(path, cookkey, *fs, env->_extent_m, w);
          slot->push_back(fs);
        }
        flush_sema.notify();
      }, "terra_capflush_mc");
      njobs++;
      continue;
    }

    // SERIAL part: the field's host copy — already read back at sink-run time
    // (incremental flush), or mapped off the live pinned SSBO (deferred mode) —
    // then the whole CPU tail goes to a worker.
    std::shared_ptr<std::vector<float>> raw = req._hostcopy;
    req._hostcopy.reset();
    if (not raw) {
      raw = std::make_shared<std::vector<float>>(n);
      auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
      memcpy(raw->data(), mapping->_mappedaddr, n * sizeof(float));
      fxi->unmapStorageBuffer(mapping.get());
    }
    auto channels = split_channels(req._channels, "height");
    // world scale for the normal gradient (assumes a square field, W==H==dim). Heights
    // are TRUE METERS on the plug — the gradient needs only the horizontal texel size.
    float texel_m  = (w > 0) ? (env->_extent_m / float(w)) : 1.0f;

    opq::concurrentQueue()->enqueue([=, &flush_sema]() {
    const float* src = raw->data();

    // pass 1: field stats.
    float vmin = 1e30f, vmax = -1e30f;
    double vsum = 0.0;
    for (size_t i = 0; i < n; i++) {
      float v = src[i];
      vmin = (v < vmin) ? v : vmin;
      vmax = (v > vmax) ? v : vmax;
      vsum += v;
    }
    float vmean = float(vsum / double(n));

    // NATURAL UNITS: the field's VALUES are the product — EXR emits them verbatim
    // (heights are true meters; no auto-exposure, no clamp). Only the quantized PNG16
    // preview normalizes ([min,max] -> 0..65535 — the range rides the sidecar stats so
    // a consumer can re-expand). Exaggeration is AUTHORED (remap nodes), never implied.
    if(0)printf("[terrain bake] field stats: min<%g> max<%g> mean<%g>  (raw units)\n",
           vmin, vmax, vmean);

    // emit one image per requested channel — the source field was read back ONCE above.
    for (auto& ch : channels) {
      // resolve the per-channel path: substitute "{channel}" in the template if present
      // (multi-channel asset bakes), else use the path verbatim (single-channel / test
      // callers that pass a literal path).
      std::string path = ork::file::Path::expandPathString(path_tpl);
      {
        const std::string mark = "{channel}";
        size_t pos = path.find(mark);
        if (pos != std::string::npos)
          path.replace(pos, mark.size(), ch);
      }
      bool as_png = path.size() >= 4 && (path.compare(path.size() - 4, 4, ".png") == 0);

      Image oimg;
      if (ch == "normal") {
        // SYNTHESIZE the highest-quality normal: a SCHARR gradient (8-neighbor,
        // rotationally-symmetric) of the surface height — TRUE METERS, straight off the
        // field. n = normalize(-dH/dx, +1, -dH/dz), +Y up. EXR -> signed xyz float
        // (RGBA32F, a=1); PNG -> n*0.5+0.5 (RGBA8). Height and normal are consistent by
        // construction (same meters, no display scale involved).
        std::vector<float>   nf; // .exr
        std::vector<uint8_t> nb; // .png
        if (as_png) nb.resize(n * 4); else nf.resize(n * 4);
        auto Hm = [&](int x, int y) -> float {
          x = (x < 0) ? 0 : (x >= w ? w - 1 : x);   // CLAMP_TO_EDGE
          y = (y < 0) ? 0 : (y >= h ? h - 1 : y);
          return src[size_t(y) * size_t(w) + size_t(x)];
        };
        auto enc8 = [](float v) -> uint8_t {        // signed [-1,1] -> unorm byte
          float u = (v * 0.5f + 0.5f) * 255.0f + 0.5f;
          return uint8_t(u < 0.0f ? 0.0f : (u > 255.0f ? 255.0f : u));
        };
        float ginv = 1.0f / (32.0f * texel_m);      // Scharr norm (radius 1; span 2*texel_m)
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            float tl = Hm(x - 1, y - 1), tt = Hm(x, y - 1), tr = Hm(x + 1, y - 1);
            float ll = Hm(x - 1, y),                        rr = Hm(x + 1, y);
            float bl = Hm(x - 1, y + 1), bb = Hm(x, y + 1), br = Hm(x + 1, y + 1);
            float gx = (-3.0f * tl + 3.0f * tr - 10.0f * ll + 10.0f * rr - 3.0f * bl + 3.0f * br) * ginv;
            float gz = (-3.0f * tl - 10.0f * tt - 3.0f * tr + 3.0f * bl + 10.0f * bb + 3.0f * br) * ginv;
            float nx = -gx, ny = 1.0f, nz = -gz;
            float il = 1.0f / sqrtf(nx * nx + ny * ny + nz * nz);
            nx *= il; ny *= il; nz *= il;
            size_t i = size_t(y) * size_t(w) + size_t(x);
            if (as_png) {
              nb[i * 4 + 0] = enc8(nx); nb[i * 4 + 1] = enc8(ny);
              nb[i * 4 + 2] = enc8(nz); nb[i * 4 + 3] = 255;
            } else {
              nf[i * 4 + 0] = nx; nf[i * 4 + 1] = ny; nf[i * 4 + 2] = nz; nf[i * 4 + 3] = 1.0f;
            }
          }
        }
        if (as_png) {
          oimg.initWithFormat(w, h, EBufferFormat::RGBA8);
          memcpy((void*)oimg._data->data(), nb.data(), nb.size());
        } else {
          oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
          memcpy((void*)oimg._data->data(), nf.data(), nf.size() * sizeof(float));
        }
      } else {
        // scalar field (height / mask): EXR -> the RAW values, R32F (true units, no
        // quantization); PNG -> R16UI preview, normalized [min,max] -> 0..65535 (the
        // only place auto-exposure survives; range recoverable via the sidecar stats).
        if (as_png) {
          float range = vmax - vmin;
          float inv   = (range > 1e-12f) ? (1.0f / range) : 0.0f;
          std::vector<uint16_t> g16(n);
          for (size_t i = 0; i < n; i++) {
            float t = (src[i] - vmin) * inv;
            t       = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            g16[i]  = uint16_t(t * 65535.0f + 0.5f);
          }
          oimg.initWithFormat(w, h, EBufferFormat::R16UI);
          memcpy((void*)oimg._data->data(), g16.data(), n * sizeof(uint16_t));
        } else {
          oimg.initWithFormat(w, h, EBufferFormat::R32F);
          memcpy((void*)oimg._data->data(), src, n * sizeof(float));
        }
      }
      // heightfields/masks/normals are LINEAR data — tag PNG linear (engine PNG default
      // is sRGB). EXR is always linear float.
      oimg.writeToFile(ork::file::Path(path.c_str()), /*linear_colorspace=*/ as_png);
      if(0)printf("[terrain bake] wrote <%s> (%dx%d, %s/%s)\n", path.c_str(), w, h,
             ch.c_str(), as_png ? "png" : "exr");

      // one FieldStats per emitted channel (all derive from the same field), so the
      // returned vector lines up index-for-index with the caller's flat channel list.
      auto fs      = std::make_shared<FieldStats>();
      fs->_min     = vmin;
      fs->_max     = vmax;
      fs->_mean    = vmean;
      fs->_channel = ch;
      if (cookkey) // capture-currency: next unchanged bake skips this file
        _writeCaptureSidecar(path, cookkey, *fs, env->_extent_m, w);
      slot->push_back(fs);
    }
    flush_sema.notify();
    }, "terra_capflush");
    njobs++;
  }

  // JOIN: every capture job signals once. Encodes/writes/sidecars are complete —
  // and the on-disk products consistent — before bakeHeightfield returns.
  for (int j = 0; j < njobs; j++)
    flush_sema.wait();
  // append per-request stats in FLUSH (request) order — same contract as the old
  // serial loop (consumers key by _channel, tests index within a single capture).
  for (auto& rs : req_stats)
    for (auto& fs : rs)
      stats.push_back(fs);

  // stats recovered from capture-currency sidecars (sinks that never ran this bake) —
  // consumers key by _channel name, so append order is irrelevant.
  for (auto& fs : current_stats)
    stats.push_back(fs);

  // the bake's outputs are fully consumed (cook cache stored, captures encoded to
  // files) and ginst dies at return — free the whole graph's GPU buffers. GPU-idle
  // holds here: the cacheable path syncs per op, and every storage-buffer map above
  // is itself a pending-dispatch hazard point that waits.
  env->freeAllocs();
  // restore the authored capture edges the select-as-output re-point displaced
  // (bake-scoped mutation: the editor rebakes this SAME GraphData across param edits).
  for (auto& item : marker_restore) {
    graph->disconnect(item.first);
    if (item.second)
      graph->safeConnect(item.first, item.second);
  }
  // gpuUpdate seam: remember which family baked this graph + its params, so a later
  // family-neutral gpuUpdate() re-dispatches this same bake (WARM cache -> byte-identical
  // capture files).
  auto stamp      = std::make_shared<GpuUpdateStamp>();
  stamp->_family  = GraphFamily::TERRAIN;
  stamp->_dim     = dim;
  stamp->_extent_m = extent_m;
  graph->_impl.setShared<GpuUpdateStamp>(stamp);
  return stats;
}

///////////////////////////////////////////////////////////////////////////////
// MT3 — TerrainCookMicrotask: the SLICED re-bake client (JUL13_DFLOW §2.6/§E5).
//
// ONE node's dispatch triplet per slice (slice grain = the topo node; sub-dispatch
// slicing is deliberately unsupported, gpumicrotask.h:174-178). SOFT_DEADLINE.
// Setup runs at CONSTRUCTION (bake-start input freeze, T12); the cook advances
// across scheduler slices; a final slice flushes + tears down + fires on_complete.
// T7: every slice fully submits+WAITs its node (endDispatchPhase) — a slice never
// leaves a dispatch phase open across frames. T10: the BakeEnv + its pooled SSBOs
// stay resident across the paused slices (100s of MB for a big frontier — accepted).
///////////////////////////////////////////////////////////////////////////////

// T9 (MANDATORY, cache-poison risk): a SLICED cook may NOT populate the content-
// addressed dflowcache until sliced-vs-blocking byte-identity is PROVEN. That gate is
// now GREEN — test_terrain_mt3_oracle.py shows the slicer produces byte-identical
// products to the burst path on every graph where byte-identity is even achievable
// (the deterministic corpus: fbm/terrace/lpf/remap + a composite T.loop + basin_fill's
// CPU-readback). The erosion ops (flow3d/flow_erode) are submit-order-sensitive GPU
// atomics — NOT byte-reproducible even burst-vs-burst — so a sliced-stored flow blob is
// a VALID cook sample, no more "poison" than the first-write-wins blob blocking already
// stores (and it gives editor<->export consistency). cacheStore is therefore ENABLED.
// ORKID_MT3_NOSTORE=1 reverts to no-store (diagnostic); MT4 folds this into the default.
static bool _mt3SlicedAllowStore() {
  static bool v = (std::getenv("ORKID_MT3_NOSTORE") == nullptr);
  return v;
}

namespace {

struct TerrainCookMicrotask final : public GpuMicrotask, public SlicedBakeHandle {
  enum Phase { PLAN, COOK, FLUSH, DONE };

  TerrainCookMicrotask(
      dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m,
      std::function<void()> on_complete)
      : _ctx(ctx)
      , _on_complete(std::move(on_complete)) {
    // freeze the bake inputs NOW (topo sort + instantiate + BakeEnv), before any slice.
    _setup                   = _terrainBakeSetup(graph, ctx, dim, extent_m);
    _setup._env->_allow_store = _mt3SlicedAllowStore(); // T9 gate (reaches nested cooks via env)
    _name               = "TerrainCook";
    _class              = MicrotaskClass::SOFT_DEADLINE;
    _deadlineFrame      = -1; // always-due
    s_lastSubgraphComputes = 0;
    s_lastSubgraphLoads    = 0;
  }

  int64_t sliceEstimateUs() const override {
    // T1: honest per-node estimate = the upcoming node CLASS's measured-cost EMA
    // (terrain per-node cost spans 3+ orders of magnitude; a single EMA is useless).
    // The scheduler's own auto-halving + the >4x-loud-log ride on top of this.
    if (_phase != COOK)
      return 4000;
    auto& order = _setup._ginst->_ordered_module_insts;
    if (_slice._cursor >= order.size())
      return 4000;
    std::string cn = order[_slice._cursor]->_abstract_module_data->GetClass()->Name().c_str();
    auto it = _classEmaUs.find(cn);
    return int64_t(it != _classEmaUs.end() ? it->second : 6000.0f);
  }

  float progress() const override { // satisfies BOTH GpuMicrotask + SlicedBakeHandle
    if (_phase == DONE)
      return 1.0f;
    if (nullptr == _setup._ginst)
      return 0.0f;
    size_t N = _setup._ginst->_ordered_module_insts.size();
    return (N > 0) ? float(_slice._cursor) / float(N) : 0.0f;
  }

  bool runSlice(MicrotaskContext& mctx) override {
    _framesDriven++; // gate-6: total slices driven (one node's dispatch triplet each)
    Context* ctx = mctx._gfxctx;
    if (_phase == PLAN) {
      bool more = false; // plan-only (max_nodes==0): the demand plan + hashes, no node runs
      runCookedGraph(_setup._ginst, _setup._env, _setup._updata, 0, false, {}, true, &_slice, 0, &more);
      _phase = COOK;
      return true;
    }
    if (_phase == COOK) {
      auto& order = _setup._ginst->_ordered_module_insts;
      std::string cn;
      if (_slice._cursor < order.size())
        cn = order[_slice._cursor]->_abstract_module_data->GetClass()->Name().c_str();
      Timer t;
      t.Start();
      bool more = false; // advance exactly ONE topo node (its full dispatch triplet)
      auto r = runCookedGraph(_setup._ginst, _setup._env, _setup._updata, 0, false, {}, true, &_slice, 1, &more);
      float us = float(t.SecsSinceStart() * 1.0e6);
      if (not cn.empty()) {
        float& ema = _classEmaUs[cn];
        ema = (ema <= 0.0f) ? us : (0.7f * ema + 0.3f * us);
      }
      if (not more) {
        _cookResult = r;
        _phase      = FLUSH;
      }
      return true;
    }
    if (_phase == FLUSH) {
      // final slice: readback+encode (WS7 concurrent tails, joined here), free arena,
      // restore markers, stamp. Bounded CPU hitch — the heavy per-channel EXR encode
      // fans to the concurrent queue; the join is the residual (a later slice could
      // pipeline it, but the dominant cook cost is already spread across the slices).
      std::vector<fieldstats_ptr_t> cs = _cookResult._current_stats;
      _stats = _terrainBakeFlush(_setup, ctx, cs);
      _phase = DONE;
      _done.store(true);
      if (_on_complete)
        _on_complete();
      return false; // complete — scheduler drops us; the caller's handle ref keeps us alive
    }
    return false;
  }

  // ---- SlicedBakeHandle ----
  bool     done() const override { return _done.load(); }
  uint64_t framesDriven() const override { return _framesDriven; }

  void pumpToCompletion(Context* ctx) override {
    // Headless gate driver: ONE node per frame (max slice-boundary stress — the
    // strongest byte-identity exercise). A live editor drives runSlice via the
    // context scheduler instead (budget-paced, many nodes/frame under the frame law).
    // ORKID_MT3_PUMP_ONEFRAME=1 (diagnostic): drive every slice inside ONE frame — the
    // A/B lever isolating cross-frame GPU-state effects from the slicing logic itself.
    static const bool one_frame = (std::getenv("ORKID_MT3_PUMP_ONEFRAME") != nullptr);
    uint64_t frame = 0;
    if (one_frame) {
      ctx->beginFrame(false);
      while (not _done.load()) {
        MicrotaskContext mctx{ctx, 1'000'000, frame++};
        runSlice(mctx);
      }
      ctx->endFrame();
      return;
    }
    while (not _done.load()) {
      ctx->beginFrame(false);
      MicrotaskContext mctx{ctx, 1'000'000, frame++};
      runSlice(mctx);
      ctx->endFrame();
    }
  }

  Context*                             _ctx = nullptr;
  TerrainBakeSetup                     _setup;
  CookSliceState                       _slice;
  CookedGraphResult                    _cookResult;
  std::vector<fieldstats_ptr_t>        _stats;
  std::function<void()>                _on_complete;
  std::atomic<bool>                    _done{false};
  uint64_t                             _framesDriven = 0;
  Phase                                _phase = PLAN;
  mutable std::map<std::string, float> _classEmaUs;
};

} // anonymous namespace

slicedbake_handle_ptr_t enqueueSlicedBake(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m,
    std::function<void()> on_complete, bool enqueue) {
  if (enqueue and std::getenv("ORKID_MT3_DISABLE")) {
    // MT4-removal fallback: run the burst bake inline + hand back an already-done handle
    // (the caller then proceeds exactly as the pre-MT3 blocking path).
    bakeHeightfield(graph, ctx, dim, extent_m);
    struct DoneHandle final : public SlicedBakeHandle {
      bool     done() const override { return true; }
      float    progress() const override { return 1.0f; }
      uint64_t framesDriven() const override { return 1; } // burst == one call
      void     pumpToCompletion(Context*) override {}
    };
    return std::make_shared<DoneHandle>();
  }
  auto task = std::make_shared<TerrainCookMicrotask>(graph, ctx, dim, extent_m, std::move(on_complete));
  if (enqueue)
    // editor path: the context scheduler drives it (budget-paced) at each beginFrame.
    ctx->_microtaskScheduler.enqueue(std::static_pointer_cast<GpuMicrotask>(task));
  // else (headless oracle): NOT registered with the scheduler — pumpToCompletion drives
  // runSlice directly one node/frame, so no double-drive and real slice boundaries fire
  // even on an UNBOUNDED offscreen context (which would otherwise drain it in one frame).
  return std::static_pointer_cast<SlicedBakeHandle>(task);
}

void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim) {
  // fbm -> remap -> capture. remap doubles + clamps, so the field stats shift
  // visibly (proves the input-reading module + multi-SSBO bind + barrier).
  auto graph = std::make_shared<dflow::GraphData>();
  auto fbm   = FbmModuleData::createShared();
  auto remap = RemapModuleData::createShared();
  auto cap   = CaptureModuleData::createShared();
  cap->_path = outpath;
  remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(2.0f);
  dflow::GraphData::addModule(graph, "fbm", fbm);
  dflow::GraphData::addModule(graph, "remap", remap);
  dflow::GraphData::addModule(graph, "capture", cap);
  graph->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
  graph->safeConnect(cap->inputNamed("In"), remap->outputNamed("Out"));
  bakeHeightfield(graph, ctx, dim);
}

///////////////////////////////////////////////////////////////////////////////
// op self-test — every case is driven by Const inputs (or a closed-form
// Gradient), so the expected min/max/mean is known analytically. This exercises:
//   Const     1 SSBO  generator
//   Gradient  1 SSBO  generator w/ spatial addressing (both axes)
//   Combine   3 SSBO  out=0,a=1,b=2  (the strongest multi-SSBO-bind test)
//   Terrace   2 SSBO  quantize math
///////////////////////////////////////////////////////////////////////////////

int terrainOpsSelfTest(Context* ctx, int dim) {
  int fails    = 0;
  const float D = float(dim);

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  auto check = [&](const char* name, fieldstats_ptr_t s, float emin, float emax, float emean, float tol) {
    bool ok = aeq(s->_min, emin, tol) && aeq(s->_max, emax, tol) && aeq(s->_mean, emean, tol);
    printf(
        "[selftest] %-26s min<%.5f|%.5f> max<%.5f|%.5f> mean<%.5f|%.5f> : %s\n",
        name, s->_min, emin, s->_max, emax, s->_mean, emean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  };

  // --- builders ----------------------------------------------------------------
  auto mkConst = [&](float lvl) -> constmoduledata_ptr_t {
    auto m = ConstModuleData::createShared();
    m->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(lvl);
    return m;
  };
  auto capTo = [&](dflow::graphdata_ptr_t g, dflow::moduledata_ptr_t producer, const std::string& path) {
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(path.c_str());
    dflow::GraphData::addModule(g, "capture", cap);
    g->safeConnect(cap->inputNamed("In"), producer->outputNamed("Out"));
  };
  auto path = [](const char* nm) { return FormatString("/tmp/terrain_selftest_%s.exr", nm); };

  // --- Const -------------------------------------------------------------------
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.5f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_half"));
    check("Const(0.5)", bakeHeightfield(g, ctx, dim)[0], 0.5f, 0.5f, 0.5f, 1e-4f);
  }
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.25f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_quarter"));
    check("Const(0.25)", bakeHeightfield(g, ctx, dim)[0], 0.25f, 0.25f, 0.25f, 1e-4f);
  }

  // --- Gradient (closed form: linear ramp along an axis) -----------------------
  // value = uv.axis ; over xi=0..D-1 : min=0, max=(D-1)/D, mean=(D-1)/(2D)
  auto runGrad = [&](const char* nm, float dx, float dy) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(dx, dy));
    dflow::GraphData::addModule(g, "grad", gr);
    capTo(g, gr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Gradient(x)", runGrad("grad_x", 1.0f, 0.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);
  check("Gradient(y)", runGrad("grad_y", 0.0f, 1.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);

  // --- Combine (3 SSBO: out,a,b) ----------------------------------------------
  auto runCombine = [&](const char* nm, int op, float a, float b, float t) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = CombineModuleData::createShared();
    cm->_op = CombineOp(op);
    cm->typedInputNamed<dflow::FloatPlugTraits>("t")->setValue(t);
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    g->safeConnect(cm->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(cm->inputNamed("B"), cb->outputNamed("Out"));
    capTo(g, cm, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Combine(ADD,.3,.2)", runCombine("comb_add", int(CombineOp::ADD), 0.3f, 0.2f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(SUB,.8,.3)", runCombine("comb_sub", int(CombineOp::SUB), 0.8f, 0.3f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MUL,.5,.5)", runCombine("comb_mul", int(CombineOp::MUL), 0.5f, 0.5f, 0.0f), 0.25f, 0.25f, 0.25f, 1e-4f);
  check("Combine(MIN,.3,.7)", runCombine("comb_min", int(CombineOp::MIN), 0.3f, 0.7f, 0.0f), 0.3f, 0.3f, 0.3f, 1e-4f);
  check("Combine(MAX,.3,.7)", runCombine("comb_max", int(CombineOp::MAX), 0.3f, 0.7f, 0.0f), 0.7f, 0.7f, 0.7f, 1e-4f);
  check("Combine(MIX,.2,.8,.5)", runCombine("comb_mix5", int(CombineOp::MIX), 0.2f, 0.8f, 0.5f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MIX,.2,.8,.25)", runCombine("comb_mix25", int(CombineOp::MIX), 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Terrace (2 SSBO) : quantize a constant to the nearest plateau -----------
  // step_m is METERS per plateau (natural units): v/step_m picks the plateau index,
  // result = plateau * step_m.
  auto runTerrace = [&](const char* nm, float in, float step_m, float sharp) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ci = mkConst(in);
    auto tr = TerraceModuleData::createShared();
    tr->typedInputNamed<dflow::FloatPlugTraits>("step_m")->setValue(step_m);
    tr->typedInputNamed<dflow::FloatPlugTraits>("sharpness")->setValue(sharp);
    dflow::GraphData::addModule(g, "c", ci);
    dflow::GraphData::addModule(g, "t", tr);
    g->safeConnect(tr->inputNamed("In"), ci->outputNamed("Out"));
    capTo(g, tr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  // 0.6/0.25=2.4 -> plateau 2 -> 0.5m ; 0.7/0.25=2.8 -> riser -> 0.75m ; 0.9/0.5=1.8 -> 1.0m
  check("Terrace(.6m,step.25)", runTerrace("terr_06", 0.6f, 0.25f, 1.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Terrace(.7m,step.25)", runTerrace("terr_07", 0.7f, 0.25f, 1.0f), 0.75f, 0.75f, 0.75f, 1e-4f);
  check("Terrace(.9m,step.5)", runTerrace("terr_09", 0.9f, 0.5f, 1.0f), 1.0f, 1.0f, 1.0f, 1e-4f);

  // --- Slope (pre-blurred gradient + soft rolloff, Mask by Feature) -----------
  // flat field -> slope 0 everywhere. Heights are TRUE METERS: a ramp of E meters
  // over an extent of E meters has slope exactly 1 (box-averaging a linear field is
  // exact), so slope = soft_rolloff(1*scale) = 1/(1+1) = 0.5 uniform in the interior;
  // the border ring (margin radius+box) is 0. extent==dim -> 1 texel == 1 m
  // (radius_m == radius_texels); the gradient's `scale` plug supplies the E-meter
  // amplitude the old height_scale constant used to provide.
  const float E = float(dim);
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f; // -> 2 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, sl, path("slope_flat"));
    check("Slope(flat)", bakeHeightfield(g, ctx, dim, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    gr->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(E); // E meters over E meters -> slope 1
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f;
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), gr->outputNamed("Out"));
    capTo(g, sl, path("slope_ramp"));
    auto s  = bakeHeightfield(g, ctx, dim, E)[0];
    bool ok = (s->_min < 1e-4f) && aeq(s->_max, 0.5f, 2e-3f) && (s->_mean > 0.3f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Slope(ramp,x)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // --- MaskBlend (4 SSBO): per-texel mix(A,B,M) -------------------------------
  auto runBlend = [&](const char* nm, float a, float b, float mk) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = mkConst(mk);
    auto mb = MaskBlendModuleData::createShared();
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    dflow::GraphData::addModule(g, "mb", mb);
    g->safeConnect(mb->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("B"), cb->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("M"), cm->outputNamed("Out"));
    capTo(g, mb, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("MaskBlend(.2,.8,0)",   runBlend("mask_0",  0.2f, 0.8f, 0.0f),  0.2f, 0.2f, 0.2f, 1e-4f);
  check("MaskBlend(.2,.8,1)",   runBlend("mask_1",  0.2f, 0.8f, 1.0f),  0.8f, 0.8f, 0.8f, 1e-4f);
  check("MaskBlend(.2,.8,.25)", runBlend("mask_25", 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Curvature (band-pass diff-of-box + soft rolloff, Mask by Feature) -------
  // a flat field has zero curvature (diff-of-box of a constant is 0, soft rolloff
  // of 0 is 0). A squared ramp uv.x^2 is concave everywhere (positive curvature),
  // so CONVEX (ridge) -> 0 everywhere and CONCAVE responds (>0, soft-rolled into (0,1)).
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = CurvatureMode::MAGNITUDE;
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(cv->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, cv, path("curv_flat"));
    check("Curvature(flat)", bakeHeightfield(g, ctx, dim, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  auto runCurv = [&](const char* nm, int mode, float scale) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared(); // value = uv.x
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sq = CombineModuleData::createShared(); // uv.x * uv.x = uv.x^2
    sq->_op = CombineOp::MUL;
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = CurvatureMode(mode);
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    cv->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(scale);
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "sq", sq);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(sq->inputNamed("A"), gr->outputNamed("Out"));
    g->safeConnect(sq->inputNamed("B"), gr->outputNamed("Out"));
    g->safeConnect(cv->inputNamed("In"), sq->outputNamed("Out"));
    capTo(g, cv, path(nm));
    return bakeHeightfield(g, ctx, dim, E)[0];
  };
  // convex of a concave (valley-shaped) field -> 0 everywhere (no ridges); border zeroed.
  check("Curvature(x^2,convex)", runCurv("curv_convex", int(CurvatureMode::CONVEX), 4.0f), 0.0f, 0.0f, 0.0f, 1e-4f);
  // concave responds: min 0 (border ring), max in (0,1) (soft rolloff never saturates),
  // interior ~uniform for a quadratic. Robust property check (not an exact pin).
  {
    auto s  = runCurv("curv_concave", int(CurvatureMode::CONCAVE), 4.0f);
    bool ok = (s->_min < 1e-4f) && (s->_max > 0.05f) && (s->_max < 0.999f) && (s->_mean > 0.04f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Curvature(x^2,concave)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  printf("[selftest] %d case(s) FAILED\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// round-trip gate — the serialized GraphData is the portable, python-decoupled
// artifact (the future dflow UI editor loads exactly this JSON). This proves a
// terrain graph survives serialize -> deserialize -> bake with NO python and NO
// loss: build fbm(octaves=7) * const(0.5) via Combine(MUL), bake (stats A); JSON
// round-trip to a fresh clone; re-supply the capture path (wrapper-owned, not in
// the graph); assert the clone's _octaves/_op survived AND it bakes identical
// stats (so the baked scalars, float plug values, and connections all round-trip).
///////////////////////////////////////////////////////////////////////////////

int terrainRoundTripTest(Context* ctx, int dim) {
  int fails = 0;
  auto aeq  = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g    = std::make_shared<dflow::GraphData>();
    auto fbm  = FbmModuleData::createShared();
    fbm->_octaves = 7; // non-default baked scalar (default is 5)
    auto cst  = ConstModuleData::createShared();
    cst->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(0.5f);
    auto comb = CombineModuleData::createShared();
    comb->_op = CombineOp::MUL; // non-default baked scalar (default is ADD)
    // Gradient with a NON-default vec2 "dir" — exercises the vec2 plug VALUE
    // surviving the JSON round-trip (the inplugdata<Vec2fPlugTraits> reflection).
    auto grad = GradientModuleData::createShared();
    grad->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(0.6f, 0.8f)); // default is (1,0)
    grad->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    auto comb2 = CombineModuleData::createShared();
    comb2->_op = CombineOp::MUL;
    auto cap  = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "cst", cst);
    dflow::GraphData::addModule(g, "comb", comb);
    dflow::GraphData::addModule(g, "grad", grad);
    dflow::GraphData::addModule(g, "comb2", comb2);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(comb->inputNamed("A"), fbm->outputNamed("Out"));
    g->safeConnect(comb->inputNamed("B"), cst->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("A"), comb->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("B"), grad->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), comb2->outputNamed("Out"));
    return g;
  };

  // (1) original bake
  auto g0 = build("/tmp/terrain_rt_orig.exr");
  auto s0 = bakeHeightfield(g0, ctx, dim);

  // (2) serialize -> JSON -> deserialize a fresh clone
  ork::reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(g0);
  std::string json = ser.output();
  ork::object_ptr_t out;
  ork::reflect::serdes::JsonDeserializer deser(json.c_str());
  deser.deserializeTop(out);
  auto g1 = std::dynamic_pointer_cast<dflow::GraphData>(out);
  if (not g1) {
    printf("[roundtrip] deserialize -> GraphData FAILED\n");
    return 1;
  }

  // (3) direct assertions: the baked scalars survived the round-trip
  auto fbm1  = std::dynamic_pointer_cast<FbmModuleData>(g1->module("fbm"));
  auto comb1 = std::dynamic_pointer_cast<CombineModuleData>(g1->module("comb"));
  if (not fbm1 or fbm1->_octaves != 7) {
    printf("[roundtrip] _octaves LOST (got %d, want 7)\n", fbm1 ? fbm1->_octaves : -1);
    fails++;
  }
  if (not comb1 or comb1->_op != CombineOp::MUL) {
    printf("[roundtrip] _op LOST (got %d, want %d=MUL)\n", comb1 ? int(comb1->_op) : -1, int(CombineOp::MUL));
    fails++;
  }
  // the vec2 "dir" plug VALUE must survive JSON (inplugdata<Vec2fPlugTraits> reflection)
  auto grad1 = std::dynamic_pointer_cast<GradientModuleData>(g1->module("grad"));
  if (not grad1) {
    printf("[roundtrip] clone missing 'grad' module\n");
    fails++;
  } else {
    auto dir = grad1->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->value();
    bool ok  = aeq(dir.x, 0.6f, 1e-6f) and aeq(dir.y, 0.8f, 1e-6f);
    printf("[roundtrip] gradient vec2 'dir' got(%.3f, %.3f) want(0.600, 0.800) : %s\n", dir.x, dir.y, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // (4) re-supply the capture path (wrapper-owned, deliberately NOT serialized),
  // then bake the clone and require byte-identical field stats.
  auto cap1 = std::dynamic_pointer_cast<CaptureModuleData>(g1->module("cap"));
  if (not cap1) {
    printf("[roundtrip] clone missing 'cap' module\n");
    return fails + 1;
  }
  cap1->_path = ork::file::Path("/tmp/terrain_rt_clone.exr");
  auto s1 = bakeHeightfield(g1, ctx, dim);

  if (s0.size() == 1 and s1.size() == 1) {
    bool ok = aeq(s0[0]->_min, s1[0]->_min, 1e-6f)   //
              and aeq(s0[0]->_max, s1[0]->_max, 1e-6f) //
              and aeq(s0[0]->_mean, s1[0]->_mean, 1e-6f);
    printf(
        "[roundtrip] bake-equivalence orig(min %.6f max %.6f mean %.6f) vs clone(min %.6f max %.6f mean %.6f) : %s\n",
        s0[0]->_min, s0[0]->_max, s0[0]->_mean, s1[0]->_min, s1[0]->_max, s1[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[roundtrip] capture-count mismatch s0=%zu s1=%zu\n", s0.size(), s1.size());
    fails++;
  }

  ///////////////////////////////////////////////////////////////////////////
  // BYPASS + OUTPUT-NODE round-trip (first-class C++ dataflow state). A bypassed
  // mid-chain module is a transparent pass-through: the bake must equal the graph
  // WITHOUT that module; a select-as-output marker re-points the height/normal
  // captures to the marked node and drops the rest. Both flags must survive the
  // JSON round-trip AND be honored by the clone's bake.
  ///////////////////////////////////////////////////////////////////////////
  auto meanOf = [](const std::vector<fieldstats_ptr_t>& s) -> float {
    return (s.size() >= 1) ? s[0]->_mean : 1e30f;
  };
  auto statsEq = [&](const std::vector<fieldstats_ptr_t>& a,
                     const std::vector<fieldstats_ptr_t>& b) -> bool {
    return a.size() == 1 and b.size() == 1              //
           and aeq(a[0]->_min, b[0]->_min, 1e-6f)       //
           and aeq(a[0]->_max, b[0]->_max, 1e-6f)       //
           and aeq(a[0]->_mean, b[0]->_mean, 1e-6f);
  };
  auto mkRemap = [&](float scale, float bias) -> remapmoduledata_ptr_t {
    auto r = RemapModuleData::createShared();
    r->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(scale);
    r->typedInputNamed<dflow::FloatPlugTraits>("bias")->setValue(bias);
    return r;
  };

  // ---- bypass: fbm -> remap(scale,bias) -> cap ; remap optionally bypassed -----
  auto buildChain = [&](const char* cappath, bool bypass_remap) -> dflow::graphdata_ptr_t {
    auto g        = std::make_shared<dflow::GraphData>();
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remap       = mkRemap(0.5f, 0.25f); // non-identity -> bypass changes the result
    remap->_bypassed = bypass_remap;
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remap", remap);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), remap->outputNamed("Out"));
    return g;
  };
  auto buildDirect = [&](const char* cappath) -> dflow::graphdata_ptr_t { // "without remap"
    auto g        = std::make_shared<dflow::GraphData>();
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(cap->inputNamed("In"), fbm->outputNamed("Out"));
    return g;
  };

  auto s_active  = bakeHeightfield(buildChain("/tmp/terrain_bypass_active.exr", false), ctx, dim);
  auto s_without = bakeHeightfield(buildDirect("/tmp/terrain_bypass_direct.exr"), ctx, dim);

  auto g_by = buildChain("/tmp/terrain_bypass_orig.exr", true);
  ork::reflect::serdes::JsonSerializer ser_by;
  ser_by.serializeRoot(g_by);
  std::string json_by = ser_by.output();
  ork::object_ptr_t out_by;
  ork::reflect::serdes::JsonDeserializer deser_by(json_by.c_str());
  deser_by.deserializeTop(out_by);
  auto g_by_clone = std::dynamic_pointer_cast<dflow::GraphData>(out_by);
  auto rm_clone   = g_by_clone ? g_by_clone->module("remap") : nullptr;
  if (not rm_clone or not rm_clone->_bypassed) {
    printf("[roundtrip] _bypassed LOST on clone (got %d, want 1)\n", rm_clone ? int(rm_clone->_bypassed) : -1);
    fails++;
  }
  auto s_bypass = bakeHeightfield(g_by, ctx, dim);
  if (auto cc = g_by_clone ? std::dynamic_pointer_cast<CaptureModuleData>(g_by_clone->module("cap")) : nullptr)
    cc->_path = ork::file::Path("/tmp/terrain_bypass_clone.exr");
  auto s_bypass_clone = g_by_clone ? bakeHeightfield(g_by_clone, ctx, dim) : std::vector<fieldstats_ptr_t>{};

  bool by_eq_without = statsEq(s_bypass, s_without);
  bool by_ne_active  = not statsEq(s_bypass, s_active);
  bool by_clone_eq   = statsEq(s_bypass, s_bypass_clone);
  printf(
      "[roundtrip] bypass==without:%d bypass!=active:%d clone-honors-bypass:%d "
      "(mean bypass %.6f without %.6f active %.6f) : %s\n",
      int(by_eq_without), int(by_ne_active), int(by_clone_eq),
      meanOf(s_bypass), meanOf(s_without), meanOf(s_active),
      (by_eq_without and by_ne_active and by_clone_eq) ? "PASS" : "FAIL");
  if (not by_eq_without) fails++;
  if (not by_ne_active) fails++;
  if (not by_clone_eq) fails++;

  // ---- output-node: fbm -> remapA -> remapB, a height + a mask capture on remapB.
  // Marking remapA as the output re-points the height capture to remapA and drops
  // the mask capture -> bake equals fbm -> remapA -> cap(height).
  auto buildOutputNode = [&](const char* disp_path, const char* mask_path) -> dflow::graphdata_ptr_t {
    auto g        = std::make_shared<dflow::GraphData>();
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remapA = mkRemap(0.5f, 0.25f);
    auto remapB = mkRemap(2.0f, 0.0f);
    auto disp      = CaptureModuleData::createShared();
    disp->_channel = "height";
    disp->_path    = ork::file::Path(disp_path);
    auto mask      = CaptureModuleData::createShared();
    mask->_channel = "mask";
    mask->_path    = ork::file::Path(mask_path);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remapA", remapA);
    dflow::GraphData::addModule(g, "remapB", remapB);
    dflow::GraphData::addModule(g, "disp", disp);
    dflow::GraphData::addModule(g, "mask", mask);
    g->safeConnect(remapA->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(remapB->inputNamed("In"), remapA->outputNamed("Out"));
    g->safeConnect(disp->inputNamed("In"), remapB->outputNamed("Out"));
    g->safeConnect(mask->inputNamed("In"), remapB->outputNamed("Out"));
    g->_output_node = "remapA";
    return g;
  };
  auto buildRef = [&](const char* path) -> dflow::graphdata_ptr_t { // == remapA(fbm)
    auto g        = std::make_shared<dflow::GraphData>();
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remapA = mkRemap(0.5f, 0.25f);
    auto cap       = CaptureModuleData::createShared();
    cap->_channel  = "height";
    cap->_path     = ork::file::Path(path);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remapA", remapA);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(remapA->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), remapA->outputNamed("Out"));
    return g;
  };

  // serialize the AUTHORED (un-rewired) output-node graph before baking (the bake
  // rewires captures in place), clone it, verify _output_node survived.
  auto g_on = buildOutputNode("/tmp/terrain_on_disp.exr", "/tmp/terrain_on_mask.exr");
  ork::reflect::serdes::JsonSerializer ser_on;
  ser_on.serializeRoot(g_on);
  std::string json_on = ser_on.output();
  ork::object_ptr_t out_on;
  ork::reflect::serdes::JsonDeserializer deser_on(json_on.c_str());
  deser_on.deserializeTop(out_on);
  auto g_on_clone = std::dynamic_pointer_cast<dflow::GraphData>(out_on);
  if (not g_on_clone or g_on_clone->_output_node != "remapA") {
    printf("[roundtrip] _output_node LOST on clone (got '%s', want 'remapA')\n",
           g_on_clone ? g_on_clone->_output_node.c_str() : "<null>");
    fails++;
  }
  auto s_ref = bakeHeightfield(buildRef("/tmp/terrain_on_ref.exr"), ctx, dim);
  auto s_on  = bakeHeightfield(g_on, ctx, dim); // rewires g_on in place
  if (g_on_clone) {
    if (auto d = std::dynamic_pointer_cast<CaptureModuleData>(g_on_clone->module("disp")))
      d->_path = ork::file::Path("/tmp/terrain_on_clone_disp.exr");
    if (auto m = std::dynamic_pointer_cast<CaptureModuleData>(g_on_clone->module("mask")))
      m->_path = ork::file::Path("/tmp/terrain_on_clone_mask.exr");
  }
  auto s_on_clone = g_on_clone ? bakeHeightfield(g_on_clone, ctx, dim) : std::vector<fieldstats_ptr_t>{};

  bool on_drops_mask = (s_on.size() == 1) and (s_on[0]->_channel == "height");
  bool on_eq_ref     = statsEq(s_on, s_ref);
  bool on_clone_eq   = statsEq(s_on, s_on_clone);
  printf(
      "[roundtrip] output-node: drops-mask:%d ==ref(remapA):%d clone-honors-marker:%d "
      "(sinks kept %zu) : %s\n",
      int(on_drops_mask), int(on_eq_ref), int(on_clone_eq), s_on.size(),
      (on_drops_mask and on_eq_ref and on_clone_eq) ? "PASS" : "FAIL");
  if (not on_drops_mask) fails++;
  if (not on_eq_ref) fails++;
  if (not on_clone_eq) fails++;

  printf("[roundtrip] %d failure(s)\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// per-node cook cache gate — bake a cacheable graph twice. The cold bake
// computes + stores every node's field (anonymously, by content hash) in the
// DataBlockCache; the warm bake (same graph -> same node hashes) loads them, so
// it must (a) produce a byte-identical field and (b) report cache hits for the
// compute nodes (only the Capture sink recomputes).
///////////////////////////////////////////////////////////////////////////////

int terrainCacheTest(Context* ctx, int dim) {
  int fails = 0;

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g        = std::make_shared<dflow::GraphData>();
    g->_cacheable = true; // opt in to the per-node cook cache
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remap    = RemapModuleData::createShared();
    remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    remap->typedInputNamed<dflow::FloatPlugTraits>("bias")->setValue(0.5f);
    auto terr = TerraceModuleData::createShared();
    terr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(6.0f);
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remap", remap);
    dflow::GraphData::addModule(g, "terr", terr);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(terr->inputNamed("In"), remap->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), terr->outputNamed("Out"));
    return g;
  };

  printf("[cachetest] COLD bake:\n");
  auto s1 = bakeHeightfield(build("/tmp/terrain_cache_cold.exr"), ctx, dim);
  int cold_hits = s_lastCookHits;

  printf("[cachetest] WARM bake (same graph -> expect cache hits):\n");
  auto s2 = bakeHeightfield(build("/tmp/terrain_cache_warm.exr"), ctx, dim);
  int warm_hits = s_lastCookHits;

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  if (s1.size() == 1 and s2.size() == 1) {
    bool ok = aeq(s1[0]->_min, s2[0]->_min, 1e-6f)   //
              and aeq(s1[0]->_max, s2[0]->_max, 1e-6f) //
              and aeq(s1[0]->_mean, s2[0]->_mean, 1e-6f);
    printf("[cachetest] field cold(mean %.6f) vs warm(mean %.6f) : %s\n", s1[0]->_mean, s2[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[cachetest] capture-count mismatch\n");
    fails++;
  }
  // the warm bake must actually have hit the cache for the 3 compute nodes
  // (fbm/remap/terr); the Capture sink always recomputes.
  printf("[cachetest] cook hits: cold=%d warm=%d\n", cold_hits, warm_hits);
  if (warm_hits < 3) {
    printf("[cachetest] FAIL: warm bake hit cache only %d times (expected >= 3)\n", warm_hits);
    fails++;
  }

  printf("[cachetest] %d failure(s)\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// SubGraphModule (E4) — terrain's CookGraphDriver: the GPU/memory-model HOOKS the
// GENERIC composite runtime (dflow::SubGraphModuleInst / LoopModuleInst, in
// subgraph_module.cpp) delegates to. The family-neutral orchestration — per-iteration
// salt, promoted-input forwarding, loop control, iter-feeds, forced-root demand — now
// lives core-side; only these hooks (the nested cook, the boundary-resource device
// blits, the phase choreography, env sharing) stay terrain-side. The driver is stocked
// on the host GraphInst's _impl (where the BakeEnv lives) by bakeHeightfield and the
// nested-inst builder, and resolved by the core insts.
//
// CARRY MODEL: each carry's boundary OUTPUT plug doubles as its persistent carry
// buffer (a GPU SSBO — like every bake plane, VRAM-resident under the residency
// budget). The core loop points the carry's promoted-INPUT inner plug at that boundary
// output (the carry value), runs the nested cook (which writes the inner promoted-
// output to a POOL buffer — a different buffer, so no read/write hazard), then this
// driver copies the inner output back into the carry buffer with a DEVICE-TO-DEVICE
// copy (ci->copyBufferRegion — stays in VRAM, no host bounce). After the last iteration
// the boundary output already holds the final carry value (published for free). Constant
// (non-carry) promoted inputs feed from the composite's external input every iteration;
// non-carry promoted outputs publish on the last iteration. Multi-IO throughout.
//
// PHASE MODEL: the core composite closes the host's dispatch phase on entry (via
// closeHostPhase; the nested runCookedGraph opens its own per-node phases), the publish/
// init hooks bracket each carry copy in their own begin/endDispatchPhase (copyBufferRegion
// asserts in-phase; endDispatchPhase submits+WAITs, so the copied plane is valid before
// the next iteration reads it), and the core reopens a phase on exit (reopenHostPhase)
// for the host cook loop's balancing endDispatchPhase.
//
// HASH / SALT (now core — see subgraph_module.cpp): salt base = H(cook-context,
// [external input hashes]) stashed by cookComputeHash; per-iteration salt = H(salt_base,
// i); the loop node's own hash includes _count. The salt STRINGS are frozen for
// byte-identity. The runNested hook folds each iteration's salt into runCookedGraph.
///////////////////////////////////////////////////////////////////////////////

// the nested-inst builder — SHARES the parent BakeEnv + stocks the cook driver; defined
// below TerrainCookDriver (which its makeNestedInst hook calls).
static dflow::graphinst_ptr_t _makeTerrainNestedInst(dflow::graphdata_ptr_t graph, const bakeenv_ptr_t& env);

// host-write zero-fill for the DEGENERATE unfed-carry path only (ops self-defend). Runs
// with no dispatch phase open (same pattern as cookLoad's writeStorageBuffer). The normal
// carry copy is device-to-device (ci->copyBufferRegion) — see the inst compute()s.
static void _zeroField(const bakeenv_ptr_t& env, FxShaderStorageBuffer* dst, size_t bytes) {
  if (not(dst and bytes))
    return;
  auto fxi = env->_ctx->FXI();
  std::vector<float> host(bytes / sizeof(float), 0.0f);
  fxi->writeStorageBuffer(dst, 0, bytes, host.data());
}

// resolve a promoted boundary OUTPUT plug to its inner pluginst inside a nested GraphInst
// (the driver's publish hooks read the nested inner output out; the INPUT side is wired
// core-side via base plug pointers, so no terrain _innerIn is needed).
static hfimg_outpluginst_ptr_t _innerOut(dflow::graphinst_ptr_t g, dflow::subgraphpromotion_ptr_t p) {
  auto it = g->_module_inst_map.find(p->_inner_module);
  if (it == g->_module_inst_map.end())
    return nullptr;
  return it->second->typedOutputNamed<HfImagePlugTraits>(p->_inner_plug);
}

///////////////////////////////////////////////////////////////////////////////
// TerrainCookDriver — the terrain CookGraphDriver. Each hook is verbatim the GPU /
// memory-model body of the old TerrainSubGraphModuleInst / TerrainLoopModuleInst; the
// family-neutral control flow around them (salt, promoted-input forwarding, loop, iter-
// feeds, forced roots) moved to the core composite insts. Stocked on the host GraphInst
// _impl beside the BakeEnv; resolved by dflow::SubGraphModuleInst / LoopModuleInst.
///////////////////////////////////////////////////////////////////////////////

struct TerrainCookDriver : public dflow::CookGraphDriver {
  TerrainCookDriver(const bakeenv_ptr_t& env)
      : _env(env) {
  }

  dflow::graphinst_ptr_t makeNestedInst(dflow::graphdata_ptr_t subgraph) override {
    return _makeTerrainNestedInst(subgraph, _env);
  }

  void closeHostPhase() override {
    _env->_ctx->CI()->endDispatchPhase(); // close the host's phase; the nested cook opens its own per node
  }
  void reopenHostPhase() override {
    _env->_ctx->CI()->beginDispatchPhase(); // reopen for the host cook loop's balancing endDispatchPhase
  }

  dflow::CompositeCookCounts runNested(
      dflow::graphinst_ptr_t nested, ui::updatedata_ptr_t updata, uint64_t iteration_salt,
      const std::vector<dflow::dgmoduleinst_ptr_t>& forced_roots, bool allow_disk_cache) override {
    auto res = runCookedGraph(nested, _env, updata, iteration_salt, true, forced_roots, allow_disk_cache);
    s_lastSubgraphComputes += res._cook_computes; // nested-cook accounting (terrainSubgraphTest)
    s_lastSubgraphLoads += res._cook_loaded;
    return dflow::CompositeCookCounts{res._cook_computes, res._cook_loaded};
  }

  void initCarries(dflow::DgModuleInst* composite, const dflow::LoopModuleData* ld) override {
    const int W     = _env->_w, H = _env->_h;
    const size_t fb = size_t(W) * size_t(H) * sizeof(float);
    auto ci         = _env->_ctx->CI();
    // each carry's boundary OUTPUT plug doubles as the persistent (GPU) carry buffer;
    // init it from the carry's promoted-INPUT external value. Fed carries: device copy
    // (batched in one phase); an unfed carry self-defends to zeros (host write, no phase).
    std::vector<std::pair<FxShaderStorageBuffer*, FxShaderStorageBuffer*>> init_copies; // (external src, carry)
    for (auto c : ld->_carries) {
      auto outp = composite->typedOutputNamed<HfImagePlugTraits>(c->_promoted_output);
      OrkAssert(outp and outp->_value); // reshapeIOs must have built this boundary plug
      auto img       = outp->_value;
      img->_w        = W;
      img->_h        = H;
      img->_channels = 1;
      img->_ssbo     = _env->createStorageBuffer(fb);
      auto inp       = composite->typedInputNamed<HfImagePlugTraits>(c->_promoted_input);
      auto src       = inp ? _srcImg(inp) : nullptr;
      if (src and src->_ssbo)
        init_copies.emplace_back(src->_ssbo, img->_ssbo);
      else // ops self-defend: an unfed carry starts at zeros (loud), never garbage
        _zeroField(_env, img->_ssbo, fb);
    }
    if (not init_copies.empty()) {
      ci->beginDispatchPhase();
      for (auto& ic : init_copies)
        ci->copyBufferRegion(ic.first, 0, ic.second, 0, fb);
      ci->endDispatchPhase(); // submit + WAIT -> carries initialized
    }
  }

  void publishSubgraphOutputs(
      dflow::DgModuleInst* composite, dflow::graphinst_ptr_t nested,
      const dflow::SubGraphModuleData* sd) override {
    const int W     = _env->_w, H = _env->_h;
    const size_t fb = size_t(W) * size_t(H) * sizeof(float);
    auto ci         = _env->_ctx->CI();
    // allocate boundary output buffers (no phase needed), then device-copy inner->boundary.
    for (auto p : sd->_promoted_outputs) {
      auto inner_out = _innerOut(nested, p);
      auto bound     = composite->typedOutputNamed<HfImagePlugTraits>(p->_outer);
      if (inner_out and inner_out->_value and inner_out->_value->_ssbo and bound and bound->_value) {
        bound->_value->_w        = W;
        bound->_value->_h        = H;
        bound->_value->_channels = inner_out->_value->_channels;
        if (not bound->_value->_ssbo)
          bound->_value->_ssbo = _env->createStorageBuffer(fb);
      }
    }
    ci->beginDispatchPhase(); // device-to-device copies (copyBufferRegion must be in-phase)
    std::vector<FxShaderStorageBuffer*> to_release;
    for (auto p : sd->_promoted_outputs) {
      auto inner_out = _innerOut(nested, p);
      auto bound     = composite->typedOutputNamed<HfImagePlugTraits>(p->_outer);
      if (inner_out and inner_out->_value and inner_out->_value->_ssbo and bound and bound->_value and
          bound->_value->_ssbo) {
        ci->copyBufferRegion(inner_out->_value->_ssbo, 0, bound->_value->_ssbo, 0, fb);
        to_release.push_back(inner_out->_value->_ssbo);
      }
    }
    ci->endDispatchPhase(); // submit + WAIT -> boundary outputs valid
    for (auto b : to_release)
      _env->releaseToPool(b); // forced-root pinned -> release now (copy complete)
  }

  void publishLoopOutputs(
      dflow::DgModuleInst* composite, dflow::graphinst_ptr_t nested,
      const dflow::LoopModuleData* ld, int iteration, int count) override {
    const int W     = _env->_w, H = _env->_h;
    const size_t fb = size_t(W) * size_t(H) * sizeof(float);
    auto ci         = _env->_ctx->CI();
    // allocate any lazy (non-carry) boundary buffers, then device-copy inner->boundary:
    // carries every iter (feeds the next iteration), non-carry outputs on the last iter.
    for (auto p : ld->_promoted_outputs) {
      if (not(ld->isCarryOutput(p->_outer) or iteration == count - 1))
        continue;
      auto inner_out = _innerOut(nested, p);
      auto bound     = composite->typedOutputNamed<HfImagePlugTraits>(p->_outer);
      if (inner_out and inner_out->_value and inner_out->_value->_ssbo and bound and bound->_value) {
        bound->_value->_w        = W;
        bound->_value->_h        = H;
        bound->_value->_channels = inner_out->_value->_channels;
        if (not bound->_value->_ssbo) // non-carry output: allocate its buffer lazily
          bound->_value->_ssbo = _env->createStorageBuffer(fb);
      }
    }
    ci->beginDispatchPhase(); // device-to-device carry copies (copyBufferRegion must be in-phase)
    std::vector<FxShaderStorageBuffer*> to_release;
    for (auto p : ld->_promoted_outputs) {
      auto inner_out = _innerOut(nested, p);
      if (not(inner_out and inner_out->_value and inner_out->_value->_ssbo))
        continue;
      if (ld->isCarryOutput(p->_outer) or iteration == count - 1) {
        auto bound = composite->typedOutputNamed<HfImagePlugTraits>(p->_outer);
        if (bound and bound->_value and bound->_value->_ssbo)
          ci->copyBufferRegion(inner_out->_value->_ssbo, 0, bound->_value->_ssbo, 0, fb);
      }
      to_release.push_back(inner_out->_value->_ssbo);
    }
    ci->endDispatchPhase(); // submit + WAIT -> carries/outputs valid for the next iter / downstream
    for (auto b : to_release)
      _env->releaseToPool(b); // pinned forced roots -> release now (copy complete)
  }

  bakeenv_ptr_t _env;
};

static void _stockTerrainCookDriver(dflow::graphinst_ptr_t ginst, const bakeenv_ptr_t& env) {
  ginst->_impl.setShared<dflow::CookGraphDriver>(std::make_shared<TerrainCookDriver>(env));
}

// nested-inst builder — SHARES the parent BakeEnv (context / units / pool / cook cache)
// AND stocks a TerrainCookDriver so a NESTED composite can resolve it. Mirrors the
// bakeHeightfield sorter setup.
static dflow::graphinst_ptr_t _makeTerrainNestedInst(dflow::graphdata_ptr_t graph, const bakeenv_ptr_t& env) {
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuComputeImage2DData>("hf_img", 64);
  dgctx->createRegisters<float>("hf_float", 256);
  dgctx->createRegisters<fvec2>("hf_vec2", 256);
  dgctx->createRegisters<fvec4>("hf_vec4", 256);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);
  auto gi = dflow::GraphData::createGraphInst(graph);
  gi->_impl.setShared<BakeEnv>(env); // SHARE the parent env
  _stockTerrainCookDriver(gi, env);  // driver for nested composites
  gi->updateTopology(topo);          // instantiate + link + activate
  return gi;
}

///////////////////////////////////////////////////////////////////////////////
// terrain composite DATA — thin subclasses of the core dflow schema. reshapeIOs
// builds the HfImage boundary plugs from the (already-populated) promotion tables.
///////////////////////////////////////////////////////////////////////////////

static void _reshapeSubgraphIOs(dataflow::moduledata_ptr_t data) {
  auto sg = std::dynamic_pointer_cast<dflow::SubGraphModuleData>(data);
  if (not sg)
    return;
  for (auto p : sg->_promoted_inputs)
    if (p)
      dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, p->_outer.c_str());
  for (auto p : sg->_promoted_outputs)
    if (p)
      dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, p->_outer.c_str());
}

TerrainSubGraphModuleData::TerrainSubGraphModuleData() {
}
std::shared_ptr<TerrainSubGraphModuleData> TerrainSubGraphModuleData::createShared() {
  auto d       = std::make_shared<TerrainSubGraphModuleData>();
  d->_subgraph = std::make_shared<dflow::GraphData>();
  return d;
}
// createInstance inherited from dflow::SubGraphModuleData -> the core SubGraphModuleInst
// (the generic composite runtime); the GPU cook work is delegated to the stocked
// TerrainCookDriver.
void TerrainSubGraphModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TerrainSubGraphModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSubgraphIOs(m); });
}

TerrainLoopModuleData::TerrainLoopModuleData() {
}
std::shared_ptr<TerrainLoopModuleData> TerrainLoopModuleData::createShared() {
  auto d       = std::make_shared<TerrainLoopModuleData>();
  d->_subgraph = std::make_shared<dflow::GraphData>();
  return d;
}
// createInstance inherited from dflow::LoopModuleData -> the core LoopModuleInst (the
// generic loop runtime); the GPU cook work is delegated to the stocked TerrainCookDriver.
void TerrainLoopModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TerrainLoopModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSubgraphIOs(m); });
}

///////////////////////////////////////////////////////////////////////////////
// SubGraphModule gate — a LoopModule (body = a low-iteration thermal erode) inside a
// host graph. See the header for the three assertions.
///////////////////////////////////////////////////////////////////////////////

// build a host graph: fbm -> [loop(thermal x THERMAL_ITERS) x count] -> capture.
// LOOP_THERMAL_ITERS is deliberately small so the gate is seconds, not minutes.
static constexpr int kSubgraphThermalIters = 2;

// per-PROCESS-run fbm seed nonce (fbm hashes _seed, so it flows through the loop's
// input hash into the nested per-iteration salts). Makes each test-process run's cook
// cache entries UNIQUE, so the exact-count N->N+k oracle below can't be polluted by a
// prior run that already cached the count+2 tail iterations. Static -> the SAME nonce
// for every graph built in ONE run (within-run cache hits still work).
static int _subgraphSeedNonce() {
  static const int s_nonce =
      int(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0x7fffff); // < 2^24 (exact fbm seed)
  return s_nonce;
}

static dflow::graphdata_ptr_t _buildSubgraphHostGraph(const char* cappath, int count, bool loop_bypassed) {
  auto g        = std::make_shared<dflow::GraphData>();
  g->_cacheable = true;
  auto fbm      = FbmModuleData::createShared();
  fbm->_octaves = 5;
  fbm->_seed    = _subgraphSeedNonce();

  // the loop body: a one-node subgraph "thermal" (In -> thermal -> Out).
  auto loop        = TerrainLoopModuleData::createShared();
  loop->_count     = count;
  loop->_bypassed  = loop_bypassed;
  auto body        = loop->_subgraph;
  auto thermal     = ThermalErodeModuleData::createShared();
  thermal->_iterations = kSubgraphThermalIters;
  dflow::GraphData::addModule(body, "thermal", thermal);
  // promotions: boundary "In" -> thermal.In ; thermal.Out -> boundary "Out".
  auto pin           = std::make_shared<dflow::SubGraphPromotion>();
  pin->_outer        = "In";
  pin->_inner_module = "thermal";
  pin->_inner_plug   = "In";
  loop->_promoted_inputs.push_back(pin);
  auto pout           = std::make_shared<dflow::SubGraphPromotion>();
  pout->_outer        = "Out";
  pout->_inner_module = "thermal";
  pout->_inner_plug   = "Out";
  loop->_promoted_outputs.push_back(pout);
  // one carry: "In" (height) fed back from "Out" each iteration.
  auto carry              = std::make_shared<dflow::LoopCarry>();
  carry->_name            = "h";
  carry->_promoted_input  = "In";
  carry->_promoted_output = "Out";
  loop->_carries.push_back(carry);
  // build the boundary HfImage plugs from the (now-populated) promotion tables.
  _reshapeSubgraphIOs(loop);

  auto cap   = CaptureModuleData::createShared();
  cap->_path = ork::file::Path(cappath);
  dflow::GraphData::addModule(g, "fbm", fbm);
  dflow::GraphData::addModule(g, "loop", loop);
  dflow::GraphData::addModule(g, "cap", cap);
  g->safeConnect(loop->inputNamed("In"), fbm->outputNamed("Out"));
  g->safeConnect(cap->inputNamed("In"), loop->outputNamed("Out"));
  return g;
}

int terrainSubgraphTest(Context* ctx, int dim) {
  int fails = 0;
  auto aeq  = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  auto statsEq = [&](const std::vector<fieldstats_ptr_t>& a, const std::vector<fieldstats_ptr_t>& b) -> bool {
    return a.size() == 1 and b.size() == 1                //
           and aeq(a[0]->_min, b[0]->_min, 1e-6f)         //
           and aeq(a[0]->_max, b[0]->_max, 1e-6f)         //
           and aeq(a[0]->_mean, b[0]->_mean, 1e-6f);
  };

  ///////////////////////////////////////////////////////////////////////////
  // (a) JSON round-trip preserves subgraph + count + promotions, and the clone
  // bakes identically to the original.
  ///////////////////////////////////////////////////////////////////////////
  const int kBaseCount = 3;
  auto g0 = _buildSubgraphHostGraph("/tmp/terrain_subgraph_orig.exr", kBaseCount, false);
  auto s0 = bakeHeightfield(g0, ctx, dim);

  ork::reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(g0);
  std::string json = ser.output();
  ork::object_ptr_t out;
  ork::reflect::serdes::JsonDeserializer deser(json.c_str());
  deser.deserializeTop(out);
  auto g1 = std::dynamic_pointer_cast<dflow::GraphData>(out);
  if (not g1) {
    printf("[subgraph] deserialize -> GraphData FAILED\n");
    return 1;
  }
  auto loop1 = std::dynamic_pointer_cast<dflow::LoopModuleData>(g1->module("loop"));
  if (not loop1) {
    printf("[subgraph] clone missing 'loop' (or wrong class)\n");
    fails++;
  } else {
    bool count_ok = (loop1->_count == kBaseCount);
    bool sub_ok   = loop1->_subgraph and (loop1->_subgraph->module("thermal") != nullptr);
    bool prom_ok  = (loop1->_promoted_inputs.size() == 1) and (loop1->_promoted_outputs.size() == 1) //
                   and (loop1->_carries.size() == 1)                                                 //
                   and loop1->promotedInput("In") and loop1->promotedOutput("Out");
    printf("[subgraph] round-trip: count(%d==%d):%d subgraph:%d promotions+carry:%d\n",
           loop1->_count, kBaseCount, int(count_ok), int(sub_ok), int(prom_ok));
    if (not(count_ok and sub_ok and prom_ok))
      fails++;
    if (auto th = std::dynamic_pointer_cast<ThermalErodeModuleData>(loop1->_subgraph->module("thermal")))
      if (th->_iterations != kSubgraphThermalIters) {
        printf("[subgraph] nested thermal _iterations LOST (got %d want %d)\n", th->_iterations, kSubgraphThermalIters);
        fails++;
      }
  }
  if (auto cc = g1 ? std::dynamic_pointer_cast<CaptureModuleData>(g1->module("cap")) : nullptr)
    cc->_path = ork::file::Path("/tmp/terrain_subgraph_clone.exr");
  auto s1 = bakeHeightfield(g1, ctx, dim);
  bool clone_eq = statsEq(s0, s1);
  printf("[subgraph] clone bakes identically: %s (orig mean %.6f clone mean %.6f)\n",
         clone_eq ? "PASS" : "FAIL", s0.size() ? s0[0]->_mean : 0.0f, s1.size() ? s1[0]->_mean : 0.0f);
  if (not clone_eq)
    fails++;

  ///////////////////////////////////////////////////////////////////////////
  // (b) N->N+k oracle: rebake at count+2 recomputes EXACTLY 2 more nested
  // iterations (the first N cache-load). The (a) bakes above already stored the
  // first kBaseCount iterations, so a warm count bake loads all N and a count+2
  // bake computes exactly 2. Compare via the nested compute counter.
  ///////////////////////////////////////////////////////////////////////////
  auto g_warm = _buildSubgraphHostGraph("/tmp/terrain_subgraph_warmN.exr", kBaseCount, false);
  bakeHeightfield(g_warm, ctx, dim);
  int warmN_computes = s_lastSubgraphComputes;
  int warmN_loads    = s_lastSubgraphLoads;

  const int kMoreCount = kBaseCount + 2;
  auto g_more = _buildSubgraphHostGraph("/tmp/terrain_subgraph_more.exr", kMoreCount, false);
  bakeHeightfield(g_more, ctx, dim);
  int more_computes = s_lastSubgraphComputes;
  int more_loads    = s_lastSubgraphLoads;

  // warm count=N: every iteration's thermal is a cache hit -> 0 computes, N loads.
  // count=N+2: first N iterations load, last 2 compute -> 2 computes, N loads.
  bool oracle_ok = (warmN_computes == 0) and (warmN_loads == kBaseCount) //
                   and (more_computes == 2) and (more_loads == kBaseCount);
  printf("[subgraph] N->N+k oracle: warmN(computes %d loads %d) more(computes %d loads %d) "
         "expect warmN(0,%d) more(2,%d) : %s\n",
         warmN_computes, warmN_loads, more_computes, more_loads, kBaseCount, kBaseCount,
         oracle_ok ? "PASS" : "FAIL");
  if (not oracle_ok)
    fails++;

  ///////////////////////////////////////////////////////////////////////////
  // (c) module bypass on the composite splices it out (== a bake WITHOUT the loop:
  // the d63a7acb4 resolver aliases the loop's output to its first same-typed
  // connected input = the fbm feeding "In").
  ///////////////////////////////////////////////////////////////////////////
  auto g_by = _buildSubgraphHostGraph("/tmp/terrain_subgraph_bypass.exr", kBaseCount, true);
  auto s_by = bakeHeightfield(g_by, ctx, dim);
  // reference: fbm -> cap (no loop at all).
  auto g_ref        = std::make_shared<dflow::GraphData>();
  g_ref->_cacheable = true;
  auto fbmr         = FbmModuleData::createShared();
  fbmr->_octaves    = 5;
  fbmr->_seed       = _subgraphSeedNonce(); // match the bypassed loop's fbm so s_by == s_ref
  auto capr         = CaptureModuleData::createShared();
  capr->_path       = ork::file::Path("/tmp/terrain_subgraph_bypass_ref.exr");
  dflow::GraphData::addModule(g_ref, "fbm", fbmr);
  dflow::GraphData::addModule(g_ref, "cap", capr);
  g_ref->safeConnect(capr->inputNamed("In"), fbmr->outputNamed("Out"));
  auto s_ref     = bakeHeightfield(g_ref, ctx, dim);
  bool bypass_ok = statsEq(s_by, s_ref);
  printf("[subgraph] composite bypass == without-loop: %s (bypass mean %.6f ref mean %.6f)\n",
         bypass_ok ? "PASS" : "FAIL", s_by.size() ? s_by[0]->_mean : 0.0f, s_ref.size() ? s_ref[0]->_mean : 0.0f);
  if (not bypass_ok)
    fails++;

  printf("[subgraph] %d failure(s)\n", fails);
  return fails;
}

std::pair<int, int> lastSubgraphCookCounts() {
  return {s_lastSubgraphComputes, s_lastSubgraphLoads};
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
