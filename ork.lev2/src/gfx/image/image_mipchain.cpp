////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>
#include <math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_ISPC)

CompressedImageMipChain Image::compressedMipChainBC7_b() const {
  CompressedImageMipChain rval;
  rval._width         = _width;
  rval._height        = _height;
  rval._format        = EBufferFormat::RGBA_BPTC_UNORM;
  rval._numcomponents = 4;
  Image imga          = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.compressBC7(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}

compressedmipchain_ptr_t Image::compressedMipChainBC7() const {
  compressedmipchain_ptr_t rval = std::make_shared<CompressedImageMipChain>();
  rval->_width         = _width;
  rval->_height        = _height;
  rval->_format        = EBufferFormat::RGBA_BPTC_UNORM;
  rval->_numcomponents = 4;
  Image imga          = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.compressBC7(cimg);
    rval->_levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}
#endif

///////////////////////////////////////////////////////////////////////////////

CompressedImageMipChain Image::uncompressedMipChain_b() const {
  CompressedImageMipChain rval;
  rval._width           = _width;
  rval._height          = _height;
  rval._format          = _format;
  rval._numcomponents   = _numcomponents;
  rval._bytesPerChannel = _bytesPerChannel;
  Image imga            = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.uncompressed(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}

compressedmipchain_ptr_t Image::uncompressedMipChain() const {
  compressedmipchain_ptr_t rval = std::make_shared<CompressedImageMipChain>();
  rval->_width           = _width;
  rval->_height          = _height;
  rval->_format          = _format;
  rval->_numcomponents   = _numcomponents;
  rval->_bytesPerChannel = _bytesPerChannel;
  Image imga            = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.uncompressed(cimg);
    rval->_levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::initWithPrecompressedMipLevels(miplevels_t levels) {
  _levels          = levels;
  _width           = levels[0]._width;
  _height          = levels[0]._height;
  _format          = levels[0]._format;
  _numcomponents   = levels[0]._numcomponents;
  _bytesPerChannel = levels[0]._bytesPerChannel;
}

///////////////////////////////////////////////////////////////////////////////

CompressedImageMipChain Image::compressedMipChainDefault_b() const {
#if defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
  return uncompressedMipChain_b();
#else
  if (GfxEnv::supportsBC7()) {
    return compressedMipChainBC7_b();
  } else {
    return uncompressedMipChain_b();
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

compressedmipchain_ptr_t Image::compressedMipChainDefault() const {
#if defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
  return uncompressedMipChain();
#else
  if (GfxEnv::supportsBC7()) {
    return compressedMipChainBC7();
  } else {
    return uncompressedMipChain();
  }
#endif
}

} // namespace ork::lev2 {