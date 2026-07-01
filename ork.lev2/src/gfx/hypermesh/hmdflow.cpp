////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// GPU mesh compute-dataflow — foundation: the SSBO pool, the mesh plug type, the bake
// driver, and a readback self-test. See hmdflow.h for the design.
//
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <atomic>
#include <chrono>
#include <algorithm>
#include <ork/util/logger.h>
#include <ork/kernel/datacache.h>                  // DataBlockCache (E.6/2.19 cook cache)
#include <ork/reflect/serialize/JsonSerializer.h>  // content-only module identity hash
#include <rapidjson/document.h>                    // uuid-envelope strip
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

ImplementReflectionX(ork::lev2::hypermesh::MeshModuleData, "hypermesh::MeshModuleData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// global dflow-clock pause (per-graph pause lives on LiveHypermesh::_paused)
///////////////////////////////////////////////////////////////////////////////
static std::atomic<bool> g_clock_paused{false};
void setClockPaused(bool paused) {
  g_clock_paused.store(paused);
}
bool clockPaused() {
  return g_clock_paused.load();
}

///////////////////////////////////////////////////////////////////////////////
// HmPerf — see hmdflow.h. Reported on the yellow HYPERMESH channel every 5 seconds.
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_hm = logger()->configureChannel("HYPERMESH", fvec3(1, 1, 0), false);

HmPerf& HmPerf::instance() {
  static HmPerf p;
  return p;
}
void HmPerf::addModule(const char* clazz, double secs) {
  std::lock_guard<std::mutex> lk(_mtx);
  auto& e = _byClass[clazz];
  e.first += secs;
  e.second++;
}
void HmPerf::addGraph(double secs, double gpu_secs, int polys, int verts, int instances) {
  std::lock_guard<std::mutex> lk(_mtx);
  _graphSecs += secs;
  _gpuSecs += gpu_secs;
  _polySum += uint64_t(std::max(0, polys));
  _vertSum += uint64_t(std::max(0, verts));
  _drawnSum += uint64_t(std::max(0, polys)) * uint64_t(std::max(1, instances));
  _graphEvals++;
}
void HmPerf::report() {
  using clk  = std::chrono::steady_clock;
  double now = std::chrono::duration<double>(clk::now().time_since_epoch()).count();
  std::lock_guard<std::mutex> lk(_mtx);
  if (_windowStart < 0.0) { // first eval opens the window
    _windowStart = now;
    return;
  }
  double elapsed = now - _windowStart;
  if (elapsed < 5.0)
    return;
  if (_graphEvals > 0) {
    auto rate = [&](uint64_t sum) -> std::string { // human-compact per-second rate
      double r = double(sum) / elapsed;
      if (r >= 1e6) return FormatString("%.2fM/s", r * 1e-6);
      if (r >= 1e3) return FormatString("%.1fK/s", r * 1e-3);
      return FormatString("%.0f/s", r);
    };
    uint64_t avgpoly  = _polySum / _graphEvals;
    uint64_t avgvert  = _vertSum / _graphEvals;
    uint64_t avgdrawn = _drawnSum / _graphEvals;    // includes instancing (drawn = polys * instances)
    std::string outline = FormatString(
        "out: polys %llu (%s)  verts %llu (%s)",
        (unsigned long long)avgpoly, rate(_polySum).c_str(),
        (unsigned long long)avgvert, rate(_vertSum).c_str());
    if (avgdrawn > avgpoly)                         // instanced: what actually draws
      outline += FormatString(
          "  x%llu inst -> drawn %llu (%s)",
          (unsigned long long)(avgpoly ? (avgdrawn / avgpoly) : 1),
          (unsigned long long)avgdrawn, rate(_drawnSum).c_str());
    std::string block = FormatString(
        "perf %.1fs: evals=%llu (%.1f/s)  graph %.3f ms/eval (cpu %.3f + gpu-wait %.3f)\n%s",
        elapsed,
        (unsigned long long)_graphEvals,
        double(_graphEvals) / elapsed,
        1e3 * _graphSecs / double(_graphEvals),
        1e3 * (_graphSecs - _gpuSecs) / double(_graphEvals),
        1e3 * _gpuSecs / double(_graphEvals),
        outline.c_str());
    { // channel output stays line-oriented (the retained block keeps the embedded newline for the HUD)
      auto nl = block.find('\n');
      logchan_hm->log("%s", block.substr(0, nl).c_str());
      logchan_hm->log("%s", block.substr(nl + 1).c_str());
    }
    std::vector<std::pair<std::string, std::pair<double, uint64_t>>> rows(_byClass.begin(), _byClass.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    for (const auto& r : rows) {
      std::string nm = r.first; // "hypermesh::ExtrudeFacesData" -> "ExtrudeFaces"
      auto cp        = nm.rfind(':');
      if (cp != std::string::npos)
        nm = nm.substr(cp + 1);
      if (nm.size() > 4 and nm.compare(nm.size() - 4, 4, "Data") == 0)
        nm.resize(nm.size() - 4);
      auto line = FormatString(
          "  %-20s %8.3f ms/call  %10.3f ms total  %7llu calls",
          nm.c_str(),
          1e3 * r.second.first / double(r.second.second),
          1e3 * r.second.first,
          (unsigned long long)r.second.second);
      logchan_hm->log("%s", line.c_str());
      block += "\n";
      block += line;
    }
    _lastReport = block; // retained for the HUD (perfStats)
  }
  _byClass.clear();
  _graphSecs   = 0.0;
  _gpuSecs     = 0.0;
  _polySum     = 0;
  _vertSum     = 0;
  _drawnSum    = 0;
  _graphEvals  = 0;
  _windowStart = now;
}
std::string HmPerf::statsText() {
  std::lock_guard<std::mutex> lk(_mtx);
  return _lastReport;
}

namespace dflow = ::ork::dataflow;

// the GraphInst per-module compute() timing sink (installed on every hypermesh graphinst)
static void _installPerfSink(dflow::graphinst_ptr_t ginst) {
  ginst->_moduleTimingSink = [](const char* clazz, double secs) { HmPerf::instance().addModule(clazz, secs); };
}

///////////////////////////////////////////////////////////////////////////////
// cook cache (E.6/2.19) — content-only module identity + full-mesh store/load.
// See the decls in hmdflow.h and the contract comment on MeshComputeInst.
///////////////////////////////////////////////////////////////////////////////

static int s_lastCookHits   = 0;
static int s_lastCookStores = 0;
int hypermeshLastCookHits()   { return s_lastCookHits; }
int hypermeshLastCookStores() { return s_lastCookStores; }

uint64_t hypermeshModuleIdentityHash(const dflow::DgModuleData* mod) {
  // serialize the module's reflected state (class + properties + plug values + plug
  // sub-objects). Non-owning alias: serializeRoot wants a shared_ptr, the graph owns
  // the module; the serializer does not retain it.
  object_ptr_t alias(const_cast<dflow::DgModuleData*>(mod), [](Object*) {});
  reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(alias);
  rapidjson::Document doc;
  doc.Parse(ser.output().c_str());
  // strip every "uuid" member recursively: uuids are durable per-OBJECT identity for
  // git history (deserialize preserves them) — but the primary workflow re-authors
  // graphs every run (scene.py -> tojson), minting FRESH uuids each time, so cook
  // identity must be content-only or the cache would never hit where it matters.
  std::function<void(rapidjson::Value&)> strip = [&](rapidjson::Value& v) {
    if (v.IsObject()) {
      if (v.HasMember("uuid"))
        v.RemoveMember("uuid");
      for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
        strip(it->value);
    } else if (v.IsArray()) {
      for (auto& e : v.GetArray())
        strip(e);
    }
  };
  strip(doc);
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);
  doc.Accept(w);
  auto h = DataBlock::createHasher();
  h->accumulateString(sb.GetString());
  h->finish();
  return h->result();
}

uint64_t MeshComputeInst::cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const {
  auto h = DataBlock::createHasher();
  h->accumulateString("hm.cook.v1"); // global format epoch
  h->accumulateString(std::string(_dgmodule_data->GetClass()->Name().c_str()));
  h->accumulateString(_cookSalt()); // per-class KERNEL version
  h->accumulateItem<uint64_t>(hypermeshModuleIdentityHash(_dgmodule_data));
  h->accumulateItem<uint64_t>(context);
  for (auto x : input_hashes)
    h->accumulateItem<uint64_t>(x);
  h->finish();
  return h->result();
}

static constexpr int kHmCookFmt = 0x484D4B31; // 'HMK1' — bump if the layout below changes

// topology-setup cascade fixpoint bound: how many (onTopologyReady-pass + re-eval) rounds before giving
// up. Real chains (extrude->extrude, merge<-select->assign_gid) settle in 2-3; this only caps a bug.
static constexpr int kTopoCascadeMax = 16;

datablock_ptr_t MeshComputeInst::cookStore() const {
  auto mesh = _outMesh();
  if (not mesh or mesh->_channels.empty() or not mesh->_vidx or not mesh->_face_offsets)
    return nullptr; // not a mesh producer (ScatterSource, sinks pre-eval) -> uncached
  auto env = _graphinst->_impl.getShared<MeshEnv>();
  auto fxi = env->_ctx->FXI();
  auto db  = std::make_shared<DataBlock>();
  bool ok  = true;
  auto putbuf = [&](FxShaderStorageBuffer* b, size_t bytes) {
    if (not ok)
      return;
    auto m = b ? fxi->mapStorageBuffer(b, 0, bytes, BufferMapAccess::READ_ONLY) : nullptr;
    if (not m or not m->_mappedaddr) { // null buffer / oversized map -> DON'T cache this node
      ok = false;
      return;
    }
    db->addData(m->_mappedaddr, bytes);
    fxi->unmapStorageBuffer(m.get());
  };
  // a channel whose pooled capacity can't cover the live count would map OOB —
  // refuse to cache the node LOUDLY (it also means an op emitted an undersized
  // channel; surfacing beats serializing garbage).
  auto fits = [&](const gpuchannel_ptr_t& ch, int count) -> bool {
    if (ch and ch->_ssbo and ch->_capacity >= count)
      return true;
    printf(
        "[hm cook] %s: channel capacity %d < live count %d — node NOT cached\n",
        _dgmodule_data->GetClass()->Name().c_str(), ch ? ch->_capacity : -1, count);
    return false;
  };
  auto putname = [&](const std::string& s) {
    db->addItem<int>(int(s.size()));
    db->addData(s.data(), s.size());
  };
  db->addItem<int>(kHmCookFmt);
  db->addItem<int>(mesh->_num_verts);
  db->addItem<int>(mesh->_num_corners);
  db->addItem<int>(mesh->_num_faces);
  db->addItem<int>(int(mesh->_channels.size()));
  for (const auto& [sem, ch] : mesh->_channels) {
    if (not fits(ch, mesh->_num_verts))
      return nullptr;
    db->addItem<int>(int(sem));
    db->addItem<int>(ch->_stride);
    putbuf(ch->_ssbo, size_t(ch->_stride) * mesh->_num_verts);
  }
  db->addItem<int>(int(mesh->_vattrs.size()));
  for (const auto& [nm, ch] : mesh->_vattrs) {
    if (not fits(ch, mesh->_num_verts))
      return nullptr;
    putname(nm);
    db->addItem<int>(ch->_stride);
    putbuf(ch->_ssbo, size_t(ch->_stride) * mesh->_num_verts);
  }
  db->addItem<int>(int(mesh->_faces.size()));
  for (const auto& [nm, ch] : mesh->_faces) {
    if (not fits(ch, mesh->_num_faces))
      return nullptr;
    putname(nm);
    db->addItem<int>(ch->_stride);
    putbuf(ch->_ssbo, size_t(ch->_stride) * mesh->_num_faces);
  }
  if (not fits(mesh->_vidx, mesh->_num_corners) or not fits(mesh->_face_offsets, mesh->_num_faces + 1))
    return nullptr;
  putbuf(mesh->_vidx->_ssbo, size_t(mesh->_num_corners) * 4);
  putbuf(mesh->_face_offsets->_ssbo, size_t(mesh->_num_faces + 1) * 4);
  putbuf(mesh->_header, kMeshHeaderBytes); // exact blob -> no layout knowledge here
  return ok ? db : nullptr;
}

bool MeshComputeInst::cookLoad(datablock_constptr_t db) {
  auto outp = typedOutputNamed<MeshPlugTraits>("Out");
  if (not outp or not outp->_value)
    return false;
  auto env  = _graphinst->_impl.getShared<MeshEnv>();
  auto fxi  = env->_ctx->FXI();
  auto mesh = outp->_value;
  DataBlockInputStream istr(db);
  if (istr.getItem<int>() != kHmCookFmt)
    return false; // old/foreign layout -> recompute
  int nv = istr.getItem<int>();
  int nc = istr.getItem<int>();
  int nf = istr.getItem<int>();
  auto getbuf = [&](FxShaderStorageBuffer* b, size_t bytes) {
    auto m = fxi->mapStorageBuffer(b, 0, bytes, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, istr.current(), bytes);
    fxi->unmapStorageBuffer(m.get());
    istr.advance(bytes);
  };
  auto getname = [&]() -> std::string {
    int len = istr.getItem<int>();
    std::string s((const char*)istr.current(), size_t(len));
    istr.advance(len);
    return s;
  };
  mesh->_num_verts   = nv;
  mesh->_num_corners = nc;
  mesh->_num_faces   = nf;
  mesh->_capacity    = meshNextPow2(nv);
  int nchan = istr.getItem<int>();
  for (int i = 0; i < nchan; i++) {
    auto sem   = MeshChannel(istr.getItem<int>());
    int stride = istr.getItem<int>();
    auto ch    = env->_pool->acquireChannel(sem, nv);
    if (ch->_stride != stride)
      return false; // semantic stride drifted -> recompute
    getbuf(ch->_ssbo, size_t(stride) * nv);
    mesh->_channels[sem] = ch;
  }
  int nvattr = istr.getItem<int>();
  for (int i = 0; i < nvattr; i++) {
    auto nm    = getname();
    int stride = istr.getItem<int>();
    auto ch    = env->_pool->acquireChannel(stride, nv);
    getbuf(ch->_ssbo, size_t(stride) * nv);
    mesh->_vattrs[nm] = ch;
  }
  int nfattr = istr.getItem<int>();
  for (int i = 0; i < nfattr; i++) {
    auto nm    = getname();
    int stride = istr.getItem<int>();
    auto ch    = env->_pool->acquireChannel(stride, nf);
    getbuf(ch->_ssbo, size_t(stride) * nf);
    mesh->_faces[nm] = ch;
  }
  mesh->_vidx         = env->_pool->acquireChannel(4, nc);
  mesh->_face_offsets = env->_pool->acquireChannel(4, nf + 1);
  getbuf(mesh->_vidx->_ssbo, size_t(nc) * 4);
  getbuf(mesh->_face_offsets->_ssbo, size_t(nf + 1) * 4);
  if (not mesh->_header)
    mesh->_header = env->_pool->acquire(kMeshHeaderBytes, 1);
  getbuf(mesh->_header, kMeshHeaderBytes);
  mesh->markTopoChanged(); // re-emission IS the dirty signal — downstream consumers rebuild
  return true;
}

// per-node load pass for a cacheable graph: hash every node (content-only Merkle), then
// restore what the disk cache has (host uploads — pre-dispatch legal). Returns the
// hit-set; the driver computes only the misses and stores them after its FINAL sync
// (hypermesh outputs aren't final until the topology-readback cascade settles, so no
// per-op submit+wait is needed — unlike terrain, stores all happen post-cascade).
static std::set<dflow::DgModuleInst*> _cookLoadPass(dflow::graphinst_ptr_t ginst) {
  std::set<dflow::DgModuleInst*> loaded;
  ginst->_cookContextHash = 0; // nothing external changes a node's output (budget = pool sizing only)
  ginst->computeNodeHashes();
  s_lastCookHits = 0;
  for (auto inst : ginst->_ordered_module_insts) {
    auto mci = std::dynamic_pointer_cast<MeshComputeInst>(inst);
    if (not mci or mci->_cookHash == 0)
      continue; // foreign-family insts (terrain fields via DisplaceByField) always recompute (v1)
    auto db = DataBlockCache::findDataBlock("dflowcache", mci->_cookHash);
    if (db and mci->cookLoad(db)) {
      loaded.insert(inst.get());
      s_lastCookHits++;
    }
  }
  return loaded;
}

// store pass — call ONLY after the final eval's endDispatchPhase (submit + WAIT).
static void _cookStorePass(dflow::graphinst_ptr_t ginst, const std::set<dflow::DgModuleInst*>& loaded) {
  s_lastCookStores = 0;
  for (auto inst : ginst->_ordered_module_insts) {
    auto mci = std::dynamic_pointer_cast<MeshComputeInst>(inst);
    if (not mci or mci->_cookHash == 0 or loaded.count(inst.get()))
      continue;
    if (auto store = mci->cookStore()) {
      DataBlockCache::setDataBlock("dflowcache", mci->_cookHash, store);
      s_lastCookStores++;
    }
  }
  if (s_lastCookHits or s_lastCookStores)
    printf("[hm cook] cacheable graph: %d loaded from cache, %d computed+stored\n", s_lastCookHits, s_lastCookStores);
}

// one dispatch-phase eval skipping cook-loaded nodes. NEVER routes through
// GraphInst::compute on a cacheable graph — that would take the core's SYNCHRONOUS
// cachedCompute branch (store-before-submit reads garbage on a GPU graph).
static void _computeSkipping(
    dflow::graphinst_ptr_t ginst, ui::updatedata_ptr_t updata, const std::set<dflow::DgModuleInst*>& skip) {
  using clk = std::chrono::steady_clock;
  for (auto inst : ginst->_ordered_module_insts) {
    if (skip.count(inst.get()))
      continue;
    auto t0 = clk::now();
    inst->compute(ginst.get(), updata);
    if (ginst->_moduleTimingSink)
      ginst->_moduleTimingSink(
          inst->_dgmodule_data->GetClass()->Name().c_str(),
          std::chrono::duration<double>(clk::now() - t0).count());
  }
}

///////////////////////////////////////////////////////////////////////////////

const char* meshChannelName(MeshChannel c) {
  switch (c) {
    case MeshChannel::POSITION: return "P";
    case MeshChannel::NORMAL:   return "N";
    case MeshChannel::BINORMAL: return "B";
    case MeshChannel::UV0:      return "uv";
    case MeshChannel::COLOR:    return "color";
    case MeshChannel::INDEX:    return "index";
    case MeshChannel::BONEIDX:  return "boneidx";
    case MeshChannel::BONEWT:   return "bonewt";
    default:                    return "?";
  }
}

gpumesh_ptr_t MeshPlugTraits::data_to_inst(gpumesh_data_ptr_t inp) {
  return std::make_shared<GpuMesh>(inp);
}

void MeshModuleData::describeX(class_t* clazz) {
  clazz->directProperty("precheck", &MeshModuleData::_precheck);     // self-defend: assert on bad INPUT
  clazz->directProperty("postcheck", &MeshModuleData::_postcheck);   // self-defend: assert on bad OUTPUT
}
MeshModuleData::MeshModuleData() {
}

///////////////////////////////////////////////////////////////////////////////
// MeshPool — pow2 size-classed SSBO pool (see hmdflow.h).
///////////////////////////////////////////////////////////////////////////////

MeshPool::~MeshPool() {
  // buffers are reclaimed at gfx-context teardown; nothing to free explicitly here.
  // (a destroyStorageBuffer funnel would slot in here for long-lived live graphs.)
}

FxShaderStorageBuffer* MeshPool::acquire(int stride, int capacity) {
  int cap  = meshNextPow2(capacity);
  auto key = std::make_pair(stride, cap);
  auto& fl = _free[key];
  if (not fl.empty()) {
    auto b = fl.back();
    fl.pop_back();
    return b;
  }
  // hot buffers (vertex channels, topology tables, scan intermediates) are GPU-resident: the compute
  // reads/writes them every frame, the CPU only touches them at setup/readback (via staging). On a
  // discrete GPU this is the difference between VRAM bandwidth and PCIe.
  auto b = _ctx->FXI()->createStorageBuffer(
      size_t(stride) * size_t(cap), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
  _all.push_back(b);
  return b;
}

void MeshPool::release(FxShaderStorageBuffer* b, int stride, int pow2cap) {
  _free[std::make_pair(stride, pow2cap)].push_back(b);
}

gpuchannel_ptr_t MeshPool::acquireChannel(MeshChannel sem, int capacity) {
  auto ch       = acquireChannel(meshChannelStride(sem), capacity);
  ch->_semantic = sem;
  return ch;
}

// raw (face attrs / topology buffers, e.g. uint stride 4): same pow2 pool + auto-return deleter.
gpuchannel_ptr_t MeshPool::acquireChannel(int stride, int capacity) {
  int cap  = meshNextPow2(capacity);
  auto buf = acquire(stride, cap);
  // the shared_ptr deleter returns the buffer to THIS pool's free-list on last ref (COW recycle); it
  // captures a shared_ptr to the pool so the pool outlives its channels (a baked mesh can be read back
  // after the GraphInst is gone).
  auto poolref = shared_from_this();
  return gpuchannel_ptr_t(
      new GpuChannel{buf, stride, cap},
      [poolref, stride, cap](GpuChannel* g) {
        poolref->release(g->_ssbo, stride, cap);
        delete g;
      });
}

///////////////////////////////////////////////////////////////////////////////
// driver — sort, instantiate (fresh MeshEnv + pool), run the compute. Returns the
// terminal mesh (the last ordered module's "Out") for readback / asset export / render.
// NOTE (E.1): the drivers know NOTHING about cross-family fields — a module that brings
// another family's subgraph in (DisplaceByField) stocks that family's env itself (onLink,
// TypeKeyedVars) and carries its own parameters. No per-module special cases here.
///////////////////////////////////////////////////////////////////////////////

gpumesh_ptr_t bakeMesh(dflow::graphdata_ptr_t graph, Context* ctx, int vtx_budget) {
  // register pools so the sorter can allocate a slot for every connected output type.
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuMeshData>("hm_mesh", 64);
  dgctx->createRegisters<dflowgfx::GpuComputeImage2DData>("hm_hfimage", 64); // interchange (B.3); 64 = terrain expression chains fan wide
  dgctx->createRegisters<dflowgfx::InstanceSetData>("hm_instset", 16);        // interchange (E.2): the typed instance edge
  dgctx->createRegisters<dflowgfx::SdfGridData>("hm_sdfgrid", 16);            // interchange (E.7): the SDF boolean chain
  dgctx->createRegisters<::ork::hyper::XfNodeGraphData>("hm_xfng", 16);       // interchange (G0a): the XfNodeGraph transform-graph spine
  dgctx->createRegisters<int>("hm_int", 256);
  dgctx->createRegisters<float>("hm_float", 256);
  dgctx->createRegisters<fvec2>("hm_vec2", 64);
  dgctx->createRegisters<fvec3>("hm_vec3", 64);
  dgctx->createRegisters<fvec4>("hm_vec4", 64);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);

  auto ginst = dflow::GraphData::createGraphInst(graph);
  _installPerfSink(ginst);                       // per-CLASS module counters (HmPerf)
  auto env   = std::make_shared<MeshEnv>();
  env->_ctx        = ctx;
  env->_pool       = std::make_shared<MeshPool>(ctx);
  env->_vtx_budget = vtx_budget;
  ginst->_impl.setShared<MeshEnv>(env);
  ginst->updateTopology(topo);

  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0f;
  updata->_dt      = 0.0f;

  // E.6/2.19 — cacheable (STATIC) graphs: restore every disk-cached node up front
  // (host uploads); the evals below compute only the misses and the store pass runs
  // after the final sync. NEVER route through GraphInst::compute on a cacheable
  // graph (the core's synchronous cachedCompute branch is wrong for GPU graphs).
  std::set<dflow::DgModuleInst*> cook_loaded;
  if (graph->_cacheable)
    cook_loaded = _cookLoadPass(ginst);

  auto ci      = ctx->CI();
  auto evalOnce = [&]() {
    ctx->beginFrame();
    for (auto inst : ginst->_ordered_module_insts) { // PRE-phase: host-write runtime params from plugs
      if (cook_loaded.count(inst.get()))
        continue;
      if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst)) pp->writeParams(ctx);
    }
    ci->beginDispatchPhase();
    _computeSkipping(ginst, updata, cook_loaded);
    ci->endDispatchPhase();
    ctx->endFrame();
  };
  evalOnce();
  // topology-setup cascade: producers' GPU topology is now computed + synced, so modules that need it on
  // the CPU (subdivide read-back tables, merge CPU concat) build their output HERE; re-eval after EACH so
  // the next module reads its upstream's BUILT topology, and LOOP to a fixpoint so a chain that settles
  // over several evals converges instead of reading a half-wired input (e.g. a merge fed by a
  // select->assign_gid passthrough that wires its topology one eval late). The bound only backstops a
  // pathological always-defer; real chains settle in 2-3 passes. Cook-loaded nodes skip (output final).
  for (int pass = 0; pass < kTopoCascadeMax; pass++) {
    bool any = false;
    for (auto inst : ginst->_ordered_module_insts)
      if (auto mci = std::dynamic_pointer_cast<MeshComputeInst>(inst))
        if (not cook_loaded.count(inst.get()) and mci->onTopologyReady(ctx)) {
          any = true;
          evalOnce();
        }
    if (not any)
      break;
  }
  if (graph->_cacheable)
    _cookStorePass(ginst, cook_loaded); // after the final endDispatchPhase (submit+WAIT)

  gpumesh_ptr_t result;
  for (auto inst : ginst->_ordered_module_insts) {
    auto outp = inst->typedOutputNamed<MeshPlugTraits>("Out");
    if (outp and outp->_value and not outp->_value->_channels.empty())
      result = outp->_value; // last producing module = terminal
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// LIVE path — persistent GraphInst + per-frame recompute (see hmdflow.h).
///////////////////////////////////////////////////////////////////////////////

// FRAME-LESS: must be called from WITHIN a render frame (ComputeDrawable::onPreRender) — it manages
// only its dispatch phase, never beginFrame/endFrame (doing so cycles the per-frame depth/SSAO/RT
// buffers behind the renderer's back -> screen-space artifacts). writeParams is pre-phase (host map
// mid-phase is invisible); the dispatch reads them.
void LiveHypermesh::recompute(Context* ctx) {
  using clk = std::chrono::steady_clock;
  auto g0   = clk::now();
  for (auto inst : _ginst->_ordered_module_insts) {
    if (_cookLoaded.count(inst.get())) // E.6/2.19: disk-restored nodes are final
      continue;
    if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst)) { // family-neutral pre-phase
      auto t0 = clk::now();
      pp->writeParams(ctx);
      HmPerf::instance().addModule(
          inst->_dgmodule_data->GetClass()->Name().c_str(),
          std::chrono::duration<double>(clk::now() - t0).count());
    }
  }
  auto ci = ctx->CI();
  double gw0 = ci->_gpuWaitAccum;
  ci->beginDispatchPhase();
  // direct per-inst loop, NOT GraphInst::compute — a cacheable graph would take the
  // core's synchronous cachedCompute branch there (wrong for GPU graphs), and the
  // cook-loaded nodes must be skipped.
  _computeSkipping(_ginst, _updata, _cookLoaded);
  ci->endDispatchPhase();
  HmPerf::instance().addGraph(
      std::chrono::duration<double>(clk::now() - g0).count(),
      ci->_gpuWaitAccum - gw0,
      _mesh ? _mesh->_num_faces : 0,
      _mesh ? _mesh->_num_verts : 0);
  HmPerf::instance().report();
}

livehypermesh_ptr_t materializeLive(dflow::graphdata_ptr_t graph, Context* ctx, int vtx_budget) {
  auto live    = std::make_shared<LiveHypermesh>();
  live->_graph = graph;
  live->_dgctx = std::make_shared<dflow::dgcontext>();
  live->_dgctx->createRegisters<GpuMeshData>("hm_mesh", 64);
  live->_dgctx->createRegisters<dflowgfx::GpuComputeImage2DData>("hm_hfimage", 64); // interchange (B.3); terrain chains fan wide
  live->_dgctx->createRegisters<dflowgfx::InstanceSetData>("hm_instset", 16);       // interchange (E.2): the typed instance edge
  live->_dgctx->createRegisters<dflowgfx::SdfGridData>("hm_sdfgrid", 16);           // interchange (E.7): the SDF boolean chain
  live->_dgctx->createRegisters<::ork::hyper::XfNodeGraphData>("hm_xfng", 16);      // interchange (G0a): the XfNodeGraph transform-graph spine
  live->_dgctx->createRegisters<int>("hm_int", 256);
  live->_dgctx->createRegisters<float>("hm_float", 256);
  live->_dgctx->createRegisters<fvec2>("hm_vec2", 64);
  live->_dgctx->createRegisters<fvec3>("hm_vec3", 64);
  live->_dgctx->createRegisters<fvec4>("hm_vec4", 64);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), live->_dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);
  live->_ginst            = dflow::GraphData::createGraphInst(graph);
  _installPerfSink(live->_ginst);                // per-CLASS module counters (HmPerf)
  live->_env              = std::make_shared<MeshEnv>();
  live->_env->_ctx        = ctx;
  live->_env->_pool       = std::make_shared<MeshPool>(ctx);
  live->_env->_vtx_budget = vtx_budget;
  live->_ginst->_impl.setShared<MeshEnv>(live->_env);
  live->_ginst->updateTopology(topo);
  live->_updata           = std::make_shared<ui::UpdateData>();
  live->_updata->_abstime = 0.0f;
  live->_updata->_dt      = 0.0f;
  // E.6/2.19 — cacheable (STATIC) graphs: restore disk-cached nodes before the first
  // eval; recompute() skips them, the misses compute, the store pass runs post-cascade.
  if (graph->_cacheable)
    live->_cookLoaded = _cookLoadPass(live->_ginst);
  // ONE-TIME setup: alloc + compile + first eval. Historically called OUTSIDE a frame
  // (e.g. _onGpuInit) and wrapped in its own throwaway frame; the D.3 lazy bootstrap
  // (HypermeshDrawableData's first onGpuUpdate) lands here mid-frame, where the evals'
  // dispatch phases are already legal (the per-frame live hook runs them in-frame every
  // frame) and a nested beginFrame would assert — so wrap only when not already in one.
  bool own_frame = (ctx->_currentPhase == 0);
  if (own_frame) ctx->beginFrame();
  live->recompute(ctx);
  if (own_frame) ctx->endFrame();
  // topology-setup cascade: build each topology-readback module (subdivide / extrude) IN TOPO ORDER,
  // re-evaluating after EACH so the next module's onTopologyReady reads its upstream's FINAL (built)
  // topology. A single pass + one re-eval is insufficient for CHAINED topology ops (extrude -> extrude):
  // the downstream op would build its boundary tables against the upstream's passthrough (pre-built)
  // output, then compute against the real (different-sized) output -> GPU OOB / hang.
  for (int pass = 0; pass < kTopoCascadeMax; pass++) {
    bool any = false;
    for (auto inst : live->_ginst->_ordered_module_insts) {
      auto mci = std::dynamic_pointer_cast<MeshComputeInst>(inst);
      if (mci and not live->_cookLoaded.count(inst.get()) and mci->onTopologyReady(ctx)) {
        any = true;
        if (own_frame) ctx->beginFrame();
        live->recompute(ctx);
        if (own_frame) ctx->endFrame();
      }
    }
    if (not any) // fixpoint: a chain settling over several evals (merge <- select->assign_gid) converges
      break;
  }
  // E.6/2.19 — store the misses now that the cascade settled (outputs FINAL, last
  // dispatch phase submitted + waited -> inline readback valid).
  if (graph->_cacheable)
    _cookStorePass(live->_ginst, live->_cookLoaded);
  for (auto inst : live->_ginst->_ordered_module_insts) {
    auto outp = inst->typedOutputNamed<MeshPlugTraits>("Out");
    if (outp and outp->_value and not outp->_value->_channels.empty())
      live->_mesh = outp->_value;
    // E.2: the graph's InstanceSet terminal (a ScatterSource output) — last one wins
    auto iout = inst->typedOutputNamed<dflowgfx::InstanceSetPlugTraits>("Out");
    if (iout and iout->_value)
      live->_instances = iout->_value;
  }
  return live;
}

///////////////////////////////////////////////////////////////////////////////
// foundation gate (v2 indexed) — bake ripple / ripple->subdivide / box, read the indexed topology
// back and assert vert/corner/face counts (CPU + GPU header), the bbox, a unit normal, the CSR
// face_offsets terminal + in-range vidx, and the box's per-FACE material_id attribute.
///////////////////////////////////////////////////////////////////////////////

// read uint[count] from a pooled buffer.
static std::vector<uint32_t> _readU(FxInterface* fxi, FxShaderStorageBuffer* b, int count) {
  std::vector<uint32_t> v(count);
  auto m = fxi->mapStorageBuffer(b, 0, size_t(count) * 4, BufferMapAccess::READ_ONLY);
  std::memcpy(v.data(), m->_mappedaddr, size_t(count) * 4);
  fxi->unmapStorageBuffer(m.get());
  return v;
}

int hypermeshFoundationSelfTest(Context* ctx) {
  int fails = 0;
  auto fxi  = ctx->FXI();

  // ---- (A) RIPPLE: indexed grid. verts=(g+1)^2, faces=g^2*2 (tris), corners=faces*3. ----
  const int grid     = 96;
  const int nverts   = (grid + 1) * (grid + 1);
  const int nfaces   = grid * grid * 2;
  const int ncorners = nfaces * 3;
  {
    auto g    = std::make_shared<dflow::GraphData>();
    auto prim = RipplePrimitiveData::createShared();
    prim->typedInputNamed<dflow::IntPlugTraits>("grid")->setValue(grid); // exercise the INT PLUG
    dflow::GraphData::addModule(g, "prim", prim);
    auto mesh = bakeMesh(g, ctx);
    if (not mesh) { printf("[hypermesh selftest] FAIL: ripple bake returned no mesh\n"); return 1; }

    bool count_ok = mesh->_num_verts == nverts and mesh->_num_corners == ncorners and mesh->_num_faces == nfaces;
    auto h = _readU(fxi, mesh->_header, 3); // num_verts, num_corners, num_faces
    bool hdr_ok = h[0] == uint32_t(nverts) and h[1] == uint32_t(ncorners) and h[2] == uint32_t(nfaces);
    if (not count_ok or not hdr_ok) {
      printf("[hypermesh ripple] FAIL counts: cpu(v=%d c=%d f=%d) gpu(v=%u c=%u f=%u) want(v=%d c=%d f=%d)\n",
             mesh->_num_verts, mesh->_num_corners, mesh->_num_faces, h[0], h[1], h[2], nverts, ncorners, nfaces);
      fails++;
    }

    // positions over the UNIQUE verts: bbox ~[-4,4] in x/z (extent 8), |y| bounded; normals unit.
    auto pch = mesh->channel(MeshChannel::POSITION);
    auto nch = mesh->channel(MeshChannel::NORMAL);
    auto pm  = fxi->mapStorageBuffer(pch->_ssbo, 0, size_t(nverts) * 16, BufferMapAccess::READ_ONLY);
    auto nm  = fxi->mapStorageBuffer(nch->_ssbo, 0, size_t(nverts) * 16, BufferMapAccess::READ_ONLY);
    const float* P = (const float*)pm->_mappedaddr;
    const float* N = (const float*)nm->_mappedaddr;
    float minx = 1e30f, maxx = -1e30f, minz = 1e30f, maxz = -1e30f, maxay = 0.0f;
    double nlen_acc = 0.0;
    for (int i = 0; i < nverts; i++) {
      float x = P[i*4+0], y = P[i*4+1], z = P[i*4+2];
      minx = std::min(minx, x); maxx = std::max(maxx, x);
      minz = std::min(minz, z); maxz = std::max(maxz, z);
      maxay = std::max(maxay, std::fabs(y));
      nlen_acc += std::sqrt(N[i*4]*N[i*4] + N[i*4+1]*N[i*4+1] + N[i*4+2]*N[i*4+2]);
    }
    fxi->unmapStorageBuffer(pm.get());
    fxi->unmapStorageBuffer(nm.get());
    float nlen   = float(nlen_acc / double(nverts));
    bool bbox_ok = std::fabs(minx + 4.0f) < 0.1f and std::fabs(maxx - 4.0f) < 0.1f and
                   std::fabs(minz + 4.0f) < 0.1f and std::fabs(maxz - 4.0f) < 0.1f and maxay < 1.35f;
    bool nrm_ok  = std::fabs(nlen - 1.0f) < 1e-3f;

    // TOPOLOGY: CSR face_offsets [0,3,...,ncorners]; every vidx in [0,num_verts).
    auto fo  = _readU(fxi, mesh->_face_offsets->_ssbo, nfaces + 1);
    auto vi  = _readU(fxi, mesh->_vidx->_ssbo, ncorners);
    bool topo_ok = fo[0] == 0u and fo[nfaces] == uint32_t(ncorners) and fo[1] == 3u;
    uint32_t vmax = 0; for (auto x : vi) vmax = std::max(vmax, x);
    topo_ok = topo_ok and vmax < uint32_t(nverts);
    printf("[hypermesh ripple] v=%d c=%d f=%d bbox x[%.2f,%.2f] |N|=%.5f fo[end]=%u vmax=%u : %s\n",
           nverts, ncorners, nfaces, minx, maxx, nlen, fo[nfaces], vmax,
           (count_ok and hdr_ok and bbox_ok and nrm_ok and topo_ok) ? "PASS" : "FAIL");
    if (not bbox_ok) fails++;
    if (not nrm_ok)  fails++;
    if (not topo_ok) fails++;
  }

  // ---- (B) CHAIN: ripple(grid=8) -> subdivide(level=1 UNIFORM midpoint). all-tri: tri->4 tris, every
  //         edge halves -> V'=V+E, F'=4F, C'=3F'. grid g=8: V=81, F=128, E=208 (144 axis + 64 diag). ----
  {
    const int cg = 8;
    int v1 = 81 + 208, f1 = 4 * 128, c1 = f1 * 3;                 // 289, 512, 1536
    auto cgph  = std::make_shared<dflow::GraphData>();
    auto cprim = RipplePrimitiveData::createShared();
    cprim->typedInputNamed<dflow::IntPlugTraits>("grid")->setValue(cg);
    auto sub = SubdivideModuleData::createShared();
    sub->typedInputNamed<dflow::IntPlugTraits>("level")->setValue(1);
    dflow::GraphData::addModule(cgph, "prim", cprim);
    dflow::GraphData::addModule(cgph, "sub", sub);
    cgph->safeConnect(sub->inputNamed("In"), cprim->outputNamed("Out"));
    auto cmesh    = bakeMesh(cgph, ctx);
    bool chain_ok = cmesh and cmesh->_num_verts == v1 and cmesh->_num_corners == c1 and cmesh->_num_faces == f1;
    float nlen = 0.0f;
    if (cmesh) {
      auto nm = fxi->mapStorageBuffer(cmesh->channel(MeshChannel::NORMAL)->_ssbo, 0, size_t(v1) * 16, BufferMapAccess::READ_ONLY);
      const float* N = (const float*)nm->_mappedaddr;
      double acc = 0.0;
      for (int i = 0; i < v1; i++) acc += std::sqrt(N[i*4]*N[i*4] + N[i*4+1]*N[i*4+1] + N[i*4+2]*N[i*4+2]);
      fxi->unmapStorageBuffer(nm.get());
      nlen = float(acc / double(v1));
      chain_ok = chain_ok and std::fabs(nlen - 1.0f) < 1e-3f;
    }
    printf("[hypermesh chain]  ripple(8)->subdiv(1): v=%d c=%d f=%d (want %d/%d/%d) |N|=%.5f : %s\n",
           cmesh ? cmesh->_num_verts : -1, cmesh ? cmesh->_num_corners : -1, cmesh ? cmesh->_num_faces : -1,
           v1, c1, f1, nlen, chain_ok ? "PASS" : "FAIL");
    if (not chain_ok) fails++;
  }

  // ---- (C) BOX: 24 verts / 24 corners / 6 quad faces + per-FACE material_id (MAT[f]==f). ----
  {
    auto bg  = std::make_shared<dflow::GraphData>();
    auto box = BoxData::createShared();
    dflow::GraphData::addModule(bg, "box", box);
    auto bmesh = bakeMesh(bg, ctx);
    bool box_ok = bmesh and bmesh->_num_verts == 24 and bmesh->_num_corners == 24 and bmesh->_num_faces == 6;
    bool mat_ok = false;
    if (bmesh and bmesh->face("material_id")) {
      auto mat = _readU(fxi, bmesh->face("material_id")->_ssbo, 6);
      mat_ok = true;
      for (uint32_t f = 0; f < 6; f++) mat_ok = mat_ok and mat[f] == f;
      auto fo = _readU(fxi, bmesh->_face_offsets->_ssbo, 7);
      box_ok = box_ok and fo[0] == 0u and fo[1] == 4u and fo[6] == 24u; // all-quad CSR
    }
    printf("[hypermesh box]    v=%d c=%d f=%d (24/24/6) material_id[f]==f:%d : %s\n",
           bmesh ? bmesh->_num_verts : -1, bmesh ? bmesh->_num_corners : -1, bmesh ? bmesh->_num_faces : -1,
           mat_ok ? 1 : 0, (box_ok and mat_ok) ? "PASS" : "FAIL");
    if (not box_ok or not mat_ok) fails++;
  }

  // ---- (D) UVSPHERE (mixed quad/tri) + (E) ICOSPHERE (all-tri): counts + CSR + in-range vidx. ----
  auto checkMesh = [&](const char* tag, gpumesh_ptr_t m, int wv, int wc, int wf) {
    bool ok = m and m->_num_verts == wv and m->_num_corners == wc and m->_num_faces == wf;
    uint32_t vmax = 0, foEnd = 0;
    if (m) {
      auto vi = _readU(fxi, m->_vidx->_ssbo, wc);
      for (auto x : vi) vmax = std::max(vmax, x);
      auto fo = _readU(fxi, m->_face_offsets->_ssbo, wf + 1);
      foEnd = fo[wf];
      ok = ok and fo[0] == 0u and foEnd == uint32_t(wc) and vmax < uint32_t(wv);
    }
    printf("[hypermesh %s] v=%d c=%d f=%d (want %d/%d/%d) fo[end]=%u vmax=%u : %s\n", tag,
           m ? m->_num_verts : -1, m ? m->_num_corners : -1, m ? m->_num_faces : -1, wv, wc, wf,
           foEnd, vmax, ok ? "PASS" : "FAIL");
    if (not ok) fails++;
  };
  {
    int seg = 24, rings = 12;
    int wv = (rings - 1) * (seg + 1) + 2, wf = seg * rings, wc = seg * (4 * rings - 2);
    auto ug  = std::make_shared<dflow::GraphData>();
    auto usp = UvSphereData::createShared();
    usp->typedInputNamed<dflow::IntPlugTraits>("segments")->setValue(seg);
    usp->typedInputNamed<dflow::IntPlugTraits>("rings")->setValue(rings);
    dflow::GraphData::addModule(ug, "uvsphere", usp);
    checkMesh("uvsphere", bakeMesh(ug, ctx), wv, wc, wf);

    auto ig  = std::make_shared<dflow::GraphData>();
    auto isp = IcoSphereData::createShared();
    isp->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->setValue(0); // base icosahedron
    dflow::GraphData::addModule(ig, "icosphere", isp);
    checkMesh("icosphere", bakeMesh(ig, ctx), 12, 60, 20);
    // subdivisions=2 -> V=10*4^2+2=162, F=20*16=320, C=960 (projected midpoint icosphere).
    auto ig2  = std::make_shared<dflow::GraphData>();
    auto isp2 = IcoSphereData::createShared();
    isp2->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->setValue(2);
    dflow::GraphData::addModule(ig2, "icosphere", isp2);
    checkMesh("icosphere2", bakeMesh(ig2, ctx), 162, 960, 320);

    // CONE sides=24 (mixed tri + ngon): verts=1+2*24=49, faces=24 tri + 1 ngon=25, corners=24*4=96.
    int cs = 24;
    auto cog = std::make_shared<dflow::GraphData>();
    auto con = ConeData::createShared();
    con->typedInputNamed<dflow::IntPlugTraits>("sides")->setValue(cs);
    dflow::GraphData::addModule(cog, "cone", con);
    checkMesh("cone", bakeMesh(cog, ctx), 1 + 2 * cs, cs * 4, cs + 1);
  }

  // ---- (F) SELECTION -> EXTRUDE: icosphere(1) -> Select(+Y cap) -> ExtrudeFaces. The region boundary
  //         becomes wall quads: new_corners == orig_corners + 4*(new_faces - orig_faces). ----
  {
    auto eg  = std::make_shared<dflow::GraphData>();
    auto ico = IcoSphereData::createShared();
    ico->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->setValue(1); // 42v / 240c / 80f
    auto sel = SelectData::createShared();
    sel->_predicate  = "_sel = step(0.55, fN.y);"; // +Y-facing cap
    sel->_sel_or     = 1u;                          // replace(group(0)): matched -> set bit 0,
    sel->_unsel_and  = ~1u;                          //                    unmatched -> clear bit 0
    auto ext = ExtrudeFacesData::createShared();
    ext->_slot = 0;
    ext->typedInputNamed<dflow::FloatPlugTraits>("distance")->setValue(0.4f);
    dflow::GraphData::addModule(eg, "ico", ico);
    dflow::GraphData::addModule(eg, "sel", sel);
    dflow::GraphData::addModule(eg, "ext", ext);
    eg->safeConnect(sel->inputNamed("In"), ico->outputNamed("Out"));
    eg->safeConnect(ext->inputNamed("In"), sel->outputNamed("Out"));
    auto em = bakeMesh(eg, ctx);
    bool ext_ok = false;
    if (em) {
      int walls = em->_num_faces - 80;
      auto fo = _readU(fxi, em->_face_offsets->_ssbo, em->_num_faces + 1);
      ext_ok = walls > 0 and em->_num_corners == 240 + 4 * walls and fo[0] == 0u and
               fo[em->_num_faces] == uint32_t(em->_num_corners);
    }
    printf("[hypermesh extrude] ico(1)->select(+Y)->extrude: v=%d c=%d f=%d (walls=%d, c==240+4*walls) : %s\n",
           em ? em->_num_verts : -1, em ? em->_num_corners : -1, em ? em->_num_faces : -1,
           em ? em->_num_faces - 80 : -1, ext_ok ? "PASS" : "FAIL");
    if (not ext_ok) fails++;
  }

  // ---- (G) BIT-BANKING: icosphere(1) -> Select(+Y cap -> working bank 0) -> save bank0->bank1 ->
  //         clear bank0 (copy empty bank2) -> restore bank0<-bank1. The working band must round-trip:
  //         every face's working bits (t & 0x1F) == its bank-1 bits ((t>>5) & 0x1F), and the cap is
  //         non-empty. All banking params are runtime (no per-op shader recompile). ----
  {
    auto bg  = std::make_shared<dflow::GraphData>();
    auto ico = IcoSphereData::createShared();
    ico->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->setValue(1); // 80 faces
    auto sel = SelectData::createShared();
    sel->_predicate = "_sel = step(0.55, fN.y);"; // +Y cap -> working bit 0
    sel->_sel_or    = 1u;
    sel->_unsel_and = ~1u;
    auto save  = BitOpData::createShared(); save->_dst  = 5; save->_a = 0;  save->_op = 0;  // bank1 <- bank0
    auto clear = BitOpData::createShared(); clear->_dst = 0; clear->_a = 10; clear->_op = 0; // bank0 <- bank2(0)
    auto rest  = BitOpData::createShared(); rest->_dst  = 0; rest->_a = 5;  rest->_op = 0;  // bank0 <- bank1
    dflow::GraphData::addModule(bg, "ico", ico);
    dflow::GraphData::addModule(bg, "sel", sel);
    dflow::GraphData::addModule(bg, "save", save);
    dflow::GraphData::addModule(bg, "clear", clear);
    dflow::GraphData::addModule(bg, "rest", rest);
    bg->safeConnect(sel->inputNamed("In"),   ico->outputNamed("Out"));
    bg->safeConnect(save->inputNamed("In"),  sel->outputNamed("Out"));
    bg->safeConnect(clear->inputNamed("In"), save->outputNamed("Out"));
    bg->safeConnect(rest->inputNamed("In"),  clear->outputNamed("Out"));
    auto bm = bakeMesh(bg, ctx);
    bool bit_ok = false; int cap = 0;
    if (bm and bm->face("__tags")) {
      auto tg = _readU(fxi, bm->face("__tags")->_ssbo, bm->_num_faces);
      bool roundtrip = true;
      for (int f = 0; f < bm->_num_faces; f++) {
        uint32_t work = tg[f] & 0x1Fu, b1 = (tg[f] >> 5) & 0x1Fu;
        if (work != b1) roundtrip = false;
        if (work & 1u) cap++;
      }
      bit_ok = roundtrip and cap > 0;
    }
    printf("[hypermesh bitop]  ico(1)->sel->save->clear->restore: cap_faces=%d working==bank1 : %s\n",
           cap, bit_ok ? "PASS" : "FAIL");
    if (not bit_ok) fails++;
  }

  printf("[hypermesh selftest] %d failure(s)\n", fails);
  return fails;
}

} // namespace ork::lev2::hypermesh

///////////////////////////////////////////////////////////////////////////////
// plug template instantiations (mirrors terrain hfimg plugs — custom plug types need
// explicit describeX/createInstance specializations + reflection or the vtables don't link).
///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;
namespace hm    = ork::lev2::hypermesh;

template <> //
void hm::mesh_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void hm::mesh_inplugdata_t::describeX(class_t* clazz) {
}
template <> //
dflow::inpluginst_ptr_t hm::mesh_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<hm::mesh_inpluginst_t>(this, minst);
}
template <> //
dflow::outpluginst_ptr_t hm::mesh_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<hm::mesh_outpluginst_t>(this, minst);
}

ImplementTemplateReflectionX(hm::mesh_outplugdata_t, "hypermesh::meshoutplug");
ImplementTemplateReflectionX(hm::mesh_inplugdata_t, "hypermesh::meshinplug");
