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

////////////////////////////////////////////////////////////////////////////////
// XIRReader implementation
////////////////////////////////////////////////////////////////////////////////


XIRReader::XIRData XIRReader::readXirDatablocks(datablock_ptr_t xir_data) {
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

} // namespace ork::lev2::xir