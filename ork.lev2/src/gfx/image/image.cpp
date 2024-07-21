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
#include <ork/kernel/memcpy.inl>
#include <math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void Image::init(size_t w, size_t h, size_t numc, int bpc) {
  _numcomponents   = numc;
  _width           = w;
  _height          = h;
  _bytesPerChannel = bpc;
  _data            = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents * bpc);
}

///////////////////////////////////////////////////////////////////////////////

Image Image::clone() const {
  Image rval;
  rval._format          = _format;
  rval._bytesPerChannel = _bytesPerChannel;
  rval._numcomponents   = _numcomponents;
  rval._width           = _width;
  rval._height          = _height;
  rval._data            = std::make_shared<DataBlock>(_data->data(), _data->length());
  rval._debugName       = _debugName;
  return rval;
}

static fvec3 _image_deco(0.1, 0.2, 0.3);

///////////////////////////////////////////////////////////////////////////////

uint8_t* Image::pixel8(int x, int y) {
  int index = (y * _width + x) * _numcomponents;
  return ((uint8_t*)_data->data()) + index;
}
const uint8_t* Image::pixel8(int x, int y) const {
  int index = (y * _width + x) * _numcomponents;
  return ((const uint8_t*)_data->data()) + index;
}
uint16_t* Image::pixel16(int x, int y) {
  int index = (y * _width + x) * _numcomponents;
  return ((uint16_t*)_data->data()) + index;
}
const uint16_t* Image::pixel16(int x, int y) const {
  int index = (y * _width + x) * _numcomponents;
  return ((const uint16_t*)_data->data()) + index;
}

///////////////////////////////////////////////////////////////////////////////

void Image::initRGB8WithColor(size_t w, size_t h, fvec3 color, EBufferFormat fmt) {
  uint8_t r        = uint8_t(color.x * 255.0f);
  uint8_t g        = uint8_t(color.y * 255.0f);
  uint8_t b        = uint8_t(color.z * 255.0f);
  _format          = fmt; // EBufferFormat::RGB8;
  _bytesPerChannel = 1;
  init(w, h, 3, 1);
  auto outptr = (uint8_t*)_data->data();
  switch (fmt) {
    case EBufferFormat::RGB8:
      break;
    case EBufferFormat::BGR8:
      std::swap(r, b);
      break;
    default:
      OrkAssert(false);
      break;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int pixelindex       = y * w + x;
      int elembase         = pixelindex * 3;
      outptr[elembase + 0] = r;
      outptr[elembase + 1] = g;
      outptr[elembase + 2] = b;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::initRGBA8WithColor(size_t w, size_t h, fvec4 color, EBufferFormat fmt) {
  uint8_t r        = uint8_t(color.x * 255.0f);
  uint8_t g        = uint8_t(color.y * 255.0f);
  uint8_t b        = uint8_t(color.z * 255.0f);
  uint8_t a        = uint8_t(color.w * 255.0f);
  _format          = fmt;
  _bytesPerChannel = 1;
  init(w, h, 4, 1);
  auto outptr = (uint8_t*)_data->data();
  switch (fmt) {
    case EBufferFormat::RGBA8:
      break;
    case EBufferFormat::BGRA8:
      std::swap(r, b);
      break;
    default:
      OrkAssert(false);
      break;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int pixelindex       = y * w + x;
      int elembase         = pixelindex * 4;
      outptr[elembase + 0] = r;
      outptr[elembase + 1] = g;
      outptr[elembase + 2] = b;
      outptr[elembase + 3] = a;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::initRGBA8WithNormalizedFloatBuffer(size_t w, size_t h, size_t numc, const float* buffer) {
  switch (numc) {
    case 1:
      _format = EBufferFormat::R8;
      break;
    case 3:
      _format = EBufferFormat::RGB8;
      break;
    case 4:
      _format = EBufferFormat::RGBA8;
      break;
    default:
      OrkAssert(false);
      break;
  }
  _bytesPerChannel = 1;
  init(w, h, numc, 1);
  auto outptr = (uint8_t*)_data->data();
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int pixelindex = y * w + x;
      int elembase   = pixelindex * numc;
      for (int c = 0; c < numc; c++) {
        outptr[elembase + c] = uint8_t(buffer[elembase + c] * 255.0f);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::compressDefault(CompressedImage& imgout) const {
#if defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
  uncompressed(imgout);
#else
  if (GfxEnv::supportsBC7()) {
    compressBC7(imgout);
  } else {
    uncompressed(imgout);
  }
#endif
}


///////////////////////////////////////////////////////////////////////////////

void Image::uncompressed(CompressedImage& imgout) const {
  deco::printf(
      _image_deco,
      "// Image::uncompressed(%s) w<%zu> h<%zu> BPC<%d> _format<%x>\n",
      _debugName.c_str(),
      _width,
      _height,
      _bytesPerChannel,
      _format);
  imgout._format = _format;
  OrkAssert((_numcomponents == 1) or (_numcomponents == 3) or (_numcomponents == 4));
  imgout._width          = _width;
  imgout._height         = _height;
  imgout._blocked_width  = 0;
  imgout._blocked_height = 0;
  imgout._numcomponents  = _numcomponents;
  imgout._data           = std::make_shared<DataBlock>();
  size_t data_size       = _width * _height * _numcomponents * _bytesPerChannel;
  ork::Timer timer;
  timer.Start();
  std::atomic<int> pending = 0;
  size_t src_stride        = _width * _numcomponents * _bytesPerChannel;
  size_t dst_stride        = src_stride;
  auto src_base            = (uint8_t*)this->_data->data();
  auto dst_base            = (uint8_t*)imgout._data->allocateBlock(data_size);
  std::memcpy(dst_base, src_base, data_size);

  float time = timer.SecsSinceStart();
  float MPPS = float(_width * _height) * 1e-6 / time;
  deco::printf(_image_deco, "// compression time<%g> MPPS<%g>\n", time, MPPS);
}


///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
