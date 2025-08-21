////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/datablock.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

namespace ork::lev2::xir {

////////////////////////////////////////////////////////////////////////////////
// XIR (eXtended Irradiance) Format
// 
// A chunked file format for storing pre-filtered environment maps
// Contains both diffuse and specular irradiance maps as compressed textures
////////////////////////////////////////////////////////////////////////////////

struct XIRWriter {
  // Write irradiance maps to XIR format datablock
  static datablock_ptr_t writeIrradianceMaps(
      datablock_ptr_t diffuse_data,
      datablock_ptr_t specular_data) {
    
    chunkfile::Writer writer("xir-1.0");
    
    // Add metadata
    auto meta_stream = writer.AddStream("metadata");
    meta_stream->AddItem<uint32_t>(1); // version
    
    // Add texture data
    auto diffuse_stream = writer.AddStream("diffuse");
    diffuse_stream->AddDataBlock(diffuse_data);
    
    auto specular_stream = writer.AddStream("specular");
    specular_stream->AddDataBlock(specular_data);
    
    datablock_ptr_t result = std::make_shared<DataBlock>();
    writer.writeToDataBlock(result);
    return result;
  }
};

struct XIRReader {
  // Read irradiance maps from XIR format - creates textures immediately with context
  static pbr::irradiancemaps_ptr_t readIrradianceMaps(
      datablock_ptr_t xir_data,
      Context* context) {
    
    chunkfile::DefaultLoadAllocator allocator;
    chunkfile::Reader reader(xir_data, allocator);
    
    if (reader._chunkfiletype != "xir-1.0") {
      return nullptr;
    }
    
    auto maps = std::make_shared<pbr::IrradianceMaps>();
    auto txi = context->TXI();
    
    // Load diffuse
    if (auto stream = reader.GetStream("diffuse")) {
      auto data = stream->readData(stream->GetLength());
      auto dblock = std::make_shared<DataBlock>(data.data(), data.size());
      maps->_filtenvDiffuseMap = std::make_shared<Texture>();
      txi->LoadTexture(maps->_filtenvDiffuseMap, dblock);
    }
    
    // Load specular
    if (auto stream = reader.GetStream("specular")) {
      auto data = stream->readData(stream->GetLength());
      auto dblock = std::make_shared<DataBlock>(data.data(), data.size());
      maps->_filtenvSpecularMap = std::make_shared<Texture>();
      txi->LoadTexture(maps->_filtenvSpecularMap, dblock);
    }
    
    return maps;
  }
  
  // Read irradiance maps from XIR format - returns raw datablocks for deferred loading
  struct XIRData {
    datablock_ptr_t _diffuse_data;
    datablock_ptr_t _specular_data;
    bool _valid = false;
  };
  
  static XIRData readIrradianceDatablocks(datablock_ptr_t xir_data) {
    XIRData result;
    
    chunkfile::DefaultLoadAllocator allocator;
    chunkfile::Reader reader(xir_data, allocator);
    
    if (reader._chunkfiletype != "xir-1.0") {
      return result;
    }
    
    // Get raw datablocks
    if (auto stream = reader.GetStream("diffuse")) {
      auto data = stream->readData(stream->GetLength());
      result._diffuse_data = std::make_shared<DataBlock>(data.data(), data.size());
    }
    
    if (auto stream = reader.GetStream("specular")) {
      auto data = stream->readData(stream->GetLength());
      result._specular_data = std::make_shared<DataBlock>(data.data(), data.size());
    }
    
    result._valid = result._diffuse_data && result._specular_data;
    return result;
  }
};

} // namespace ork::lev2::xir