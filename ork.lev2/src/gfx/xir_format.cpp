////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/util/logger.h>
#include <ork/util/hexdump.inl>
#include <ork/kernel/datablock.h>

namespace ork::lev2::xir {
static logchannel_ptr_t logchan_xirio = logger()->configureChannel("XIRIO", fvec3(0.8, 0.8, 0.1), true);

////////////////////////////////////////////////////////////////////////////////
// XIRWriter implementation
////////////////////////////////////////////////////////////////////////////////

datablock_ptr_t XIRWriter::writeXirDatablocks(
    datablock_ptr_t diffuse_data,
    datablock_ptr_t specular_data) {
  
  logchan_xirio->log("specular datablock: %zu bytes", specular_data->length());
  hexdumpbytes(specular_data->data(), std::min<size_t>(specular_data->length(), 64));
  logchan_xirio->log("diffuse datablock: %zu bytes", diffuse_data->length());
  hexdumpbytes(diffuse_data->data(), std::min<size_t>(diffuse_data->length(), 64));

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

  logchan_xirio->log("xir datablock: %zu bytes", result->length());
  hexdumpbytes(result->data(), std::min<size_t>(result->length(), 64));

  return result;
}

datablock_ptr_t XIRWriter::writeXirDatablocksWithArray(
    datablock_ptr_t diffuse_data,
    const std::vector<datablock_ptr_t>& specular_datablocks,
    const std::vector<float>& roughness_values) {
  
  logchan_xirio->log("Writing XIR with %zu specular roughness levels", specular_datablocks.size());
  
  chunkfile::Writer writer("xir-2.0");  // New version for array format
  
  // Add metadata
  auto meta_stream = writer.AddStream("metadata");
  meta_stream->AddItem<uint32_t>(2); // version 2 for array format
  meta_stream->AddItem<uint32_t>(specular_datablocks.size()); // number of roughness levels
  
  // Add roughness values
  auto roughness_stream = writer.AddStream("roughness_values");
  for (float r : roughness_values) {
    roughness_stream->AddItem<float>(r);
  }
  
  // Add diffuse texture data (optional)
  if (diffuse_data && diffuse_data->length() > 0) {
    auto diffuse_stream = writer.AddStream("diffuse");
    diffuse_stream->AddDataBlock(diffuse_data);
  }
  
  // Add each specular roughness level
  for (size_t i = 0; i < specular_datablocks.size(); i++) {
    std::string stream_name = FormatString("specular_%zu", i);
    auto spec_stream = writer.AddStream(stream_name.c_str());
    spec_stream->AddDataBlock(specular_datablocks[i]);
  }
  
  datablock_ptr_t result = std::make_shared<DataBlock>();
  writer.writeToDataBlock(result);
  
  logchan_xirio->log("XIR array format datablock: %zu bytes", result->length());
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// XIRReader implementation
////////////////////////////////////////////////////////////////////////////////


XIRReader::XIRData XIRReader::readXirDatablocks(datablock_ptr_t xir_data) {
  XIRData result;
  
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader reader(xir_data, allocator);
  
  // Check version
  if (reader._chunkfiletype == "xir-2.0") {
    // New array format
    result._is_array_format = true;
    
    // Read metadata
    if (auto meta_stream = reader.GetStream("metadata")) {
      uint32_t version = 0;
      uint32_t num_levels = 0;
      meta_stream->GetItem(version);
      meta_stream->GetItem(num_levels);
      result._num_roughness_levels = num_levels;
      
      logchan_xirio->log("Reading XIR v2 with %d roughness levels", num_levels);
    }
    
    // Read roughness values
    if (auto roughness_stream = reader.GetStream("roughness_values")) {
      result._roughness_values.resize(result._num_roughness_levels);
      for (int i = 0; i < result._num_roughness_levels; i++) {
        roughness_stream->GetItem(result._roughness_values[i]);
      }
    }
    
    // Read diffuse
    if (auto stream = reader.GetStream("diffuse")) {
      auto data = stream->readData(stream->GetLength());
      result._diffuse_data = std::make_shared<DataBlock>(data.data(), data.size());
    }
    
    // Read each specular roughness level
    for (int i = 0; i < result._num_roughness_levels; i++) {
      std::string stream_name = FormatString("specular_%d", i);
      if (auto stream = reader.GetStream(stream_name.c_str())) {
        auto data = stream->readData(stream->GetLength());
        result._specular_datablocks.push_back(
          std::make_shared<DataBlock>(data.data(), data.size())
        );
      }
    }
    
    result._valid = (result._specular_datablocks.size() == result._num_roughness_levels);
    
  } else if (reader._chunkfiletype == "xir-1.0") {
    // Legacy format
    result._is_array_format = false;
    
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
  }
  
  return result;
}

} // namespace ork::lev2::xir