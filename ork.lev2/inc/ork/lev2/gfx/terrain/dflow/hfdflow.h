////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Heightfield compute-dataflow (HyperSyn `terrain` family, bake side).
//
// A serializable ork::dataflow GraphData whose modules dispatch COMPUTE shaders
// to produce 2D float-field channels (height, slope, masks, ...), then read the
// result back and encode PNG/EXR. This is a BAKE tool — it runs once at
// materialization; there is no runtime GraphInst. See the hypersyn skill.
//
// First slice: the `GpuComputeImage2D` plug type (R32F, SSBO-backed — Vulkan
// storage-image binding for compute isn't implemented yet, so a std430 float[]
// SSBO addressed buf[y*W+x] is the working backing), a `TerrainModuleData` base,
// an `FbmModule` generator, a `CaptureModule` sink (readback -> writeToFile), and
// a `bakeHeightfield()` driver building+sorting+computing a 2-node graph.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/all.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/file/path.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <ork/lev2/gfx/dflow/interchange.h>

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// GpuComputeImage2D + HfImagePlugTraits — PROMOTED to the family-neutral interchange header
// (HYPERECS B.3: gfx/dflow/interchange.h) so any family can carry a heightfield channel on a plug
// (terrain -> hypermesh field-input etc.). Re-exported here so terrain sources stay unchanged; the
// serialized reflection names ("terrain::hfimg{out,inp}plug") are unchanged.
///////////////////////////////////////////////////////////////////////////////

using GpuComputeImage2DData       = ::ork::lev2::dflowgfx::GpuComputeImage2DData;
using GpuComputeImage2DInst       = ::ork::lev2::dflowgfx::GpuComputeImage2DInst;
using gpucomputeimage2d_data_ptr_t = ::ork::lev2::dflowgfx::gpucomputeimage2d_data_ptr_t;
using gpucomputeimage2d_inst_ptr_t = ::ork::lev2::dflowgfx::gpucomputeimage2d_inst_ptr_t;
using HfImagePlugTraits           = ::ork::lev2::dflowgfx::HfImagePlugTraits;
using hfimg_inplugdata_t          = ::ork::lev2::dflowgfx::hfimg_inplugdata_t;
using hfimg_outplugdata_t         = ::ork::lev2::dflowgfx::hfimg_outplugdata_t;
using hfimg_inpluginst_t          = ::ork::lev2::dflowgfx::hfimg_inpluginst_t;
using hfimg_outpluginst_t         = ::ork::lev2::dflowgfx::hfimg_outpluginst_t;
using hfimg_inpluginst_ptr_t      = ::ork::lev2::dflowgfx::hfimg_inpluginst_ptr_t;
using hfimg_outpluginst_ptr_t     = ::ork::lev2::dflowgfx::hfimg_outpluginst_ptr_t;

///////////////////////////////////////////////////////////////////////////////
// BakeEnv — per-bake environment stashed on the GraphInst _impl so every module's
// compute() can reach the gfx Context + grid resolution.
///////////////////////////////////////////////////////////////////////////////

struct CaptureRequest {
  gpucomputeimage2d_inst_ptr_t _img; // the SOURCE field (resolved from the connected output)
  std::string _channels;             // comma-joined output channels ("height" or "height,normal")
  ork::file::Path _path;             // path template; "{channel}" is substituted per channel at flush
  uint64_t _cookkey = 0;             // capture-currency key (producer cook-hash mix) — the flush
                                     // writes it to a "<file>.cookhash" sidecar so an unchanged
                                     // bake can skip the capture entirely next run. 0 = no sidecar.
  // INCREMENTAL FLUSH (default): the raw field (w*h*channels floats), read back the
  // moment the sink ran, so the source plane releases to the pool immediately instead
  // of staying VRAM-pinned until the post-loop flush (the accumulated pinned sources
  // are what spilled eflow's frontier past the DEVICE budget). Null under
  // ORKID_BAKE_DEFERRED_FLUSH=1 (the flush maps the live SSBO as before).
  std::shared_ptr<std::vector<float>> _hostcopy;
};

// min/max/mean of a captured field — returned by the driver so callers (the
// self-test) can assert against analytically-known expectations.
struct FieldStats {
  float _min  = 0.0f;
  float _max  = 0.0f;
  float _mean = 0.0f;
  // which channel these stats describe. The bake returns stats in FLUSH (topo) order while
  // callers enumerate captures in name-sorted module order — index-zipping the two silently
  // shuffles stats across channels (the test_terrain_hfbake RED). Key by THIS, never by index.
  std::string _channel;
};
using fieldstats_ptr_t = std::shared_ptr<FieldStats>;

struct BakeEnv {
  Context* _ctx           = nullptr;
  int _w                  = 0;
  int _h                  = 0;
  // CAPABILITIES of the DRIVER that stocked this env. The terrain bake driver
  // (bakeHeightfield) provides both: per-op submit+wait (mid-graph CPU readback is
  // valid) and capture flushing. A CROSS-FAMILY host (DisplaceByField stocking the env
  // in a mesh graph, which runs ONE dispatch phase) leaves them false; a module that
  // REQUIRES a capability asserts it loudly (ops self-defend) instead of mis-running.
  bool _per_op_sync       = false; // CPU mid-graph readback modules (basin_fill, fill_closed_basins) require this
  bool _flushes_captures  = false; // CaptureModule sinks require this
  // B.4 CLOCK FEED (same contract as hypermesh MeshEnv): a LIVE host advances these each
  // frame (the hypermesh live render hook mirrors its clock here when a field subgraph
  // rides a mesh graph); module writeParams reads them (e.g. fbm/noise offset_vel pan).
  // BAKE leaves them at 0.0 -> a bake is the deterministic t=0 snapshot.
  double _abstime         = 0.0;
  double _dt              = 0.0;
  // world units (make the graph resolution-independent): spatial op params are in
  // meters and converted to texels here per-bake. texelsPerMeter() == dim / extent.
  float _extent_m         = 4096.0f;  // horizontal world size (meters across the field)
  float _height_scale_m   = 9830.25f; // PHYSICAL: what normalized height 1.0 means in meters
                                      // (measurements: normal/slope/curvature). Erosion uses
                                      // its OWN per-op exaggerated_height_m, NOT this.
  std::vector<CaptureRequest> _captures; // collected during compute, flushed after submit

  float texelsPerMeter() const { return (_extent_m > 0.0f) ? (float(_w) / _extent_m) : 1.0f; }
  // meters -> texel radius, clamped to [1, dim/4] (sub-texel features can't be
  // resolved; an over-large kernel would border-out the whole field).
  int radiusTexels(float radius_m) const {
    int r = int(radius_m * texelsPerMeter() + 0.5f);
    if (r < 1) r = 1;
    int rmax = _w / 4;
    if (rmax < 1) rmax = 1;
    if (r > rmax) r = rmax;
    return r;
  }

  // bake-scoped GPU allocation ARENA: every terrain-module SSBO (plug outputs AND
  // module-internal scratch) allocates through here so the owning driver can free
  // the whole graph's buffers when the eval's outputs have been consumed. Plug
  // aliasing (erox publishes one of its ping-pong buffers as its output) makes
  // per-plug ownership ambiguous — the arena dedups, so each buffer frees exactly
  // once. A LIVE/cross-family host (persistent graph) simply never calls
  // freeAllocs and keeps today's persistent-buffer behavior.
  //
  // WS4 FRONTIER MODE (_lazy_acquire, set ONLY by the per-op-synced bake driver):
  // module allocations move from onActivate to bakeAcquire (the driver calls it
  // just before the node runs), createStorageBuffer becomes a size-classed POOL
  // acquire, and the driver returns buffers at their last dispatching reader.
  // Reuse is GPU-safe because the cook loop submits+WAITs per node — a released
  // buffer's producer/consumers have fully executed before it is handed out
  // again. Peak collapses from whole-graph to the live frontier. LIVE/cross-family
  // envs leave _lazy_acquire false: allocation stays at activate, nothing is
  // released mid-eval (the WS6 "persistent" class), and the pool is inert.
  FxShaderStorageBuffer* createStorageBuffer(size_t length);
  void freeAllocs(); // caller guarantees GPU idle for these buffers (post per-op sync / endFrame)

  // return a buffer to the size-classed free-list for reuse by a later node.
  // Caller (the bake driver) guarantees the GPU is done with it (per-op sync) and
  // that no live plug still publishes it. Dedup'd: releasing the same pointer
  // twice (plug aliasing) is a no-op the second time.
  void releaseToPool(FxShaderStorageBuffer* buf);
  // node-scoped scratch: the driver brackets each node's run; endNodeScope returns
  // every buffer acquired during the scope EXCEPT those a plug currently publishes
  // (the keep set is read AFTER compute — erox picks its output ping-pong buffer
  // at compute end).
  void beginNodeScope();
  void endNodeScope(const std::unordered_set<FxShaderStorageBuffer*>& keep);

  bool _lazy_acquire = false; // frontier mode (bake driver only — see block comment)

  std::vector<FxShaderStorageBuffer*> _allocs;
  // pool state (all inert unless _lazy_acquire).
  // FREE-LIST KEY = (byte size, residency class): PCIEopt syncpoint-2 prep — the linux
  // budgeted-DEVICE bake policy decides residency inside createStorageBuffer, and pool
  // reuse must never hand a HOST-pooled plane where DEVICE was chosen (or vice versa).
  // Today every allocation is class 0 (HOST); the policy patch supplies the real class.
  using poolkey_t = std::pair<size_t, int>; // (bytes, residency class)
  struct PoolKeyHash {
    size_t operator()(const poolkey_t& k) const { return k.first * 31 + size_t(k.second); }
  };
  std::unordered_map<poolkey_t, std::vector<FxShaderStorageBuffer*>, PoolKeyHash> _pool_free;
  std::unordered_set<FxShaderStorageBuffer*> _pool_free_set;      // membership (double-release guard)
  std::unordered_map<FxShaderStorageBuffer*, poolkey_t> _alloc_size; // buffer -> (bytes, class)
  std::vector<FxShaderStorageBuffer*> _scope;                                 // current node's acquisitions
  bool _in_scope = false;
  // frontier metrics (reported by the driver at bake end)
  size_t _arena_bytes      = 0; // bytes currently backed by real VK allocations
  size_t _peak_arena_bytes = 0; // high-water mark — THE WS4 A/B metric
  int _pool_reuses         = 0; // acquisitions served from the free-list
  // DISCRETE-GPU residency budget (PCIEopt §6): bytes of this arena currently
  // DEVICE_LOCAL. Big planes allocate DEVICE until the budget is spent, then degrade
  // to HOST (forest-class >24GB transient peaks must never OOM VRAM). UMA/apple: 0.
  size_t _device_bytes     = 0;
};
using bakeenv_ptr_t = std::shared_ptr<BakeEnv>;

///////////////////////////////////////////////////////////////////////////////
// Module base
///////////////////////////////////////////////////////////////////////////////

struct TerrainModuleData : public dflow::DgModuleData {
  DeclareAbstractX(TerrainModuleData, dflow::DgModuleData);
  TerrainModuleData();
};
using terrainmoduledata_ptr_t = std::shared_ptr<TerrainModuleData>;

///////////////////////////////////////////////////////////////////////////////
// FbmModule — generator. Output "Out" : GpuComputeImage2D (R32F). Dispatches a
// compute shader writing fbm into the SSBO (dim + params baked into the text).
///////////////////////////////////////////////////////////////////////////////

struct FbmModuleData : public TerrainModuleData {
  DeclareConcreteX(FbmModuleData, TerrainModuleData);
  FbmModuleData();
  static std::shared_ptr<FbmModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  // float input plugs: "frequency", "amplitude". `octaves` is a baked loop bound.
  int _octaves = 5;
  // lattice-hash seed — RUNTIME data (rides the params SSBO p_r0 slot), so seed changes
  // never rebuild the shader. Exact through the float slot for |seed| < 2^24.
  int _seed = 0;
};
using fbmmoduledata_ptr_t = std::shared_ptr<FbmModuleData>;

///////////////////////////////////////////////////////////////////////////////
// NoiseModule — generator. One module, four NOISE-BASIS PRIMITIVES via a baked `_basis`
// enum (0 perlin / 1 simplex / 2 worley-F1 / 3 voronoi). Output "Out" : GpuComputeImage2D
// (R32F). Float plugs "frequency"/"amplitude"; `_octaves` (default 1 = pure primitive)
// fBm-stacks the chosen basis the same way FbmModule does (compose, don't fork a module).
///////////////////////////////////////////////////////////////////////////////

struct NoiseModuleData : public TerrainModuleData {
  DeclareConcreteX(NoiseModuleData, TerrainModuleData);
  NoiseModuleData();
  static std::shared_ptr<NoiseModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  // baked scalars (not plugs): noise basis primitive + fBm octave count.
  int _basis   = 0;   // 0 perlin / 1 simplex / 2 worley-F1 / 3 voronoi
  int _octaves = 1;   // 1 = pure primitive; >1 = fBm-stacked
  // lattice-hash seed — RUNTIME data (params SSBO p_r0), never rebuilds the shader.
  // NOTE: the simplex basis (permute-based) ignores it.
  int _seed = 0;
};
using noisemoduledata_ptr_t = std::shared_ptr<NoiseModuleData>;

///////////////////////////////////////////////////////////////////////////////
// ExprModule — GENERIC compute generator for the unified procedural substrate.
// Runs a Python-authored GLSL expression body (emitted from a ptex3d SurfNode by
// emit_compute_field) over the grid into the output field. The FULL compute
// shader text — with %DIMU%/%DIMSQ%/%DIM%/%EXTENT_M%/%HEIGHT_M% placeholders the
// bake substitutes per-resolution — is stored as a reflected string (so the
// embedded graph self-describes / round-trips). Entry point: cs_expr.
// Backs self.hfbake/hfmask now; self.hfdisplacement (input-reading) later.
///////////////////////////////////////////////////////////////////////////////
struct ExprModuleData : public TerrainModuleData {
  DeclareConcreteX(ExprModuleData, TerrainModuleData);
  ExprModuleData();
  static std::shared_ptr<ExprModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  std::string _shadertext; // full compute text w/ %DIMU%/%DIMSQ%/%EXTENT_M%/%HEIGHT_M% holes
};
using exprmoduledata_ptr_t = std::shared_ptr<ExprModuleData>;

///////////////////////////////////////////////////////////////////////////////
// NormalizeModule — rescale a field's [min,max] to [out_lo,out_hi] (default
// [0,1]). The EXPLICIT, controllable counterpart to the bake flush's
// unconditional auto-exposure (put the renorm where you want it). GPU reduction
// (atomic min/max over an order-preserving uint key) + a rescale pass; float
// plugs out_lo / out_hi.
///////////////////////////////////////////////////////////////////////////////
struct NormalizeModuleData : public TerrainModuleData {
  DeclareConcreteX(NormalizeModuleData, TerrainModuleData);
  NormalizeModuleData();
  static std::shared_ptr<NormalizeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using normalizemoduledata_ptr_t = std::shared_ptr<NormalizeModuleData>;

///////////////////////////////////////////////////////////////////////////////
// RemapModule — 1-in elementwise: Out = clamp(In*scale + bias, lo, hi). The
// canary for input-reading modules (proves the multi-SSBO bind + storageBarrier).
///////////////////////////////////////////////////////////////////////////////

struct RemapModuleData : public TerrainModuleData {
  DeclareConcreteX(RemapModuleData, TerrainModuleData);
  RemapModuleData();
  static std::shared_ptr<RemapModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  // image input "In"; float input plugs: "scale","bias","lo","hi". output "Out".
};
using remapmoduledata_ptr_t = std::shared_ptr<RemapModuleData>;

///////////////////////////////////////////////////////////////////////////////
// ConstModule — generator. Out = constant "level" (float plug). 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

struct ConstModuleData : public TerrainModuleData {
  DeclareConcreteX(ConstModuleData, TerrainModuleData);
  ConstModuleData();
  static std::shared_ptr<ConstModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using constmoduledata_ptr_t = std::shared_ptr<ConstModuleData>;

///////////////////////////////////////////////////////////////////////////////
// GradientModule — generator. Out = dot(uv, dir)*scale + bias. 1 SSBO.
// vec2 plug: dir (the ramp direction). float plugs: scale, bias.
///////////////////////////////////////////////////////////////////////////////

struct GradientModuleData : public TerrainModuleData {
  DeclareConcreteX(GradientModuleData, TerrainModuleData);
  GradientModuleData();
  static std::shared_ptr<GradientModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using gradientmoduledata_ptr_t = std::shared_ptr<GradientModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CombineModule — 2-in: Out = op(A, B). `op` (ctor, baked): 0=add 1=sub 2=mul
// 3=min 4=max 5=mix. float plug "t" = mix factor. 3 SSBOs (out + A + B).
///////////////////////////////////////////////////////////////////////////////

enum class CombineOp { ADD = 0, SUB, MUL, MIN, MAX, MIX };

struct CombineModuleData : public TerrainModuleData {
  DeclareConcreteX(CombineModuleData, TerrainModuleData);
  CombineModuleData();
  static std::shared_ptr<CombineModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _op = int(CombineOp::ADD); // baked (selects the GLSL expression)
};
using combinemoduledata_ptr_t = std::shared_ptr<CombineModuleData>;

///////////////////////////////////////////////////////////////////////////////
// TerraceModule — 1-in: quantize to `steps` plateaus with a `sharpness` riser.
// float plugs: steps, sharpness. 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

struct TerraceModuleData : public TerrainModuleData {
  DeclareConcreteX(TerraceModuleData, TerrainModuleData);
  TerraceModuleData();
  static std::shared_ptr<TerraceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using terracemoduledata_ptr_t = std::shared_ptr<TerraceModuleData>;

///////////////////////////////////////////////////////////////////////////////
// SlopeModule — 1-in mask generator ("Mask by Feature: slope"). Out is the
// gradient magnitude of In, soft-rolled-off (Reinhard m/(1+m)) into [0,1). The
// gradient is PRE-BLURRED: a difference of box averages offset by +/-`_radius`
// (each box radius ~radius/2), so it tracks LANDFORM slope at the chosen scale
// instead of per-pixel noise. float plug: scale (sensitivity). An edge ring of
// width (radius + box) is 0. A mask is just a [0,1] field on the SAME plug type
// — apply it compositionally via MaskBlend/mix.
///////////////////////////////////////////////////////////////////////////////

struct SlopeModuleData : public TerrainModuleData {
  DeclareConcreteX(SlopeModuleData, TerrainModuleData);
  SlopeModuleData();
  static std::shared_ptr<SlopeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  float _radius_m = 8.0f; // baked: pre-blur / gradient-baseline scale (METERS)
};
using slopemoduledata_ptr_t = std::shared_ptr<SlopeModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CurvatureModule — 1-in mask generator ("Mask by Feature: curvature"). Curvature
// is a band-pass of In: difference of two box averages (inner radius ~radius/2,
// outer `_radius`) — a difference-of-box / Laplacian-of-Gaussian that is inherently
// PRE-BLURRED, so it tracks LANDFORM ridges/valleys at the chosen scale instead of
// per-pixel noise. `_mode` selects the flavor: CONVEX = ridges/peaks, CONCAVE =
// valleys/pits, MAGNITUDE = both. A SOFT (Reinhard m/(1+m)) rolloff maps it to [0,1)
// so curvature magnitude survives (no hard clamp -> no binary speckle). float plug:
// scale (sensitivity). Edge ring of width `_radius` is 0 (undefined at the border).
///////////////////////////////////////////////////////////////////////////////

enum class CurvatureMode { CONVEX = 0, CONCAVE, MAGNITUDE };

struct CurvatureModuleData : public TerrainModuleData {
  DeclareConcreteX(CurvatureModuleData, TerrainModuleData);
  CurvatureModuleData();
  static std::shared_ptr<CurvatureModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _mode       = int(CurvatureMode::MAGNITUDE); // baked (selects the GLSL output)
  float _radius_m = 96.0f;                          // baked: pre-blur / curvature scale (METERS)
};
using curvaturemoduledata_ptr_t = std::shared_ptr<CurvatureModuleData>;

///////////////////////////////////////////////////////////////////////////////
// RelaxUvModule — equal-area UV relaxation (the slope-stretch fix). In = height; Out = RGBA
// (relaxed_uv.xy + normal.x,z), Binormal = RGBA (relaxed binormal.xyz + 1). See the .cpp for the
// linearized-OT algorithm. Outputs feed the chunk VS's uv0 + tangent frame (downsampled on load).
///////////////////////////////////////////////////////////////////////////////

struct RelaxUvModuleData : public TerrainModuleData {
  DeclareConcreteX(RelaxUvModuleData, TerrainModuleData);
  RelaxUvModuleData();
  static std::shared_ptr<RelaxUvModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  float _strength   = 1.0f; // warp gain: psi-gradient (texels) -> uv displacement. 0 = planar.
  int   _iterations = 0;    // Poisson Jacobi sweeps (0 = auto ~ 4*dim, capped)
};
using relaxuvmoduledata_ptr_t = std::shared_ptr<RelaxUvModuleData>;

///////////////////////////////////////////////////////////////////////////////
// MaskBlendModule — 3-in per-texel blend: Out = mix(A, B, M). The masking
// PRIMITIVE (Houdini's lerp(input, op(input), mask)); M is a [0,1] field. Unlike
// CombineModule's MIX (a uniform-scalar t), this blends by a per-texel field. 4 SSBOs.
///////////////////////////////////////////////////////////////////////////////

struct MaskBlendModuleData : public TerrainModuleData {
  DeclareConcreteX(MaskBlendModuleData, TerrainModuleData);
  MaskBlendModuleData();
  static std::shared_ptr<MaskBlendModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using maskblendmoduledata_ptr_t = std::shared_ptr<MaskBlendModuleData>;

///////////////////////////////////////////////////////////////////////////////
// ThermalErodeModule — 1-in iterative THERMAL erosion (talus / angle-of-repose).
// Each of `_iterations` steps moves material from a cell to lower neighbors wherever
// the inter-cell height STEP exceeds the talus threshold (tan(talus_deg)*cell_size),
// relaxing slopes toward the angle of repose -> scree/talus, softened ridges, filled
// hollows. A symmetric pairwise GATHER (read old, write new) is parallel-safe AND
// mass-conserving (closed domain); the module ping-pongs two SSBOs across the steps.
// talus_deg is PHYSICAL (meters model) so erosion is resolution-independent. float
// plugs: talus_deg (angle of repose), rate (per-step relaxation, keep <~0.25). 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

struct ThermalErodeModuleData : public TerrainModuleData {
  DeclareConcreteX(ThermalErodeModuleData, TerrainModuleData);
  ThermalErodeModuleData();
  static std::shared_ptr<ThermalErodeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _iterations = 40; // baked: number of thermal relaxation steps
};
using thermalerodemoduledata_ptr_t = std::shared_ptr<ThermalErodeModuleData>;

// EroxModule — 1-in PHYSICAL hydraulic erosion (Mei et al. 2007 virtual-pipes),
// expressed entirely in PHYSICAL UNITS (meters / seconds) so the bake is
// RESOLUTION-INDEPENDENT. cell_size_m = extent_m/dim and height_scale_m bridge the
// grid to the world; the timestep dt is DERIVED per-bake from a CFL bound
// (dt = CFL*cell_size_m/flow_speed_max_mps) so iterations = ceil(sim_time_s/dt) scale
// with dim to hold the same physical time + meters-scale diffusion. Every knob is in
// m / s and is HASHED dim-free; the per-bake (dim,extent,height_scale) lives in the
// cook CONTEXT hash (so two resolutions of one graph share node identity, differ in
// context). Unlike the droplet (texel-coupled, racy, no fields) this is a
// DETERMINISTIC Eulerian field solver — gather-only, cache-safe — whose water /
// velocity / sediment fields are the basis for Houdini-style layer outputs later.
//
// DATA MODEL is MATERIALS-READY (so multi-material slots in additively, no rewrite):
//   - terr  = the TOTAL surface column height (normalized; *height_scale_m = meters).
//             A future bedrock/regolith split is `terr - bedrock = soil_depth` — additive.
//   - sed   = SUSPENDED load (meters); the future Sediment (+Debris) layer precursor.
//   - the erosion/deposition strengths are isolated, per-cell-READY scalars (a future
//     per-cell erodibility field replaces the uniform coefficient at one line).
//   - the water/vel fields already exist internally; exposing Flow/Flowdir/Sediment as
//     extra output plugs (Milestone B) needs no solver change.
//
// Milestone A (this): single hydraulic layer, single "Out" = Height; pure carving +
// transport, no thermal/spread yet. float plugs (all PHYSICAL): sim_time_s, rain_mps,
// evaporation_per_s, flow_speed_max_mps, capacity_Kc, erosion_rate_per_s,
// deposition_rate_per_s. NO baked iteration count (derived). ~6 SSBOs, 3 dispatches/
// iteration (each its own submit — the one-descriptor-set-per-pipeline rule).
///////////////////////////////////////////////////////////////////////////////

struct EroxModuleData : public TerrainModuleData {
  DeclareConcreteX(EroxModuleData, TerrainModuleData);
  EroxModuleData();
  static std::shared_ptr<EroxModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  // all tunables are PHYSICAL float plugs (see comment above); iterations + dt are
  // derived per-bake from sim_time_s + the CFL bound, never stored.
};
using eroxmoduledata_ptr_t = std::shared_ptr<EroxModuleData>;

///////////////////////////////////////////////////////////////////////////////
// PhaModule — PROCEDURAL "phacelle" erosion filter (Rune Skovbo Johansen, MPL-2.0):
// a SINGLE-PASS analytic filter that stacks `octaves` of slope-aligned "faded gully"
// noise -> drainage-like gullies. Smooth function of continuous uv -> naturally
// resolution-independent + speckle-free + fast + predictable (vs the erox sim). float
// plugs: strength/gully_weight/detail/scale/cell_scale/normalization/lacunarity/gain/
// default_height (in a params SSBO); `octaves` is a baked loop bound. 1 dispatch.
///////////////////////////////////////////////////////////////////////////////

struct PhaModuleData : public TerrainModuleData {
  DeclareConcreteX(PhaModuleData, TerrainModuleData);
  PhaModuleData();
  static std::shared_ptr<PhaModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _octaves = 5; // baked loop bound (gully octaves)
};
using phamoduledata_ptr_t = std::shared_ptr<PhaModuleData>;

///////////////////////////////////////////////////////////////////////////////
// LpfModule — separable GAUSSIAN low-pass filter; `cutoff_texels` float plug is the
// cutoff scale in TEXELS (sigma = cutoff/6, radius = 3*sigma). Soft rolloff (no
// ringing). A smoothing / hillslope-relaxation primitive (e.g. between erosion passes).
///////////////////////////////////////////////////////////////////////////////

struct LpfModuleData : public TerrainModuleData {
  DeclareConcreteX(LpfModuleData, TerrainModuleData);
  LpfModuleData();
  static std::shared_ptr<LpfModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using lpfmoduledata_ptr_t = std::shared_ptr<LpfModuleData>;

///////////////////////////////////////////////////////////////////////////////
// BasinFillModule — depression / pit / sink FILL (only): raises closed basins to their
// spill elevation so every cell drains to the boundary (no interior minima; filled basins
// become flat lakes). Priority-Flood (Barnes 2014). The FIRST CPU module — it reads the
// field back, floods on the CPU (exact, one pass), and writes the filled field. float plug
// `epsilon` (normalized; 0 = flat fill) adds a tiny drainage gradient.
///////////////////////////////////////////////////////////////////////////////

struct BasinFillModuleData : public TerrainModuleData {
  DeclareConcreteX(BasinFillModuleData, TerrainModuleData);
  BasinFillModuleData();
  static std::shared_ptr<BasinFillModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using basinfillmoduledata_ptr_t = std::shared_ptr<BasinFillModuleData>;

// Flow3DModule — CONTINUOUS FLOW FIELD: emits a per-cell flow DIRECTION + slope (the
// continuous −∇z, any angle, NOT D8) plus the MFD drainage DISCHARGE. Two outputs:
//   "Out"       — RGBA32F display field: R,G = downhill flow direction (unit −∇z, *0.5+0.5),
//                 B = slope (scaled to [0,1]), A = 1. View as a colour image / feed erosion.
//   "Discharge" — R32F mono: MFD drainage area (log(1+area) default), the flow map.
// Direction/slope are a cheap gradient; discharge is an MFD (Holmgren) gather. Basis for
// continuous transport-limited erosion+deposition (advect sediment along the flow vector — no
// SFD tree needed; the tree is only a constraint of the implicit FastFlow solve, not of erosion).
///////////////////////////////////////////////////////////////////////////////

struct Flow3DModuleData : public TerrainModuleData {
  DeclareConcreteX(Flow3DModuleData, TerrainModuleData);
  Flow3DModuleData();
  static std::shared_ptr<Flow3DModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  float _exponent     = 1.1f;  // MFD slope exponent for discharge (Holmgren)
  int   _iterations   = 0;     // discharge gather passes (0 = auto = min(dim,1024))
  bool  _log_compress = true;  // discharge: log(1+area) vs raw area
  float _slope_scale  = 10.0f; // display scale for the PHYSICAL slope (rise/run) in the B channel
  // "Metrics" RGBA output = flatness, curvature (ridge/valley), wetness (TWI). display scales:
  float _flat_scale   = 8.0f;  // flatness = 1/(1+slope*flat_scale)
  float _curv_scale   = 80.0f; // curvature display gain (physical ∇²z, 1/m); 0.5=flat, >0.5 valley
  float _twi_scale    = 24.0f; // wetness = clamp(ln(A/slope)/twi_scale, 0,1)
};
using flow3dmoduledata_ptr_t = std::shared_ptr<Flow3DModuleData>;

///////////////////////////////////////////////////////////////////////////////
// FlowErodeModule — CONTINUOUS (MFD/vector-field) erosion+deposition iteration, driven by a flow
// map. Inputs: "In" = heightfield z, "Discharge" = drainage area A (from T.flow / T.flow3d.discharge).
// Per step (no SFD tree): slope S + flatness from a continuous gradient of z (recomputed each step,
// in PHYSICAL units); A from the discharge input. Erode by stream power (k_erode·Aᵐ·Sⁿ), deposit in
// flat high-flow cells (k_deposit·flatness·A^dep_m); the per-cell change is CLAMPED to a fraction of
// the local relief (drop to lowest neighbour / rise to highest) so it can never invert a cell —
// unconditionally bounded, no implicit solve. `niter` internal steps hold A fixed (cheap, approximate);
// for accuracy chain T.flow3d -> T.flow_erode in the DSL so A is recomputed as z evolves.
///////////////////////////////////////////////////////////////////////////////

struct FlowErodeModuleData : public TerrainModuleData {
  DeclareConcreteX(FlowErodeModuleData, TerrainModuleData);
  FlowErodeModuleData();
  static std::shared_ptr<FlowErodeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int   _niter      = 1;       // internal steps (A held fixed across them)
  float _dt         = 1.0f;    // timestep
  float _k_erode    = 0.02f;   // stream-power erosion gain
  float _k_deposit  = 0.02f;   // deposition gain (flat high-flow fill)
  float _m          = 0.5f;    // erosion drainage-area exponent
  float _n          = 1.0f;    // erosion slope exponent
  float _dep_m      = 0.5f;    // deposition drainage-area exponent
  float _flat_k     = 8.0f;    // flatness = 1/(1+slope*flat_k)
  float _clamp_frac = 0.5f;    // max per-step change as a fraction of local relief (stability)
  bool  _disch_log  = true;    // discharge input is log(1+A) (T.flow default) -> exp() to raw A
};
using flowerodemoduledata_ptr_t = std::shared_ptr<FlowErodeModuleData>;

// FillClosedBasinsModule — detect + fill CLOSED basins (depressions where water enters but
// can't exit, excluding evaporation) with PERSISTENCE control, so nested basins don't collapse
// to one level like plain basin_fill. Self-contained CPU: sorted-cell union-find = the watershed
// merge tree (handles flats); per basin persistence = pour - pit; `min_depth` keeps basins >= that
// (shallower sub-basins merge into their parent). The MAP EDGE is an OUTLET, not a wall. Outputs:
// Out = filled dem; Basin RGBA (spill/depth/id/mask); CenterPit RGBA (3D offset to pit + dist).
///////////////////////////////////////////////////////////////////////////////

struct FillClosedBasinsModuleData : public TerrainModuleData {
  DeclareConcreteX(FillClosedBasinsModuleData, TerrainModuleData);
  FillClosedBasinsModuleData();
  static std::shared_ptr<FillClosedBasinsModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using fillclosedbasinsmoduledata_ptr_t = std::shared_ptr<FillClosedBasinsModuleData>;

// CaptureModule — sink. Input "In" : GpuComputeImage2D. At bake-flush time the
// source SSBO is read back and encoded to `_path` (PNG/EXR by extension).
///////////////////////////////////////////////////////////////////////////////

struct CaptureModuleData : public TerrainModuleData {
  DeclareConcreteX(CaptureModuleData, TerrainModuleData);
  CaptureModuleData();
  static std::shared_ptr<CaptureModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  // stable output-channel identity (e.g. "height", "slope") — REFLECTED, so the
  // serialized graph is self-describing (the bake / dflow editor know the sinks).
  std::string _channel;
  // bake-time absolute output path — machine-specific, derived from _channel at
  // materialize time, deliberately NOT reflected (would not be portable).
  ork::file::Path _path;
  // per-bake cook-cache opt-out (self.capture(..., cache=False)). The cook cache is
  // whole-bake (per-node), so if ANY capture sets this false the driver skips the
  // disk find/store for the entire bake while still running the per-op synced
  // compute. REFLECTED so it round-trips with the embedded graph.
  bool _cache = true;
};
using capturemoduledata_ptr_t = std::shared_ptr<CaptureModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Driver — sort, instantiate, run the compute, flush captures. `dim` is the
// square grid resolution (W=H=dim).
///////////////////////////////////////////////////////////////////////////////

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph,
    Context* ctx,
    int dim,
    float extent_m       = 4096.0f,    // horizontal world size (meters) -> resolution independence
    float height_scale_m = 9830.25f);  // normalized 1.0 in meters (for real slope angles)

// first-slice convenience: build a 2-node fbm -> capture graph and bake it to
// `outpath` (PNG/EXR by extension), the minimal end-to-end exerciser.
void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim);

// bread-and-butter op self-test: bakes Const/Gradient/Combine(6 ops)/Terrace
// graphs over Const inputs and asserts each field's min/max/mean against the
// analytically-known result. Returns the number of FAILED cases (0 == all pass).
// Writes each field to /tmp/terrain_selftest_<case>.exr for visual inspection.
int terrainOpsSelfTest(Context* ctx, int dim);

// serialize -> deserialize -> bake round-trip gate: proves a terrain GraphData
// survives JSON round-trip with no loss (baked scalars _octaves/_op, float plug
// values, connections) and bakes identical stats. Returns FAILED-check count.
int terrainRoundTripTest(Context* ctx, int dim);

// per-node cook-cache gate: bake a cacheable graph twice; the warm bake must
// load every compute node's field from the DataBlockCache (content-addressed)
// and produce a byte-identical result. Returns FAILED-check count.
int terrainCacheTest(Context* ctx, int dim);

} // namespace ork::lev2::terrain
