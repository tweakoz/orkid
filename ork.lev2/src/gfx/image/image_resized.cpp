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
#include <algorithm>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////

// Chunk size for parallelized image downsampling
constexpr size_t IMG_DOWNSAMPLE_CHUNK_SIZE = 128;

////////////////////////////////////////////////////////////////

void Image::resizedOf(const Image& inp, int w, int h) {
  int original_width = inp._width;
  int original_height = inp._height;
  int original_depth = inp._depth;
  int original_numcomponents = inp._numcomponents;
  int original_bytesPerChannel = inp._bytesPerChannel;
  //printf("Image::resize ow<%d>->nw<%d> oh<%d>->nh<%d> numc<%d> bpc<%d>\n", original_width, original_height, w, h, original_numcomponents, original_bytesPerChannel);
  this->init(w, h, original_numcomponents, original_bytesPerChannel);

  _format = inp._format;
  _debugName = inp._debugName+"_resized";
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
    case R16UI:
    case D16UI:
    case Y16UI:{
          OrkAssert(false);
      break;
    }
    case R32F:{
      OrkAssert(false);
      break;
    }
    case BGR8:
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
      printf("unknown format <%08x>\n", (uint32_t) inp._format );
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::downsample(Image& imgout) const {
  imgout.init(_width >> 1, _height >> 1, _numcomponents, _bytesPerChannel);
  imgout._format = _format;

  // 4x4 Gaussian-like kernel weights for high-quality 2x downsampling
  // Layout:  1  2  2  1
  //          2  4  4  2
  //          2  4  4  2
  //          1  2  2  1
  // Total weight = 36
  constexpr double kernel[4][4] = {
    {1.0/36.0, 2.0/36.0, 2.0/36.0, 1.0/36.0},
    {2.0/36.0, 4.0/36.0, 4.0/36.0, 2.0/36.0},
    {2.0/36.0, 4.0/36.0, 4.0/36.0, 2.0/36.0},
    {1.0/36.0, 2.0/36.0, 2.0/36.0, 1.0/36.0}
  };

  // Parallelize downsampling by chunking output rows
  size_t num_chunks = (imgout._height + IMG_DOWNSAMPLE_CHUNK_SIZE - 1) / IMG_DOWNSAMPLE_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  using enum EBufferFormat;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, &imgout, &chunkcounter, &kernel]() {
      size_t y_start = chunk * IMG_DOWNSAMPLE_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_DOWNSAMPLE_CHUNK_SIZE, imgout._height);

      for (size_t y = y_start; y < y_end; y++) {
        // Map output pixel to input 4x4 region
        // Center of output pixel y corresponds to input pixels [y*2, y*2+1]
        // We sample from [y*2-1 ... y*2+2] for the 4x4 kernel
        int base_y = int(y) * 2 - 1;

        for (size_t x = 0; x < imgout._width; x++) {
          int base_x = int(x) * 2 - 1;

          switch (this->_format) {
            case R8:
            case BGR8:
            case RGB8:
            case BGRA8:
            case RGBA8: {
              auto outpixel = imgout.pixel8(x, y);

              for (size_t c = 0; c < this->_numcomponents; c++) {
                double sum = 0.0;

                // Sample 4x4 region with weights
                for (int ky = 0; ky < 4; ky++) {
                  int sy = base_y + ky;
                  // Clamp to image bounds
                  if (sy < 0) sy = 0;
                  if (sy >= int(this->_height)) sy = this->_height - 1;

                  for (int kx = 0; kx < 4; kx++) {
                    int sx = base_x + kx;
                    // Clamp to image bounds
                    if (sx < 0) sx = 0;
                    if (sx >= int(this->_width)) sx = this->_width - 1;

                    auto pixel = this->pixel8(sx, sy);
                    sum += double(pixel[c]) * kernel[ky][kx];
                  }
                }

                outpixel[c] = uint8_t(sum);
              }
              break;
            }
            case R16UI:
            case RGB16:
            case RGBA16: {
              auto outpixel = imgout.pixel16(x, y);

              for (size_t c = 0; c < this->_numcomponents; c++) {
                double sum = 0.0;

                // Sample 4x4 region with weights
                for (int ky = 0; ky < 4; ky++) {
                  int sy = base_y + ky;
                  // Clamp to image bounds
                  if (sy < 0) sy = 0;
                  if (sy >= int(this->_height)) sy = this->_height - 1;

                  for (int kx = 0; kx < 4; kx++) {
                    int sx = base_x + kx;
                    // Clamp to image bounds
                    if (sx < 0) sx = 0;
                    if (sx >= int(this->_width)) sx = this->_width - 1;

                    auto pixel = this->pixel16(sx, sy);
                    sum += double(pixel[c]) * kernel[ky][kx];
                  }
                }

                outpixel[c] = uint16_t(sum);
              }
              break;
            }
            case RGBA32F:
            case RGB32F: {
              auto outpixel = imgout.pixel32f(x, y);

              for (size_t c = 0; c < this->_numcomponents; c++) {
                double sum = 0.0;

                // Sample 4x4 region with weights
                for (int ky = 0; ky < 4; ky++) {
                  int sy = base_y + ky;
                  // Clamp to image bounds
                  if (sy < 0) sy = 0;
                  if (sy >= int(this->_height)) sy = this->_height - 1;

                  for (int kx = 0; kx < 4; kx++) {
                    int sx = base_x + kx;
                    // Clamp to image bounds
                    if (sx < 0) sx = 0;
                    if (sx >= int(this->_width)) sx = this->_width - 1;

                    auto pixel = this->pixel32f(sx, sy);
                    sum += double(pixel[c]) * kernel[ky][kx];
                  }
                }

                outpixel[c] = float(sum);
              }
              break;
            }
            default:
              auto fmt_str = EBufferFormatToName(this->_format);
              printf("UNKNOWN FORMAT<%s>\n", fmt_str.c_str());
              OrkAssert(false);
              break;
          }
        }
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
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
