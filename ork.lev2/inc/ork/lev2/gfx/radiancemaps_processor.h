////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/datablock.h>
#include <ork/file/path.h>
#include <ork/kernel/taskgraph.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/math/box.h>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////
// Tile rendering parameters
////////////////////////////////////////////////////////////////////////////////

struct TileParams {
  int x, y;          // Tile position in pixels
  int width, height; // Tile dimensions
  float roughness;   // For specular filtering  
  
  // NDC and UV coordinates computed from tile position
  fvec4 getNDC(int tex_width, int tex_height) const;
  fvec4 getUV(int tex_width, int tex_height) const;
};

////////////////////////////////////////////////////////////////////////////////
// XIRProcessFuture - Future for async XIR processing
////////////////////////////////////////////////////////////////////////////////

struct XIRProcessFuture {
  std::atomic<bool> _is_complete{false};
  datablock_ptr_t _result;
  std::mutex _mutex;
  std::condition_variable _cv;
  
  // Debug images - captured during processing
  image_list_t _specular_images;  // One per roughness level
  
  datablock_ptr_t get() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] { return _is_complete.load(); });
    return _result;
  }
  
  XIRProcessFuture();
  ~XIRProcessFuture();
  
  bool isReady() const {
    return _is_complete.load();
  }
  
  void setResult(datablock_ptr_t result) {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _result = result;
      _is_complete = true;
    }
    _cv.notify_all();
  }
  
  void setDebugImages(const image_list_t& spec_images) {
    std::lock_guard<std::mutex> lock(_mutex);
    _specular_images = spec_images;
  }
};

using xirprocessfuture_ptr_t = std::shared_ptr<XIRProcessFuture>;
using xirprocessfuture_wkptr_t = std::weak_ptr<XIRProcessFuture>;

////////////////////////////////////////////////////////////////////////////////
// EnvMapProcessor - Build-time processing of environment maps
//
// Processes raw environment maps (HDR, EXR, etc.) into pre-filtered
// Radiance maps stored in XIR format. This moves the expensive
// filtering operations from runtime to build time.
////////////////////////////////////////////////////////////////////////////////

struct EnvMapProcessor {

  // COMFORT-1: importance-sample count for the specular filter pass — the only
  // filter pass there is (W4-S9 deleted the prefiltered diffuse). This is the
  // BAKE-TIME value and every baked .xir in the tree was filtered with it, so
  // changing it changes those bytes. A caller that can afford less quality (the
  // procedural sky feed) passes its own; the count rides the filterenv UBO
  // (A8), never shader text.
  static constexpr int kBakedSpecularSamples = 8192;

  // Process a single environment map file to XIR format
  // Returns true on success, XIR data written to output_path
  static bool processToXIR(
      const file::Path& input_path,
      const file::Path& output_path);
  
  // Process to XIR datablock - returns a future
  static xirprocessfuture_ptr_t processToXIRDataBlockAsync(
      const file::Path& input_path);
  
  // Batch processing support - returns futures for async processing
  static std::vector<xirprocessfuture_ptr_t> processDirectory(
      const file::Path& source_dir,
      const file::Path& output_dir,
      const std::vector<std::string>& extensions = {".exr", ".hdr", ".png", ".dds"});
  
  // TaskGraph-based filtering - creates the full filtering pipeline
  // Every capture is RGBA16F, whatever the source encoding was — see
  // initEnvFilterState.
  static taskgraph_ptr_t createFilteringTaskGraph(
      texture_ptr_t rawenvmap,
      bool is_equirectangular);

  // MT2 (JUL05_GPUMICROTASK §2.6): the sliced, scheduler-driven equivalent of
  // createFilteringTaskGraph. Filters `rawenvmap` ONE roughness/mip level per
  // slice under the per-frame budget (T7 option "a": each level render+capture
  // is a self-contained GPU submit via Context::executeInlineGpuJob, so its
  // cost is measured + throttled). Shares the exact same per-level render +
  // packaging code as the burst path — output is byte-identical (determinism
  // law, §5). If `target` is non-null the result is published into it on the
  // final slice (in-scene refilter, the fence-gated live swap). `on_complete`
  // fires last with the packaged XIR datablock. Enqueue on a Context's
  // _microtaskScheduler; runs on that context's owner thread.
  static gpumicrotask_ptr_t createRadiancePrefilterMicrotask(
      texture_ptr_t rawenvmap,
      bool is_equirectangular,
      pbr::radiancemaps_ptr_t target,
      std::function<void(datablock_ptr_t)> on_complete,
      int specular_samples = kBakedSpecularSamples,
      // How this job's work is CUT UP (plain ints — this class knows nothing of
      // SkyAtmosphereData and must not): submits per level, slices per frame, and
      // one publish-chain slice's output-pixel budget. 0 on any of them means
      // UNSET and resolves to the matching ORKID_MT_IBL_* env var, else the
      // measured default in the .cpp — so the bake callers, which pass none of
      // them, keep the pre-property behavior by construction.
      int level_batches      = 0,
      int slices_per_frame   = 0,
      int mipchain_budget_px = 0,
      // COLD START: this is a feed's FIRST bake and nothing usable is published
      // yet, so the job drops its per-frame quota and its budget gate and
      // drains in the frame it starts in (GpuMicrotask::_unboundedDrain). Only
      // the caller can know that — a bake caller passes nothing and keeps the
      // paced behavior, and the flag is per-instance, so the next cycle paces
      // normally with no reset anywhere.
      bool cold_start        = false);

  // Byte-identity harness (§3 MT2 gate b): bake `input_path` to XIR via the
  // microtask path instead of the burst taskgraph. Enqueues on gloadercontext's
  // scheduler (UNBOUNDED — drains like the burst). Result delivered via future.
  static xirprocessfuture_ptr_t processToXIRDataBlockAsyncViaMicrotask(
      const file::Path& input_path);
  
  // Individual tile rendering methods
  static void renderSpecularTile(
      Context* ctx,
      const TileParams& tile,
      texture_ptr_t src_tex,
      rtbuffer_ptr_t target_buffer,
      float roughness,
      int num_samples);
  
  // Constants for tile sizes
  static constexpr int SPECULAR_TILE_SIZE = 128;
};

} // namespace ork::lev2