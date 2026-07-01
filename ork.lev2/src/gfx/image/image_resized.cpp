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
#include <functional>
#include <vector>

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
      // bilinear interpolation
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = std::min(int(x0), original_width - 1);
          int y0i = std::min(int(y0), original_height - 1);
          int x1i = std::min(x0i + 1, original_width - 1);
          int y1i = std::min(y0i + 1, original_height - 1);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel8(x, y);
          auto pixel00 = inp.pixel8(x0i, y0i);
          auto pixel01 = inp.pixel8(x0i, y1i);
          auto pixel10 = inp.pixel8(x1i, y0i);
          auto pixel11 = inp.pixel8(x1i, y1i);
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
    case RGB8:
    case BGRA8:
    case RGBA8:{
      // bilinear interpolation
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = std::min(int(x0), original_width - 1);
          int y0i = std::min(int(y0), original_height - 1);
          int x1i = std::min(x0i + 1, original_width - 1);
          int y1i = std::min(y0i + 1, original_height - 1);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel8(x, y);
          auto pixel00 = inp.pixel8(x0i, y0i);
          auto pixel01 = inp.pixel8(x0i, y1i);
          auto pixel10 = inp.pixel8(x1i, y0i);
          auto pixel11 = inp.pixel8(x1i, y1i);
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
    case RGB16:{
      OrkAssert(false);
      break;
    }
    case RGBA16:{
      OrkAssert(false);
      break;
    }
    case RGB32F:
    case RGBA32F:{
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = std::min(int(x0), original_width - 1);
          int y0i = std::min(int(y0), original_height - 1);
          int x1i = std::min(x0i + 1, original_width - 1);
          int y1i = std::min(y0i + 1, original_height - 1);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel32f(x, y);
          auto pixel00 = inp.pixel32f(x0i, y0i);
          auto pixel01 = inp.pixel32f(x0i, y1i);
          auto pixel10 = inp.pixel32f(x1i, y0i);
          auto pixel11 = inp.pixel32f(x1i, y1i);
          for (size_t c = 0; c < original_numcomponents; c++) {
            double val = 0.0;
            val += x1f * y1f * double(pixel00[c]);
            val += x1f * y0f * double(pixel01[c]);
            val += x0f * y1f * double(pixel10[c]);
            val += x0f * y0f * double(pixel11[c]);
            pixel[c] = float(val);
          }
        }
      }
      break;
    }
    case RGBA16F:{
      auto half_to_float = [](uint16_t h) -> float {
        uint32_t sign     = (h & 0x8000) << 16;
        uint32_t exponent = ((h & 0x7C00) >> 10);
        uint32_t mantissa = (h & 0x03FF) << 13;
        if (exponent == 0) {
          if (mantissa == 0) {
            uint32_t result = sign;
            return *reinterpret_cast<float*>(&result);
          }
          exponent = 1;
          while (!(mantissa & 0x00800000)) { mantissa <<= 1; exponent--; }
          mantissa &= ~0x00800000;
          exponent = (exponent + 127 - 15) << 23;
        } else if (exponent == 31) {
          exponent = 0xFF << 23;
        } else {
          exponent = (exponent + 127 - 15) << 23;
        }
        uint32_t result = sign | exponent | mantissa;
        return *reinterpret_cast<float*>(&result);
      };
      auto float_to_half = [](float f) -> uint16_t {
        uint32_t bits = *reinterpret_cast<uint32_t*>(&f);
        uint32_t sign = (bits >> 16) & 0x8000;
        int32_t exp32 = ((bits >> 23) & 0xFF) - 127 + 15;
        uint32_t mant = (bits & 0x007FFFFF);
        if (exp32 <= 0) return uint16_t(sign);
        if (exp32 >= 31) return uint16_t(sign | 0x7C00);
        return uint16_t(sign | (exp32 << 10) | (mant >> 13));
      };
      for (size_t y = 0; y<h; y++) {
        for (size_t x = 0; x<w; x++) {
          double u = double(x) / double(w);
          double v = double(y) / double(h);
          double x0 = u * double(original_width);
          double y0 = v * double(original_height);
          int x0i = std::min(int(x0), original_width - 1);
          int y0i = std::min(int(y0), original_height - 1);
          int x1i = std::min(x0i + 1, original_width - 1);
          int y1i = std::min(y0i + 1, original_height - 1);
          double x0f = x0 - double(x0i);
          double y0f = y0 - double(y0i);
          double x1f = 1.0 - x0f;
          double y1f = 1.0 - y0f;
          auto pixel = this->pixel16(x, y);
          auto pixel00 = inp.pixel16(x0i, y0i);
          auto pixel01 = inp.pixel16(x0i, y1i);
          auto pixel10 = inp.pixel16(x1i, y0i);
          auto pixel11 = inp.pixel16(x1i, y1i);
          for (size_t c = 0; c < original_numcomponents; c++) {
            double val = 0.0;
            val += x1f * y1f * double(half_to_float(pixel00[c]));
            val += x1f * y0f * double(half_to_float(pixel01[c]));
            val += x0f * y1f * double(half_to_float(pixel10[c]));
            val += x0f * y0f * double(half_to_float(pixel11[c]));
            pixel[c] = float_to_half(float(val));
          }
        }
      }
      break;
    }
    default:
      printf("unknown format <%08x>\n", (uint32_t) inp._format );
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// resampledOf — high-quality separable filtered resample (anti-aliased, arbitrary ratio, float-capable).
// Two passes (horizontal then vertical). The filter support STRETCHES with the reduction ratio, so a
// downsample integrates the whole source footprint per output texel (no aliasing — unlike fixed 2x2
// bilinear). Texel-center aligned to match the renderer's heights sampling ((t+0.5)/dim). Runs in float
// internally, so the float heightmap formats (R32F / RGB32F / RGBA32F) work as well as 8-bit.
///////////////////////////////////////////////////////////////////////////////
namespace {

inline double _rs_sinc(double x) {
  if (x == 0.0)
    return 1.0;
  const double PI = 3.14159265358979323846;
  x *= PI;
  return std::sin(x) / x;
}

// cubic filter family parameterized by (B,C): BSpline=(1,0), Mitchell=(1/3,1/3), CatmullRom=(0,1/2).
inline double _rs_cubic(double x, double B, double C) {
  x         = std::abs(x);
  double x2 = x * x, x3 = x2 * x;
  if (x < 1.0)
    return ((12.0 - 9.0 * B - 6.0 * C) * x3 + (-18.0 + 12.0 * B + 6.0 * C) * x2 + (6.0 - 2.0 * B)) / 6.0;
  if (x < 2.0)
    return ((-B - 6.0 * C) * x3 + (6.0 * B + 30.0 * C) * x2 + (-12.0 * B - 48.0 * C) * x + (8.0 * B + 24.0 * C)) / 6.0;
  return 0.0;
}

struct RsFilter {
  double _radius;
  std::function<double(double)> _k;
};

RsFilter _rs_filterDef(Image::ResampleFilter f) {
  using RF = Image::ResampleFilter;
  switch (f) {
    case RF::BOX:         return {0.5, [](double x) { return (std::abs(x) <= 0.5) ? 1.0 : 0.0; }};
    case RF::TRIANGLE:    return {1.0, [](double x) { x = std::abs(x); return (x < 1.0) ? (1.0 - x) : 0.0; }};
    case RF::BSPLINE:     return {2.0, [](double x) { return _rs_cubic(x, 1.0, 0.0); }};
    case RF::MITCHELL:    return {2.0, [](double x) { return _rs_cubic(x, 1.0 / 3.0, 1.0 / 3.0); }};
    case RF::CATMULL_ROM: return {2.0, [](double x) { return _rs_cubic(x, 0.0, 0.5); }};
    case RF::LANCZOS3:    return {3.0, [](double x) { x = std::abs(x); return (x < 3.0) ? (_rs_sinc(x) * _rs_sinc(x / 3.0)) : 0.0; }};
  }
  return {1.0, [](double x) { x = std::abs(x); return (x < 1.0) ? (1.0 - x) : 0.0; }};
}

struct RsContrib {
  int _start = 0;
  std::vector<float> _w;
};

// per-output-pixel source taps + normalized weights for one axis (texel-center, ratio-scaled support).
std::vector<RsContrib> _rs_axis(int src, int dst, const RsFilter& fd) {
  std::vector<RsContrib> out(dst);
  const double scale   = double(dst) / double(src);
  const double fscale  = (scale < 1.0) ? scale : 1.0; // stretch filter on downsample => anti-alias
  const double support = fd._radius / fscale;
  for (int i = 0; i < dst; i++) {
    const double center = (double(i) + 0.5) / scale - 0.5; // dst -> src texel center
    const int lo        = int(std::ceil(center - support));
    const int hi        = int(std::floor(center + support));
    RsContrib c;
    c._start   = lo;
    double sum = 0.0;
    for (int s = lo; s <= hi; s++) {
      double w = fd._k((double(s) - center) * fscale);
      c._w.push_back(float(w));
      sum += w;
    }
    if (sum > 0.0)
      for (auto& w : c._w)
        w = float(double(w) / sum);
    out[i] = std::move(c);
  }
  return out;
}

} // anonymous namespace

void Image::resampledOf(const Image& inp, int w, int h, ResampleFilter filter) {
  const int nc   = int(inp._numcomponents);
  const bool f32 = (inp._bytesPerChannel == 4);
  const bool u8  = (inp._bytesPerChannel == 1);
  OrkAssert(f32 or u8); // float (R32F/RGB32F/RGBA32F) or 8-bit; convert 16-bit/half first
  const int sw = int(inp._width);
  const int sh = int(inp._height);

  this->init(w, h, inp._numcomponents, inp._bytesPerChannel);
  _format    = inp._format;
  _debugName = inp._debugName + "_resampled";

  const RsFilter fd = _rs_filterDef(filter);
  const auto cx     = _rs_axis(sw, w, fd);
  const auto cy     = _rs_axis(sh, h, fd);

  auto getf = [&](int x, int y, int c) -> float {
    x = std::clamp(x, 0, sw - 1);
    y = std::clamp(y, 0, sh - 1);
    return f32 ? inp.pixel32f(x, y)[c] : (float(inp.pixel8(x, y)[c]) * (1.0f / 255.0f));
  };

  // pass 1 (horizontal): inp(sw x sh) -> tmp(w x sh), in float
  std::vector<float> tmp(size_t(w) * size_t(sh) * size_t(nc), 0.0f);
  for (int y = 0; y < sh; y++) {
    for (int x = 0; x < w; x++) {
      const RsContrib& c = cx[x];
      for (int ch = 0; ch < nc; ch++) {
        double acc = 0.0;
        for (size_t k = 0; k < c._w.size(); k++)
          acc += double(c._w[k]) * double(getf(c._start + int(k), y, ch));
        tmp[(size_t(y) * size_t(w) + size_t(x)) * size_t(nc) + size_t(ch)] = float(acc);
      }
    }
  }

  // pass 2 (vertical): tmp(w x sh) -> this(w x h)
  for (int y = 0; y < h; y++) {
    const RsContrib& c = cy[y];
    for (int x = 0; x < w; x++) {
      for (int ch = 0; ch < nc; ch++) {
        double acc = 0.0;
        for (size_t k = 0; k < c._w.size(); k++) {
          const int sy = std::clamp(c._start + int(k), 0, sh - 1);
          acc += double(c._w[k]) * double(tmp[(size_t(sy) * size_t(w) + size_t(x)) * size_t(nc) + size_t(ch)]);
        }
        if (f32)
          this->pixel32f(x, y)[ch] = float(acc);
        else
          this->pixel8(x, y)[ch] = uint8_t(std::clamp(float(acc) * 255.0f + 0.5f, 0.0f, 255.0f));
      }
    }
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
            case RGBA16F: {
              // Half-float format: read as uint16_t, convert to float for math, write back as half
              auto half_to_float = [](uint16_t h) -> float {
                uint32_t sign     = (h & 0x8000) << 16;
                uint32_t exponent = ((h & 0x7C00) >> 10);
                uint32_t mantissa = (h & 0x03FF) << 13;
                if (exponent == 0) {
                  if (mantissa == 0) {
                    uint32_t result = sign;
                    return *reinterpret_cast<float*>(&result);
                  }
                  exponent = 1;
                  while (!(mantissa & 0x00800000)) { mantissa <<= 1; exponent--; }
                  mantissa &= ~0x00800000;
                  exponent = (exponent + 127 - 15) << 23;
                } else if (exponent == 31) {
                  exponent = 0xFF << 23;
                } else {
                  exponent = (exponent + 127 - 15) << 23;
                }
                uint32_t result = sign | exponent | mantissa;
                return *reinterpret_cast<float*>(&result);
              };
              auto float_to_half = [](float f) -> uint16_t {
                uint32_t bits = *reinterpret_cast<uint32_t*>(&f);
                uint32_t sign = (bits >> 16) & 0x8000;
                int32_t exp32 = ((bits >> 23) & 0xFF) - 127 + 15;
                uint32_t mant = (bits & 0x007FFFFF);
                if (exp32 <= 0) return uint16_t(sign); // underflow to zero
                if (exp32 >= 31) return uint16_t(sign | 0x7C00); // overflow to inf
                return uint16_t(sign | (exp32 << 10) | (mant >> 13));
              };

              auto outpixel = imgout.pixel16(x, y);

              for (size_t c = 0; c < this->_numcomponents; c++) {
                double sum = 0.0;

                for (int ky = 0; ky < 4; ky++) {
                  int sy = base_y + ky;
                  if (sy < 0) sy = 0;
                  if (sy >= int(this->_height)) sy = this->_height - 1;

                  for (int kx = 0; kx < 4; kx++) {
                    int sx = base_x + kx;
                    if (sx < 0) sx = 0;
                    if (sx >= int(this->_width)) sx = this->_width - 1;

                    auto pixel = this->pixel16(sx, sy);
                    sum += double(half_to_float(pixel[c])) * kernel[ky][kx];
                  }
                }

                outpixel[c] = float_to_half(float(sum));
              }
              break;
            }
            case R32F:
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
