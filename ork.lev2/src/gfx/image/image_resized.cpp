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
#include <ork/lev2/gfx/image.h>

#include <math.h>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////

void Image::resizedOf(const Image& inp, int w, int h) {
  int original_width = inp._width;
  int original_height = inp._height;
  int original_depth = inp._depth;
  int original_numcomponents = inp._numcomponents;
  int original_bytesPerChannel = inp._bytesPerChannel;
  
  this->init(w, h, original_numcomponents, original_bytesPerChannel);

  _format = inp._format;
  
  using enum EBufferFormat;
  switch( inp._format ){
    case R8:{
      // bicubic interpolation
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = int(x0);
          int y0i = int(y0);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel8(x, y);
          auto pixel00 = inp.pixel8(x0i, y0i);
          auto pixel01 = inp.pixel8(x0i, y0i+1);
          auto pixel10 = inp.pixel8(x0i+1, y0i);
          auto pixel11 = inp.pixel8(x0i+1, y0i+1);
          for (size_t c = 0; c < original_numcomponents; c++) {
            double val = 0.0;
            val += x1f * y1f * double(pixel00[c]);
            val += x1f * y0f * double(pixel01[c]);
            val += x0f * y1f * double(pixel10[c]);
            val += x0f * y0f * double(pixel11[c]);
            pixel[c] = uint8_t(val);
          }
        }
      }
      break;
    }
    case R16:{
      OrkAssert(false);
      break;
    }
    case R32F:{
      OrkAssert(false);
      break;
    }
    case RGB8:{
      // bicubic interpolation
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = int(x0);
          int y0i = int(y0);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel8(x, y);
          auto pixel00 = inp.pixel8(x0i, y0i);
          auto pixel01 = inp.pixel8(x0i, y0i+1);
          auto pixel10 = inp.pixel8(x0i+1, y0i);
          auto pixel11 = inp.pixel8(x0i+1, y0i+1);
          for (size_t c = 0; c < original_numcomponents; c++) {
            double val = 0.0;
            val += x1f * y1f * double(pixel00[c]);
            val += x1f * y0f * double(pixel01[c]);
            val += x0f * y1f * double(pixel10[c]);
            val += x0f * y0f * double(pixel11[c]);
            pixel[c] = uint8_t(val);
          }
        }
      }
      break;
    }
    case RGBA8:{
      OrkAssert(false);
      break;
    }
    case RGB16:{
      OrkAssert(false);
      break;
    }
    case RGBA16:{
      OrkAssert(false);
      break;
    }
    case RGB32F:{
      OrkAssert(false);
      break;
    }
    case RGBA32F:{
      OrkAssert(false);
      break;
    }
    case RGBA16F:{
      OrkAssert(false);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::downsample(Image& imgout) const {
  imgout.init(_width >> 1, _height >> 1, _numcomponents, _bytesPerChannel);
  for (size_t y = 0; y < imgout._height; y++) {
    size_t ya = y * 2;
    size_t yb = ya + 1;
    if (yb > (_height - 1))
      yb = _height - 1;
    for (size_t x = 0; x < imgout._width; x++) {
      size_t xa = x * 2;
      size_t xb = xa + 1;
      if (xb > (_width - 1))
        xb = _width - 1;

      using enum EBufferFormat;
      switch (_format) {
        case R8:
        case BGR8:
        case RGB8:
        case BGRA8:
        case RGBA8: {
          auto outpixel     = imgout.pixel8(x, y);
          auto inppixelXAYA = pixel8(xa, ya);
          auto inppixelXBYA = pixel8(xb, ya);
          auto inppixelXAYB = pixel8(xa, yb);
          auto inppixelXBYB = pixel8(xb, yb);
          for (size_t c = 0; c < _numcomponents; c++) {
            double xaya  = double(inppixelXAYA[c]);
            double xbya  = double(inppixelXBYA[c]);
            double xayb  = double(inppixelXAYB[c]);
            double xbyb  = double(inppixelXBYB[c]);
            double avg   = (xaya + xbya + xayb + xbyb) * 0.25;
            uint8_t uavg = uint8_t(avg);
            outpixel[c]  = uavg;
          }
          break;
        }
        case R16:
        case RGB16:
        case RGBA16: {
          auto outpixel     = imgout.pixel16(x, y);
          auto inppixelXAYA = pixel16(xa, ya);
          auto inppixelXBYA = pixel16(xb, ya);
          auto inppixelXAYB = pixel16(xa, yb);
          auto inppixelXBYB = pixel16(xb, yb);
          for (size_t c = 0; c < _numcomponents; c++) {
            double xaya   = double(inppixelXAYA[c]);
            double xbya   = double(inppixelXBYA[c]);
            double xayb   = double(inppixelXAYB[c]);
            double xbyb   = double(inppixelXBYB[c]);
            double avg    = (xaya + xbya + xayb + xbyb) * 0.25;
            uint16_t uavg = uint16_t(avg);
            outpixel[c]   = uavg;
          }
          break;
        }
        default:
          auto fmt_str = EBufferFormatToName(_format);
          printf("UNKNOWN FORMAT<%s>\n", fmt_str.c_str());
          OrkAssert(false);
          break;
      }
    }
  }
  imgout._debugName = _debugName + "_ds";
  // auto pathr        = FormatString("%s.png", imgout._debugName.c_str());
  // auto path         = file::Path::temp_dir() / pathr;
  // writeToFile(path);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
  // deco::printf(_image_deco, "// Image::downsample(%s)\n", imgout._debugName.c_str());
  // deco::printf(_image_deco, "// imgout._width<%zu>\n", imgout._width);
  // deco::printf(_image_deco, "// imgout._height<%zu>\n", imgout._height);
  // deco::printf(_image_deco, "// imgout._numcomponents<%zu>\n", imgout._numcomponents);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
