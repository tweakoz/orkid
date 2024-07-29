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
#include <ork/kernel/memcpy.inl>
#include <math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

Image Image::convertToFormat(EBufferFormat fmt) const {

  if (fmt == _format)
    return *this;
  else if (fmt == EBufferFormat::BGR8 and _format == EBufferFormat::RGB8) {
    Image img;
    img.init(_width, _height, 3, _bytesPerChannel);
    auto outptr = (uint8_t*)img._data->data();
    auto inptr  = (const uint8_t*)_data->data();
    for (int y = 0; y < _height; y++) {
      for (int x = 0; x < _width; x++) {
        int pixelindex       = y * _width + x;
        int elembase         = pixelindex * 3;
        outptr[elembase + 0] = inptr[elembase + 2];
        outptr[elembase + 1] = inptr[elembase + 1];
        outptr[elembase + 2] = inptr[elembase + 0];
      }
    }
    img._format = fmt;
    return img;
  } else if (fmt == EBufferFormat::RGB8 and _format == EBufferFormat::BGR8) {
    Image img;
    img.init(_width, _height, 3, _bytesPerChannel);
    auto outptr = (uint8_t*)img._data->data();
    auto inptr  = (const uint8_t*)_data->data();
    for (int y = 0; y < _height; y++) {
      for (int x = 0; x < _width; x++) {
        int pixelindex       = y * _width + x;
        int elembase         = pixelindex * 3;
        outptr[elembase + 0] = inptr[elembase + 2];
        outptr[elembase + 1] = inptr[elembase + 1];
        outptr[elembase + 2] = inptr[elembase + 0];
      }
    }
    img._format = fmt;
    return img;
  } else if (fmt == EBufferFormat::BGRA8 and _format == EBufferFormat::RGBA8) {
    Image img;
    img.init(_width, _height, 4, _bytesPerChannel);
    auto outptr = (uint8_t*)img._data->data();
    auto inptr  = (const uint8_t*)_data->data();
    for (int y = 0; y < _height; y++) {
      for (int x = 0; x < _width; x++) {
        int pixelindex       = y * _width + x;
        int elembase         = pixelindex * 4;
        outptr[elembase + 0] = inptr[elembase + 2];
        outptr[elembase + 1] = inptr[elembase + 1];
        outptr[elembase + 2] = inptr[elembase + 0];
        outptr[elembase + 3] = inptr[elembase + 3];
      }
    }
    img._format = fmt;
    return img;
  } else if (fmt == EBufferFormat::RGBA8 and _format == EBufferFormat::BGRA8) {
    Image img;
    img.init(_width, _height, 4, _bytesPerChannel);
    auto outptr = (uint8_t*)img._data->data();
    auto inptr  = (const uint8_t*)_data->data();
    for (int y = 0; y < _height; y++) {
      for (int x = 0; x < _width; x++) {
        int pixelindex       = y * _width + x;
        int elembase         = pixelindex * 4;
        outptr[elembase + 0] = inptr[elembase + 2];
        outptr[elembase + 1] = inptr[elembase + 1];
        outptr[elembase + 2] = inptr[elembase + 0];
        outptr[elembase + 3] = inptr[elembase + 3];
      }
    }
    img._format = fmt;
    return img;
  } else {
    OrkAssert(false);
    return *this;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::convertFromImageToFormat(const Image& inp, EBufferFormat fmt) {

  /////////////////////////////
  if (fmt == inp._format) {
    init(inp._width, inp._height, inp._numcomponents, inp._bytesPerChannel);
    auto outptr = (uint8_t*)_data->data();
    auto inptr  = (const uint8_t*)inp._data->data();
    memcpy_fast(outptr, inptr, _width * _height * _numcomponents * _bytesPerChannel);
  }
  /////////////////////////////
  else if (fmt == EBufferFormat::RGB8 and inp._format == EBufferFormat::RGBA_BPTC_UNORM) {
    
  }
  /////////////////////////////
  else if (fmt == EBufferFormat::BGR8 and inp._format == EBufferFormat::RGB8) {
    init(inp._width, inp._height, 3, inp._bytesPerChannel);
    auto outptr = (uint8_t*)_data->data();
    auto inptr  = (const uint8_t*)inp._data->data();
    for (int y = 0; y < inp._height; y++) {
      for (int x = 0; x < inp._width; x++) {
        int pixelindex       = y * inp._width + x;
        int elembase         = pixelindex * 3;
        outptr[elembase + 0] = inptr[elembase + 2];
        outptr[elembase + 1] = inptr[elembase + 1];
        outptr[elembase + 2] = inptr[elembase + 0];
      }
    }
    _format = fmt;
  }
  /////////////////////////////
  else if (fmt == EBufferFormat::RGB8) {
    init(inp._width, inp._height, 3, inp._bytesPerChannel);
    _format     = fmt;
    auto outptr = (uint8_t*)_data->data();
    if (inp._format == EBufferFormat::BGR8) {
      auto inptr  = (const uint8_t*)inp._data->data();
      for (int y = 0; y < inp._height; y++) {
        for (int x = 0; x < inp._width; x++) {
          int pixelindex       = y * inp._width + x;
          int elembase         = pixelindex * 3;
          outptr[elembase + 0] = inptr[elembase + 2];
          outptr[elembase + 1] = inptr[elembase + 1];
          outptr[elembase + 2] = inptr[elembase + 0];
        }
      }
    } else if (inp._format == EBufferFormat::BGRA8) {
      printf( "convert from BGRA8 to RGB8\n");
      auto inptr  = (const uint8_t*)inp._data->data();
      for (int y = 0; y < inp._height; y++) {
        for (int x = 0; x < inp._width; x++) {
          int pixelindex       = y * inp._width + x;
          int elembase         = pixelindex * 4;
          outptr[pixelindex * 3 + 0] = inptr[elembase + 2];
          outptr[pixelindex * 3 + 1] = inptr[elembase + 1];
          outptr[pixelindex * 3 + 2] = inptr[elembase + 0];
        }
      }
    } else if (inp._format == EBufferFormat::RGBA8) {
      auto inptr  = (const uint8_t*)inp._data->data();
      for (int y = 0; y < inp._height; y++) {
        for (int x = 0; x < inp._width; x++) {
          int pixelindex           = y * inp._width + x;
          int in_elembase          = pixelindex * 4;
          int out_elembase         = pixelindex * 3;
          outptr[out_elembase + 0] = inptr[in_elembase + 0];
          outptr[out_elembase + 1] = inptr[in_elembase + 1];
          outptr[out_elembase + 2] = inptr[in_elembase + 2];
        }
      }
    } else if (inp._format == EBufferFormat::RGB16) {
      auto inptr  = (const uint16_t*)inp._data->data();
      for (int y = 0; y < inp._height; y++) {
        for (int x = 0; x < inp._width; x++) {
          int pixelindex           = y * inp._width + x;
          int in_elembase          = pixelindex * 3;
          int out_elembase         = pixelindex * 3;
          outptr[out_elembase + 0] = uint8_t(inptr[in_elembase + 0]>>8);
        }
      }
    } else {
      auto inp_fmt_str = EBufferFormatToName(inp._format);
      auto fmt_str     = EBufferFormatToName(fmt);
      printf( "Image::convertFromImageToFormat unsupported combo : fmt<%s> inp_fmt<%s>\n", fmt_str.c_str(), inp_fmt_str.c_str());
      OrkAssert(false);
    }
  }
  /////////////////////////////
  else if (fmt == EBufferFormat::BGRA8 and inp._format == EBufferFormat::RGBA8) {
    init(inp._width, inp._height, 4, inp._bytesPerChannel);

  } else {
    OrkAssert(false);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::convertToRGBA(Image& imgout, bool force_8bc) const {

  if (force_8bc) {
    imgout.init(_width, _height, 4, 1);
  } else {
    imgout.init(_width, _height, 4, _bytesPerChannel);
  }

  switch (_numcomponents) {
    case 1:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        for (size_t y = 0; y < imgout._height; y++) {
          for (size_t x = 0; x < imgout._width; x++) {
            auto inppixels = pixel8(x, y);
            auto outpixels = imgout.pixel8(x, y);
            outpixels[0]   = inppixels[0];
            outpixels[1]   = inppixels[0];
            outpixels[2]   = inppixels[0];
            outpixels[3]   = 0xff;
          }
        }
      } else if (_bytesPerChannel == 2) {
        auto inp_pixels = (const uint16_t*)_data->data();
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[0] >> 8);
              outpixels[2]   = uint8_t(inppixels[0] >> 8);
              outpixels[3]   = 0xff;
            }
          }
        } else {
          auto out_pixels = (uint16_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel16(x, y);
              outpixels[0]   = inppixels[0];
              outpixels[1]   = inppixels[0];
              outpixels[2]   = inppixels[0];
              outpixels[3]   = 0xffff;
            }
          }
        }
      }
      break;
    case 3:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        for (size_t y = 0; y < imgout._height; y++) {
          for (size_t x = 0; x < imgout._width; x++) {
            auto inppixels = pixel8(x, y);
            auto outpixels = imgout.pixel8(x, y);
            outpixels[0]   = inppixels[0];
            outpixels[1]   = inppixels[1];
            outpixels[2]   = inppixels[2];
            outpixels[3]   = 0xff;
          }
        }
      } else if (_bytesPerChannel == 2) {
        auto inp_pixels = (const uint16_t*)_data->data();
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[1] >> 8);
              outpixels[2]   = uint8_t(inppixels[2] >> 8);
              outpixels[3]   = 0xff;
            }
          }
        } else {
          auto out_pixels = (uint16_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel16(x, y);
              outpixels[0]   = inppixels[0];
              outpixels[1]   = inppixels[1];
              outpixels[2]   = inppixels[2];
              outpixels[3]   = 0xffff;
            }
          }
        }
      }
      break;
    case 4:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        memcpy_fast(out_pixels, inp_pixels, _width * _height * 4);
      } else if (_bytesPerChannel == 2) {
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[1] >> 8);
              outpixels[2]   = uint8_t(inppixels[2] >> 8);
              outpixels[3]   = uint8_t(inppixels[3] >> 8);
            }
          }
        } else {
          auto inp_pixels16 = (const uint16_t*)_data->data();
          auto out_pixels16 = (uint16_t*)imgout._data->data();
          memcpy_fast(out_pixels16, inp_pixels16, _width * _height * 4 * 2);
        }
      }
      break;
    default:
      OrkAssert(false);
      break;
  }
  imgout._debugName = _debugName + "_2rgba";
  // auto pathr        = FormatString("%s.png", imgout._debugName.c_str());
  // auto path         = file::Path::temp_dir() / pathr;
  // imgout.writeToFile(path);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
  // deco::printf(_image_deco, "// Image::convertToRGBA(%s)\n", imgout._debugName.c_str());
  // deco::printf(_image_deco, "// imgout._width<%zu>\n", imgout._width);
  // deco::printf(_image_deco, "// imgout._height<%zu>\n", imgout._height);
  // deco::printf(_image_deco, "// imgout._numcomponents<%zu>\n", imgout._numcomponents);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
