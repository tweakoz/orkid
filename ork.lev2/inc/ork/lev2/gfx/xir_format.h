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
// XIR (eXtended Radiance) Format
// 
// A chunked file format for storing pre-filtered environment maps
// Contains both diffuse and specular Radiance maps as compressed textures
////////////////////////////////////////////////////////////////////////////////

struct XIRWriter {
  // Write Radiance maps to XIR format datablock (legacy single specular)
  static datablock_ptr_t writeXirDatablocks(
      datablock_ptr_t diffuse_data,
      datablock_ptr_t specular_data);
      
  // Write Radiance maps with specular roughness array
  static datablock_ptr_t writeXirDatablocksWithArray(
      datablock_ptr_t diffuse_data,
      const std::vector<datablock_ptr_t>& specular_datablocks,
      const std::vector<float>& roughness_values);
};

////////////////////////////////////////////////////////////////////////////////
// XIRArrayWriter — the array-format container built ACROSS CALLS.
//
// A refilter cycle's container is one megabyte per roughness level plus the
// diffuse chain; assembling it in one go is a double copy of the lot (payload
// into the stream, streams into the datablock) and far more than a frame.
// Levels are added as they are serialized and the container is emitted one
// stream per emitNext(). writeXirDatablocksWithArray IS this run to
// completion, so the bytes do not depend on how the caller paces it.
//
// Levels MUST be added in ascending index order: the stream order is the
// container's chunk order.
////////////////////////////////////////////////////////////////////////////////

struct XIRArrayWriter {

  XIRArrayWriter(
      datablock_ptr_t diffuse_data,
      const std::vector<float>& roughness_values,
      size_t num_specular_levels);

  void addSpecularLevel(size_t index, datablock_ptr_t specular_datablock);

  // header, then one stream payload per call; false once the container is done
  bool emitNext(datablock_ptr_t& out_datablock);
  bool complete() const;

  // how many emitNext() calls a container of `num_specular_levels` takes — a
  // step plan needs this before the first level exists
  static size_t emitStepCount(size_t num_specular_levels);

  chunkfile::Writer _writer;
  size_t _num_specular_levels = 0;
  size_t _num_added           = 0;
  size_t _emit_cursor         = 0;
  bool _has_diffuse           = false;
};

using xirarraywriter_ptr_t = std::shared_ptr<XIRArrayWriter>;

////////////////////////////////////////////////////////////////////////////////

struct XIRReader {

  // Read Radiance maps from XIR format - returns raw datablocks for deferred loading
  struct XIRData {
    datablock_ptr_t _diffuse_data;
    datablock_ptr_t _specular_data;  // Legacy single specular
    std::vector<datablock_ptr_t> _specular_datablocks;  // New: array of roughness levels
    std::vector<float> _roughness_values;  // New: roughness value per slice
    int _num_roughness_levels = 0;  // New: count of roughness levels
    bool _valid = false;
    bool _is_array_format = false;  // New: flag to indicate new format
  };
  
  static XIRData readXirDatablocks(datablock_ptr_t xir_data);
};

////////////////////////////////////////////////////////////////////////////////
// XIRArrayReader — the array-format container decoded ACROSS CALLS.
//
// The mirror of XIRArrayWriter, and for the same reason: the container header,
// roughness values and diffuse block are read on construction, each roughness
// level's ~megabyte extraction is its own call. readXirDatablocks IS this run
// to completion (v2 containers), so a sliced consumer sees the same bytes as
// the load path.
//
// _data._valid means "the container parsed"; it does NOT mean every level has
// been read — readSpecularLevel is what fills those in.
////////////////////////////////////////////////////////////////////////////////

struct XIRArrayReader {

  XIRArrayReader(datablock_ptr_t xir_data);
  ~XIRArrayReader();

  // extracts level `level` and appends it to _data._specular_datablocks
  void readSpecularLevel(int level);

  XIRReader::XIRData _data;
  chunkfile::DefaultLoadAllocator _allocator;
  std::shared_ptr<chunkfile::Reader> _reader;
};

using xirarrayreader_ptr_t = std::shared_ptr<XIRArrayReader>;

} // namespace ork::lev2::xir