////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hm_meshlet.cpp — CPU meshlet partitioner (see meshlet.h for the four contracts).
//
////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/hypermesh/meshlet.h>
#include <ork/kernel/timer.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ork::lev2::hypermesh {

static constexpr uint32_t kNoTri = 0xFFFFFFFFu;

///////////////////////////////////////////////////////////////////////////////

std::string MeshletStats::report(const std::string& label) const {
  char buf[512];
  snprintf(
      buf,
      sizeof(buf),
      "meshlets<%s> buckets=%u tris=%u vrefs=%u vfill=%.3f pfill=%.3f reuse=%.3f",
      label.c_str(),
      _meshletCount,
      _triCount,
      _vertexRefCount,
      double(_avgVertexFill),
      double(_avgPrimFill),
      double(_vertexReuse));
  return std::string(buf);
}

///////////////////////////////////////////////////////////////////////////////
// MeshletTopology
///////////////////////////////////////////////////////////////////////////////

meshlettopology_ptr_t MeshletTopology::fromIndices(std::vector<uint32_t> tri_indices, int num_verts) {
  static std::atomic<uint64_t> s_nextId{1};
  OrkAssertI((tri_indices.size() % 3) == 0, "meshlet topology: index count is not a multiple of 3");
  OrkAssertI(num_verts >= 0, "meshlet topology: negative vertex count");
  for (auto idx : tri_indices)
    OrkAssertI(int(idx) < num_verts, "meshlet topology: triangle index out of range (truncated readback?)");
  auto topo         = std::make_shared<MeshletTopology>();
  topo->_triIndices = std::move(tri_indices);
  topo->_numVerts   = num_verts;
  topo->_snapshotId = s_nextId.fetch_add(1);
  return topo;
}

meshlettopology_ptr_t MeshletTopology::fromMesh(gpumesh_ptr_t mesh, Context* ctx) {
  OrkAssertI(mesh != nullptr, "meshlet topology: null mesh");
  OrkAssertI(ctx != nullptr, "meshlet topology: null context");
  auto fxi = ctx->FXI();
  int nv = mesh->_num_verts, nf = mesh->_num_faces, nc = mesh->_num_corners;
  // M2.5 parity with dump_obj: a GPU-resident-count mesh keeps CAPACITY in the CPU counts and the
  // LIVE counts in the header — partitioning the capacity tail would bucket degenerate geometry.
  if (mesh->_gpuResidentCount and mesh->_header) {
    auto hm_ = fxi->mapStorageBuffer(mesh->_header, 0, 12, BufferMapAccess::READ_ONLY);
    uint32_t hdr[3];
    std::memcpy(hdr, hm_->_mappedaddr, 12);
    fxi->unmapStorageBuffer(hm_.get());
    nv = int(hdr[0]);
    nc = int(hdr[1]);
    nf = int(hdr[2]);
  }
  auto rdu = [&](FxShaderStorageBuffer* b, int n) {
    std::vector<uint32_t> v(size_t(std::max(1, n)), 0u);
    if (b) {
      auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    return v;
  };
  auto vidx = rdu(mesh->_vidx ? mesh->_vidx->_ssbo : nullptr, nc);
  auto fo   = rdu(mesh->_face_offsets ? mesh->_face_offsets->_ssbo : nullptr, nf + 1);
  // CPU fan-triangulation, face order — the deterministic counterpart of the render triangulator
  // (whose per-face atomicAdd cursor makes its OUTPUT order irreproducible; see meshlet.h).
  std::vector<uint32_t> tris;
  tris.reserve(size_t(std::max(0, nc)) * 3);
  for (int f = 0; f < nf; f++) {
    uint32_t a = fo[f], b = fo[f + 1];
    OrkAssertI(b >= a, "meshlet topology: non-monotonic face_offsets (CSR corrupt)");
    uint32_t n = b - a;
    if (n < 3)
      continue;
    OrkAssertI(int(b) <= nc, "meshlet topology: face_offsets overruns the corner count");
    for (uint32_t k = 1; k + 1 < n; k++) {
      tris.push_back(vidx[a]);
      tris.push_back(vidx[a + k]);
      tris.push_back(vidx[a + k + 1]);
    }
  }
  auto topo          = fromIndices(std::move(tris), nv);
  topo->_topoVersion = mesh->_topoVersion;
  return topo;
}

bool MeshletTopology::isAppendOf(const MeshletTopology& prior) const {
  if (_numVerts < prior._numVerts)
    return false;
  if (_triIndices.size() < prior._triIndices.size())
    return false;
  if (prior._triIndices.empty())
    return true;
  return 0 == std::memcmp(_triIndices.data(), prior._triIndices.data(), prior._triIndices.size() * 4);
}

///////////////////////////////////////////////////////////////////////////////
// MeshletChannel
///////////////////////////////////////////////////////////////////////////////

meshletchannel_ptr_t MeshletChannel::create() {
  return std::make_shared<MeshletChannel>();
}

meshletpartition_ptr_t MeshletChannel::front() const {
  std::lock_guard<std::mutex> lock(_mtx);
  return _front;
}

void MeshletChannel::publish(meshletpartition_ptr_t p) {
  OrkAssertI(p != nullptr, "meshlet channel: publish of a null partition");
  OrkAssertI(p->_topology != nullptr, "meshlet channel: partition without its topology (pair broken)");
  std::lock_guard<std::mutex> lock(_mtx);
  _front = p; // the retiring pair stays alive in every reader that still holds it
}

///////////////////////////////////////////////////////////////////////////////
// MeshletBuilder
///////////////////////////////////////////////////////////////////////////////

meshletbuilder_ptr_t MeshletBuilder::create(meshlettopology_ptr_t topo, meshletpartition_ptr_t prior) {
  OrkAssertI(topo != nullptr, "meshlet builder: null topology");
  auto b      = std::make_shared<MeshletBuilder>();
  b->_topo    = topo;
  b->_numTris = topo->numTris();
  // APPEND FAST PATH: prefix-identical growth carries every prior bucket verbatim.
  if (prior and prior->_topology and topo->isAppendOf(*prior->_topology)) {
    b->_prior           = prior;
    b->_appendPath      = true;
    b->_startTri        = prior->_topology->numTris();
    b->_outDesc         = prior->_desc;
    b->_outVertexList   = prior->_vertexList;
    b->_outPrimIndices  = prior->_primIndices;
    b->_immutableCount  = uint32_t(prior->_desc.size());
  }
  int nv = topo->numVerts();
  b->_adjOff.assign(size_t(nv) + 1, 0u);
  b->_used.assign(b->_numTris, 0);
  for (uint32_t t = 0; t < b->_startTri; t++)
    b->_used[t] = 1; // carried triangles are never re-clustered nor offered as candidates
  b->_vertLocal.assign(size_t(nv), 0u);
  b->_vertStamp.assign(size_t(nv), 0u);
  b->_candScore.assign(b->_numTris, 0);
  b->_candStamp.assign(b->_numTris, 0u);
  b->_adjCursor = b->_startTri;
  b->_seedCursor = b->_startTri;
  return b;
}

meshletpartition_ptr_t MeshletBuilder::buildComplete(meshlettopology_ptr_t topo, meshletpartition_ptr_t prior) {
  auto b = create(topo, prior);
  while (b->step(1u << 20))
    ;
  return b->partition();
}

float MeshletBuilder::progress() const {
  if (_phase == Phase::DONE)
    return 1.0f;
  uint32_t total = _numTris - _startTri;
  if (0 == total)
    return 1.0f;
  if (_phase == Phase::ADJACENCY) {
    float per = float(_adjCursor - _startTri) / float(total);
    return 0.25f * (0.5f * float(_adjStage) + 0.5f * per);
  }
  return 0.25f + 0.75f * (float(_assigned) / float(total));
}

bool MeshletBuilder::step(uint32_t budget_tris) {
  if (_phase == Phase::DONE)
    return false;
  uint32_t budget = std::max(1u, budget_tris);
  if (_phase == Phase::ADJACENCY) {
    _buildAdjacencyChunk(budget);
    return _phase != Phase::DONE;
  }
  _clusterChunk(budget);
  return _phase != Phase::DONE;
}

// vertex->triangle CSR over the CLUSTERED RANGE only. Two passes (count, fill), both chunked by
// triangle; the O(numVerts) prefix scan between them runs whole (it is a scan of an int array,
// cheap next to the per-triangle work it indexes).
void MeshletBuilder::_buildAdjacencyChunk(uint32_t budget) {
  const auto& I = _topo->_triIndices;
  if (0 == _adjStage) {
    uint32_t end = std::min(_numTris, _adjCursor + budget);
    for (uint32_t t = _adjCursor; t < end; t++)
      for (int k = 0; k < 3; k++)
        _adjOff[size_t(I[t * 3 + k]) + 1]++;
    _adjCursor = end;
    if (_adjCursor >= _numTris) {
      for (size_t v = 1; v < _adjOff.size(); v++)
        _adjOff[v] += _adjOff[v - 1];
      _adjTri.assign(_adjOff.back(), 0u);
      _adjPos.assign(_adjOff.begin(), _adjOff.end() - 1);
      _adjStage  = 1;
      _adjCursor = _startTri;
    }
    return;
  }
  uint32_t end = std::min(_numTris, _adjCursor + budget);
  for (uint32_t t = _adjCursor; t < end; t++)
    for (int k = 0; k < 3; k++)
      _adjTri[_adjPos[I[t * 3 + k]]++] = t;
  _adjCursor = end;
  if (_adjCursor >= _numTris) {
    _adjPos.clear();
    _adjPos.shrink_to_fit();
    _phase = Phase::CLUSTER;
    if (_startTri >= _numTris) { // append with nothing appended: the carried buckets ARE the answer
      _seal();
    }
  }
}

// the frontier pick: most bucket vertices already shared, ties to the lowest triangle id. The scan
// compacts the frontier in place — a consumed triangle, or one that can no longer fit this bucket
// (the bucket's vertex set only grows), is dropped and will re-seed or re-join a later bucket.
// vertices this triangle would ADD to the current bucket (duplicate corners counted once).
uint32_t MeshletBuilder::_freshVerts(uint32_t tri) const {
  const auto& I = _topo->_triIndices;
  uint32_t v0 = I[tri * 3 + 0], v1 = I[tri * 3 + 1], v2 = I[tri * 3 + 2];
  uint32_t fresh = 0;
  if (_vertStamp[v0] != _generation)
    fresh++;
  if (_vertStamp[v1] != _generation and v1 != v0)
    fresh++;
  if (_vertStamp[v2] != _generation and v2 != v0 and v2 != v1)
    fresh++;
  return fresh;
}

uint32_t MeshletBuilder::_pickCandidate() {
  auto& cand        = _cand;
  uint32_t best     = kNoTri;
  int bestScore     = -1;
  size_t w          = 0;
  for (size_t r = 0; r < cand.size(); r++) {
    uint32_t t = cand[r];
    if (_used[t])
      continue;
    if (uint32_t(_curVerts.size()) + _freshVerts(t) > kMeshletMaxVerts)
      continue;
    cand[w++] = t;
    int s     = int(_candScore[t]);
    if (s > bestScore or (s == bestScore and t < best)) {
      bestScore = s;
      best      = t;
    }
  }
  cand.resize(w);
  return best;
}

void MeshletBuilder::_insert(uint32_t tri) {
  const auto& I = _topo->_triIndices;
  _used[tri]    = 1;
  _assigned++;
  uint32_t local[3];
  for (int k = 0; k < 3; k++) {
    uint32_t v = I[tri * 3 + k];
    if (_vertStamp[v] != _generation) {
      _vertStamp[v] = _generation;
      _vertLocal[v] = uint32_t(_curVerts.size());
      _curVerts.push_back(v);
      // a NEW bucket vertex raises the score of every unclustered triangle touching it.
      for (uint32_t i = _adjOff[v]; i < _adjOff[v + 1]; i++) {
        uint32_t t2 = _adjTri[i];
        if (_used[t2])
          continue;
        if (_candStamp[t2] != _generation) {
          _candStamp[t2] = _generation;
          _candScore[t2] = 0;
          _cand.push_back(t2);
        }
        _candScore[t2]++;
      }
    }
    local[k] = _vertLocal[v];
  }
  _curPrims.push_back(local[0] | (local[1] << 8) | (local[2] << 16));
}

void MeshletBuilder::_closeBucket() {
  if (_curPrims.empty())
    return;
  MeshletDesc d;
  d._vertexOffset = uint32_t(_outVertexList.size());
  d._vertexCount  = uint32_t(_curVerts.size());
  d._primOffset   = uint32_t(_outPrimIndices.size());
  d._primCount    = uint32_t(_curPrims.size());
  OrkAssertI(d._vertexCount <= kMeshletMaxVerts, "meshlet builder: vertex cap exceeded");
  OrkAssertI(d._primCount <= kMeshletMaxPrims, "meshlet builder: prim cap exceeded");
  _outDesc.push_back(d);
  _outVertexList.insert(_outVertexList.end(), _curVerts.begin(), _curVerts.end());
  _outPrimIndices.insert(_outPrimIndices.end(), _curPrims.begin(), _curPrims.end());
  _curVerts.clear();
  _curPrims.clear();
  _cand.clear();
  _generation++; // retires every _vertStamp/_candStamp from the closed bucket
}

void MeshletBuilder::_clusterChunk(uint32_t budget) {
  uint32_t total = _numTris - _startTri;
  while (budget > 0 and _assigned < total) {
    uint32_t pick = _curPrims.empty() ? kNoTri : _pickCandidate();
    if (kNoTri == pick) {
      // FRONTIER EXHAUSTED (or a fresh bucket): take the lowest-numbered unclustered triangle.
      // A meshlet does NOT have to be connected, and closing a bucket at every island boundary is
      // what wrecks fill on organic content — a swept tube or a branchy plant is hundreds of small
      // adjacency islands (rings, segments, caps), so one-island-per-bucket leaves every bucket a
      // fraction full. Seeding the next island into the SAME bucket keeps the caps as the only
      // reason a bucket ever closes. Lowest-id seeding keeps the whole build deterministic.
      while (_seedCursor < _numTris and _used[_seedCursor])
        _seedCursor++;
      if (_seedCursor >= _numTris)
        break;
      pick = _seedCursor;
      // an island seed shares no vertex with this bucket, so it costs its full vertex count; if
      // that no longer fits, the bucket is done (the vertex set only grows).
      if (not _curPrims.empty() and
          uint32_t(_curVerts.size()) + _freshVerts(pick) > kMeshletMaxVerts) {
        _closeBucket();
        continue;
      }
    }
    _insert(pick);
    budget--;
    if (uint32_t(_curVerts.size()) >= kMeshletMaxVerts or uint32_t(_curPrims.size()) >= kMeshletMaxPrims)
      _closeBucket();
  }
  if (_assigned >= total) {
    _closeBucket();
    _seal();
  }
}

void MeshletBuilder::_seal() {
  auto part              = std::make_shared<MeshletPartition>();
  part->_topology        = _topo; // PAIR INVARIANT: the partition carries what it was built from
  part->_desc            = std::move(_outDesc);
  part->_vertexList      = std::move(_outVertexList);
  part->_primIndices     = std::move(_outPrimIndices);
  part->_immutableCount  = _immutableCount;
  auto& st               = part->_stats;
  st._meshletCount       = uint32_t(part->_desc.size());
  st._triCount           = uint32_t(part->_primIndices.size());
  st._vertexRefCount     = uint32_t(part->_vertexList.size());
  if (st._meshletCount) {
    st._avgVertexFill = float(st._vertexRefCount) / float(st._meshletCount * kMeshletMaxVerts);
    st._avgPrimFill   = float(st._triCount) / float(st._meshletCount * kMeshletMaxPrims);
  }
  if (st._vertexRefCount)
    st._vertexReuse = float(3 * st._triCount) / float(st._vertexRefCount);
  OrkAssertI(st._triCount == _topo->numTris(), "meshlet builder: partitioned tri count != topology tri count");
  _partition = part;
  _phase     = Phase::DONE;
  // the working set dwarfs the result; a completed builder can sit in a microtask for a frame.
  auto drop = [](std::vector<uint32_t>& v) { v.clear(); v.shrink_to_fit(); };
  drop(_adjOff);
  drop(_adjTri);
  drop(_vertLocal);
  drop(_vertStamp);
  drop(_candStamp);
  drop(_cand);
  _used.clear();
  _used.shrink_to_fit();
  _candScore.clear();
  _candScore.shrink_to_fit();
}

///////////////////////////////////////////////////////////////////////////////
// MeshletHost + the microtask that trickles a build
///////////////////////////////////////////////////////////////////////////////

int meshShaderMode() {
  static const int s_mode = []() -> int {
    const char* e = getenv("ORKID_HYPERMESH_MESHSHADER");
    return e ? atoi(e) : 0;
  }();
  return s_mode;
}

bool meshCullStatsEnabled() {
  static const bool s_on = []() -> bool {
    const char* e = getenv("ORKID_HYPERMESH_MESHCULL_STATS");
    return e and (0 != strcmp(e, "0"));
  }();
  return s_on;
}

bool meshletsEnabled() {
  static const bool s_on = (getenv("ORKID_HYPERMESH_MESHLETS") != nullptr) or (meshShaderMode() >= 1);
  return s_on;
}

namespace {

struct MeshletBuildMicrotask final : public GpuMicrotask {

  MeshletBuildMicrotask(meshletbuilder_ptr_t builder, meshletchannel_ptr_t channel)
      : _builder(builder)
      , _channel(channel) {
    _name    = "HypermeshMeshletBuild";
    _costKey = "HypermeshMeshletBuild";
    _class   = MicrotaskClass::OPPORTUNISTIC; // nothing draws from it until it lands
  }

  int64_t sliceEstimateUs() const override {
    return _estUs;
  }
  float progress() const override {
    return _builder->progress();
  }

  bool runSlice(MicrotaskContext& mctx) override {
    // convert the granted microsecond budget into a triangle count via the measured per-triangle
    // cost. Floor of 256 keeps a starved budget from live-locking the build one triangle at a time.
    int64_t budget_us = (mctx._sliceBudgetUs > 0) ? mctx._sliceBudgetUs : 2000;
    uint32_t tris     = uint32_t(std::clamp<int64_t>(int64_t(double(budget_us) / _usPerTri), 256, 1 << 18));
    Timer timer;
    timer.Start();
    bool more  = _builder->step(tris);
    double us  = timer.SecsSinceStart() * 1.0e6;
    _usPerTri  = std::max(0.01, 0.7 * _usPerTri + 0.3 * (us / double(tris)));
    _estUs     = int64_t(_usPerTri * double(tris));
    if (not more) {
      auto part = _builder->partition();
      _channel->publish(part);
      printf("hypermesh %s\n", part->stats().report("live").c_str());
      return false;
    }
    return true;
  }

  meshletbuilder_ptr_t _builder;
  meshletchannel_ptr_t _channel;
  double _usPerTri = 1.0; // pessimistic seed; the first measured slice corrects it
  int64_t _estUs   = 2000;
};

} // namespace

std::shared_ptr<MeshletHost> MeshletHost::create(Context* ctx) {
  OrkAssertI(ctx != nullptr, "meshlet host: null context");
  auto host      = std::make_shared<MeshletHost>();
  host->_ctx     = ctx;
  host->_channel = MeshletChannel::create();
  return host;
}

MeshletHost::~MeshletHost() {
  if (_inflight and _ctx)
    _ctx->_microtaskScheduler.cancel(_inflight); // cooperative: never runs again
}

// the SAME key the render triangulator uses (hmdflow_render MeshRenderTri::topoDirty): version bump,
// counts, and the COW buffer handles — so a partition rebuild is triggered by exactly the events
// that re-triangulate.
bool MeshletHost::topoDirty(gpumesh_ptr_t mesh) const {
  return mesh->_topoVersion != _builtTopoV or mesh->_num_faces != _builtNF or mesh->_num_corners != _builtNC or
         (const void*)mesh->_vidx.get() != _builtVidx or (const void*)mesh->_face_offsets.get() != _builtFO;
}

void MeshletHost::requestRebuild(gpumesh_ptr_t mesh) {
  OrkAssertI(_ctx != nullptr, "meshlet host: rebuild without a context");
  if (_inflight) {
    _ctx->_microtaskScheduler.cancel(_inflight); // superseded — never publish a stale pair
    _inflight = nullptr;
  }
  auto topo    = MeshletTopology::fromMesh(mesh, _ctx);
  auto builder = MeshletBuilder::create(topo, _channel->front());
  auto task    = std::make_shared<MeshletBuildMicrotask>(builder, _channel);
  _inflight    = task;
  _ctx->_microtaskScheduler.enqueue(task);
  _builtTopoV = mesh->_topoVersion;
  _builtNF    = mesh->_num_faces;
  _builtNC    = mesh->_num_corners;
  _builtVidx  = mesh->_vidx.get();
  _builtFO    = mesh->_face_offsets.get();
}

bool MeshletHost::uploadPartition(const meshletpartition_ptr_t& part) {
  if (not part)
    return false;
  if (_uploaded == part)
    return true; // already resident (identity, not content — a partition is immutable once sealed)
  OrkAssertI(_ctx != nullptr, "meshlet host: upload without a context");
  auto fxi     = _ctx->FXI();
  auto& desc   = part->descriptors();
  auto& verts  = part->vertexList();
  auto& prims  = part->primIndices();
  uint32_t nml = uint32_t(desc.size());
  auto ensure  = [&](FxShaderStorageBuffer*& buf, int& cap, size_t need_uints) {
    int need = meshNextPow2(int(std::max<size_t>(1, need_uints)));
    if (need > cap) {
      buf = fxi->createStorageBuffer(size_t(need) * 4);
      cap = need;
    }
  };
  auto write = [&](FxShaderStorageBuffer* buf, const uint32_t* src, size_t count) {
    if (0 == count)
      return;
    auto m = fxi->mapStorageBuffer(buf, 0, count * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, src, count * 4);
    fxi->unmapStorageBuffer(m.get());
  };
  // header (live count) + the descriptor table, one contiguous block: the mesh stage reads
  // ml_count from slot 0 and MLDesc[] from slot 4 (std430 — the 4-uint header IS the alignment).
  std::vector<uint32_t> table(size_t(nml) * 4 + 4, 0u);
  table[0] = nml;
  for (uint32_t i = 0; i < nml; i++) {
    table[4 + i * 4 + 0] = desc[i]._vertexOffset;
    table[4 + i * 4 + 1] = desc[i]._vertexCount;
    table[4 + i * 4 + 2] = desc[i]._primOffset;
    table[4 + i * 4 + 3] = desc[i]._primCount;
  }
  ensure(_descSSBO, _descCap, table.size());
  ensure(_vtxSSBO, _vtxCap, verts.size());
  ensure(_primSSBO, _primCap, prims.size());
  // 8 floats/meshlet, ALLOCATED ONLY — the bounds are a GPU product (MeshletBounds). Writing them
  // here would both bake stale values and, on MoltenVK, shadow the compute pass that fills them.
  ensure(_boundsSSBO, _boundsCap, size_t(nml) * 8 + 8);
  if (not _cullStatSSBO and meshCullStatsEnabled())
    _cullStatSSBO = fxi->createStorageBuffer(4 * 4); // left UNINITIALIZED on purpose (see meshlet.h)
  write(_descSSBO, table.data(), table.size());
  write(_vtxSSBO, verts.data(), verts.size());
  write(_primSSBO, prims.data(), prims.size());
  _uploaded = part;
  return true;
}

} // namespace ork::lev2::hypermesh
