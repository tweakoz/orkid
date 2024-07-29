////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <mdspan>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>
#include <math.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/memcpy.inl>
#include <ork/gfx/dds.h>

namespace ork::lev2 {


///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::readDDS(datablock_ptr_t datablock) {
  OrkAssert(datablock);
  DataBlockInputStream inpstream;
  inpstream._datablock = datablock;
  inpstream.advance(sizeof(dds::DDS_HEADER));
  auto ddsh        = (const dds::DDS_HEADER*)inpstream.data(0);
  int width        = ddsh->dwWidth;
  int height       = ddsh->dwHeight;
  int depth        = (ddsh->dwDepth > 1) ? ddsh->dwDepth : 1;
  int num_mips     = (ddsh->dwFlags & dds::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
  int block_width  = (width + 3) / 4;
  int block_height = (height + 3) / 4;
  bool is_volume   = (depth > 1);
  int flags        = ddsh->dwFlags;
  int fourcc       = ddsh->ddspf.dwFourCC;
  int bpp          = ddsh->ddspf.dwRGBBitCount;
  int rmask        = ddsh->ddspf.dwRBitMask;
  int gmask        = ddsh->ddspf.dwGBitMask;
  int bmask        = ddsh->ddspf.dwBBitMask;

  _width  = width;
  _height = height;
  _depth  = depth;

  if (0)
    printf(
        "width<%d> height<%d> depth<%d> num_mips<%d> block_width<%d> block_height<%d> is_volume<%d> flags<%08x> fourcc<%08x> "
        "bpp<%d> "
        "rmask<%08x> gmask<%08x> bmask<%08x>\n",
        width,
        height,
        depth,
        num_mips,
        block_width,
        block_height,
        is_volume,
        flags,
        fourcc,
        bpp,
        rmask,
        gmask,
        bmask);

  if (dds::IsLUM(ddsh->ddspf)) {
    int size = width * height;
    _format  = EBufferFormat::R8;
    printf("DDS.LUM\n");
    OrkAssert(false);

  }
  //////////////////////////////////////////////////////////////////////
  else if (dds::IsBGRA8(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    int BPP                    = 4;
    int size                   = width * height * BPP;
    _format                    = EBufferFormat::BGRA8;
    _numcomponents             = 4;
    _bytesPerChannel           = 1;
    // printf("DDS.BGRA8\n");
    if (num_mips == 1) {
      Image imga, imgb;
      imga._data            = std::make_shared<DataBlock>(inpstream.current(), size);
      imga._format          = _format;
      imga._width           = width;
      imga._height          = height;
      imga._depth           = depth;
      imga._numcomponents   = 4;
      imga._bytesPerChannel = 1;
      int mipindex          = 0;
      while ((imga._width >= 4) and (imga._height >= 4)) {
        CompressedImage cimg;
        imga.uncompressed(cimg);
        _levels.push_back(cimg);
        imgb = imga;
        imgb.downsample(imga);
        mipindex++;
      }
    } else {
      for (int lidx = 0; lidx < num_mips; lidx++) {
        auto mipdata = inpstream.current();
        CompressedImage level;
        level._data            = std::make_shared<DataBlock>(mipdata, size);
        level._format          = _format;
        level._width           = width;
        level._height          = height;
        level._depth           = depth;
        level._blocked_width   = width;
        level._blocked_height  = height;
        level._numcomponents   = 4;
        level._bytesPerChannel = 1;
        _levels.push_back(level);
        width >>= 1;
        height >>= 1;
        size = width * height * BPP;
        inpstream.advance(size);
      }
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (dds::IsBGR8(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGR8;
    int BPP                    = 3;
    int size                   = width * height * BPP;
    _format                    = EBufferFormat::BGR8;
    _numcomponents             = 3;
    // printf("DDS.BGR8\n");
    for (int lidx = 0; lidx < num_mips; lidx++) {
      auto mipdata = inpstream.current();
      CompressedImage level;
      level._data            = std::make_shared<DataBlock>(mipdata, size);
      level._format          = _format;
      level._width           = width;
      level._height          = height;
      level._depth           = depth;
      level._blocked_width   = width;
      level._blocked_height  = height;
      level._numcomponents   = 3;
      level._bytesPerChannel = 1;
      _levels.push_back(level);
      width >>= 1;
      height >>= 1;
      size = width * height * BPP;
      inpstream.advance(size);
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (dds::IsDXT5(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT5;
    int size                   = (block_width * block_height) * li.blockBytes;
    _format                    = EBufferFormat::S3TC_DXT5;
    printf("DDS.DXT5\n");
    OrkAssert(false);
  } else if (dds::IsDXT3(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT3;
    int size                   = (block_width * block_height) * li.blockBytes;
    _format                    = EBufferFormat::S3TC_DXT3;
    printf("DDS.DXT3\n");
    OrkAssert(false);
  } else if (dds::IsDXT1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    int size                   = (block_width * block_height) * li.blockBytes;
    _format                    = EBufferFormat::S3TC_DXT1;
    printf("DDS.DXT1\n");
    OrkAssert(false);
  } else {
    OrkAssert(false);
  }
}

} //  namespace ork::lev2 {