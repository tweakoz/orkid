////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/image.h>
#include <algorithm>
#include <cmath>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////

// Chunk size for parallelized image processing
constexpr size_t IMG_GAMMA_CHUNK_SIZE = 128;

////////////////////////////////////////////////////////////////
// Gamma correction with bitmask control
// channel_mask bits: 0=R, 1=G, 2=B, 3=A
////////////////////////////////////////////////////////////////

void Image::gamma(float gamma_value, uint8_t channel_mask) {

  // Build lookup tables for integer formats
  uint8_t lut8[256];
  uint16_t lut16[65536];

  // Precompute 8-bit LUT
  for (int i = 0; i < 256; i++) {
    float normalized = float(i) / 255.0f;
    float corrected = std::pow(normalized, gamma_value);
    lut8[i] = uint8_t(corrected * 255.0f);
  }

  // Precompute 16-bit LUT
  for (int i = 0; i < 65536; i++) {
    float normalized = float(i) / 65535.0f;
    float corrected = std::pow(normalized, gamma_value);
    lut16[i] = uint16_t(corrected * 65535.0f);
  }

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_GAMMA_CHUNK_SIZE - 1) / IMG_GAMMA_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  using enum EBufferFormat;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, channel_mask, gamma_value, &lut8, &lut16, &chunkcounter]() {
      size_t y_start = chunk * IMG_GAMMA_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_GAMMA_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        for (size_t x = 0; x < this->_width; x++) {

          switch (this->_format) {
            ////////////////////////////////////////////////////////////////
            // 8-bit formats (use LUT)
            ////////////////////////////////////////////////////////////////
            case R8: {
              if (channel_mask & 0x01) {
                auto pixel = this->pixel8(x, y);
                pixel[0] = lut8[pixel[0]];
              }
              break;
            }
            case RGB8:
            case BGR8: {
              auto pixel = this->pixel8(x, y);
              if (channel_mask & 0x01) pixel[0] = lut8[pixel[0]];
              if (channel_mask & 0x02) pixel[1] = lut8[pixel[1]];
              if (channel_mask & 0x04) pixel[2] = lut8[pixel[2]];
              break;
            }
            case RGBA8:
            case BGRA8: {
              auto pixel = this->pixel8(x, y);
              if (channel_mask & 0x01) pixel[0] = lut8[pixel[0]];
              if (channel_mask & 0x02) pixel[1] = lut8[pixel[1]];
              if (channel_mask & 0x04) pixel[2] = lut8[pixel[2]];
              if (channel_mask & 0x08) pixel[3] = lut8[pixel[3]];
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit integer formats (use LUT)
            ////////////////////////////////////////////////////////////////
            case R16UI: {
              if (channel_mask & 0x01) {
                auto pixel = this->pixel16(x, y);
                pixel[0] = lut16[pixel[0]];
              }
              break;
            }
            case RGB16: {
              auto pixel = this->pixel16(x, y);
              if (channel_mask & 0x01) pixel[0] = lut16[pixel[0]];
              if (channel_mask & 0x02) pixel[1] = lut16[pixel[1]];
              if (channel_mask & 0x04) pixel[2] = lut16[pixel[2]];
              break;
            }
            case RGBA16: {
              auto pixel = this->pixel16(x, y);
              if (channel_mask & 0x01) pixel[0] = lut16[pixel[0]];
              if (channel_mask & 0x02) pixel[1] = lut16[pixel[1]];
              if (channel_mask & 0x04) pixel[2] = lut16[pixel[2]];
              if (channel_mask & 0x08) pixel[3] = lut16[pixel[3]];
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 32-bit float formats (direct pow)
            ////////////////////////////////////////////////////////////////
            case R32F: {
              if (channel_mask & 0x01) {
                auto data = (float*)this->_data->data();
                size_t pixel_index = y * this->_width + x;
                data[pixel_index] = std::pow(data[pixel_index], gamma_value);
              }
              break;
            }
            case RGB32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 3;
              if (channel_mask & 0x01) data[pixel_index + 0] = std::pow(data[pixel_index + 0], gamma_value);
              if (channel_mask & 0x02) data[pixel_index + 1] = std::pow(data[pixel_index + 1], gamma_value);
              if (channel_mask & 0x04) data[pixel_index + 2] = std::pow(data[pixel_index + 2], gamma_value);
              break;
            }
            case RGBA32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 4;
              if (channel_mask & 0x01) data[pixel_index + 0] = std::pow(data[pixel_index + 0], gamma_value);
              if (channel_mask & 0x02) data[pixel_index + 1] = std::pow(data[pixel_index + 1], gamma_value);
              if (channel_mask & 0x04) data[pixel_index + 2] = std::pow(data[pixel_index + 2], gamma_value);
              if (channel_mask & 0x08) data[pixel_index + 3] = std::pow(data[pixel_index + 3], gamma_value);
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit half float format (convert, apply, convert back)
            ////////////////////////////////////////////////////////////////
            case RGBA16F: {
              auto pixel = this->pixel16(x, y);
              for (size_t c = 0; c < 4; c++) {
                if (channel_mask & (1 << c)) {
                  // Convert half to float
                  uint16_t h = pixel[c];
                  uint32_t sign = (h & 0x8000) << 16;
                  uint32_t exp_half = (h & 0x7C00) >> 10;
                  uint32_t mant_half = h & 0x03FF;

                  uint32_t f_bits;
                  if (exp_half == 0) {
                    f_bits = sign; // Zero or denorm -> treat as zero
                  } else if (exp_half == 31) {
                    f_bits = sign | 0x7F800000 | (mant_half << 13); // Inf/NaN
                  } else {
                    uint32_t exp_f = exp_half - 15 + 127;
                    uint32_t mant_f = mant_half << 13;
                    f_bits = sign | (exp_f << 23) | mant_f;
                  }

                  float val = *reinterpret_cast<float*>(&f_bits);
                  val = std::pow(val, gamma_value);
                  f_bits = *reinterpret_cast<uint32_t*>(&val);

                  // Convert float back to half
                  sign = f_bits & 0x80000000;
                  uint32_t exp_f = (f_bits >> 23) & 0xFF;
                  uint32_t mant_f = f_bits & 0x007FFFFF;

                  uint16_t h_result;
                  if (exp_f == 0) {
                    h_result = sign >> 16;
                  } else if (exp_f == 255) {
                    h_result = (sign >> 16) | 0x7C00 | (mant_f >> 13);
                  } else {
                    int exp_h = int(exp_f) - 127 + 15;
                    if (exp_h <= 0) {
                      h_result = sign >> 16;
                    } else if (exp_h >= 31) {
                      h_result = (sign >> 16) | 0x7C00;
                    } else {
                      h_result = (sign >> 16) | (exp_h << 10) | (mant_f >> 13);
                    }
                  }
                  pixel[c] = h_result;
                }
              }
              break;
            }

            default:
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
}

////////////////////////////////////////////////////////////////
// Per-channel gamma correction
////////////////////////////////////////////////////////////////

void Image::gammaPerChannel(float gamma_r, float gamma_g, float gamma_b, float gamma_a) {

  // Build lookup tables for each channel (integer formats)
  uint8_t lut8_r[256], lut8_g[256], lut8_b[256], lut8_a[256];
  uint16_t lut16_r[65536], lut16_g[65536], lut16_b[65536], lut16_a[65536];

  // Precompute 8-bit LUTs
  for (int i = 0; i < 256; i++) {
    float normalized = float(i) / 255.0f;
    lut8_r[i] = uint8_t(std::pow(normalized, gamma_r) * 255.0f);
    lut8_g[i] = uint8_t(std::pow(normalized, gamma_g) * 255.0f);
    lut8_b[i] = uint8_t(std::pow(normalized, gamma_b) * 255.0f);
    lut8_a[i] = uint8_t(std::pow(normalized, gamma_a) * 255.0f);
  }

  // Precompute 16-bit LUTs
  for (int i = 0; i < 65536; i++) {
    float normalized = float(i) / 65535.0f;
    lut16_r[i] = uint16_t(std::pow(normalized, gamma_r) * 65535.0f);
    lut16_g[i] = uint16_t(std::pow(normalized, gamma_g) * 65535.0f);
    lut16_b[i] = uint16_t(std::pow(normalized, gamma_b) * 65535.0f);
    lut16_a[i] = uint16_t(std::pow(normalized, gamma_a) * 65535.0f);
  }

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_GAMMA_CHUNK_SIZE - 1) / IMG_GAMMA_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  using enum EBufferFormat;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, gamma_r, gamma_g, gamma_b, gamma_a,
               &lut8_r, &lut8_g, &lut8_b, &lut8_a,
               &lut16_r, &lut16_g, &lut16_b, &lut16_a, &chunkcounter]() {
      size_t y_start = chunk * IMG_GAMMA_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_GAMMA_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        for (size_t x = 0; x < this->_width; x++) {

          switch (this->_format) {
            ////////////////////////////////////////////////////////////////
            // 8-bit formats
            ////////////////////////////////////////////////////////////////
            case R8: {
              auto pixel = this->pixel8(x, y);
              pixel[0] = lut8_r[pixel[0]];
              break;
            }
            case RGB8:
            case BGR8: {
              auto pixel = this->pixel8(x, y);
              pixel[0] = lut8_r[pixel[0]];
              pixel[1] = lut8_g[pixel[1]];
              pixel[2] = lut8_b[pixel[2]];
              break;
            }
            case RGBA8:
            case BGRA8: {
              auto pixel = this->pixel8(x, y);
              pixel[0] = lut8_r[pixel[0]];
              pixel[1] = lut8_g[pixel[1]];
              pixel[2] = lut8_b[pixel[2]];
              pixel[3] = lut8_a[pixel[3]];
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit integer formats
            ////////////////////////////////////////////////////////////////
            case R16UI: {
              auto pixel = this->pixel16(x, y);
              pixel[0] = lut16_r[pixel[0]];
              break;
            }
            case RGB16: {
              auto pixel = this->pixel16(x, y);
              pixel[0] = lut16_r[pixel[0]];
              pixel[1] = lut16_g[pixel[1]];
              pixel[2] = lut16_b[pixel[2]];
              break;
            }
            case RGBA16: {
              auto pixel = this->pixel16(x, y);
              pixel[0] = lut16_r[pixel[0]];
              pixel[1] = lut16_g[pixel[1]];
              pixel[2] = lut16_b[pixel[2]];
              pixel[3] = lut16_a[pixel[3]];
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 32-bit float formats
            ////////////////////////////////////////////////////////////////
            case R32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = y * this->_width + x;
              data[pixel_index] = std::pow(data[pixel_index], gamma_r);
              break;
            }
            case RGB32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 3;
              data[pixel_index + 0] = std::pow(data[pixel_index + 0], gamma_r);
              data[pixel_index + 1] = std::pow(data[pixel_index + 1], gamma_g);
              data[pixel_index + 2] = std::pow(data[pixel_index + 2], gamma_b);
              break;
            }
            case RGBA32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 4;
              data[pixel_index + 0] = std::pow(data[pixel_index + 0], gamma_r);
              data[pixel_index + 1] = std::pow(data[pixel_index + 1], gamma_g);
              data[pixel_index + 2] = std::pow(data[pixel_index + 2], gamma_b);
              data[pixel_index + 3] = std::pow(data[pixel_index + 3], gamma_a);
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit half float format
            ////////////////////////////////////////////////////////////////
            case RGBA16F: {
              auto pixel = this->pixel16(x, y);
              float gammas[4] = {gamma_r, gamma_g, gamma_b, gamma_a};

              for (size_t c = 0; c < 4; c++) {
                // Convert half to float
                uint16_t h = pixel[c];
                uint32_t sign = (h & 0x8000) << 16;
                uint32_t exp_half = (h & 0x7C00) >> 10;
                uint32_t mant_half = h & 0x03FF;

                uint32_t f_bits;
                if (exp_half == 0) {
                  f_bits = sign;
                } else if (exp_half == 31) {
                  f_bits = sign | 0x7F800000 | (mant_half << 13);
                } else {
                  uint32_t exp_f = exp_half - 15 + 127;
                  uint32_t mant_f = mant_half << 13;
                  f_bits = sign | (exp_f << 23) | mant_f;
                }

                float val = *reinterpret_cast<float*>(&f_bits);
                val = std::pow(val, gammas[c]);
                f_bits = *reinterpret_cast<uint32_t*>(&val);

                // Convert float back to half
                sign = f_bits & 0x80000000;
                uint32_t exp_f = (f_bits >> 23) & 0xFF;
                uint32_t mant_f = f_bits & 0x007FFFFF;

                uint16_t h_result;
                if (exp_f == 0) {
                  h_result = sign >> 16;
                } else if (exp_f == 255) {
                  h_result = (sign >> 16) | 0x7C00 | (mant_f >> 13);
                } else {
                  int exp_h = int(exp_f) - 127 + 15;
                  if (exp_h <= 0) {
                    h_result = sign >> 16;
                  } else if (exp_h >= 31) {
                    h_result = (sign >> 16) | 0x7C00;
                  } else {
                    h_result = (sign >> 16) | (exp_h << 10) | (mant_f >> 13);
                  }
                }
                pixel[c] = h_result;
              }
              break;
            }

            default:
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
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2
