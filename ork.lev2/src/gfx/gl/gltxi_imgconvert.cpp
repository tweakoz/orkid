////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>
#include <ork/kernel/datacache.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::_loadImageTexture(texture_ptr_t ptex, datablock_ptr_t src_datablock) {
  auto asset_load_req = ptex->loadRequest();
  DataBlockInputStream checkstream(src_datablock);
  uint8_t magic[4];
  magic[0]                     = checkstream.getItem<uint8_t>();
  magic[1]                     = checkstream.getItem<uint8_t>();
  magic[2]                     = checkstream.getItem<uint8_t>();
  magic[3]                     = checkstream.getItem<uint8_t>();
  bool rval                    = false;
  datablock_ptr_t xtx_datablock = nullptr;

  if (magic[1] == 'P' and //
      magic[2] == 'N' and //
      magic[3] == 'G') {

    auto basehasher = DataBlock::createHasher();
    basehasher->accumulateString("GlTextureInterface::_loadImageTexture");
    basehasher->accumulateString("png2xtx");
    basehasher->accumulateItem(src_datablock->hash());
    basehasher->finish();
    uint64_t hashkey = basehasher->result();
    xtx_datablock    = DataBlockCache::findDataBlock(hashkey);
    if(asset_load_req and asset_load_req->_on_event){
      auto data = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<datablock_ptr_t>("srcDatablock", src_datablock);
      data->makeValueForKey<datablock_ptr_t>("xtxDatablock", xtx_datablock);
      data->makeValueForKey<uint64_t>("hashKey", hashkey);
      data->makeValueForKey<std::string>("cachePath", DataBlockCache::_generateCachePath(hashkey));
      asset_load_req->_on_event("cacheCheck"_crcu, data);
    }

    if (xtx_datablock) {
      //printf("GlTextureInterface::_loadImageTexture tex<%p> precompressed!\n", ptex.get());
    } else {
      Image img;
      img.initFromInMemoryFile("png", src_datablock->data(), src_datablock->length());
      img._debugName = ptex->_debugName;
      auto cmipchain = img.compressedMipChainDefault();
      xtx_datablock  = std::make_shared<DataBlock>();
      cmipchain.writeXTX(xtx_datablock);
      DataBlockCache::setDataBlock(hashkey, xtx_datablock);
      // OrkAssert(false);
    }
  } else {
    printf("unknown texture<%s>\n", ptex->_debugName.c_str() );
    printf("magic0<%c>\n", magic[0]);
    printf("magic1<%c>\n", magic[1]);
    printf("magic2<%c>\n", magic[2]);
    printf("magic3<%c>\n", magic[3]);
    OrkAssert(false);
  }
  if (xtx_datablock) {
    rval = _loadXTXTexture(ptex, xtx_datablock);
  } else {
    OrkAssert(false);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
