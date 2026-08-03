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
static logchannel_ptr_t logchan_xirio = logger()->configureChannel("XIRIO", fvec3(0.8, 0.8, 0.1), false);

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

XIRArrayWriter::XIRArrayWriter(
    datablock_ptr_t diffuse_data,
    const std::vector<float>& roughness_values,
    size_t num_specular_levels)
    : _writer("xir-2.0") // New version for array format
    , _num_specular_levels(num_specular_levels) {

  logchan_xirio->log("Writing XIR with %zu specular roughness levels", num_specular_levels);

  // Add metadata
  auto meta_stream = _writer.AddStream("metadata");
  meta_stream->AddItem<uint32_t>(2); // version 2 for array format
  meta_stream->AddItem<uint32_t>(num_specular_levels); // number of roughness levels

  // Add roughness values
  auto roughness_stream = _writer.AddStream("roughness_values");
  for (float r : roughness_values) {
    roughness_stream->AddItem<float>(r);
  }

  // Add diffuse texture data (optional)
  _has_diffuse = (diffuse_data && diffuse_data->length() > 0);
  if (_has_diffuse) {
    auto diffuse_stream = _writer.AddStream("diffuse");
    diffuse_stream->AddDataBlock(diffuse_data);
  }
}

void XIRArrayWriter::addSpecularLevel(size_t index, datablock_ptr_t specular_datablock) {
  // ascending order or the container's chunk order is not the level order
  OrkAssert(index == _num_added);
  OrkAssert(index < _num_specular_levels);
  std::string stream_name = FormatString("specular_%zu", index);
  auto spec_stream        = _writer.AddStream(stream_name.c_str());
  spec_stream->AddDataBlock(specular_datablock);
  _num_added++;
}

size_t XIRArrayWriter::emitStepCount(size_t num_specular_levels) {
  // header + metadata + roughness_values + diffuse + one per level
  return 4 + num_specular_levels;
}

bool XIRArrayWriter::complete() const {
  return _emit_cursor > _writer.numStreams();
}

bool XIRArrayWriter::emitNext(datablock_ptr_t& out_datablock) {
  OrkAssert(_num_added == _num_specular_levels); // every level in before any byte goes out
  if (complete())
    return false;
  if (0 == _emit_cursor)
    _writer.writeHeaderToDataBlock(out_datablock);
  else
    _writer.appendStreamToDataBlock(out_datablock, _emit_cursor - 1);
  _emit_cursor++;
  if (complete())
    logchan_xirio->log("XIR array format datablock: %zu bytes", out_datablock->length());
  return true;
}

datablock_ptr_t XIRWriter::writeXirDatablocksWithArray(
    datablock_ptr_t diffuse_data,
    const std::vector<datablock_ptr_t>& specular_datablocks,
    const std::vector<float>& roughness_values) {

  XIRArrayWriter writer(diffuse_data, roughness_values, specular_datablocks.size());
  for (size_t i = 0; i < specular_datablocks.size(); i++)
    writer.addSpecularLevel(i, specular_datablocks[i]);

  datablock_ptr_t result = std::make_shared<DataBlock>();
  while (writer.emitNext(result)) {
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// XIRReader implementation
////////////////////////////////////////////////////////////////////////////////


XIRArrayReader::XIRArrayReader(datablock_ptr_t xir_data) {
  _reader = std::make_shared<chunkfile::Reader>(xir_data, _allocator);
  if (_reader->_chunkfiletype != "xir-2.0")
    return;
  _data._is_array_format = true;

  // Read metadata
  if (auto meta_stream = _reader->GetStream("metadata")) {
    uint32_t version = 0;
    uint32_t num_levels = 0;
    meta_stream->GetItem(version);
    meta_stream->GetItem(num_levels);
    _data._num_roughness_levels = num_levels;

    logchan_xirio->log("Reading XIR v2 with %d roughness levels", num_levels);
  }

  // Read roughness values
  if (auto roughness_stream = _reader->GetStream("roughness_values")) {
    _data._roughness_values.resize(_data._num_roughness_levels);
    for (int i = 0; i < _data._num_roughness_levels; i++) {
      roughness_stream->GetItem(_data._roughness_values[i]);
    }
  }

  // Read diffuse
  if (auto stream = _reader->GetStream("diffuse")) {
    auto data = stream->readData(stream->GetLength());
    _data._diffuse_data = std::make_shared<DataBlock>(data.data(), data.size());
  }

  _data._valid = (_data._num_roughness_levels > 0);
}

XIRArrayReader::~XIRArrayReader() {
}

void XIRArrayReader::readSpecularLevel(int level) {
  if (not _data._is_array_format)
    return;
  std::string stream_name = FormatString("specular_%d", level);
  if (auto stream = _reader->GetStream(stream_name.c_str())) {
    auto data = stream->readData(stream->GetLength());
    _data._specular_datablocks.push_back(
      std::make_shared<DataBlock>(data.data(), data.size())
    );
  }
}

////////////////////////////////////////////////////////////////////////////////

XIRReader::XIRData XIRReader::readXirDatablocks(datablock_ptr_t xir_data) {
  XIRArrayReader array_reader(xir_data);
  if (array_reader._data._is_array_format) {
    // New array format
    for (int i = 0; i < array_reader._data._num_roughness_levels; i++)
      array_reader.readSpecularLevel(i);
    auto result = array_reader._data;
    result._valid = (result._specular_datablocks.size() == result._num_roughness_levels);
    return result;
  }

  XIRData result;
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader reader(xir_data, allocator);

  if (reader._chunkfiletype == "xir-1.0") {
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