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
  int mip_level;     // For diffuse filtering (specular uses roughness level)
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
  image_list_t _diffuse_images;   // One per mip level
  
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
  
  void setDebugImages(const image_list_t& spec_images, 
                      const image_list_t& diff_images) {
    std::lock_guard<std::mutex> lock(_mutex);
    _specular_images = spec_images;
    _diffuse_images = diff_images;
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
  static taskgraph_ptr_t createFilteringTaskGraph(
      texture_ptr_t rawenvmap,
      bool is_equirectangular);
  
  // Individual tile rendering methods
  static void renderSpecularTile(
      Context* ctx,
      const TileParams& tile,
      texture_ptr_t src_tex,
      rtbuffer_ptr_t target_buffer,
      float roughness,
      int num_samples);
  
  static void renderDiffuseTile(
      Context* ctx,
      const TileParams& tile,
      texture_ptr_t src_tex,
      rtbuffer_ptr_t target_buffer);
  
  // Constants for tile sizes
  static constexpr int SPECULAR_TILE_SIZE = 128;
  static constexpr int DIFFUSE_TILE_SIZE = 256;
};

} // namespace ork::lev2