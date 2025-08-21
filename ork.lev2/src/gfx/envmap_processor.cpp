////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/envmap_processor.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/asset/Asset.inl>
#include <ork/kernel/timer.h>
#include <ork/file/file.h>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <atomic>
#include <unistd.h>

namespace ork::lev2 {

bool EnvMapProcessor::processToXIR(
    const file::Path& input_path,
    const file::Path& output_path) {
  
  auto future = processToXIRDataBlockAsync(input_path);
  auto xir_data = future->get();  // Block waiting for result
  if (!xir_data) {
    return false;
  }
  
  // Write to file
  auto result = File::saveDatablock(output_path, xir_data);
  return (result == EFEC_FILE_OK);
}

xirprocessfuture_ptr_t EnvMapProcessor::processToXIRDataBlockAsync(
    const file::Path& input_path) {
  
  // Create the future
  auto future = std::make_shared<XIRProcessFuture>();
  
  // Load source texture
  auto load_req = std::make_shared<asset::LoadRequest>(input_path);
  auto texasset = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    future->setResult(nullptr);
    return future;
  }
  
  auto rawenvmap = texasset->GetTexture();
  
  // Determine format from extension
  auto ext_str = input_path.getExtension();
  // Convert to lowercase manually
  std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
  bool is_equirectangular = (ext_str == ".exr" || ext_str == ".hdr");
  
  // Queue the operation for when context is available
  GfxEnv::GetRef().enqueueDeferredContextOp(
    [rawenvmap, is_equirectangular, future](Context* ctx) {
      // Filter environment maps (using new datablock-returning methods)
      auto specular_data = PBRMaterial::filterSpecularEnvMapToDataBlock(
          rawenvmap, ctx, is_equirectangular);
      auto diffuse_data = PBRMaterial::filterDiffuseEnvMapToDataBlock(
          rawenvmap, ctx, is_equirectangular);
      
      datablock_ptr_t result_data;
      if (specular_data && diffuse_data) {
        // Package as XIR
        result_data = xir::XIRWriter::writeIrradianceMaps(diffuse_data, specular_data);
      }
      
      // Set the result in the future
      future->setResult(result_data);
    });
  
  return future;
}

EnvMapProcessor::ProcessResult EnvMapProcessor::processDirectory(
    const file::Path& source_dir,
    const file::Path& output_dir,
    const std::vector<std::string>& extensions) {
  
  ProcessResult result;
  Timer timer;
  timer.Start();
  
  namespace bfs = boost::filesystem;
  if (!bfs::exists(source_dir.toBFS()) || !bfs::is_directory(source_dir.toBFS())) {
    result._error_message = "Source directory does not exist";
    return result;
  }
  
  // Create output directory
  output_dir.ensureDirectoryExists();
  
  int processed = 0;
  int failed = 0;
  
  // Process each file
  namespace bfs = boost::filesystem;
  for (const auto& ext : extensions) {
    // Use boost filesystem to iterate directory
    bfs::path dir_path = source_dir.toBFS();
    if (bfs::exists(dir_path) && bfs::is_directory(dir_path)) {
      for (auto& entry : bfs::directory_iterator(dir_path)) {
        if (bfs::is_regular_file(entry.path())) {
          file::Path input_file(entry.path());
          auto file_ext = input_file.getExtension();
          
          // Check if extension matches
          if (std::find(extensions.begin(), extensions.end(), file_ext) != extensions.end()) {
            auto stem = entry.path().stem().string();
            auto output_file = output_dir / FormatString("%s.xir", stem.c_str());
            
            if (processToXIR(input_file, output_file)) {
              processed++;
              // Get file size
              result._output_size += bfs::file_size(output_file.toBFS());
            } else {
              failed++;
            }
          }
        }
      }
    }
  }
  
  result._processing_time = timer.SecsSinceStart();
  result._success = (failed == 0);
  
  if (failed > 0) {
    result._error_message = FormatString("Failed to process %d files", failed);
  }
  
  return result;
}

} // namespace ork::lev2