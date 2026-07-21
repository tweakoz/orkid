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
  datablock->_name = inpath; // carry path into initFromDataBlock's self-defend error
  auto img = std::make_shared<Image>();
  img->initFromDataBlock(datablock);
  return img;
}

bool Image::readFromFile(const ork::file::Path& inpath) {
  auto datablock = ork::File::loadDatablock(inpath);
  datablock->_name = inpath.c_str(); // carry path into initFromDataBlock's self-defend error
  bool ok = initFromDataBlock(datablock);
  _debugName = inpath.c_str();
  if (not ok) {
    _debugName += "_error";
  }
  return ok;
}

///////////////////////////////////////////////////////////////////////////////

bool Image::initFromDataBlock(datablock_ptr_t datablock) {
  // self-defend (ops fail-loud): a 0-byte / truncated datablock would walk cursor math
  // straight into getItem<>'s OrkAssert, whose -O2 force-segfault (often with an unflushed
  // banner) yields a backtrace far from the real cause (a 0-byte PNG once masqueraded as a
  // PBR env-map crash). Refuse with a named, catchable error carrying path/size BEFORE any
  // cursor read.
  size_t dblen = datablock ? datablock->length() : 0;
  if (dblen < 4) {
    throw std::runtime_error(FormatString(
        "Image::initFromDataBlock: refusing empty/truncated image datablock path<%s> size<%zu> (need >= 4 magic bytes)",
        datablock ? datablock->_name.c_str() : "<null>",
        dblen));
  }
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
    } else if (magic[0] == 0x76 and magic[1] == 0x2f and magic[2] == 0x31 and magic[3] == 0x01) {
      // EXR magic bytes
      ok = initFromInMemoryFile("exr", datablock->data(), datablock->length());
    } else if (magic[0] == '#' and magic[1] == '?') {
      // HDR/RGBE magic bytes
      ok = initFromInMemoryFile("hdr", datablock->data(), datablock->length());
    } else {
      // self-defend (ops fail-loud): unrecognized magic -> named, catchable error
      // (not an OrkAssert force-segfault) carrying path/size/magic.
      throw std::runtime_error(FormatString(
          "Image::initFromDataBlock: unrecognized image format path<%s> size<%zu> magic<%02x %02x %02x %02x>",
          datablock->_name.c_str(),
          datablock->length(),
          magic[0],
          magic[1],
          magic[2],
          magic[3]));
    }
    // ok = _loadImageTexture(ptex, datablock);
  }
  return ok;
}

} // namespace ork::lev2 {
