////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/kernel/datacache.inl>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::_loadImageTexture(Texture* ptex, datablockptr_t src_datablock) {
  DataBlockInputStream checkstream(src_datablock);
  uint8_t magic[4];
  magic[0]                     = checkstream.getItem<uint8_t>();
  magic[1]                     = checkstream.getItem<uint8_t>();
  magic[2]                     = checkstream.getItem<uint8_t>();
  magic[3]                     = checkstream.getItem<uint8_t>();
  bool rval                    = false;
  datablockptr_t xtx_datablock = nullptr;

  printf("magic0<%c>\n", magic[0]);
  printf("magic1<%c>\n", magic[1]);
  printf("magic2<%c>\n", magic[2]);
  printf("magic3<%c>\n", magic[3]);

  if (magic[1] == 'P' and magic[2] == 'N' and magic[3] == 'G') {

    boost::Crc64 basehasher;
    basehasher.accumulateString("GlTextureInterface::_loadImageTexture");
    basehasher.accumulateString("png2xtx");
    basehasher.accumulateItem(src_datablock->hash());
    basehasher.finish();

    uint64_t hashkey = basehasher.result();
    xtx_datablock    = DataBlockCache::findDataBlock(hashkey);

    if (not xtx_datablock) {
      Image img;
      img.initFromInMemoryFile("png", src_datablock->data(), src_datablock->length());
      img._debugName = ptex->_debugName;
      auto cimgchain = img.compressedMipChainBC7();
      xtx_datablock  = std::make_shared<DataBlock>();
      cimgchain.writeXTX(xtx_datablock);
      DataBlockCache::setDataBlock(hashkey, xtx_datablock);
    }
  } else {
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
