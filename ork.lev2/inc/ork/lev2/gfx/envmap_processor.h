////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/datablock.h>
#include <ork/file/path.h>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////
// XIRProcessFuture - Future for async XIR processing
////////////////////////////////////////////////////////////////////////////////

struct XIRProcessFuture {
  std::atomic<bool> _is_complete{false};
  datablock_ptr_t _result;
  std::mutex _mutex;
  std::condition_variable _cv;
  
  datablock_ptr_t get() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] { return _is_complete.load(); });
    return _result;
  }
  
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
};

using xirprocessfuture_ptr_t = std::shared_ptr<XIRProcessFuture>;

////////////////////////////////////////////////////////////////////////////////
// EnvMapProcessor - Build-time processing of environment maps
//
// Processes raw environment maps (HDR, EXR, etc.) into pre-filtered
// irradiance maps stored in XIR format. This moves the expensive
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
};

} // namespace ork::lev2