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
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cinttypes>
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::TerrainModuleData, "terrain::TerrainModuleData");
ImplementReflectionX(ork::lev2::terrain::CaptureModuleData, "terrain::CaptureModuleData");

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

static void _writeCaptureSidecar(const std::string& imgpath, uint64_t key, const FieldStats& fs) {
  std::ofstream out(imgpath + ".cookhash", std::ios::trunc);
  if (out)
    out << "hfcap1 hash<" << std::hex << key << std::dec //
        << "> channel<" << fs._channel                   //
        << "> min<" << std::setprecision(9) << fs._min   //
        << "> max<" << fs._max                           //
        << "> mean<" << fs._mean << ">\n";
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
  if (sscanf(line.c_str(), "hfcap1 hash<%" SCNx64 "> channel<%127[^>]> min<%g> max<%g> mean<%g>", //
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
}

///////////////////////////////////////////////////////////////////////////////
// BakeEnv allocation arena — see the declaration notes in hfdflow.h.
///////////////////////////////////////////////////////////////////////////////

// Bake-buffer residency policy for DISCRETE GPUs (PCIEopt.md §6): iterative bake
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

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m, float height_scale_m) {
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
  env->_height_scale_m = height_scale_m;
  env->_per_op_sync      = true; // THIS driver syncs per op (the cook loop / capture flush below)
  env->_flushes_captures = true;
  // WS4 FRONTIER: only the cacheable path runs the per-op-synced cook loop below,
  // which is what makes lazy acquire + pooled release GPU-safe. The non-cacheable
  // path records the whole graph in ONE dispatch phase, so it keeps eager
  // activate-time allocation (the onActivate shim honors this flag).
  env->_lazy_acquire = graph->_cacheable;
  ginst->_impl.setShared<BakeEnv>(env);

  ginst->updateTopology(topo);

  // run the compute (modules dispatch; capture modules record requests)
  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0f;
  updata->_dt      = 0.0f;

  ctx->beginFrame();
  auto ci = ctx->CI();
  // FieldStats recovered from capture-currency sidecars (skipped sinks) — appended to
  // the flush's stats before return so the caller's manifest contract holds either way.
  std::vector<fieldstats_ptr_t> current_stats;
  if (graph->_cacheable) {
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
    bool do_disk_cache = true;
    for (size_t i = 0; i < graph->numModules(); i++) {
      if (auto cap = std::dynamic_pointer_cast<CaptureModuleData>(graph->module(i))) {
        if (not cap->_cache) {
          do_disk_cache = false;
          break;
        }
      }
    }
    // cook context = everything outside the graph that changes a node's OUTPUT:
    // the bake resolution AND the world units (meters->texels depends on dim/extent;
    // real slope depends on height_scale). Node hashes carry the resolution-independent
    // meter params; this context folds in the per-bake resolution + scale.
    if (do_disk_cache) {
      {
        auto ch = DataBlock::createHasher();
        ch->accumulateItem<int>(dim);
        ch->accumulateItem<float>(extent_m);
        ch->accumulateItem<float>(height_scale_m);
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
    auto& order    = ginst->_ordered_module_insts;
    const size_t N = order.size();

    // --- classify: sink / cache-hit. Probe via PREFIX reads (~256B of header) —
    // the full plane is fetched from disk only when a needed hit actually LOADS
    // (below), and released right after upload. Reading whole entries here held
    // ~33GB of planes for a warm 4096 bake (the 40+GB load-RSS incident).
    // ORKID_COOK_DEBUG=1: record WHY a node isn't a hit (printed if it dispatches:
    // no-cache-entry = hash not on disk; probe-reject = entry fails validation;
    // not-cacheable-type = not a TerrainComputeInst).
    static const bool s_cookdbg = (getenv("ORKID_COOK_DEBUG") != nullptr);
    std::vector<bool> hit(N, false), needed(N, false), sink(N, false);
    std::vector<const char*> missreason(N, nullptr); // cookdbg: why not a cache hit
    for (size_t i = 0; i < N; i++) {
      auto inst = order[i];
      sink[i]   = (inst->numOutputs() == 0); // Capture — always runs, reads at flush
      if (s_cookdbg) // full topo map (node -> cook hash) for offline blob forensics
        printf("[cookdbg] PLAN i<%zu> node<%s> hash<0x%zx>%s\n", i,
               inst->_abstract_module_data->_name.c_str(), size_t(inst->_cookHash),
               sink[i] ? " (sink)" : "");
      if (do_disk_cache and not sink[i]) {
        if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst)) {
          auto hdr = DataBlockCache::findDataBlockPrefix("dflowcache", inst->_cookHash, 256);
          if (hdr and tci->cookProbe(hdr, dim, dim))
            hit[i] = true;
          else
            missreason[i] = hdr ? "probe-reject" : "no-cache-entry";
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
    std::vector<bool> capcurrent(N, false);
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

    // --- demand (reverse-topo): non-current sinks run; a dispatching node pulls its producers
    for (size_t ri = N; ri > 0; ri--) {
      size_t j = ri - 1;
      if (sink[j] and not capcurrent[j])
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

    // --- release schedule: for each producer OUTPUT PLUG, the last dispatching
    // reader's index. INCREMENTAL FLUSH (default): a sink counts as a normal last
    // reader — its field is read back to host the moment it runs (below), so the
    // plane needs no post-loop pinning. ORKID_BAKE_DEFERRED_FLUSH=1 restores the
    // old behavior (pin sink-read plugs; the post-loop flush maps the live SSBOs) —
    // costs O(#sinks) planes of VRAM for the whole bake, which is what spilled
    // eflow's frontier past the DEVICE budget (2026-07-03 profile).
    // Buffers resolve from the plug AT RELEASE TIME (erox aliases its output to a
    // ping-pong buffer during compute, so the pointer is only final after it runs).
    static const bool s_deferred_flush = (getenv("ORKID_BAKE_DEFERRED_FLUSH") != nullptr);
    std::unordered_map<const void*, size_t> last_reader; // outpluginst -> index
    std::unordered_set<const void*> pinned;
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
    std::vector<std::vector<dflow::outpluginst_ptr_t>> release_at(N);
    for (size_t j = 0; j < N; j++)
      for (int o = 0; o < order[j]->numOutputs(); o++) {
        auto op  = order[j]->output(o);
        auto key = (const void*)op.get();
        if (pinned.count(key))
          continue;
        auto it = last_reader.find(key);
        if (it != last_reader.end())
          release_at[it->second].push_back(op);
      }

    // --- the loop
    int cook_loaded = 0, cook_computes = 0, cook_skipped = 0;
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
    size_t ncap_readback = 0; // incremental flush: captures already read back to host
    // per-module-CLASS rollup — the cost model for the strategic cache-point set
    // (cache a class iff its recompute cost beats its blob load cost).
    struct BpClass { double dsp = 0, wait = 0, sto = 0; size_t bytes = 0; int n = 0; };
    std::map<std::string, BpClass> bp_class;
    size_t bp_bytes = 0;
    std::string bp_max_dsp_n, bp_max_sto_n;
    const double bp_loop0 = bp_now();
    for (size_t i = 0; i < N; i++) {
      auto inst = order[i];
      if (not needed[i]) {
        cook_skipped++;
        continue; // no acquire, no disk read, no upload
      }
      double bp_t0 = s_bakeprof ? bp_now() : 0.0;
      env->beginNodeScope();
      if (auto tci = std::dynamic_pointer_cast<TerrainComputeInst>(inst))
        tci->bakeAcquire(ginst.get()); // outputs + scratch, pool-served (lazy mode)
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
        if (do_disk_cache) {
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
          req._hostcopy = std::make_shared<std::vector<float>>(cnt);
          fxi_rb->readStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), req._hostcopy->data());
        }
        ncap_readback = env->_captures.size();
      }
      // scratch back to the pool — everything this node acquired EXCEPT what its
      // output plugs publish (read NOW, post-compute: erox finalized its alias).
      std::unordered_set<FxShaderStorageBuffer*> keep;
      for (int o = 0; o < inst->numOutputs(); o++)
        if (auto op = std::dynamic_pointer_cast<hfimg_outpluginst_t>(inst->output(o)))
          if (op->_value and op->_value->_ssbo)
            keep.insert(op->_value->_ssbo);
      env->endNodeScope(keep);
      // upstream buffers whose last dispatching reader was this node
      for (auto& op : release_at[i])
        if (auto hop = std::dynamic_pointer_cast<hfimg_outpluginst_t>(op))
          if (hop->_value)
            env->releaseToPool(hop->_value->_ssbo);
    }
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
    // cache-SATISFIED count: loaded + demand-skipped (a skipped node is satisfied
    // by the cache too — nothing dispatched against it needed recomputing). Keeps
    // terrainCacheTest's invariant: warm bake recomputes nothing but the sink.
    s_lastCookHits = cook_loaded + cook_skipped;
  } else {
    for (auto inst : ginst->_ordered_module_insts) // family-neutral pre-phase (see above)
      if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
        pp->writeParams(ctx);
    ci->beginDispatchPhase();
    ginst->compute(updata);
    ci->endDispatchPhase();
  }
  ctx->endFrame();

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
            _writeCaptureSidecar(path, cookkey, *fs);
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
    // world scale for the normal gradient (assumes a square field, W==H==dim).
    float texel_m  = (w > 0) ? (env->_extent_m / float(w)) : 1.0f;
    float vscale_m = env->_height_scale_m;

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

    // the NORMALIZED [min,max] -> [0,1] field (auto-exposure). The "height" output IS
    // this; the "normal" output is the gradient of (this * height_scale_m). Computed
    // ONCE here and shared by every channel this capture emits — the efficiency win of
    // capture(node, ["height","normal"]) vs two separate captures (one readback).
    float range = vmax - vmin;
    float inv   = (range > 1e-12f) ? (1.0f / range) : 0.0f;
    std::vector<float> hn(n);
    for (size_t i = 0; i < n; i++) {
      float t = (src[i] - vmin) * inv;
      hn[i]   = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    if(0)printf("[terrain bake] field stats: min<%g> max<%g> mean<%g>  (normalized to [min,max])\n",
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
        // rotationally-symmetric) of the surface height (normalized * height_scale_m),
        // in world meters. n = normalize(-dH/dx, +1, -dH/dz), +Y up. EXR -> signed xyz
        // float (RGBA32F, a=1); PNG -> n*0.5+0.5 (RGBA8). NOTE: the normal is baked at
        // env->_height_scale_m — a consumer must render the height at that same scale
        // for the normal to be consistent.
        std::vector<float>   nf; // .exr
        std::vector<uint8_t> nb; // .png
        if (as_png) nb.resize(n * 4); else nf.resize(n * 4);
        auto Hm = [&](int x, int y) -> float {
          x = (x < 0) ? 0 : (x >= w ? w - 1 : x);   // CLAMP_TO_EDGE
          y = (y < 0) ? 0 : (y >= h ? h - 1 : y);
          return hn[size_t(y) * size_t(w) + size_t(x)] * vscale_m;
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
        // scalar field (height / mask): the normalized [0,1] field.
        // EXR -> R32F float (no quantization); PNG -> R16UI (×65535).
        if (as_png) {
          std::vector<uint16_t> g16(n);
          for (size_t i = 0; i < n; i++)
            g16[i] = uint16_t(hn[i] * 65535.0f + 0.5f);
          oimg.initWithFormat(w, h, EBufferFormat::R16UI);
          memcpy((void*)oimg._data->data(), g16.data(), n * sizeof(uint16_t));
        } else {
          oimg.initWithFormat(w, h, EBufferFormat::R32F);
          memcpy((void*)oimg._data->data(), hn.data(), n * sizeof(float));
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
        _writeCaptureSidecar(path, cookkey, *fs);
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
  return stats;
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
    cm->_op = op;
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
  auto runTerrace = [&](const char* nm, float in, float steps, float sharp) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ci = mkConst(in);
    auto tr = TerraceModuleData::createShared();
    tr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(steps);
    tr->typedInputNamed<dflow::FloatPlugTraits>("sharpness")->setValue(sharp);
    dflow::GraphData::addModule(g, "c", ci);
    dflow::GraphData::addModule(g, "t", tr);
    g->safeConnect(tr->inputNamed("In"), ci->outputNamed("Out"));
    capTo(g, tr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  // 0.6*4=2.4 -> plateau 2 -> 2/4=0.5 ; 0.7*4=2.8 -> riser -> 3/4=0.75 ; 0.9*2=1.8 -> 2/2=1.0
  check("Terrace(.6,4,1)", runTerrace("terr_06", 0.6f, 4.0f, 1.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Terrace(.7,4,1)", runTerrace("terr_07", 0.7f, 4.0f, 1.0f), 0.75f, 0.75f, 0.75f, 1e-4f);
  check("Terrace(.9,2,1)", runTerrace("terr_09", 0.9f, 2.0f, 1.0f), 1.0f, 1.0f, 1.0f, 1e-4f);

  // --- Slope (pre-blurred gradient + soft rolloff, Mask by Feature) -----------
  // flat field -> slope 0 everywhere. A unit ramp has gradient magnitude exactly 1
  // (box-averaging a linear field is exact), so slope = soft_rolloff(1*scale) =
  // 1/(1+1) = 0.5 uniform in the interior; the border ring (margin radius+box) is 0.
  // bake the mask-generator cases with extent==height_scale==dim, so 1 texel == 1 m
  // (radius_m == radius_texels) and the slope factor height_scale/extent == 1.
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
    check("Slope(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f;
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), gr->outputNamed("Out"));
    capTo(g, sl, path("slope_ramp"));
    auto s  = bakeHeightfield(g, ctx, dim, E, E)[0];
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
    cv->_mode     = int(CurvatureMode::MAGNITUDE);
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(cv->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, cv, path("curv_flat"));
    check("Curvature(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  auto runCurv = [&](const char* nm, int mode, float scale) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared(); // value = uv.x
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sq = CombineModuleData::createShared(); // uv.x * uv.x = uv.x^2
    sq->_op = int(CombineOp::MUL);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = mode;
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    cv->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(scale);
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "sq", sq);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(sq->inputNamed("A"), gr->outputNamed("Out"));
    g->safeConnect(sq->inputNamed("B"), gr->outputNamed("Out"));
    g->safeConnect(cv->inputNamed("In"), sq->outputNamed("Out"));
    capTo(g, cv, path(nm));
    return bakeHeightfield(g, ctx, dim, E, E)[0];
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
    comb->_op = int(CombineOp::MUL); // non-default baked scalar (default is ADD)
    // Gradient with a NON-default vec2 "dir" — exercises the vec2 plug VALUE
    // surviving the JSON round-trip (the inplugdata<Vec2fPlugTraits> reflection).
    auto grad = GradientModuleData::createShared();
    grad->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(0.6f, 0.8f)); // default is (1,0)
    grad->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    auto comb2 = CombineModuleData::createShared();
    comb2->_op = int(CombineOp::MUL);
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
  if (not comb1 or comb1->_op != int(CombineOp::MUL)) {
    printf("[roundtrip] _op LOST (got %d, want %d=MUL)\n", comb1 ? comb1->_op : -1, int(CombineOp::MUL));
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
