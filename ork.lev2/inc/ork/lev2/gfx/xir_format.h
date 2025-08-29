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

} // namespace ork::lev2::xir