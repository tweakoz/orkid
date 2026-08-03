////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// meshlet.h — CPU meshlet PARTITIONER for hypermesh content (mesh-shader bridge, build side).
//
// The GpuMesh is an indexed mixed-face polygon mesh; the mesh-shader pipeline wants TRIANGLE
// BUCKETS bounded by the taskless-tier caps (<=256 unique verts, <=256 prims per meshlet). This
// header owns the partitioning of a triangle soup into those buckets and NOTHING about drawing
// (no SSBO upload, no shader, no draw submission — those are separate deliverables).
//
// The four contracts that shape the API:
//
//  * ADJACENCY FROM INDICES ONLY. Clustering never looks at positions: a position hash welds
//    distinct vertices (Vector3::hash is 21-bit and aliases past ~+/-524m) and would silently
//    fuse unrelated shells. Two triangles are neighbours iff they share a VERTEX INDEX.
//
//  * PAIR INVARIANT. A partition is only meaningful against the exact triangle list it was built
//    from. MeshletPartition therefore OWNS its MeshletTopology snapshot, and a partition is only
//    obtainable whole — there is no API that hands a consumer a partition and a separately
//    chosen topology, so a mismatched bind cannot be expressed. Publication is a single
//    shared_ptr swap through MeshletChannel (double-buffered: readers keep the old pair alive
//    for as long as they hold it).
//
//  * APPEND FAST PATH. Growth that only APPENDS triangles (the new triangle list has the old one
//    as an exact prefix) reuses every previously built bucket verbatim and clusters only the new
//    tail. Anything else — a reordered/edited prefix — is real connectivity churn and rebuilds.
//    v1 does not re-open the prior partition's last partial bucket: every carried bucket is
//    byte-identical, which is what makes an incremental result auditable.
//
//  * MICROTASKABLE. MeshletBuilder is a resumable state machine driven by step(budget); the
//    GpuMicrotask wrapper (MeshletHost) trickles it under the scheduler's frame budget so a
//    large mesh never partitions in one burst.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ork::lev2::hypermesh {

struct MeshletTopology;
struct MeshletPartition;
struct MeshletBuilder;
struct MeshletChannel;
struct MeshletHost;
using meshlettopology_ptr_t  = std::shared_ptr<MeshletTopology>;
using meshletpartition_ptr_t = std::shared_ptr<MeshletPartition>;
using meshletbuilder_ptr_t   = std::shared_ptr<MeshletBuilder>;
using meshletchannel_ptr_t   = std::shared_ptr<MeshletChannel>;

// BUCKET CAPS — the taskless VK_EXT_mesh_shader tier allows 256/256, but on Metal a mesh
// threadgroup's declared output payload is threadgroup memory (32KB), and the ptex3d varying set
// costs ~176 bytes per vertex: at 256 vertices the pipeline fails to CREATE
// (VK_ERROR_INITIALIZATION_FAILED out of vkCreateGraphicsPipelines, on the first real mesh draw).
// Terrain's mesh path survives on the same device only because its meshlet is 81 verts / 128 prims.
// These MUST equal MESHLET_MAX_VERTS/PRIMS in dflow/hypermesh/gpu_meshlet.py, which DERIVES them
// from that budget (meshlet_caps) — a partition bucketed larger than the stage declares cannot be
// emitted. The codegen gate pins the two together.
// derived: 64*176 + 128*16 = 13312 bytes, INSIDE terrain's proven 16304-byte footprint.
#if defined(__APPLE__)
static constexpr uint32_t kMeshletMaxVerts = 64;
static constexpr uint32_t kMeshletMaxPrims = 128;
#else
static constexpr uint32_t kMeshletMaxVerts = 256;
static constexpr uint32_t kMeshletMaxPrims = 256;
#endif

///////////////////////////////////////////////////////////////////////////////
// MeshletDesc — one bucket, in the layout the descriptor SSBO will carry (4 uints, std430
// natural). _vertexOffset/_primOffset index the partition's flat vertex-list / prim-index
// arrays; counts are bounded by the caps above.
///////////////////////////////////////////////////////////////////////////////

struct MeshletDesc {
  uint32_t _vertexOffset = 0; // into MeshletPartition::vertexList() (global vertex ids)
  uint32_t _vertexCount  = 0; // <= kMeshletMaxVerts
  uint32_t _primOffset   = 0; // into MeshletPartition::primIndices(), in TRIANGLES
  uint32_t _primCount    = 0; // <= kMeshletMaxPrims
};

///////////////////////////////////////////////////////////////////////////////
// MeshletStats — partition quality, per mesh. Fill factors say how much of each bucket's cap
// budget is actually used (a low prim fill means the mesh-stage launches are mostly idle
// lanes); reuse says how many triangle corners each unique bucket vertex serves (1.0 = no
// sharing at all = the partitioner failed to find locality).
///////////////////////////////////////////////////////////////////////////////

struct MeshletStats {
  uint32_t _meshletCount   = 0;
  uint32_t _triCount       = 0;
  uint32_t _vertexRefCount = 0;   // sum of per-bucket vertex counts (the vertex-list length)
  float    _avgVertexFill  = 0.f; // _vertexRefCount / (count * kMeshletMaxVerts)
  float    _avgPrimFill    = 0.f; // _triCount       / (count * kMeshletMaxPrims)
  float    _vertexReuse    = 0.f; // (3 * _triCount) / _vertexRefCount
  std::string report(const std::string& label) const;
};

///////////////////////////////////////////////////////////////////////////////
// MeshletTopology — an IMMUTABLE triangle-list snapshot (flat uint32, 3 per triangle) plus the
// GpuMesh provenance it came from. This is the thing a partition is married to.
//
// fromMesh() fan-triangulates the mesh's CSR topology on the CPU (a READ_ONLY map of vidx +
// face_offsets; positions are never read). It deliberately does NOT consume the render
// triangulator's GPU index buffer: that buffer's triangle ORDER is produced by atomicAdd races
// across faces, so it is not reproducible frame to frame and cannot anchor a deterministic,
// append-comparable partition.
///////////////////////////////////////////////////////////////////////////////

struct MeshletTopology {

  // tri_indices must be a multiple of 3 and every index < num_verts (asserted — a partition
  // built off a truncated readback would silently drop geometry).
  static meshlettopology_ptr_t fromIndices(std::vector<uint32_t> tri_indices, int num_verts);
  // CPU snapshot of a live GpuMesh (honours _gpuResidentCount: live counts come from _header).
  static meshlettopology_ptr_t fromMesh(gpumesh_ptr_t mesh, Context* ctx);

  const std::vector<uint32_t>& triIndices() const { return _triIndices; }
  uint32_t numTris() const { return uint32_t(_triIndices.size() / 3); }
  int numVerts() const { return _numVerts; }
  uint64_t snapshotId() const { return _snapshotId; }
  uint64_t topoVersion() const { return _topoVersion; }

  // true when THIS snapshot is `prior` plus appended triangles (exact prefix match). The prefix
  // compare IS the churn detector: any edit inside the prefix fails it and forces a full rebuild.
  bool isAppendOf(const MeshletTopology& prior) const;

  std::vector<uint32_t> _triIndices; // 3 per triangle, global vertex ids
  int      _numVerts    = 0;
  uint64_t _snapshotId  = 0; // process-unique, monotonic (identity in logs/tests)
  uint64_t _topoVersion = 0; // source GpuMesh::_topoVersion (0 for synthetic topologies)
};

///////////////////////////////////////////////////////////////////////////////
// MeshletPartition — the buckets + the topology they describe. Sealed by the builder; nothing
// mutates it afterwards, so a published partition is safe to read from any thread that holds
// the shared_ptr.
///////////////////////////////////////////////////////////////////////////////

struct MeshletPartition {

  meshlettopology_ptr_t topology() const { return _topology; }
  const std::vector<MeshletDesc>& descriptors() const { return _desc; }
  const std::vector<uint32_t>& vertexList() const { return _vertexList; }
  // one uint per triangle: three 8-bit LOCAL vertex indices (bits 0..7, 8..15, 16..23) into the
  // owning bucket's slice of vertexList().
  const std::vector<uint32_t>& primIndices() const { return _primIndices; }
  // buckets carried verbatim from the prior partition (append fast path); 0 = full build.
  uint32_t immutableCount() const { return _immutableCount; }
  const MeshletStats& stats() const { return _stats; }

  meshlettopology_ptr_t _topology;
  std::vector<MeshletDesc> _desc;
  std::vector<uint32_t> _vertexList;
  std::vector<uint32_t> _primIndices;
  uint32_t _immutableCount = 0;
  MeshletStats _stats;
};

///////////////////////////////////////////////////////////////////////////////
// MeshletBuilder — resumable greedy adjacency clustering.
//
// The greedy rule (deterministic, therefore auditable): seed a bucket with the lowest-numbered
// unclustered triangle; then repeatedly take the candidate triangle with the most vertices
// ALREADY in the bucket (ties -> lowest triangle id), where candidates are the unclustered
// triangles incident to the bucket's vertices. A triangle that would overflow the vertex cap can
// never fit later (the vertex set only grows), so it leaves the frontier and re-seeds elsewhere.
// When the frontier runs dry the bucket does NOT close: the next unclustered triangle is seeded
// into the SAME bucket. A meshlet need not be connected, and organic content is many small
// adjacency islands (tube rings, branch segments, caps) — closing per island is what leaves every
// bucket a fraction full. ONLY the caps close a bucket.
//
// step(budget) advances at most `budget` triangle insertions (adjacency construction is charged
// at the same granularity), so a slice cost is bounded by the caller's budget, not the mesh.
///////////////////////////////////////////////////////////////////////////////

struct MeshletBuilder {

  // prior may be null. When non-null AND topo is an append of prior's topology, the prior's
  // buckets are carried verbatim and only the appended triangles are clustered.
  static meshletbuilder_ptr_t create(meshlettopology_ptr_t topo, meshletpartition_ptr_t prior);
  // convenience: create + drive to completion (bake / test / non-realtime callers).
  static meshletpartition_ptr_t buildComplete(meshlettopology_ptr_t topo, meshletpartition_ptr_t prior);

  bool step(uint32_t budget_tris); // true = more work remains
  bool done() const { return _phase == Phase::DONE; }
  float progress() const;
  bool isAppendPath() const { return _appendPath; }
  uint32_t startTri() const { return _startTri; } // first triangle this build clusters
  // the sealed result — null until done().
  meshletpartition_ptr_t partition() const { return _partition; }

  enum class Phase : int { ADJACENCY, CLUSTER, DONE };

  void _buildAdjacencyChunk(uint32_t budget);
  void _clusterChunk(uint32_t budget);
  void _closeBucket();
  void _seal();

  uint32_t _pickCandidate();
  uint32_t _freshVerts(uint32_t tri) const;
  void _insert(uint32_t tri);

  meshlettopology_ptr_t _topo;
  meshletpartition_ptr_t _prior;
  meshletpartition_ptr_t _partition;
  Phase _phase        = Phase::ADJACENCY;
  bool _appendPath    = false;
  uint32_t _startTri  = 0; // first triangle to cluster (== prior tri count on the append path)
  uint32_t _numTris   = 0;

  // adjacency CSR over the CLUSTERED RANGE only ([_startTri,_numTris)) — an append pays for its
  // tail, not for the whole mesh.
  std::vector<uint32_t> _adjOff;  // [numVerts+1]
  std::vector<uint32_t> _adjTri;  // triangle ids, grouped by vertex
  std::vector<uint32_t> _adjPos;  // per-vertex write cursor (fill stage)
  uint32_t _adjCursor = 0;        // counting/fill progress (triangles)
  int _adjStage       = 0;        // 0=count 1=fill

  // cluster state
  std::vector<uint8_t>  _used;       // per triangle: already in a bucket
  std::vector<uint32_t> _vertLocal;  // per vertex: local index within the CURRENT bucket
  std::vector<uint32_t> _vertStamp;  // per vertex: bucket generation _vertLocal is valid for
  std::vector<uint8_t>  _candScore;  // per triangle: bucket vertices it already shares
  std::vector<uint32_t> _candStamp;  // per triangle: bucket generation _candScore is valid for
  std::vector<uint32_t> _cand;       // frontier (triangle ids)
  std::vector<uint32_t> _curVerts;   // current bucket's global vertex ids
  std::vector<uint32_t> _curPrims;   // current bucket's packed local triples
  uint32_t _generation = 1;
  uint32_t _seedCursor = 0;
  uint32_t _assigned   = 0;

  // accumulating result (seeded from the prior partition on the append path, so new buckets'
  // offsets continue straight on from the carried ones).
  std::vector<MeshletDesc> _outDesc;
  std::vector<uint32_t> _outVertexList;
  std::vector<uint32_t> _outPrimIndices;
  uint32_t _immutableCount = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MeshletChannel — the double-buffered publication point (PAIR INVARIANT). The producer swaps a
// whole sealed partition in; a consumer takes the front pair and holds it (shared_ptr) for as
// long as it draws from it, so a rebuild landing mid-use retires the old pair only after the
// last reader drops it. There is no accessor that yields a topology without its partition.
///////////////////////////////////////////////////////////////////////////////

struct MeshletChannel {
  static meshletchannel_ptr_t create();
  meshletpartition_ptr_t front() const; // null until the first publish
  void publish(meshletpartition_ptr_t p);

  mutable std::mutex _mtx;
  meshletpartition_ptr_t _front;
};

///////////////////////////////////////////////////////////////////////////////
// MeshletHost — per-live-mesh driver hung off LiveHypermesh. Watches the SAME topology key the
// render triangulator uses (topoVersion + counts + the COW buffer handles), snapshots the mesh
// on a change, and trickles the rebuild through the context's GpuMicrotaskScheduler; the
// completed partition is published to the channel. A rebuild request supersedes (cancels) an
// in-flight one — a mesh that grows every frame never queues a backlog of stale builds.
//
// Gated by ORKID_HYPERMESH_MESHLETS (default off): until the mesh-shader draw path consumes the
// partition, the CPU snapshot readback would be pure cost.
///////////////////////////////////////////////////////////////////////////////

struct MeshletHost {
  static std::shared_ptr<MeshletHost> create(Context* ctx);
  ~MeshletHost();

  bool topoDirty(gpumesh_ptr_t mesh) const;
  void requestRebuild(gpumesh_ptr_t mesh); // snapshot + enqueue; acks the topology key
  meshletpartition_ptr_t partition() const { return _channel->front(); }

  // GPU MIRROR of one published partition — the three buffers the mesh stage reads
  // (sif_ml_desc / sif_ml_vtx / sif_ml_prim, see dflow/hypermesh/gpu_meshlet.py). Returns true when
  // the buffers hold EXACTLY `part`; the caller re-points its bindings afterwards because growth
  // re-creates a buffer. Uploading through the partition (never through the channel) is what keeps
  // the descriptor table, the vertex list and the prim indices from three different builds.
  bool uploadPartition(const meshletpartition_ptr_t& part);

  Context* _ctx = nullptr;
  meshletchannel_ptr_t _channel;
  gpumicrotask_ptr_t _inflight;
  FxShaderStorageBuffer* _descSSBO = nullptr; // {ml_count,pad,pad,pad} then 4 uints/meshlet
  FxShaderStorageBuffer* _vtxSSBO  = nullptr; // global vertex ids
  FxShaderStorageBuffer* _primSSBO = nullptr; // packed 3x8-bit local triples
  // PER-CLUSTER BOUNDS (8 floats/meshlet: sphere xyz+radius, then cone axis xyz+cutoff). The mesh
  // stage's cluster reject reads the SPHERE only; the cone is computed and stored against a future
  // backface test that needs an eye position the stage cannot currently derive per pass. Neither
  // half is EVER host-written: a host write to a host-visible buffer shadows
  // subsequent GPU compute writes under MoltenVK, and these are produced ENTIRELY on the GPU
  // (MeshletBounds, hmdflow_render.cpp) from the LIVE position channel. Allocated here only.
  //
  // They are GPU-derived rather than builder-derived on purpose: the partition is keyed on TOPOLOGY
  // and deliberately survives vertex motion (a re-pool that leaves connectivity alone keeps its
  // partition), so any bound baked at build time goes stale the moment positions move without a
  // topology bump — and a stale-small bound culls a VISIBLE cluster. Recomputing on the GPU next to
  // the triangulate keeps the bound married to the positions the mesh stage will actually emit.
  FxShaderStorageBuffer* _boundsSSBO = nullptr;
  // CLUSTER-REJECT COUNTERS (ORKID_HYPERMESH_MESHCULL_STATS): 2 monotonic uints, [0] tested and
  // [1] rejected, accumulated by the mesh stage across every pass that runs it. NEVER written by
  // the host — not even zeroed at creation. That is deliberate: a host write would shadow the GPU's
  // writes on MoltenVK, and since the consumer reports the DELTA between two reads, unsigned
  // wraparound makes the arithmetic correct from any starting value, garbage included.
  FxShaderStorageBuffer* _cullStatSSBO = nullptr;
  int _descCap = 0, _vtxCap = 0, _primCap = 0, _boundsCap = 0; // uint capacities (pow2-stepped)
  meshletpartition_ptr_t _uploaded;            // the partition the buffers currently hold
  uint64_t _builtTopoV   = ~0ull;
  int _builtNF           = -1;
  int _builtNC           = -1;
  const void* _builtVidx = nullptr;
  const void* _builtFO   = nullptr;
};

// Build meshlet partitions for live hypermeshes: ORKID_HYPERMESH_MESHLETS, or implied by the mesh
// draw path being on (a mesh draw with no partition has nothing to draw). Env, read once.
bool meshletsEnabled();

// ORKID_HYPERMESH_MESHSHADER as an INT: 0/absent = the pull-VS path, 1 = the mesh-shader draw
// (direct-sized: one workgroup per published meshlet). The SAME env gates the ptex3d codegen
// (dflow/hypermesh/gpu_meshlet.py meshmode), so a toggle-on run materializes a material that
// actually carries FWD_SSBO_CUSTOM_MESH. Env, read once.
int meshShaderMode();

// ORKID_HYPERMESH_MESHCULL_STATS — count and report per-cluster rejects. Gates BOTH the codegen
// (the mesh stage's atomics, dflow/hypermesh/gpu_meshlet.py meshcullstats) and the host-side
// readback, so with it off the shipped shader carries no counter code at all. Env, read once.
bool meshCullStatsEnabled();

} // namespace ork::lev2::hypermesh
