////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <mdspan> the mac is ahead for once ?
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>
#include <math.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/memcpy.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

image_ptr_t Image::createFromFile(const std::string& inpath) {
  auto datablock = ork::File::loadDatablock(inpath);
  auto img = std::make_shared<Image>();
  img->initFromDataBlock(datablock);
  return img;
}

bool Image::readFromFile(const ork::file::Path& inpath) {
  auto datablock = ork::File::loadDatablock(inpath);
  return initFromDataBlock(datablock);
}

///////////////////////////////////////////////////////////////////////////////

bool Image::initFromDataBlock(datablock_ptr_t datablock) {
  DataBlockInputStream checkstream(datablock);
  uint32_t magic     = checkstream.getItem<uint32_t>();
  bool ok            = false;
  _contentHash = datablock->hash();
  if (Char4("chkf") == Char4(magic)) {
    //readXTX(datablock);
    _cmipchain = std::make_shared<CompressedImageMipChain>();
    _cmipchain->readXTX(datablock);
    _cmipchain->_levels[0].convertToImage(*this);
  } else if (Char4("DDS ") == Char4(magic)) {
    _cmipchain = std::make_shared<CompressedImageMipChain>();
    _cmipchain->readDDS(datablock);
    _cmipchain->_levels[0].convertToImage(*this);
  } else {
    DataBlockInputStream checkstream(datablock);
    uint8_t magic[4];
    magic[0] = checkstream.getItem<uint8_t>();
    magic[1] = checkstream.getItem<uint8_t>();
    magic[2] = checkstream.getItem<uint8_t>();
    magic[3] = checkstream.getItem<uint8_t>();
    if (magic[1] == 'P' and //
        magic[2] == 'N' and //
        magic[3] == 'G') {
      ok = _initFromDataBlockPNG(datablock);
      OrkAssert(false);
    } else {
      OrkAssert(false);
    }
    OrkAssert(false);
    // ok = _loadImageTexture(ptex, datablock);
  }
  return ok;
}

} // namespace ork::lev2 {
