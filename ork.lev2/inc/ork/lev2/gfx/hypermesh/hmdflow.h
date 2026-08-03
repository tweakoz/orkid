////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// GPU mesh compute-dataflow (HyperSyn `hypermesh` family).
//
// A serializable ork::dataflow GraphData whose modules dispatch COMPUTE shaders that
// read/write a standardized GPU mesh (the GpuMesh: SoA channel SSBOs + a header). The
// SAME graph materializes two ways:
//   * BAKE    — run once, read channels back to an .ogeo / xgmmodel asset.
//   * LIVE    — re-run the GraphInst per frame; the output feeds a ComputeDrawable.
//
// DESIGN (Houdini-style, locked):
//   * The PLUG type is the whole mesh (GpuMesh) — one mesh-in / mesh-out per edge,
//     not per-channel wires (mirrors the PLAN's `MeshBuffer` plug).
//   * Channels are COPY-ON-WRITE refcounted GPU-buffer handles. A module allocates new
//     buffers only for the channels it WRITES and ALIASES (shares the handle for) the
//     channels it passes through. The shared_ptr refcount IS the COW refcount.
//   * The GraphInst owns an SSBO POOL (keyed by stride+capacity). Channel buffers are
//     drawn from + returned to it (on refcount->0), so re-runs (live, every frame) do
//     NOT realloc GPU memory. Reserved live-out sinks just hold a handle out of the pool.
//   * Modules are C++ (this library); composition is the Python `hypermesh` DSL. A new
//     graph needs no recompile; a new/changed module does.
//
// Channels are SoA, count-sized to a vertex CAPACITY budget; the LIVE count lives in the
// header (so the renderer reads the draw count straight off the GPU — no readback).
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/all.h>
#include <ork/math/cmatrix4.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/file/path.h>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <mutex>

namespace ork::lev2 {
struct ComputeDrawableData;
struct ComputeDrawable;
} // namespace ork::lev2

#include <ork/lev2/gfx/dflow/interchange.h> // family-neutral GPU-resource plug types (B.3)
#include <ork/lev2/gfx/hypermesh/lruleset.h> // GR1.a: the reflected LRuleSet grammar (LSystem _grammar)

namespace ork::lev2::hypermesh {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// Channels — the standardized SoA attribute set (the .ogeo attribute model on GPU).
// Vertex channels are vec4 (stride 16) for uniform std430 alignment in slice 1 (UV
// uses xy; a vec2 pool is a later optimization). INDEX is uint (stride 4). Skinning /
// user / exotic channels slot in additively (BONEIDX/BONEWT, then named customs).
///////////////////////////////////////////////////////////////////////////////

enum class MeshChannel : int {
  POSITION = 0, // vec4 (xyz, w=1)
  NORMAL,       // vec4
  BINORMAL,     // vec4 (tangent = cross(N,B))
  UV0,          // vec4 (xy used)
  COLOR,        // vec4 rgba
  INDEX,        // uint  (optional; indexed meshes)
  BONEIDX,      // uvec4 (skinning; vec4-sized)
  BONEWT,       // vec4  (skinning)
  COUNT
};

// bytes per element for a channel.
inline int meshChannelStride(MeshChannel c) {
  return (c == MeshChannel::INDEX) ? 4 : 16;
}
const char* meshChannelName(MeshChannel c);

// CHANNEL-TABLE CONVENTION (B.5b, locked): topology ops process vertex channels by ITERATING a
// channel table (the per-op `kCh[]` arrays — see extrude/bevel/mirror/compact), never by naming
// channels in op logic or shader text. New GPU ports (the C.4 Pivot-1 CPU->GPU ports and beyond)
// MUST follow this from day one: one channel-generic kernel dispatched per table entry (or a
// channel-count uniform loop), so adding BONEIDX/BONEWT/named customs later never re-touches an
// op. Channel-SPECIFIC math (e.g. normal renormalization, tag inheritance) hangs off the table
// entry as a mode flag, not off a hardcoded channel name.

///////////////////////////////////////////////////////////////////////////////
// GpuChannel — one pooled GPU buffer + its element stride/capacity + semantic. Held
// via shared_ptr (gpuchannel_ptr_t); the shared_ptr's deleter returns the SSBO to the
// owning pool when the last referencing GpuMesh drops it (COW refcount + recycle).
// NOTE: this v1 fixed-enum/non-indexed format is being replaced by the v2 indexed
// attribute mesh (see project_hypermesh_attribute_mesh.md) — that's the next major effort.
///////////////////////////////////////////////////////////////////////////////

struct GpuChannel {
  FxShaderStorageBuffer* _ssbo = nullptr; // pooled buffer (NOT owned here; the pool owns lifetime)
  int _stride                  = 16;      // bytes / element
  int _capacity                = 0;       // max elements
  MeshChannel _semantic        = MeshChannel::POSITION;
};
using gpuchannel_ptr_t = std::shared_ptr<GpuChannel>;

///////////////////////////////////////////////////////////////////////////////
// MeshPool — the GraphInst-owned SSBO pool. POW2 SIZE-CLASSES: a requested capacity is
// rounded UP to the next power of two, and the free-lists are keyed by (stride, pow2cap).
// This is what makes buffer sharing actually work — a 55,296-vert and a 60,000-vert mesh
// both round to 65,536 and reuse the SAME pooled buffer (exact-capacity keying would
// fragment into one-off buffers). The buffer holds the pow2 capacity; the LIVE count
// rides in the mesh header. acquire() pops a free buffer of the rounded class or creates
// one only if the list is empty; acquireChannel() wraps it in a gpuchannel_ptr_t whose
// deleter release()s it on last ref — so COW passthrough (handle-sharing) + recycling are
// automatic. The pool owns ALL buffers for teardown; it persists across GraphInst re-runs.
///////////////////////////////////////////////////////////////////////////////

inline int meshNextPow2(int v) { // round capacity up to the next power of two (size class)
  if (v < 1) return 1;
  int p = 1;
  while (p < v) p <<= 1;
  return p;
}

struct MeshPool : public std::enable_shared_from_this<MeshPool> {
  MeshPool(Context* ctx) : _ctx(ctx) {}
  ~MeshPool();

  // capacity is rounded up to the next pow2 internally; the returned buffer's element
  // count is that rounded class (>= requested).
  FxShaderStorageBuffer* acquire(int stride, int capacity);   // pop pow2 free-list or create
  void release(FxShaderStorageBuffer* b, int stride, int pow2cap);
  gpuchannel_ptr_t acquireChannel(MeshChannel sem, int capacity); // vertex channel (stride by semantic)
  gpuchannel_ptr_t acquireChannel(int stride, int capacity);      // raw (face attrs / topology); refcounted, auto-returns

  Context* _ctx = nullptr;
  std::map<std::pair<int, int>, std::vector<FxShaderStorageBuffer*>> _free; // (stride,pow2cap) -> free
  std::vector<FxShaderStorageBuffer*> _all;                                 // everything, for dtor
};
using meshpool_ptr_t = std::shared_ptr<MeshPool>;

///////////////////////////////////////////////////////////////////////////////
// GpuMeshData — the AUTHOR-TIME descriptor carried as the plug's data (serializable):
// which channels exist, prim type, indexed-or-not. The runtime value is the GpuMesh.
///////////////////////////////////////////////////////////////////////////////

struct GpuMeshData {
  bool _reserved = false;
};
using gpumesh_data_ptr_t = std::shared_ptr<GpuMeshData>;

///////////////////////////////////////////////////////////////////////////////
// GpuMesh (v2) — INDEXED attribute mesh. VERTEX-domain channels (P/N/B/uv/color, shared/unique verts)
// + TOPOLOGY (vidx: corner->vert, CSR face_offsets: face f = corners [off[f],off[f+1]), mixed
// tri/quad/ngon) + FACE-domain channels (per-primitive face attributes). The render fan-triangulates
// the faces (compute) -> tri vert-index buffer -> DrawIndexedIndirect; the VS reads vert[index].
// Channels are COW pooled handles (re-assigning drops the old -> back to its pow2 pool).
///////////////////////////////////////////////////////////////////////////////

struct GpuMesh {
  GpuMesh(gpumesh_data_ptr_t data) : _data(data) {}
  gpumesh_data_ptr_t _data;

  std::map<MeshChannel, gpuchannel_ptr_t> _channels; // VERTEX-domain attrs (P/N/B/uv/color); sized _num_verts
  std::map<std::string, gpuchannel_ptr_t> _vattrs;   // VERTEX-domain NAMED attrs (__tags from POINT select); sized _num_verts
  std::map<std::string, gpuchannel_ptr_t> _faces;    // FACE-domain attrs (material_id, ...); sized _num_faces
  gpuchannel_ptr_t _vidx;                            // uint[_num_corners]  corner -> vertex index (pooled, stride 4)
  gpuchannel_ptr_t _face_offsets;                    // uint[_num_faces+1]  CSR over corners (pooled, stride 4)
  FxShaderStorageBuffer* _header = nullptr;          // counts + bbox
  int _capacity    = 0;                              // vertex-channel pow2 capacity
  int _num_verts   = 0;                              // unique vertices
  int _num_corners = 0;                              // face-corners (= sum of face sizes)
  int _num_faces   = 0;                              // primitives (tri/quad/ngon, mixed)

  // ---- GPU-RESIDENT COUNT (M2.5: fully-GPU dynamic-topology meshes). When true, the LIVE face count
  //      is NOT on the CPU — it lives in _header (the producer writes it on the GPU each frame, no
  //      readback). _num_faces/_num_corners then hold the FIXED allocated CAPACITY (for dispatch
  //      sizing + index alloc), and the render triangulator reads the live count from _header
  //      (cs_importcount) to early-out — so it draws EXACTLY the live primitives with no CPU sync,
  //      no grow-only, no degenerate tail. Producers that re-size per frame off a GPU scan set this.
  bool _gpuResidentCount = false;

  // ---- DIRTY signaling. `_version` bumps on ANY re-emit of this mesh (topology OR positions/channels);
  //      `_topoVersion` bumps ONLY when the connectivity (vidx/face_offsets/counts) is re-emitted. A
  //      consumer recomputes whenever its input's _version advanced (dirty = dirty, reason-agnostic); it
  //      uses _topoVersion to decide HOW deep: advanced => full topology rebuild, else position-only refresh.
  //      A producing module bumps these via markChanged()/markTopoChanged() when it writes its output.
  uint64_t _version     = 0;
  uint64_t _topoVersion = 0;
  void markChanged()     { _version++; }
  void markTopoChanged() { _version++; _topoVersion++; }   // every topology change is also a general change

  // ---- EDGE domain (built on demand by edge Select via MeshEdges; consumed by bevel/bridge/weld). The
  //      table is the unique undirected edges; attrs (e.g. __tags) ride parallel to it. Sized to the
  //      _edge_cap UPPER BOUND (= the producing op's _num_corners); the LIVE edge count is GPU-side in
  //      _edge_count (consumers dispatch _edge_cap threads + early-out on it -> no readback).
  std::map<std::string, gpuchannel_ptr_t> _edges;    // EDGE-domain attrs (__tags, ...); sized _edge_cap (pooled)
  FxShaderStorageBuffer* _edge_table = nullptr;      // uint[_edge_cap*4]: va,vb,f0,f1 (f1=~0=boundary); aliases MeshEdges._edge
  FxShaderStorageBuffer* _edge_count = nullptr;      // MeshEdges._ectl {nc,nf,num_edges,_}: live count GPU-side (no readback)
  int _edge_cap    = 0;                              // edge upper bound (= corner count of the producing mesh)

  // ---- cached vertex->face CSR adjacency (lazily built via vtxFaceAdjacency(); shared by normals/
  //      smoothing/curvature/...). _adj_off[nv+1] CSR offsets, _adj_face[nc] face ids. Invalidated by
  //      keying on the (vidx, face_offsets) buffers it was built from -> rebuilt when topology changes.
  gpuchannel_ptr_t _adj_off, _adj_face;
  FxShaderStorageBuffer *_adj_keyV = nullptr, *_adj_keyF = nullptr;

  gpuchannel_ptr_t channel(MeshChannel c) const {
    auto it = _channels.find(c);
    return (it == _channels.end()) ? nullptr : it->second;
  }
  gpuchannel_ptr_t face(const std::string& n) const {
    auto it = _faces.find(n);
    return (it == _faces.end()) ? nullptr : it->second;
  }
  gpuchannel_ptr_t edge(const std::string& n) const {
    auto it = _edges.find(n);
    return (it == _edges.end()) ? nullptr : it->second;
  }
  gpuchannel_ptr_t vattr(const std::string& n) const {
    auto it = _vattrs.find(n);
    return (it == _vattrs.end()) ? nullptr : it->second;
  }
  bool has(MeshChannel c) const { return channel(c) != nullptr; }
};
using gpumesh_ptr_t = std::shared_ptr<GpuMesh>;

///////////////////////////////////////////////////////////////////////////////
// MeshPlugTraits — the plug type. _value (on the out-plug inst) is a GpuMesh; data_to_inst
// makes an empty GpuMesh from the descriptor (the producing module fills its channels).
///////////////////////////////////////////////////////////////////////////////

struct MeshPlugTraits {
  using elemental_data_type          = GpuMeshData;
  using elemental_inst_type          = GpuMesh;
  using data_impl_type_t             = GpuMeshData;
  using inst_impl_type_t             = GpuMesh;
  using xformer_t                    = dflow::nullpassthrudata;
  using range_type                   = no_range;
  using out_traits_t                 = MeshPlugTraits;
  static constexpr size_t max_fanout = 0; // a mesh may feed many consumers
  static gpumesh_ptr_t data_to_inst(gpumesh_data_ptr_t inp);
};

using mesh_inplugdata_t      = dflow::inplugdata<MeshPlugTraits>;
using mesh_outplugdata_t     = dflow::outplugdata<MeshPlugTraits>;
using mesh_inpluginst_t      = dflow::inpluginst<MeshPlugTraits>;
using mesh_outpluginst_t     = dflow::outpluginst<MeshPlugTraits>;
using mesh_inpluginst_ptr_t  = std::shared_ptr<mesh_inpluginst_t>;
using mesh_outpluginst_ptr_t = std::shared_ptr<mesh_outpluginst_t>;

///////////////////////////////////////////////////////////////////////////////
// MeshEnv — per-GraphInst environment (stashed on ginst->_impl). Holds the gfx Context,
// the SSBO pool (GraphInst-owned -> persists across re-runs), and the vertex budget.
///////////////////////////////////////////////////////////////////////////////

struct MeshEnv {
  Context* _ctx          = nullptr;
  meshpool_ptr_t _pool;
  int _vtx_budget        = 1 << 20; // default channel capacity (verts)
  // B.4 CLOCK FEED: the LIVE render hook advances these each frame (steady clock anchored at first
  // frame); module writeParams reads them (e.g. the extrude S.time slot). BAKE leaves them at 0.0 ->
  // a bake is the deterministic t=0 snapshot (the determinism oracle depends on this).
  double _abstime        = 0.0;
  double _dt             = 0.0;
};
using meshenv_ptr_t = std::shared_ptr<MeshEnv>;

///////////////////////////////////////////////////////////////////////////////
// Module base + compute-inst base
///////////////////////////////////////////////////////////////////////////////

struct MeshModuleData : public dflow::DgModuleData {
  DeclareAbstractX(MeshModuleData, dflow::DgModuleData);
  MeshModuleData();
  // EVERY mesh op self-defends: precheck asserts if the INPUT isn't a good mesh, postcheck if the OUTPUT isn't.
  // Default ON; disable-able (they do GPU<->CPU copies). The instance picks the actual checks via _doPreCheck /
  // _doPostCheck (template method); the base preCheck()/postCheck() gate them on these flags.
  bool _precheck  = true;
  bool _postcheck = true;
};
using meshmoduledata_ptr_t = std::shared_ptr<MeshModuleData>;

///////////////////////////////////////////////////////////////////////////////
// RipplePrimitive — generator. Out: GpuMesh. Emits a parametric primitive (slice 1:
// a sine-displaced grid) into freshly-acquired channels P/N/B/uv/color + header. Count
// bounded by params. baked scalar `_kind`; float plugs amp/freq/extent; int `_grid`.
///////////////////////////////////////////////////////////////////////////////

struct RipplePrimitiveData : public MeshModuleData {
  DeclareConcreteX(RipplePrimitiveData, MeshModuleData);
  RipplePrimitiveData();
  static std::shared_ptr<RipplePrimitiveData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _kind = 0; // 0 = grid (slice 1); sphere/torus/... later (baked: selects gen branch)
  // `grid` (resolution) is an INT INPUT PLUG (default 96), read at onActivate — connectable.
  std::vector<uint32_t> _mask; // optional whole-mesh tag MaskOp triple [and,or,xor]; empty -> untagged
};
using rippleprimitivemoduledata_ptr_t = std::shared_ptr<RipplePrimitiveData>;

///////////////////////////////////////////////////////////////////////////////
// BoxModule — generator. A cube as 6 QUAD faces (mixed-topology demo + face attrs): 24 verts
// (split per face for flat normals) + vidx (identity) + face_offsets (6 quads) + a per-FACE
// `material_id` channel. float plug `size` = half-extent.
///////////////////////////////////////////////////////////////////////////////

struct BoxData : public MeshModuleData {
  DeclareConcreteX(BoxData, MeshModuleData);
  BoxData();
  static std::shared_ptr<BoxData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<uint32_t> _mask; // optional whole-mesh tag MaskOp triple [and,or,xor]; empty -> untagged
};
using boxdata_ptr_t = std::shared_ptr<BoxData>;

///////////////////////////////////////////////////////////////////////////////
// SortTest — generator + regression harness for the MeshSort primitive (see the .cpp). Emits N verts
// whose XY encode a stably-sorted (key,payload) stream; dump_obj reads the result back for assertion.
///////////////////////////////////////////////////////////////////////////////
struct SortTestData : public MeshModuleData {
  DeclareConcreteX(SortTestData, MeshModuleData);
  SortTestData();
  static std::shared_ptr<SortTestData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _n = 252; // element count (collision-heavy: N >> 256 8-bit keys)
};
using sorttestdata_ptr_t = std::shared_ptr<SortTestData>;

///////////////////////////////////////////////////////////////////////////////
// EdgeTest — modifier + regression harness for MeshEdges (see the .cpp). Enumerates the input mesh's
// unique edges; emits one output vertex per edge encoding (va, vb, count) for dump_obj read-back.
///////////////////////////////////////////////////////////////////////////////
struct EdgeTestData : public MeshModuleData {
  DeclareConcreteX(EdgeTestData, MeshModuleData);
  EdgeTestData();
  static std::shared_ptr<EdgeTestData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using edgetestdata_ptr_t = std::shared_ptr<EdgeTestData>;

///////////////////////////////////////////////////////////////////////////////
// VertTest — modifier + regression harness for POINT (vertex) selection (see the .cpp). Emits one
// output vertex per input vertex encoding (vertid, group-0 selbit, 0) for dump_obj read-back.
///////////////////////////////////////////////////////////////////////////////
struct VertTestData : public MeshModuleData {
  DeclareConcreteX(VertTestData, MeshModuleData);
  VertTestData();
  static std::shared_ptr<VertTestData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using verttestdata_ptr_t = std::shared_ptr<VertTestData>;

///////////////////////////////////////////////////////////////////////////////
// UvSphereModule — generator. A latitude/longitude sphere with MIXED topology: QUAD body faces +
// TRIANGLE pole fans, in one indexed mesh (the mixed-CSR demo). float plug `radius`; int plugs
// `segments` (longitude) + `rings` (latitude), baked at activation (topology computed once).
///////////////////////////////////////////////////////////////////////////////

struct UvSphereData : public MeshModuleData {
  DeclareConcreteX(UvSphereData, MeshModuleData);
  UvSphereData();
  static std::shared_ptr<UvSphereData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<uint32_t> _mask; // optional whole-mesh tag MaskOp triple [and,or,xor]; empty -> untagged
};
using uvspheredata_ptr_t = std::shared_ptr<UvSphereData>;

///////////////////////////////////////////////////////////////////////////////
// IcoSphereModule — generator. An icosahedron (12 verts, 20 TRIANGLE faces) projected to a sphere;
// all-triangle (contrast to UvSphere). float plug `radius`. (Smooth midpoint-subdivision with
// sphere re-projection is a follow-up; chain SubdivideModule for a fan tessellation meanwhile.)
///////////////////////////////////////////////////////////////////////////////

struct IcoSphereData : public MeshModuleData {
  DeclareConcreteX(IcoSphereData, MeshModuleData);
  IcoSphereData();
  static std::shared_ptr<IcoSphereData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<uint32_t> _mask; // optional whole-mesh tag MaskOp triple [and,or,xor]; empty -> untagged
};
using icospheredata_ptr_t = std::shared_ptr<IcoSphereData>;

///////////////////////////////////////////////////////////////////////////////
// ConeModule — generator. MIXED tri + ngon: `sides` TRIANGLE side faces (apex fan) + ONE n-gon BASE
// cap (the render fan-triangulates it; subdivide turns it into n quads). The base rim is SPLIT (side
// ring w/ slope normals vs base ring w/ down normal) for a sharp rim. float plugs `radius`/`height`
// runtime; int plug `sides` baked (>=3 -> triangular pyramid up to a smooth cone).
///////////////////////////////////////////////////////////////////////////////

struct ConeData : public MeshModuleData {
  DeclareConcreteX(ConeData, MeshModuleData);
  ConeData();
  static std::shared_ptr<ConeData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<uint32_t> _mask; // optional whole-mesh tag MaskOp triple [and,or,xor]; empty -> untagged
};
using conedata_ptr_t = std::shared_ptr<ConeData>;

///////////////////////////////////////////////////////////////////////////////
// SubdivideModule — 1-in PER-FACE topology op (mixed tri/quad/ngon). One level fan-splits each
// face f (n corners) at its centroid -> n tris; `level` int plug applies it repeatedly. Produces
// new verts (centroids appended; non-shared midpoints for now), new vidx + face_offsets. Dynamic:
// re-pools buffers when the per-level counts change. Vert attrs (P/N/B/uv/color) interpolated.
///////////////////////////////////////////////////////////////////////////////

struct SubdivideModuleData : public MeshModuleData {
  DeclareConcreteX(SubdivideModuleData, MeshModuleData);
  SubdivideModuleData();
  static std::shared_ptr<SubdivideModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  // smooth=false: LINEAR midpoint refinement (tri->4tri, ngon->n-quad; the original behavior). smooth=true:
  // true CATMULL-CLARK — UNIFORM topology (every face -> n quads via a face point, incl. tri->3 quads) + CC
  // position rules (edge points pull to adjacent face centroids; verts move by the valence rule). TOPOLOGY.
  bool _smooth = false;
  // _slot >= 0: subdivide ONLY faces tagged with that __tags bit; unselected faces stay put but ABSORB the
  // boundary midpoints their selected neighbors introduce (-> n-gon, watertight). -1 = whole mesh. PROPERTY
  // (baked into the topology at onTopologyReady), like Normals/Inset — not a runtime plug.
  int _slot = -1;
};
using subdividemoduledata_ptr_t = std::shared_ptr<SubdivideModuleData>;

///////////////////////////////////////////////////////////////////////////////
// SelectModule — SELECTION as a TAG CHANNEL (Houdini-groups model). 1-in mesh -> mesh passthrough that
// evaluates a per-FACE predicate (the GLSL body `_predicate` from the Select DSL, reading face
// centroid/normal/area/id) and sets bit `_slot` of a uint32 `__tags` FACE channel (32 boolean named
// groups / domain). Selection rides the mesh, recomputed each frame (animates with the live mesh).
// Consumers (ExtrudeFaces) read `__tags & slot_mask`. Soft weights, if ever needed, are a separate
// optional float channel. POINT/EDGE domains + multi-Select bit-accumulation are follow-ups.
///////////////////////////////////////////////////////////////////////////////

// SERIALIZED-PREDICATE ABI VERSION. The persisted artifact for a SelExpr is its post-trace GLSL string
// (ratified: persist the trace OUTPUT, not the Python generator). That string references the op shader
// SHELL's locals (_sel, fP/fN/fArea/fUV, _tags/_ftags, EXPRP[slot], _segt/_segi, ...) — an ABI between the
// serialized text and the shell. BUMP this when any shell local is renamed/removed/retyped so an old
// serialized asset FAILS LOUDLY at load instead of mis-compiling. (Adding NEW locals is backward-compatible.)
static constexpr int kPredicateABIVersion = 1;

// B.4 PARAM CONTRACT: expression params are REAL vec4 INPUT PLUGS ("exprp0".."exprp{N-1}"), a FIXED set
// per module (the terrain kMaxExprInputs pattern: static plug layout -> no deserialize-ordering trap, no
// reshape-after-set plumbing; unused plugs idle at default). They serialize as plug values, are pokeable
// from Python between frames (the defined cross-thread channel: update thread writes via setters, the
// render thread snapshots them in writeParams), and are connectable by future oscillator/Time modules.
static constexpr int kMaxExprParams = 8;

struct SelectData : public MeshModuleData {
  DeclareConcreteX(SelectData, MeshModuleData);
  SelectData();
  static std::shared_ptr<SelectData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _predicate_abi = kPredicateABIVersion;   // see kPredicateABIVersion (checked at instance creation)
  std::string _predicate;        // GLSL assigning `float _sel` (from the SelExpr trace; constants baked)
  // E2.5 (Q6): the CANONICAL hypermesh.selexpr ExprIR TREE of the SAME predicate (exprir_selexpr
  // .capture_json). The GLSL above stays the EVAL form the op shader compiles; this tree is the
  // STORAGE-form IDENTITY (author-intent round-trip; cook hash keys off it). Empty on a legacy /
  // programmatically-built predicate (e.g. the C++ smoke fixtures) -> the GLSL remains the identity.
  std::string _predicate_tree;
  // the two-triple bit transform (MaskOp): matched faces get the sel-triple, unmatched the unsel-triple,
  //   tags = ((tags & AND) | OR) ^ XOR ; identity defaults (AND=~0, OR=0, XOR=0).
  uint32_t _sel_and   = 0xFFFFFFFFu, _sel_or   = 0u, _sel_xor   = 0u;
  uint32_t _unsel_and = 0xFFFFFFFFu, _unsel_or = 0u, _unsel_xor = 0u;
  int _domain = 0;               // 0 = POLY (POINT/LINE later)
};
using selectdata_ptr_t = std::shared_ptr<SelectData>;

///////////////////////////////////////////////////////////////////////////////
// ExtrudeFacesModule — the first selection CONSUMER. Reads the mesh's `__tags` FACE channel (bit
// `_slot` = the selected region) + a runtime `distance` float plug; lifts the selected faces along
// the vertex normal and bridges the region boundary with side-wall quads (REGION extrude). The
// boundary topology is precomputed ONCE on the CPU (onTopologyReady readback of vidx/face_offsets +
// the tags) and uploaded; per frame only the positions recompute (animate `distance` for free). New
// verts = nv + boundary-verts; new faces = nf + boundary-edges (walls); corners = nc + 4*boundary-edges.
// (Animated SELECTION -> hash-gated rebuild is the follow-up; v1 assumes a fixed selection.)
///////////////////////////////////////////////////////////////////////////////

struct ExtrudeFacesData : public MeshModuleData {
  DeclareConcreteX(ExtrudeFacesData, MeshModuleData);
  ExtrudeFacesData();
  static std::shared_ptr<ExtrudeFacesData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _slot = 0; // which __tags bit selects the region
  int _mode = 1; // 0=vertex (REGION: welded, lift along the VERTEX normal, walls only on the region boundary,
                 //   adjacent selected faces stay joined; walls are NON-planar). 1=face (INDIVIDUAL: each
                 //   selected face lifts along its OWN face normal, faces SEPARATE, walls on every edge and
                 //   PLANAR by construction -> winding trivially correct, no fold). default = face.
  // per-partition MaskOp triples [and,or,xor] in id order BASE(0), CAP(1), WALL(2); empty -> all
  // identity (the output inherits the input's __tags unchanged + walls start at 0). RUNTIME.
  std::vector<uint32_t> _part_masks;
  // FACE-mode per-face EXPRESSION fields (SelExpr-traced GLSL, POLY domain; read fP/fN/fArea/fUV/_tags).
  // Each assigns one local: dist->`_dist` (mult on the scalar `distance` plug, default 1), inset->`_inset`
  // (in-plane shrink toward centroid, default 0), dir->`_dir` (lift direction, default fN). Empty -> the
  // default. Evaluated per input face each frame on GPU (cs_field) -> animate for free, no readback.
  int _predicate_abi = kPredicateABIVersion;   // see kPredicateABIVersion (checked at instance creation)
  std::string _dist_pred, _inset_pred, _dir_pred;
  // E2.5 (Q6): the CANONICAL hypermesh.selexpr ExprIR TREE (exprir_selexpr.capture_json) of each
  // field expression — the STORAGE-form IDENTITY paired with the GLSL preds above (which stay the
  // EVAL form). Empty when the matching pred is unset. See SelectData::_predicate_tree.
  std::string _dist_tree, _inset_tree, _dir_tree;
  // MULTI-SEGMENT extrude (face mode): subdivide the lift into `_segments` rings (default 1 = single,
  // BYTE-IDENTICAL to the legacy path). With segments>1 each ring r=1.._segments evaluates the fields at
  // t=r/segments (S.t) / index r (S.seg) and stacks: `distance` accumulates per ring (the centroid PATH bends
  // when `direction` varies per-t -> droop), while `_twist_pred` (ABSOLUTE rotation in RADIANS about the
  // local extrusion axis) and `_scale_pred` (ABSOLUTE in-plane profile scale about the centroid) are the
  // ring's absolute cross-section transform. Subsumes a chained-extrude droop into ONE op. STRUCTURAL
  // (baked into topology) -> a change rebuilds. Empty preds -> twist 0 / scale 1.
  int _segments = 1;
  std::string _twist_pred, _scale_pred;
  std::string _twist_tree, _scale_tree;   // E2.5 (Q6) ExprIR tree of the twist/scale field exprs
  // keep_base (face mode): also emit each lifted face's original footprint as a FLOOR (outward-facing), so
  // extruding on a surface SHELL doesn't leave a see-through hole under the stud. default off. TOPOLOGY.
  bool _keep_base = false;
  // GENERIC runtime params for the expressions (DSL `param()` -> EXPRP[slot] in cs_field) are the
  // kMaxExprParams vec4 INPUT PLUGS "exprp0".."exprp7" (see kMaxExprParams) — serialized as plug values,
  // poked from Python between frames, uploaded to the EXPRP SSBO in writeParams. The extrude knows
  // nothing of param NAMES (they live in the DSL). `_time_slot` >= 0 marks the slot whose .x/.y the
  // module itself feeds from MeshEnv abstime/dt each frame (the S.time bridge — declarative time with
  // ZERO per-frame Python; serializes with the asset).
  int _time_slot = -1;
};
using extrudefacesdata_ptr_t = std::shared_ptr<ExtrudeFacesData>;

///////////////////////////////////////////////////////////////////////////////
// InsetModule — replace each selected face with an INSET: a smaller inner polygon (the same side count, or
// resampled to a regular `_sides`-gon) plus a COLLAR ring of faces bridging the boundary to it. The inset is
// COPLANAR (no lift), so winding is made bulletproof by flipping any collar tri whose normal opposes the face
// normal. `_fill` chooses whether the inner polygon is a face (an extrudable cap) or left open (a hole).
// `amount` (the inward distance) is a RUNTIME float plug -> animate with no recompile; `_sides`/`_fill` are
// TOPOLOGY (rebuild on change, like extrude's mode). Partition masks (collar/inner) ride the FaceTagger.
///////////////////////////////////////////////////////////////////////////////

struct InsetData : public MeshModuleData {
  DeclareConcreteX(InsetData, MeshModuleData);
  InsetData();
  static std::shared_ptr<InsetData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int   _slot  = 0;   // which __tags bit selects the faces to inset
  bool  _fill  = true;// true = inner polygon is a face (extrudable cap); false = leave it open (a hole)
  // `amount` (inward distance, 0=on boundary) and `rotate` (resampled-N-gon orientation, DEGREES about the
  // face normal from the +u axis) are RUNTIME float input plugs -> animate with no recompile (rotate spins
  // the ring in cs_inner; the collar topology is baked at rotate=0). `sides` is an int input plug (inner-ring
  // side count; 0 = keep the input face's count) -> changing it triggers a rebuild (it sets the topology count).
  // (precheck/postcheck inherited from MeshModuleData)
  bool  _cpu = false; // route: false = GPU (the DEFAULT since C.4c: GPU topology build + cs_copy/cs_inner
                      // positions); true = the pure-CPU reference (positions+channels on CPU, forces the
                      // CPU topology build too — the A.4 equivalence oracle's lever, retained forever).
  // partition MaskOp triples [and,or,xor] in id order OUTER(0, untouched faces), COLLAR(1), INNER(2);
  // empty -> identity. RUNTIME (no recompile on mask change).
  std::vector<uint32_t> _part_masks;
};
using insetdata_ptr_t = std::shared_ptr<InsetData>;

///////////////////////////////////////////////////////////////////////////////
// BevelModule — edge CHAMFER. Consumes an upstream LINE-Select's edge __tags (bit `_slot`) + the edge
// table on the GpuMesh edge domain; CPU-rebuilds the chamfer topology (offset corner-verts + chamfer
// quads + angularly-ordered corner caps), GPU-recomputes positions from baked BASE+TOWARD each frame.
// `amount` (the chamfer width) is a RUNTIME float plug. (See the .cpp.)
///////////////////////////////////////////////////////////////////////////////
struct BevelData : public MeshModuleData {
  DeclareConcreteX(BevelData, MeshModuleData);
  BevelData();
  static std::shared_ptr<BevelData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _slot = 0;                       // which edge __tags bit selects the edges to bevel
  std::vector<uint32_t> _part_masks;   // partition MaskOp triples FACE(0)/CHAMFER(1)/CAP(2); empty -> identity
};                                     // (precheck/postcheck inherited from MeshModuleData)
using beveldata_ptr_t = std::shared_ptr<BevelData>;

///////////////////////////////////////////////////////////////////////////////
// NormalsModule — recompute the NORMAL + BINORMAL channels per vertex from the CURRENT positions (so it
// tracks deformation every frame). `_smooth` = area-weighted average over the faces sharing each vertex
// (smooth shading; meaningful on SHARED meshes — vertex-mode bosses, the raw sphere); else = the vertex's
// face normal (flat; correct on UN-SHARED meshes — face-mode output). GATHER per vertex via a CPU-built
// vertex->face CSR adjacency (no float atomics). Binormal: a UV-aligned tangent T, then B = cross(T,N) so
// the material's `tangent = cross(N,B)` reconstructs T. Passthrough: aliases position/uv/color/topology.
///////////////////////////////////////////////////////////////////////////////

struct NormalsData : public MeshModuleData {
  DeclareConcreteX(NormalsData, MeshModuleData);
  NormalsData();
  static std::shared_ptr<NormalsData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  bool _smooth = true;  // true=area-weighted average (smooth); false=per-vertex face normal (flat)
  int  _slot   = -1;    // __tags bit to restrict to (-1 = whole mesh). Unselected faces pass through with
                        // their original normals; the selection boundary becomes a hard CREASE (selected
                        // verts split from unselected, averaged only over selected faces).
};
using normalsdata_ptr_t = std::shared_ptr<NormalsData>;

///////////////////////////////////////////////////////////////////////////////
// TransformModule — matrix TRANSFORM of a vertex selection (affine OR projective). GPU-NATIVE, NO readback:
// topology passes through (verts/faces unchanged); a per-frame GPU pass applies the matrix to the affected
// verts (full homogeneous P' = M·P with the perspective W-divide, so a projection works too) and recomputes
// N/B (via the inverse-transpose, derived CPU-side). `_matrix` is the full 4x4 (P' = M·P);
// it's a PARAM (uploaded as rows to an SSBO each frame) so it ANIMATES with zero shader recompile — the
// translate/rotate/scale/pivot composition lives in the Python DSL (axis-angle quats). `_slot` >= 0
// restricts to verts of faces tagged with that __tags bit (the vert mask is rebuilt on the GPU each frame
// from the live tags); -1 = whole mesh.
///////////////////////////////////////////////////////////////////////////////

struct TransformData : public MeshModuleData {
  DeclareConcreteX(TransformData, MeshModuleData);
  TransformData();
  static std::shared_ptr<TransformData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int    _slot = -1;     // restrict to this __tags bit's verts (-1 = whole mesh)
  fmtx4  _matrix;        // affine applied to affected verts: P' = _matrix · P (default identity)
};
using transformdata_ptr_t = std::shared_ptr<TransformData>;

///////////////////////////////////////////////////////////////////////////////
// DisplaceByFieldModule — the FIRST CROSS-FAMILY graph edge (HYPERECS E.1). Inputs: "In" (GpuMesh)
// + "Field" (the family-neutral GpuComputeImage2D interchange plug — produced by TERRAIN modules
// living in the SAME graph). Per UNIQUE vertex, bilinear-samples the field by the terrain planar
// convention (uv = P.xz/extent + 0.5, centered at the origin) and displaces the position along the
// vertex normal (_mode 0) or world +Y (_mode 1) by sample*amount. Topology + all other channels
// PASS THROUGH (alias); only POSITION is produced — chain smooth_normals/face_normals to refresh
// shading (the family convention; displace does not guess your shading). `amount`/`extent` are
// RUNTIME float plugs (animate with no recompile). `_field_dim` is the FIELD subgraph's bake
// resolution — carried HERE (the consumer that brings terrain modules into a mesh graph),
// NOT by the generic drivers: this module's onLink stocks the GraphInst's terrain BakeEnv
// (TypeKeyedVars — coexists with MeshEnv) before any terrain generator activates, so
// field-less hypermesh graphs and the drivers know nothing about fields.
///////////////////////////////////////////////////////////////////////////////

struct DisplaceByFieldData : public MeshModuleData {
  DeclareConcreteX(DisplaceByFieldData, MeshModuleData);
  DisplaceByFieldData();
  static std::shared_ptr<DisplaceByFieldData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _mode      = 0;   // 0 = along the vertex NORMAL; 1 = world +Y
  int _field_dim = 512; // bake resolution of the terrain FIELD subgraph feeding this module
};
using displacebyfielddata_ptr_t = std::shared_ptr<DisplaceByFieldData>;

///////////////////////////////////////////////////////////////////////////////
// DisplaceBySdf (M4b) — the cross-family SDF edge: displace a mesh's vertices by an
// SdfGrid FIELD (the DisplaceByField pattern, SDF flavor). The SDF field subgraph
// (SdfEval/Csg/Redistance) already rides the SAME MeshEnv, so — UNLIKE the terrain
// case — this module stocks NO foreign env. Per UNIQUE vertex it trilinear-samples
// phi + the trilinear gradient from the brick, then by `mode`:
//   conform (0): Gauss-Newton project onto the iso-surface (P -= phi*grad/|grad|^2),
//                ITERATED `conform_steps` times — shrinkwrap; needs a ~true SDF
//                (run .redistance() first; the |grad|^2 step tolerates non-unit).
//   offset  (1): P += amount * normalize(grad)            (inflate / shell)
//   scalar  (2): P += amount * (phi-phi_offset) * N       (the heightfield idiom)
// Topology + N/B/uv/color PASS THROUGH; only POSITION produced (chain recompute_tbn).
// amount/phi_offset are runtime plugs. NO field_dim — the SDF brick carries its own.
///////////////////////////////////////////////////////////////////////////////

struct DisplaceBySdfData : public MeshModuleData {
  DeclareConcreteX(DisplaceBySdfData, MeshModuleData);
  DisplaceBySdfData();
  static std::shared_ptr<DisplaceBySdfData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _mode          = 0; // 0 = conform (shrinkwrap), 1 = offset (along grad), 2 = scalar (phi*N)
  int _conform_steps = 3; // Gauss-Newton iterations for conform mode
  int _relax_steps   = 0; // conform-only: tangential Laplacian relax iterations (even distribution)
};
using displacebysdfdata_ptr_t = std::shared_ptr<DisplaceBySdfData>;

///////////////////////////////////////////////////////////////////////////////
// GpuComputeModule — a GENERIC per-vertex GPU compute op. Runs an arbitrary fxv2 compute kernel
// supplied as reflected DATA (`_shadertext` + `_kernel`), so new per-vertex GPU behaviors
// (ripple / twist / bend / noise-displace / ...) are authored as shader TEXT from the DSL with
// NO new C++ — the GpuCompute counterpart of terrain's ExprModule. 1 mesh-in, 1 mesh-out. The
// kernel binds: iP (input POSITION) @0, oP (produced POSITION) @1, EXPRP (8 vec4 runtime params,
// the kMaxExprParams plug set) @2, control { p_nv, p_mode, ... } @3. Topology + every OTHER channel
// PASSES THROUGH (alias); only POSITION is produced — chain recompute_tbn to refresh shading.
// `_dispatch_mode` 0 = one invocation per VERTEX. `_time_slot` >= 0 = the EXPRP slot the module
// feeds from MeshEnv abstime/dt (the S.time bridge). The reflected fields ARE the op identity —
// the base MeshComputeInst::cookComputeHash hashes them automatically (no per-inst hash). See the .cpp.
///////////////////////////////////////////////////////////////////////////////

struct GpuComputeModuleData : public MeshModuleData {
  DeclareConcreteX(GpuComputeModuleData, MeshModuleData);
  GpuComputeModuleData();
  static std::shared_ptr<GpuComputeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::string _shadertext;         // the authored fxv2 compute SHADER TEXT (the portable artifact)
  std::string _kernel = "cs_main"; // the compute_shader entry-point name within it
  int _dispatch_mode  = 0;         // 0 = one invocation per VERTEX (groups = (nv+63)/64)
  int _time_slot      = -1;        // EXPRP slot fed from MeshEnv abstime/dt (-1 = none); the S.time bridge
  // 8 generic vec4 runtime params live as INPUT PLUGS "exprp0".."exprp7" (the kMaxExprParams contract) —
  // serialized as plug values, poked from Python, snapshotted to the EXPRP SSBO each frame.
};
using gpucomputemoduledata_ptr_t = std::shared_ptr<GpuComputeModuleData>;

///////////////////////////////////////////////////////////////////////////////
// TemporalSmoothModule — inter-frame EMA on POSITION (+ NORMAL): a low-pass over TIME that damps
// cross-frame jitter on a deforming mesh. Holds a PERSISTENT previous-frame buffer in the Inst (it
// survives recompute() across frames, NOT a recycled pool channel). Per vertex:
//   out[v] = mix(prev[v], in[v], a);  prev[v] = out[v]   (a = weight of the NEW frame; small a = heavy)
// _mode 0 = `alpha` plug used directly (per-FRAME EMA; lag ~ (1-a)/a frames, framerate-dependent).
// _mode 1 = `tau` plug: a = 1 - exp(-dt/tau), a TIME-based EMA (framerate-INDEPENDENT, tau in seconds).
// REQUIRES STABLE vertex CORRESPONDENCE (vertex v = same logical point each frame): true after a
// conform on a FIXED base. It SELF-DEFENDS — on an nv change OR an input _topoVersion bump (e.g. the
// per-frame re-meshing of sdf_to_mesh) it RE-PRIMES (copies the input through, no blend) so it
// degrades to a safe pass-through instead of blending unrelated verts. Topology + uv/color alias.
///////////////////////////////////////////////////////////////////////////////

struct TemporalSmoothData : public MeshModuleData {
  DeclareConcreteX(TemporalSmoothData, MeshModuleData);
  TemporalSmoothData();
  static std::shared_ptr<TemporalSmoothData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _mode = 0; // 0 = per-frame alpha EMA; 1 = time-based tau EMA (a = 1-exp(-dt/tau))
};
using temporalsmoothdata_ptr_t = std::shared_ptr<TemporalSmoothData>;

///////////////////////////////////////////////////////////////////////////////
// DeleteFacesModule — drop the faces tagged with `_slot`. GPU-NATIVE via the shared count→scan→scatter
// (MeshScan): a per-face corner-count (survivor->size, deleted->0) is prefix-scanned into the output
// `face_offsets` CSR (deleted faces become ZERO-LENGTH -> the render's cs_tri skips n<3), then surviving
// corners are scattered into a packed vidx. NO readback: num_faces stays = input (dead slots tolerated; a
// later `compact` op repacks). Verts + all vertex/face channels pass through. `_slot` is the __tags bit;
// when it animates the faces appear/vanish live (the scan reruns each frame).
///////////////////////////////////////////////////////////////////////////////

struct DeleteFacesData : public MeshModuleData {
  DeclareConcreteX(DeleteFacesData, MeshModuleData);
  DeleteFacesData();
  static std::shared_ptr<DeleteFacesData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _slot = 0;   // delete faces tagged with this __tags bit
};
using deletefacesdata_ptr_t = std::shared_ptr<DeleteFacesData>;

///////////////////////////////////////////////////////////////////////////////
// MirrorModule — reflect the mesh across a principal plane (X/Y/Z = 0) and append the mirrored copy with
// REVERSED winding (so normals stay outward). GPU-NATIVE: per frame, originals copy + the off-plane verts
// get a reflected copy; faces double (mirror faces remap each corner to its mirror vert). `_weld` shares
// the verts ON the plane (|coord| < _eps) so the seam is a single shared loop (smooth normals flow across)
// instead of a coincident-vert crease — that compaction of the mirror-vert indices is the one place the
// shared MeshScan is used. Counts: faces/corners are a deterministic 2x (no scan); verts are <= 2x (welded
// seam removed). The model-half-then-mirror symmetry workflow (pair with delete_faces to cut a half first).
///////////////////////////////////////////////////////////////////////////////

struct MirrorData : public MeshModuleData {
  DeclareConcreteX(MirrorData, MeshModuleData);
  MirrorData();
  static std::shared_ptr<MirrorData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int   _axis = 0;       // 0=X, 1=Y, 2=Z plane (reflect that coordinate)
  bool  _weld = true;    // share verts on the plane (|coord|<_eps) -> one seam loop; else duplicate (crease)
  float _eps  = 1e-4f;   // on-plane tolerance
};
using mirrordata_ptr_t = std::shared_ptr<MirrorData>;

///////////////////////////////////////////////////////////////////////////////
// CompactModule — garbage-collect ORPHAN vertices (verts no face references — left by delete_faces and by
// mirror's 2x over-allocation) and remap `vidx`. GPU-native via the shared MeshScan: mark referenced verts
// -> scan -> the BASE doubles as the old->new vertex REMAP. Channels are packed to the kept verts; every
// corner's vidx is rewritten through the remap. Faces are untouched (face_offsets + all face attrs alias,
// so they stay valid; zero-length faces from delete remain, render-skipped). This is the one op that
// resolves the dynamic vert count -> it reads back a SINGLE uint (the new count) "when finished" (the
// agreed opt-in), via the writeParams-reads-last-frame pattern. (Face compaction + per-channel face-attr
// scatter is the follow-up; same primitive.)
///////////////////////////////////////////////////////////////////////////////

struct CompactData : public MeshModuleData {
  DeclareConcreteX(CompactData, MeshModuleData);
  CompactData();
  static std::shared_ptr<CompactData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using compactdata_ptr_t = std::shared_ptr<CompactData>;

///////////////////////////////////////////////////////////////////////////////
// ScatterSourceModule (HYPERECS E.2) — produces an InstanceSet (the typed instance edge)
// from a baked ScatterSet .ogeo: matrices SSBO (mat4[count], the baked xform channel) +
// attrs SSBO (vec4[count]: x = type_id, y = variant seed 0..1 — the typed replacement for
// the matrix-bottom-row smuggle). `_type_id` >= 0 filters to one type (each typed hypermesh
// node gets its own source; an InstanceFilter graph op generalizes later — review 3.1).
// The .ogeo resolves from `_ogeo_path` (direct, tools) or the DETERMINISTIC asset
// convention <assetcache>/terrain/<scatter_asset>/<sink>.ogeo (portable — what serialized
// scenes carry; the HF materializer placed it at load). Loads ONCE at onActivate; the
// artifact MUST exist there (ops self-defend: loud assert, never an empty silent set).
///////////////////////////////////////////////////////////////////////////////

struct ScatterSourceData : public MeshModuleData {
  DeclareConcreteX(ScatterSourceData, MeshModuleData);
  ScatterSourceData();
  static std::shared_ptr<ScatterSourceData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::string _scatter_asset; // HeightField asset name (the portable reference)
  std::string _sink;          // scatter sink name on that asset
  std::string _ogeo_path;     // direct path override (authoring/tools; takes precedence)
  int _type_id = -1;          // -1 = all types; >= 0 = only that type's instances
};
using scattersourcedata_ptr_t = std::shared_ptr<ScatterSourceData>;

///////////////////////////////////////////////////////////////////////////////
// LSystemModule (L-system family, M1/GR1) — PRODUCES the XfNodeGraph spine (ork::hyper).
// Runs the reflected LRuleSet grammar (rewrite + turtle-interpret, hmdflow_lruleset.cpp)
// at onActivate and fills one XfNode per branch joint. The grammar is MANDATORY (GR1.d):
// species are DATA — authored by the Python combinator DSL; the four stock growth models
// live as preset emitters in ork/hypergraph/dflow/lsystem/presets.py.
///////////////////////////////////////////////////////////////////////////////
struct LSystemModuleData : public MeshModuleData {
  DeclareConcreteX(LSystemModuleData, MeshModuleData);
  LSystemModuleData();
  static std::shared_ptr<LSystemModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  // the reflected grammar (GR1.a schema; REQUIRED at activation since GR1.d — a null
  // grammar fails loud in _buildSkeleton: fail-loud + regenerate, no legacy fallback).
  lruleset_ptr_t _grammar;
  int   _depth        = 7;      // recursion depth / trunk length (per archetype)
  int   _budget       = 4000;   // hard node cap (bake-cost bound)
  int   _children     = 2;      // branches per fork (or stems/whorl count per archetype)
  int   _internodes   = 1;      // segments per shoot (>1 = curved branches under tropism)
  int   _seed         = 1;      // deterministic stochastic seed
  float _seg_len      = 0.5f;   // base segment length
  float _base_radius  = 0.08f;  // trunk radius
  float _branch_angle = 35.0f;  // lateral pitch off the parent heading (degrees)
  float _roll         = 137.5f; // phyllotaxis divergence roll between successive shoots (degrees)
  float _len_decay    = 0.78f;  // length scale per generation
  float _rad_decay    = 0.72f;  // radius ratio per branch level (child/parent; "10-20% per level" = 0.8-0.9)
  float _taper        = 0.0f;   // along-shoot radius reduction (0 = cylindrical .. 1 = to a point)
  float _tropism      = 0.0f;   // gravitropism bend/internode toward +Y (>0 up, <0 droop), radians
  float _jitter       = 0.0f;   // 0..1 master stochastic-chaos amount (scales the channels below)
  float _apical       = 0.0f;   // 0..1 apical dominance (leader child continues straighter)
  // per-channel chaos weights (effective chaos = _jitter * _jit_X) — break up synthetic regularity
  float _jit_azimuth  = 0.8f;   // lateral azimuth spread (breaks radial symmetry)
  float _jit_pitch    = 0.35f;  // branch-pitch variation
  float _jit_length   = 0.5f;   // branch-length variation
  float _jit_spacing  = 0.3f;   // whorl / internode spacing variation
  float _jit_drop     = 0.25f;  // probability of dropping a lateral (asymmetric gaps)
  float _jit_wave     = 0.25f;  // per-internode heading waviness along a shoot
};
using lsystemmoduledata_ptr_t = std::shared_ptr<LSystemModuleData>;

// GR1.b — the grammar EVALUATOR (hmdflow_lruleset.cpp; pass 1 is ork::grammar in ork.core). Derives
// `grammar` (pass 1 rewrite + pass 2 turtle-interpret) into the XfNodeGraph node/slot buffers, reading `env`'s
// reflected scalars as the LExpr PARAM environment (A8: numbers flow through params, never folded into
// the grammar). PURE CPU / bake-time (boundary 5): the caller (LSystemModuleInst::_buildSkeleton) runs
// _buildFrames + the GPU upload afterward, so the determinism/budget gates can drive this headless.
void deriveLRuleSet(
    const LRuleSet*             grammar,
    const LSystemModuleData*    env,
    ::ork::hyper::xfnode_vect&  out_nodes,
    ::ork::hyper::xfslot_vect&  out_slots);

///////////////////////////////////////////////////////////////////////////////
// LSweepModule (L-system family, M1 = G0b) — the SKINNER. XfNodeGraph -> GpuMesh:
// one `_sides`-gon ring per node, `_sides` quads per parent->child edge (swept
// generalized cylinder). v1 builds on the CPU from the XfNodeGraph CPU mirror.
///////////////////////////////////////////////////////////////////////////////
struct LSweepModuleData : public MeshModuleData {
  DeclareConcreteX(LSweepModuleData, MeshModuleData);
  LSweepModuleData();
  static std::shared_ptr<LSweepModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int   _sides        = 6;    // ring resolution (tube cross-section n-gon)
  int   _cap_segments = 3;    // rounded end-cap sub-rings (0 = flat n-gon)
  float _cap_round    = 1.0f; // cap dome height as a fraction of ring radius (1 = hemisphere)
};
using lsweepmoduledata_ptr_t = std::shared_ptr<LSweepModuleData>;

///////////////////////////////////////////////////////////////////////////////
// LeafScatterModule (L-system family) — broadleaf ORGAN placer. Reads the SAME XfNodeGraph skeleton
// that LSweep skins, and emits a leaf-card GpuMesh: at high-generation nodes (twig tips) it places
// `_per_node` leaves by PHYLLOTAXIS (golden-angle `_roll` around the node heading, drooped `_pitch`
// from it), each a quad (`_style` 0) or a 2-quad cross (1). Separate leaf mesh (merged/instanced
// downstream). CPU build, like LSweep. UV0.xy = card uv (the MATERIAL owns texture vs procedural);
// COLOR.x = flutter weight (0 petiole .. 1 tip), COLOR.y = a per-leaf hash (hue/phase variation).
//
// `_source` picks WHERE the placements come from: NODES (0, the phyllotaxis default) walks the
// skeleton nodes, SLOTS (1, instance_at_slots) walks the XfNodeGraph's `_slots` side-table — the
// grammar's own SLOT ops (areoles, blooms, windows, gear mounts) — and places at each slot's WORLD
// frame (owning node xform * slot local). SLOTS is the grammar-authored placement: `_min_gen` does
// not apply (the grammar already chose the attachment points), `_per_node` reads as cards per slot.
///////////////////////////////////////////////////////////////////////////////
struct LeafScatterModuleData : public MeshModuleData {
  DeclareConcreteX(LeafScatterModuleData, MeshModuleData);
  LeafScatterModuleData();
  static std::shared_ptr<LeafScatterModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int   _style    = 0;      // 0 = single quad, 1 = 2-quad cross
  int   _source   = 0;      // 0 = NODES (phyllotaxis on eligible nodes), 1 = SLOTS (XfSlot attachment points)
  int   _per_node = 3;      // leaf cards placed per eligible node
  float _min_gen  = 4.0f;   // only nodes with _attrs[1] (generation) >= this bear leaves
  float _size     = 0.35f;  // leaf blade length
  float _aspect   = 0.6f;   // blade width / length
  float _roll     = 137.5f; // phyllotactic golden angle (deg) between successive leaves
  float _pitch    = 50.0f;  // leaf droop from the stem heading (deg)
  float _jitter   = 0.25f;  // 0..1 random pitch/roll/size jitter
  int   _seed     = 1;
};
using leafscattermoduledata_ptr_t = std::shared_ptr<LeafScatterModuleData>;

///////////////////////////////////////////////////////////////////////////////
// R-FAMILY (roads / streets / layout, v1) — Q3: the R. DSL namespace over modules
// living C++-side in the hypermesh family (the LSystem precedent). All terrain-
// coupled (spec §0): the layout is DERIVED from terrain field channels and fed
// BACK as flatten/keepout fields, in ONE Merkle graph (Q5 in-graph). The routing
// CORE (RouteSpine) is mirrored operation-for-operation by the pure-python
// reference obt.project/scripts/ork/hypergraph/dflow/roads/route_ref.py (the
// scatter.py<->hfdflow_scatter.cpp precedent; the analytic oracles pin them).
///////////////////////////////////////////////////////////////////////////////

// RouteSpineModule — least-cost routing on a DECLARED cost grid (layout_cell_m;
// dim-independent) of f(slope,curvature,discharge). Produces the XfNodeGraph
// spine FOREST (single-parent v1; cycles refused loudly). Consumes terrain HfImage
// field channels (Height/Slope/Curvature/Discharge) in the SAME graph.
struct RouteSpineModuleData : public MeshModuleData {
  DeclareConcreteX(RouteSpineModuleData, MeshModuleData);
  RouteSpineModuleData();
  static std::shared_ptr<RouteSpineModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<float> _pois;            // flat world (x,z) POI pairs (DATA; >=2 required)
  float _extent_m      = 1024.0f;      // world size across the field (natural units)
  float _layout_cell_m = 8.0f;         // DECLARED cost-grid cell size (dim-independent, Q8)
  int   _field_dim     = 512;          // terrain-field bake resolution stocked for the field subgraph
  float _width_m       = 6.0f;         // road width (rides XfNode _attrs.x)
  float _max_grade     = 0.12f;        // grade cap (over-grade edges forbidden -> slope oracle)
  float _w_slope       = 6.0f;         // steepness penalty weight
  float _w_curv        = 3.0f;         // ridge (convex-up) penalty weight
  float _w_water       = 1.0e4f;       // discharge>=thresh => ~forbidden (v1 routes AROUND water)
  float _disch_thresh  = 0.55f;        // normalized discharge => water
  float _grade_weight  = 4.0f;         // edge grade penalty multiplier
  float _base_cost     = 1.0f;
  int   _seed          = 1;
  float _min_radius_m  = 24.0f;        // curvature floor: the smoothed spine never turns tighter
  float _station_m     = 8.0f;         // resampled spine station spacing (m) after smoothing
  float _vcurve_len_m  = 120.0f;       // VERTICAL curve K-length: metres of arc to absorb a UNIT grade
                                       //   change (per-station grade-change ceiling = station/vcurve_len)
  float _clearance_m   = 0.3f;         // no-burial floor: min metres the deck rides above terrain
  // PHYSICS-PROXY LAW (owner 2026-07-22): non-empty => the built spine exports as a NAMED
  // BAKED ARTIFACT <assetcache>/roads/<export_name>/street_spine.ogeo (deck line P=(x,
  // road_elev, z) + width + parent channels). Colliders/nav/audio consume it BY NAME
  // (artifacts-not-graphs) — never the render mesh, never the live graph.
  std::string _export_name;
};
using routespinemoduledata_ptr_t = std::shared_ptr<RouteSpineModuleData>;

// RoadbedMaskModule — rasterize the spine polyline into terrain-currency HfImage
// fields at the BakeEnv (terrain) dim: Roadbed [0,1] coverage, RoadElev (grade-
// limited flatten target), and the road UV field (UvU across width [0,1], UvV
// world-metric arc-length / v_meters_per_tile). The GEOMETRY is declared in meters
// so the road is dim-independent; the output dim = bake dim so terrain MaskBlend
// (A=height,B=RoadElev,M=Roadbed) flattens with ZERO new terrain C++. Junction v1:
// nearest-segment parameterization (documented seam at forks — RoadMesh junction
// geometry is the next slice).
struct RoadbedMaskModuleData : public MeshModuleData {
  DeclareConcreteX(RoadbedMaskModuleData, MeshModuleData);
  RoadbedMaskModuleData();
  static std::shared_ptr<RoadbedMaskModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  float _width_m           = 6.0f;   // road surface width
  float _shoulder_m        = 2.0f;   // coverage falloff band beyond the half-width
  float _v_meters_per_tile = 8.0f;   // road UV V: meters of arc-length per texture tile
  float _extent_m          = 1024.0f;
  int   _out_dim           = 512;    // output field resolution (defaults to field bake dim)
};
using roadbedmaskmoduledata_ptr_t = std::shared_ptr<RoadbedMaskModuleData>;

// KeepoutMaskModule — Keepout = dilate(Roadbed>0) [UNION parcels]. Inverted into
// scatter-sink weights (no trees on roads) — spec §0 BACKWARD coupling.
struct KeepoutMaskModuleData : public MeshModuleData {
  DeclareConcreteX(KeepoutMaskModuleData, MeshModuleData);
  KeepoutMaskModuleData();
  static std::shared_ptr<KeepoutMaskModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  float _keepout_radius_m = 6.0f;    // dilation radius (world meters)
  float _extent_m         = 1024.0f;
};
using keepoutmaskmoduledata_ptr_t = std::shared_ptr<KeepoutMaskModuleData>;

// ParcelizeModule — frontage parcels along the spine (v1): walk each lane edge in
// frontage steps (counter-hash-jittered spacing), offset laterally on both sides.
// Produces an InstanceSet (OBB attrs: matrices = frame*scale(frontage,depth), attrs
// = (frontage,depth,side,_)) — the scatter-sink-compatible instance edge.
struct ParcelizeModuleData : public MeshModuleData {
  DeclareConcreteX(ParcelizeModuleData, MeshModuleData);
  ParcelizeModuleData();
  static std::shared_ptr<ParcelizeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  float _frontage_m  = 12.0f;
  float _depth_m     = 16.0f;
  float _spacing_m   = 2.0f;
  float _jitter      = 0.4f;
  float _extent_m    = 1024.0f;
  int   _seed        = 1;
};
using parcelizemoduledata_ptr_t = std::shared_ptr<ParcelizeModuleData>;

// BuildingSeedsModule — one seed per parcel (v1: frontage parcels ARE the building
// sites). Default EMIT ADAPTER = scatter-sink-compatible InstanceSet (owner Q2:
// "same as tree scatter") so gates run, PLUS an extensible freeform-SoA per-item
// schema (owner: "augment the per item data extensibly - freeform-SOA"):
//   InstanceSet.matrices  per-item placement (pos + facing-toward-spine + scale)
//   InstanceSet.attrs     x=type_id  y=variant_seed01  z=height_seed  w=style_seed
//   [SoA extension slots]  frontage_m, depth_m, orient_rad (built internally; the
//                          richer adapter — a new plug type — is the OWNER-decision
//                          seam, kept a small adapter swap: _emit_adapter selects).
// Consumers read the channels they know and IGNORE unknown ones; adding a channel
// is data-side only. A1: GidAssign stays the only MESH gid writer — type_id here is
// an INSTANCE attribute, not a mesh gid.
struct BuildingSeedsModuleData : public MeshModuleData {
  DeclareConcreteX(BuildingSeedsModuleData, MeshModuleData);
  BuildingSeedsModuleData();
  static std::shared_ptr<BuildingSeedsModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::vector<float> _type_weights;  // building-archetype weights (DATA; empty => single type)
  int   _emit_adapter = 0;           // 0 = scatter-sink InstanceSet (default); 1.. = richer (Q2 seam)
  int   _seed         = 1;
};
using buildingseedsmoduledata_ptr_t = std::shared_ptr<BuildingSeedsModuleData>;

// RoadMeshModule (R-family v2) — the SKINNER: XfNodeGraph spine FOREST -> a swept
// road-ribbon GpuMesh + JUNCTION patches + the gid material split (the LSweep
// skinner precedent, ribbon flavor). Each spine EDGE sweeps to ONE quad (sits on
// road_elev = XfNode _attrs.z; width from _attrs.x; U = normalized lateral [0,1],
// V = world-metric arc-length/v_meters_per_tile). Each FORK node (>= 2 children —
// the only junction kind in the single-parent forest) fills the gap between its
// setback-shortened approach mouths with ONE n-gon PATCH (the render's E.5 ear-clip
// triangulates it): its 2*degree boundary verts are COINCIDENT with the segment
// mouth rings, so meshvet's position-weld collapses the seam to a manifold interior
// edge (crack-free) while the patch keeps its OWN verts for a local planar UV chart.
// gid split: road segments -> _road_gid, junction patches -> _junction_gid (A1 [20:32)
// band; the parametric asphalt material binds by gid downstream). MIRRORS the pure-
// python reference obt.project/scripts/ork/hypergraph/dflow/roads/roadmesh_ref.py
// operation-for-operation (the parity gate pins them). Self-defends: zero-length
// edge / zero width / near-tangent fork REFUSE loudly; the apron setback AUTO-GROWS
// so a fork's stubs don't overlap; >4-way meets AUTO-SATISFY (a general n-gon).
struct RoadMeshModuleData : public MeshModuleData {
  DeclareConcreteX(RoadMeshModuleData, MeshModuleData);
  RoadMeshModuleData();
  static std::shared_ptr<RoadMeshModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  float _v_meters_per_tile      = 8.0f;   // road UV V: meters of arc-length per texture tile
  float _junction_setback_scale = 1.5f;   // apron depth = scale * road half-width (auto-grows)
  float _junction_min_edge_frac = 0.45f;  // setback clamp: <= this fraction of the shortest edge
  int   _road_gid               = 1;      // gid on swept ribbon segment faces (A1 band)
  int   _junction_gid           = 2;      // gid on junction patch faces
  float _extent_m               = 1024.0f;// world span across the Height field (grounding sample)
  float _shoulder_m             = 3.0f;   // v2.5: lateral width of the terrain-grounding skirt (m)
  int   _shoulder_gid           = 3;      // gid on the skirt (gravel/dirt material region)
  float _clearance_m            = 0.3f;   // v2.6: no-burial floor for the junction apron (m)
  float _max_bank_rad           = 0.10472f;// v2.6: superelevation cap (radians; ~6 deg, subtle)
  float _bank_runoff_m          = 30.0f;  // v2.6: arc length to ramp bank 0<->full (roll runoff)
  float _bank_ref_radius_m      = 24.0f;  // v2.6: turn radius at which FULL bank is reached
};
using roadmeshmoduledata_ptr_t = std::shared_ptr<RoadMeshModuleData>;

///////////////////////////////////////////////////////////////////////////////
// MergeMeshData — concatenate two input GpuMeshes (A, B) into one, tagging each source's faces with
// its own gid (A -> _gid_a, B -> _gid_b) so a multi-material BucketDraw routes them to distinct
// materials. The CPU concat (vertex/topology readback of both inputs, vidx/face_offset rebasing,
// __tags gid band) runs in onTopologyReady — both inputs must be host-readable. The canonical use is
// BAKING a leaf-card mesh INTO a tree trunk so the whole tree is ONE instanceable, single-cull mesh.
///////////////////////////////////////////////////////////////////////////////
struct MergeMeshData : public MeshModuleData {
  DeclareConcreteX(MergeMeshData, MeshModuleData);
  MergeMeshData();
  static std::shared_ptr<MergeMeshData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _gid_a = 0; // gid stamped on input A's faces (bark / trunk)
  int _gid_b = 1; // gid stamped on input B's faces (leaves / organs)
};
using mergemeshdata_ptr_t = std::shared_ptr<MergeMeshData>;

///////////////////////////////////////////////////////////////////////////////
// BitOpModule — __tags BIT-BANKING. mesh -> mesh passthrough that rewrites a width-bit destination
// band of the uint32 `__tags` channel from one or two source bands (per element):
//   res = a-band [ OP b-band ];  tags = (tags & ~dst) | (res << dst_offset)   (OP: copy/not/and/or/xor)
// Bands are `_width`-bit fields at bit offsets `_dst`/`_a`/`_b`. With the 5-bit bank convention
// (bank n = offset 5n; 0 = working/viz, 1.. = the 27 storage bits) this saves/restores/combines named
// selection masks. Domain v1 = POLY (the FACE __tags channel); requires an upstream Select (else no-op).
///////////////////////////////////////////////////////////////////////////////

struct BitOpData : public MeshModuleData {
  DeclareConcreteX(BitOpData, MeshModuleData);
  BitOpData();
  static std::shared_ptr<BitOpData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _dst = 0, _a = 0, _b = 0;  // destination / source-a / source-b band bit offsets
  int _op = 0;                   // 0=copy(a) 1=not(a) 2=and 3=or 4=xor
  int _width = 5;                // band width in bits
};
using bitopdata_ptr_t = std::shared_ptr<BitOpData>;

///////////////////////////////////////////////////////////////////////////////
// GidAssign (E.3) — THE ONLY VERB that writes the LOCKED gid band (__tags
// [20:32), the per-face material/semantic-class key; A1 tag contract). Mesh
// passthrough; sets gid on faces whose free-region selection bit `slot` is
// set (slot < 0 = ALL faces). Both params RUNTIME (ctl SSBO — no recompile).
///////////////////////////////////////////////////////////////////////////////

struct GidAssignData : public MeshModuleData {
  DeclareConcreteX(GidAssignData, MeshModuleData);
  GidAssignData();
  static std::shared_ptr<GidAssignData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _gid  = 0;   // 0..4095 (12-bit persistent gid)
  int _slot = -1;  // selection bit in the free region [0:20); -1 = all faces
};
using gidassigndata_ptr_t = std::shared_ptr<GidAssignData>;

///////////////////////////////////////////////////////////////////////////////
// SectionUnwrap (O3) — PER-SECTION UV unwrap for the baked ptex3d texture-ARRAY
// path. Mesh -> mesh CPU op that runs AFTER the gid partition (select+assign_gid):
// it reads the gid band of __tags, groups faces by gid (each distinct gid = one
// SECTION), and runs xatlas SEPARATELY per section so EVERY section gets its OWN
// full 0-1 UV domain (more texel budget than one shared atlas). The section's
// LAYER INDEX (dense 0..K-1 in sorted-gid order) is written into UV0.z for every
// one of that section's vertices (uniform per island -> interpolation-safe), so
// the forward material samples a sampler2DArray at layer = frg_uv0.z. Seam-
// duplicated verts (xatlas) copy their source vert's N/B/color; the per-face gid
// is preserved (output __tags face channel = gid<<20), and output faces are
// EMITTED GROUPED BY SECTION (contiguous ranges) so the bake driver can render one
// section at a time into its layer. No user-inlined constants (A8): pack padding
// + the layer-count safety cap are reflected props.
///////////////////////////////////////////////////////////////////////////////

struct SectionUnwrapData : public MeshModuleData {
  DeclareConcreteX(SectionUnwrapData, MeshModuleData);
  SectionUnwrapData();
  static std::shared_ptr<SectionUnwrapData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _padding    = 2;   // xatlas pack padding (texels between charts) — deterministic
  int _max_layers = 64;  // hard cap on distinct sections; exceeding it FAILS LOUD (no silent clamp)
};
using sectionunwrapdata_ptr_t = std::shared_ptr<SectionUnwrapData>;

///////////////////////////////////////////////////////////////////////////////
// MaterialParamSink (E.6/2.12) — drives a bound material's UBO param BY NAME
// (a generated ptex3d ctx.param) from the graph. Mesh passthrough (chain
// position is ergonomic only); the float "value" plug is a pokeable DATA plug.
// The DRAWABLE drains it per-frame (hm_drawable) and bindParam()s every
// resolved material (main + gid buckets) that declares the param — via the
// 2.12 rebind contract that reaches EVERY cached pipeline, so a STATIC mesh
// drives a live uniform WITHOUT per-frame geometry recompute.
///////////////////////////////////////////////////////////////////////////////

struct MaterialParamSinkData : public MeshModuleData {
  DeclareConcreteX(MaterialParamSinkData, MeshModuleData);
  MaterialParamSinkData();
  static std::shared_ptr<MaterialParamSinkData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::string _param_name; // the ctx.param name in the generated fxv2 (ublk_ptex_params member)
};
using materialparamsinkdata_ptr_t = std::shared_ptr<MaterialParamSinkData>;

///////////////////////////////////////////////////////////////////////////////
// Driver / tests
///////////////////////////////////////////////////////////////////////////////

// BAKE: sort + instantiate (with a fresh MeshEnv + pool) + run the graph's compute; the
// terminal mesh (the graph's last output) is returned for readback / asset export.
gpumesh_ptr_t bakeMesh(dflow::graphdata_ptr_t graph, Context* ctx, int vtx_budget = 1 << 20);

// E.6/2.19 — CONTENT-ONLY module identity for the cook cache: the module's reflected
// state (JSON) with the uuid envelopes STRIPPED. Uuids are durable per-object identity
// for git history (deserialize preserves them) — but the primary workflow re-authors
// graphs every run (scene.py -> tojson), minting fresh uuids, so cook identity must be
// derived purely from content. Model B makes this hash COMPLETE by construction: all
// authored state is reflected (the round-trip gates enforce it), so a new property can
// never silently dodge the hash.
uint64_t hypermeshModuleIdentityHash(const dflow::DgModuleData* mod);

// E.6/2.19 — cook stats of the most recent materialize/bake of a cacheable graph
// (for gates + telemetry): how many nodes loaded from the disk cache vs computed+stored.
int hypermeshLastCookHits();
int hypermeshLastCookStores();

///////////////////////////////////////////////////////////////////////////////
// LiveHypermesh — persistent GraphInst for the LIVE (animated) path. materializeLive builds it ONCE
// (sort + instantiate + pool; modules compile/alloc on the first recompute). recompute() re-evaluates:
// host-write each module's runtime params (from its uniform plugs, PRE dispatch phase) -> dispatch-phase
// ginst->compute(). The viewer calls recompute() each frame from onGpuUpdate (after animating a plug)
// and renders _mesh's now-updated channels. Pool stays warm (zero realloc per frame).
///////////////////////////////////////////////////////////////////////////////

struct MeshletHost; // meshlet.h — the CPU meshlet partitioner's per-mesh driver
using meshlethost_ptr_t = std::shared_ptr<MeshletHost>;

struct LiveHypermesh {
  dflow::graphdata_ptr_t _graph;
  dflow::dgcontext_ptr_t _dgctx;
  dflow::graphinst_ptr_t _ginst;
  meshenv_ptr_t _env;
  gpumesh_ptr_t _mesh;
  // E.6/2.19 cook cache — nodes restored from the disk cache at materialize
  // (cookLoad hit). recompute() skips them (their output is final; cacheable
  // graphs are STATIC by the DSL contract). Populated once by materializeLive.
  std::set<dflow::DgModuleInst*> _cookLoaded;
  // #33 LIVE-POKE EVICTION — the dflow plug-write clock snapshot taken right after cook-load.
  // recompute() evicts any still-cook-loaded node whose input-plug _writeEpoch exceeds this
  // (a poke AFTER materialize) plus everything downstream, so the DSL's cacheable=static
  // heuristic can't freeze a caller that pokes plugs. Advances as pokes are consumed.
  uint64_t _cookLoadEpoch = 0;
  // E.2: the graph's terminal InstanceSet (a ScatterSource output), discovered like _mesh;
  // null = not an instanced graph. setupMeshRender binds it (matrices + attrs SSBOs).
  dflowgfx::instanceset_inst_ptr_t _instances;
  ui::updatedata_ptr_t _updata;
  // PAUSE (B.4 contract: pause is a HOST concern — the host stops advancing the clock it feeds).
  // Per-graph flag; the GLOBAL flag (setClockPaused) gates ALL live graphs. While (either) paused the
  // clock simply stops ACCUMULATING (dt=0, abstime holds) — resume continues from the same instant,
  // no jump. The clock is incremental (_clock_last/_clock_abstime), so pause accounting is free.
  bool _paused          = false;
  double _clock_last    = -1.0; // last wall sample (-1 = not started)
  double _clock_abstime = 0.0;  // accumulated UNPAUSED seconds == the graph's time
  // MESHLETS — the CPU meshlet partition of this mesh, rebuilt off the same topology key the render
  // triangulator uses. Created lazily by the live render hook, and ONLY under ORKID_HYPERMESH_MESHLETS
  // (until the mesh-shader draw path consumes it the snapshot readback buys nothing). Null = no
  // partition has ever been requested for this mesh.
  meshlethost_ptr_t _meshlets;
  void recompute(Context* ctx);
};
using livehypermesh_ptr_t = std::shared_ptr<LiveHypermesh>;

// GLOBAL dflow-clock pause (e.g. the viewer's [SPACE]): freezes time for EVERY live hypermesh graph.
// Per-graph pause = LiveHypermesh::_paused. (When the core Time modules land (plan 1.2c), the same
// host-side rule applies: an ECS host pauses by not advancing the UpdateData it feeds.)
void setClockPaused(bool paused);
bool clockPaused();

///////////////////////////////////////////////////////////////////////////////
// HmPerf — process-wide hypermesh performance counters, reported every 5s on the yellow
// HYPERMESH log channel. Per-module times are HOST WALL of that module's work (writeParams +
// compute() recording, rebuilds, host maps), accumulated per CLASS (all instances pooled); the
// full-graph time is the whole eval (writeParams loop + graph compute + triangulate, INCLUDING
// the blocking dispatch-phase GPU wait) — so module-sum << graph time means GPU-bound,
// module-sum ~= graph time means host-bound. evals/sec counts full-graph evaluations (the C.1b
// dirty gate means a paused/static mesh contributes ZERO — that is the gate working).
///////////////////////////////////////////////////////////////////////////////
struct HmPerf {
  static HmPerf& instance();
  void addModule(const char* clazz, double secs); // accumulate one module call (per-CLASS)
  void addGraph(double secs, double gpu_secs, int polys, int verts, int instances = 1); // one FULL eval
                                                  // (+ output size; drawn = polys * instances)
  void report();                                  // rate-limited: logs once per 5s window, then resets
  std::string statsText();                        // the LAST window's formatted block (HUD display)
  std::mutex _mtx;                                // live hook (render thread) + manual recompute (main)
  std::map<std::string, std::pair<double, uint64_t>> _byClass; // class -> (total secs, calls)
  double _graphSecs    = 0.0;
  double _gpuSecs      = 0.0;                     // of _graphSecs: host blocked on the GPU fence
  uint64_t _polySum    = 0;                       // output polys (terminal mesh faces) summed over evals
  uint64_t _vertSum    = 0;
  uint64_t _drawnSum   = 0;                       // polys * instance count (what actually draws)
  uint64_t _graphEvals = 0;
  double _windowStart  = -1.0;
  std::string _lastReport;                        // retained at report() time (same content as logged)
};

livehypermesh_ptr_t materializeLive(dflow::graphdata_ptr_t graph, Context* ctx, int vtx_budget = 1 << 20);

///////////////////////////////////////////////////////////////////////////////
// RoadsBakeReadout (R-family Tier-B gate seam) — read back the baked R-graph
// products from a materialized `live` for the coupling oracles (determinism /
// keepout-zero / flatten-match). Navigates by module-DATA class name + output
// plug type; empty vectors where a module is absent. Impl in
// hmdflow_module_routespine.cpp (sees the interchange plug types).
///////////////////////////////////////////////////////////////////////////////
struct RoadsBakeReadout {
  int                   spine_count = 0;
  std::vector<float>    spine_positions;   // xyz per node (3*count)
  std::vector<uint32_t> spine_parents;     // per node
  std::vector<float>    spine_road_elev;   // _attrs.z per node
  std::string           spine_bytes;       // raw _nodes bytes (determinism memcmp key)
  int                   field_dim = 0;
  std::vector<float>    height_field;      // the field wired to RouteSpine.Height (flatten ref)
  std::vector<float>    slope_field;       // RouteSpine.Slope input (reference parity)
  std::vector<float>    curv_field;        // RouteSpine.Curvature input
  std::vector<float>    disch_field;       // RouteSpine.Discharge input
  std::vector<float>    roadbed;           // RoadbedMask.Roadbed coverage
  std::vector<float>    road_elev_field;   // RoadbedMask.RoadElev flatten target
  std::vector<float>    keepout;           // KeepoutMask.Keepout
  int                   seed_count = 0;    // BuildingSeeds InstanceSet count
};
RoadsBakeReadout roadsBakeReadout(livehypermesh_ptr_t live, Context* ctx);

///////////////////////////////////////////////////////////////////////////////
// RENDER — install the render-time triangulator + per-frame in-frame hook on a ComputeDrawableData.
// Compiles the (raw-FXI) fan-triangulate compute ONCE; per frame (in onPreRender) it: optionally
// re-evaluates the live graph (animated), fan-triangulates the mesh's faces (mixed tri/quad/ngon)
// into a GPU tri vert-index buffer + a VkDrawIndexedIndirectCommand (all on-GPU, no readback), and
// refreshes the drawable's vertex-channel bindings + index/args to the mesh's CURRENT pooled buffers
// (dynamic topology). The drawable then DrawIndexedIndirect's straight off those GPU buffers.
//   animated=false -> static mesh (still triangulated each frame from its fixed buffers).
//   faceViz=true   -> also maintain a per-triangle source-face-id buffer for the face-viz FS; RETURNS
//                     that buffer (bind it to the FS sif_triface block) — else returns null.
//   tagViz=true    -> additionally refresh graphics-storage slot 6 to the mesh's CURRENT __tags FACE
//                     channel each frame (the selection-group viz FS reads it by tri->face id). Implies
//                     faceViz (the FS needs the per-triangle face id to index __tags).
// wireframe=true -> additionally build a per-edge LINE index buffer (+ its own indirect command) each
//   frame and configure the drawable's OVERLAY draw (index/args) to it; the caller sets the overlay
//   MATERIAL + overlay graphics storage (P at 0, N at 1) in python. The hook refreshes both.
// INSTANCING (E.2, the typed edge): if the live graph carries an InstanceSet (live->_instances,
// from a ScatterSource), it is the instance source — matrices + attrs SSBOs bind as-is.
// LEGACY: instanceCount>1 + instanceMatrices (mat4-major float[instanceCount*16]) still works —
// the buffers are created here, and the matrix BOTTOM ROWS are split out into a synthesized
// attrs SSBO (rows zeroed on upload): the instanced VS now ALWAYS reads per-instance data from
// storage_inst_attr (vec4[gl_InstanceIndex] -> frg_clr) — the bottom-row smuggle is RETIRED.
// Returns {faceid, instMtx, instAttr} (nulls where not applicable); bind instMtx to
// storage_inst_mtx and instAttr to storage_inst_attr LAST (after the viz slots) in python.
struct MeshRenderBuffers {
  FxShaderStorageBuffer* _faceid   = nullptr;
  FxShaderStorageBuffer* _instMtx  = nullptr;  // the cull's interleaved OUT_M (tier 0 at offset 0)
  FxShaderStorageBuffer* _instAttr = nullptr;
  // cascade-cull fix: the SUN-SHADOW cull's compacted OUT_M/OUT_A (single tier, all sun-visible). The
  // caller re-binds these on storage_inst_mtx/attr for sun-cascade depth passes (its own material's
  // block handles), so the shadow passes draw the union-sun survivor set instead of the eye set. null
  // when the cull is disabled.
  FxShaderStorageBuffer* _instMtxShadow  = nullptr;
  FxShaderStorageBuffer* _instAttrShadow = nullptr;
  // Phase 3b LOD tiers: one entry per EXTRA tier (1..N-1). hm_drawable adds a draw per (tier × gid)
  // binding _instMtx/_instAttr at _instByteOffset via the graphics sub-range bind (the VS reads the
  // tier's OUT_M slice from gl_InstanceIndex==0). _args = the tier's own indirect command array
  // (instanceCount stamped by the cull's per-tier fanout); _index = the tier mesh's index buffer.
  struct TierDraw {
    FxShaderStorageBuffer* _args  = nullptr;
    FxShaderStorageBuffer* _index = nullptr;
    size_t _instByteOffset        = 0;
  };
  std::vector<TierDraw> _lodTiers;
};
// Resolve a baked ScatterSet (.ogeo) into an InstanceSet WITHOUT a graph module —
// the family-neutral scatter resolution lifted from ScatterSourceInst (see
// hmdflow_module_scattersource.cpp). A drawable calls this to bind instances at
// the drawable level (LOD: one set resolved ONCE, shared across N LOD meshes),
// rather than carrying a ScatterSource node in every geometry graph. Fills
// iset->{_count,_matrices,_attrs}; leaves count 0 if the set is missing/empty.
// ogeoPath wins when non-empty; else resolves <assetcache>/terrain/<asset>/<sink>.ogeo.
void fillInstanceSetFromScatter(
    Context* ctx, dflowgfx::instanceset_inst_ptr_t iset,
    const std::string& scatterAsset, const std::string& sink,
    const std::string& ogeoPath, int typeId);

MeshRenderBuffers setupMeshRender(
    ComputeDrawableData* cdd, livehypermesh_ptr_t live, Context* ctx, bool animated, bool faceViz,
    bool tagViz = false, bool wireframe = false,
    int instanceCount = 1, const std::vector<float>& instanceMatrices = {},
    const std::vector<int>& boundGids = {},   // E.3: gids with their own material (others fold to slot 0)
    bool cull = false,                        // E.4: per-view GPU frustum cull (instanced only)
    const fvec4& cullBound = fvec4(0, 0, 0, 0), // object-space sphere (xyz=center, w=radius; w<=0 = cull off)
    int cullSlabs = 1,            // occludee decomposition: 1 = whole-mesh AABB, N = N vertical slabs
    float cullTightness = 1.0f,   // occludee box scale about center (<1 culls harder; 1 = geometric)
    float cullDistance = 0.0f,    // radial distance cull from the eye in meters (0 = off)
    // Phase 3c — DISTANCE LOD. lodLives[i] is the mesh drawn for instances beyond lodDistances[i] meters
    // (ascending). One shared instance set, partitioned by the cull into tier 0 (the main `live`) + these
    // tiers; each tier is its own on-GPU triangulator reading its OUT_M slice via the sub-range bind.
    const std::vector<livehypermesh_ptr_t>& lodLives = {},
    const std::vector<float>& lodDistances = {},
    // LOD step #3 — IMPOSTOR tiers: EXTRA-tier indices (parallel to lodLives/lodDistances) whose tier draws
    // a baked hemi-oct billboard, not a mesh. Those lodLives[i] are null; the base mesh's PBR atlas is baked
    // once (the main material needs the FWD_SSBO_CUSTOM_CAPTURE technique — Ptex3d(impostor=True)) and the
    // cull routes the band's instances to one camera-facing quad each (the FWD_SSBO_CUSTOM_IMPOSTOR tek).
    const std::vector<int>& impostorTiers = {},
    // gid -> material (E.3 multi-material): the impostor bake renders each gid bucket with ITS material's
    // capture technique, so a multi-material mesh (tree bark/branch/leaf) bakes per-region into the atlas.
    const std::map<int, pbrmaterial_ptr_t>& gidMaterials = {},
    int impostorGrid = 8,    // hemi-oct atlas view count (grid×grid); from imposter(grid=)
    int impostorTile = 512,  // per-view atlas tile pixels (atlas = grid*tile square); from imposter(tile=)
    int impostorSsaa = 2,    // bake supersample factor (render tile*ssaa per view); from imposter(ssaa=)
    int impostorMsaa = 4);   // bake multisample count; from imposter(msaa=)

///////////////////////////////////////////////////////////////////////////////
// O3 stage 2 — the per-section texture-array GPU material bake driver.
//
// Renders each mesh SECTION (SectionUnwrap gid bucket) into its OWN 2D MRT (one RGBA8 per capture
// target) using the material's FWD_SSBO_CUSTOM_CAPTURE technique, routed through the SECTION cap-VS
// (gl_Position = mvp * vec4(uv0.xy,0,1) over an ortho covering the 0-1 UV domain — the MoltenVK-safe
// path, NEVER the direct-from-SSBO planar gl_Position). Each target is host round-tripped
// (captureAsFormat -> CaptureBuffer) so the section_bake cache assembles them into ONE sampler2DArray
// layer per section. Mirrors prepareImpostorBake's in-frame one-shot orchestration.
///////////////////////////////////////////////////////////////////////////////

// layer -> gid table for a SectionUnwrap'd live mesh (dense layer index -> the section's __tags gid).
// DERIVED FROM THE OUTPUT MESH (per-face gid band [20:32) + the section's per-vertex UV0.z layer), so it
// is correct whether the SectionUnwrap inst ran fresh OR was restored from the mesh cook cache — never the
// undocumented ascending-gid==layer-order coincidence. Empty when the graph has no tagged/unwrapped mesh.
std::vector<int> sectionUnwrapLayerGids(livehypermesh_ptr_t live, Context* ctx);

// Opaque bake handle (pimpl: the render internals — tri / capture pipeline / MRTs — live in
// hmdflow_render.cpp). Python holds this and polls it; the pimpl keeps the RtGroups + CaptureBuffers alive.
struct SectionBakeJobImpl;
struct SectionBakeJob {
  std::shared_ptr<SectionBakeJobImpl> _impl;
  bool isReady() const;                                     // one-shot fired AND every capture readback complete
  int  numLayers() const;
  int  numTargets() const;
  capturebuffer_ptr_t layerCapture(int layer, int target) const; // host RGBA8 of section `layer`, MRT `target`
};
using sectionbakejob_ptr_t = std::shared_ptr<SectionBakeJob>;

// Prepare the per-section bake. Registers a one-shot on `cdd` (fires in-frame, mirrors the impostor bake):
// triangulates its OWN tri (bucketed by the section gids), then per LAYER pushes the 0-1 ortho + draws that
// layer's gid bucket into a fresh 2D MRT and captureAsFormat's each target. FAILS LOUD if the material has
// no FWD_SSBO_CUSTOM_CAPTURE technique (author it with capture=True). Poll job->isReady(), then read each
// layer/target via job->layerCapture(). Must be called BEFORE the drawable node is created.
sectionbakejob_ptr_t prepareSectionBake(
    Context* ctx, ComputeDrawableData* cdd, livehypermesh_ptr_t live, pbrmaterial_ptr_t material,
    const std::vector<int>& layerGids, int bakeRes, int numTargets);

// O3 stage 3 — per-LAYER (per-gid BAKE MAP) bake: each section-layer is rendered with THAT gid's material's
// capture technique (adobe content into adobe layers, timber into timber); unbound gids fall back to
// `defaultMaterial` (the stored sampler). All bound materials MUST share the capture-target schema (same
// names, same MRT order). FAILS LOUD if the default material has no FWD_SSBO_CUSTOM_CAPTURE technique.
sectionbakejob_ptr_t prepareSectionBakeMapped(
    Context* ctx, ComputeDrawableData* cdd, livehypermesh_ptr_t live,
    pbrmaterial_ptr_t defaultMaterial, const std::map<int, pbrmaterial_ptr_t>& gidMaterials,
    const std::vector<int>& layerGids, int bakeRes, int numTargets);

// O3 stage 3 — the C++ player-path section-array CACHE + array assembly (the port of section_bake.py's
// content-addressed cache; one TextureArray per capture target). See the .cpp header note for the layout
// and the stated interop deviations (hash function; per-target dir).
std::string sectionArrayCacheDir(const std::string& contentKey, int bakeRes, int numLayers);
std::string sectionBakeContentKey(dflow::graphdata_ptr_t graph, const std::string& mainMtl,
                                  const std::map<int, std::string>& gidMtls,
                                  const std::vector<int>& layerGids, int bakeRes);
bool sectionArrayCacheWarm(const std::string& baseKey, const std::vector<std::string>& targets,
                           int bakeRes, int numLayers);
texturearray_ptr_t placeholderSectionArray(Context* ctx, int numLayers, int bakeRes);
// `genMips` (default ON) rides HypermeshDrawableData::_section_mips: build a CPU-generated trilinear mip
// chain per array layer (env ORKID_SECTION_MIPS overrides). Cache format is unchanged (mip-0 PNGs);
// mips regenerate from mip-0 on both assemble (cold) and load (warm).
std::vector<texturearray_ptr_t> assembleSectionArraysFromJob(
    Context* ctx, sectionbakejob_ptr_t job, const std::string& baseKey,
    const std::vector<std::string>& targets, int bakeRes, bool genMips = true);
std::vector<texturearray_ptr_t> loadSectionArraysFromCache(
    Context* ctx, const std::string& baseKey, const std::vector<std::string>& targets,
    int bakeRes, int numLayers, bool genMips = true);

// foundation gate: bake ripple (+ ripple->subdivide chain + box) graphs, read the indexed
// topology back and assert vert/corner/face counts + bbox + a sample normal. Returns FAILED count.
int hypermeshFoundationSelfTest(Context* ctx);

// E.5 gate: hand-built concave polygons through the REAL triangulator + the
// area oracle (orientation + area-sum). Returns FAILED case count.
int hypermeshTriangulationSelfTest(Context* ctx);

} // namespace ork::lev2::hypermesh
