////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <mdspan> the mac is ahead for once ?
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/image.h>
#include <math.h>
#include <limits>

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
  auto miplevgen = miplevelgen2D(imga._width, imga._height, 4);
  for (const auto& mipdim : miplevgen) {
    CompressedImage cimg;
    imga.compressBC7(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
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
  auto miplevgen = miplevelgen2D(imga._width, imga._height, 4);
  for (const auto& mipdim : miplevgen) {
    CompressedImage cimg;
    imga.compressBC7(cimg);
    rval->_levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
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
  auto miplevgen = miplevelgen2D(imga._width, imga._height, 4);
  for (const auto& mipdim : miplevgen) {
    CompressedImage cimg;
    imga.uncompressed(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// mip chain cursor. `mip` counts levels of miplevelgen2D(w,h,4); `row` counts
// rows of THAT level's downsample OUTPUT (half dimensions). Entering a level at
// row 0 also costs that level's copy into the chain, which is why it is charged
// against the budget here. See the header for why builder and step-plan share
// this.
///////////////////////////////////////////////////////////////////////////////

static size_t mipChainNumMips(size_t w, size_t h) {
  size_t n = 0;
  while (w >= 4 and h >= 4) {
    n++;
    w >>= 1;
    h >>= 1;
  }
  return n;
}

bool mipChainAdvance(size_t w, size_t h, size_t budget_px, size_t& mip, size_t& row) {
  const size_t nummips = mipChainNumMips(w, h);
  size_t spent         = 0;
  while (mip < nummips) {
    size_t lev_w = w >> mip;
    size_t lev_h = h >> mip;
    size_t out_w = lev_w >> 1;
    size_t out_h = lev_h >> 1;
    if (0 == row)
      spent += lev_w * lev_h;
    size_t affordable = (spent >= budget_px) ? 0 : (budget_px - spent) / std::max<size_t>(1, out_w);
    // at least one row per call, so a small budget still terminates
    size_t rows       = std::min(out_h - row, std::max<size_t>(1, affordable));
    row += rows;
    spent += rows * out_w;
    if (row >= out_h) {
      mip++;
      row = 0;
    }
    if (spent >= budget_px)
      break;
  }
  return mip < nummips;
}

size_t mipChainStepCount(size_t w, size_t h, size_t budget_px) {
  size_t mip = 0, row = 0, count = 1;
  while (mipChainAdvance(w, h, budget_px, mip, row))
    count++;
  return count;
}

///////////////////////////////////////////////////////////////////////////////

MipChainBuilder::MipChainBuilder(const Image& src) {
  _chain                  = std::make_shared<CompressedImageMipChain>();
  _chain->_width          = src._width;
  _chain->_height         = src._height;
  _chain->_format         = src._format;
  _chain->_numcomponents  = src._numcomponents;
  _chain->_bytesPerChannel = src._bytesPerChannel;
  _src_w                  = src._width;
  _src_h                  = src._height;
  _imga                   = src.clone();
}

bool MipChainBuilder::step(size_t budget_px) {
  size_t mip = _mip, row = _row;
  bool more  = mipChainAdvance(_src_w, _src_h, budget_px, mip, row);
  while ((_mip < mip) or ((_mip == mip) and (_row < row))) {
    if (0 == _row) {
      CompressedImage cimg;
      _imga.uncompressed(cimg);
      _chain->_levels.push_back(cimg);
      _imgb = _imga; // shares _imga's datablock; downsampleInit gives _imga a new one
      _imgb.downsampleInit(_imga);
    }
    size_t out_h = _imgb._height >> 1;
    size_t stop  = (_mip == mip) ? row : out_h;
    _imgb.downsampleRows(_imga, _row, stop, _downsample_queue);
    _row = stop;
    if (_row >= out_h) {
      _mip++;
      _row = 0;
    }
  }
  return more;
}

///////////////////////////////////////////////////////////////////////////////

compressedmipchain_ptr_t Image::uncompressedMipChain() const {

  auto hasher = DataBlock::createHasher();
  hasher->accumulateItem(_width);
  hasher->accumulateItem(_height);
  hasher->accumulateItem(_format);
  hasher->accumulateItem(_numcomponents);
  hasher->accumulateItem(_bytesPerChannel);
  hasher->accumulate(_data->data(), _data->length());
  hasher->finish();
  auto hash = hasher->result();
  if( hash == _contentHash )
    return _cmipchain;

  _contentHash = hash;
  MipChainBuilder builder(*this);
  while (builder.step(std::numeric_limits<size_t>::max())) {
  }
  _cmipchain = builder._chain;
  return _cmipchain;
}

compressedmipchain_ptr_t Image::uncompressedSingleMipChain() const {
  compressedmipchain_ptr_t rval = std::make_shared<CompressedImageMipChain>();
  rval->_width           = _width;
  rval->_height          = _height;
  rval->_format          = _format;
  rval->_numcomponents   = _numcomponents;
  rval->_bytesPerChannel = _bytesPerChannel;

  CompressedImage cimg;
  this->uncompressed(cimg);
  rval->_levels.push_back(cimg);

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
#if 1 //defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
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
#if 1 //defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
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