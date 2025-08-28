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
  static datablock_ptr_t writeXirDatablocks(
      datablock_ptr_t diffuse_data,
      datablock_ptr_t specular_data);
};

struct XIRReader {
  
  // Read irradiance maps from XIR format - returns raw datablocks for deferred loading
  struct XIRData {
    datablock_ptr_t _diffuse_data;
    datablock_ptr_t _specular_data;
    bool _valid = false;
  };
  
  static XIRData readXirDatablocks(datablock_ptr_t xir_data);
};

} // namespace ork::lev2::xir