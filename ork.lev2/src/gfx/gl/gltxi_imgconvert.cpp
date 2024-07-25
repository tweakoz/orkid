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
  auto asset_load_req    = ptex->loadRequest();
  ptex->_final_datablock = src_datablock;
  DataBlockInputStream checkstream(src_datablock);
  uint8_t magic[4];
  magic[0]                      = checkstream.getItem<uint8_t>();
  magic[1]                      = checkstream.getItem<uint8_t>();
  magic[2]                      = checkstream.getItem<uint8_t>();
  magic[3]                      = checkstream.getItem<uint8_t>();
  bool rval                     = false;
  datablock_ptr_t xtx_datablock = nullptr;

  /////////////////////////////////////////////
  // check if it is a PNG
  //  technically we should allow jpegs and 
  //     other non-gpu formats as well ...
  /////////////////////////////////////////////
  if (magic[1] == 'P' and //
      magic[2] == 'N' and //
      magic[3] == 'G') {

    /////////////////////////////////////////////
    // hash the png permutation state to
    //   see if we have a precompressed xtx
    /////////////////////////////////////////////
    auto basehasher = DataBlock::createHasher();
    basehasher->accumulateString("GlTextureInterface::_loadImageTexture");
    basehasher->accumulateString("png2xtx-v6");
    basehasher->accumulateItem(src_datablock->hash());
    auto forced_format = EBufferFormat::NONE;
    if (asset_load_req) {
      auto try_force_fmt = asset_load_req->_asset_vars->typedValueForKey<EBufferFormat>("force-format");
      if (try_force_fmt) {
        forced_format = try_force_fmt.value();
      }
    }
    basehasher->accumulateItem(forced_format);
    basehasher->finish();
    /////////////////////////////////////////////
    // check cache
    /////////////////////////////////////////////
    uint64_t hashkey = basehasher->result();
    xtx_datablock    = DataBlockCache::findDataBlock(hashkey);
    /////////////////////////////////////////////
    // cacheCheck load event
    /////////////////////////////////////////////
    if (asset_load_req and asset_load_req->_on_event) {
      auto data = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<datablock_ptr_t>("srcDatablock", src_datablock);
      data->makeValueForKey<datablock_ptr_t>("xtxDatablock", xtx_datablock);
      data->makeValueForKey<uint64_t>("hashKey", hashkey);
      data->makeValueForKey<std::string>("cachePath", DataBlockCache::_generateCachePath(hashkey));
      asset_load_req->_on_event("cacheCheck"_crcu, data);
    }
    /////////////////////////////////////////////
    // is it cached ?
    /////////////////////////////////////////////
    if (xtx_datablock) {
      printf("GlTextureInterface::_loadImageTexture tex<%p:%s> precached!\n", ptex.get(), ptex->_debugName.c_str());
    }
    /////////////////////////////////////////////
    // it is not cached...
    /////////////////////////////////////////////
    else {
      ////////////////////////////
      // load raw image
      ////////////////////////////
      Image img;
      img.initFromInMemoryFile("png", src_datablock->data(), src_datablock->length());
      ////////////////////////////
      // check for requested forced format
      ////////////////////////////
      auto forced_format = EBufferFormat::NONE;
      if (asset_load_req) {
        auto try_force_fmt = asset_load_req->_asset_vars->typedValueForKey<EBufferFormat>("force-format");
        if (try_force_fmt) {
          forced_format = try_force_fmt.value();
        }
      }
      ////////////////////////////
      // load raw image
      ////////////////////////////
      auto image_fmt = img._format;
      auto fmt_str   = EBufferFormatToName(image_fmt);
      img._debugName = ptex->_debugName;
      xtx_datablock  = std::make_shared<DataBlock>();
      switch (forced_format) {
        //////////////////////////////
        // no requested format
        //  use default logic
        //////////////////////////////
        case EBufferFormat::NONE: {
          //printf("writing xtx datablock default fmt<%s>\n", fmt_str.c_str());
          auto cmipchain = img.compressedMipChainDefault();
          cmipchain->writeXTX(xtx_datablock);
          break;
        }
        //////////////////////////////
        // BC7 explicit request
        //////////////////////////////
#if ! defined(__APPLE__)
        case EBufferFormat::RGBA_BPTC_UNORM: {
          auto orig_fmt_str = EBufferFormatToName(img._format);
          auto forc_fmt_str = EBufferFormatToName(forced_format);
          //printf("writing xtx : forcing format orig<%s> newfmt<%s>\n", orig_fmt_str.c_str(), forc_fmt_str.c_str());
          auto cmipchain = img.compressedMipChainBC7();
          cmipchain->writeXTX(xtx_datablock);
          break;
        }
#endif
        case EBufferFormat::RGB8:
        case EBufferFormat::BGR8:
        case EBufferFormat::BGRA8:
        case EBufferFormat::RGBA8:{
          auto orig_fmt_str = EBufferFormatToName(img._format);
          auto forc_fmt_str = EBufferFormatToName(forced_format);
          //printf("writing xtx : forcing format orig<%s> newfmt<%s>\n", orig_fmt_str.c_str(), forc_fmt_str.c_str());
          auto converted_img = img;//.convertToFormat(forced_format);
          converted_img._format = forced_format;
          auto cmipchain = converted_img.uncompressedMipChain();
          cmipchain->writeXTX(xtx_datablock);
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
      DataBlockCache::setDataBlock(hashkey, xtx_datablock);
      // OrkAssert(false);
    } // it is not cached

  } else {
    printf("unknown texture<%s>\n", ptex->_debugName.c_str());
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
