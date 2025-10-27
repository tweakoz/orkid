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

namespace ork::lev2 {
////////////////////////////////////////////////////////////////

// Chunk size for parallelized image processing
constexpr size_t IMG_PROC_CHUNK_SIZE = 128;

////////////////////////////////////////////////////////////////
// Channel inversion with bitmask control
// channel_mask bits: 0=R, 1=G, 2=B, 3=A
////////////////////////////////////////////////////////////////

void Image::invert(uint8_t channel_mask) {

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  using enum EBufferFormat;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, channel_mask, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        for (size_t x = 0; x < this->_width; x++) {

          switch (this->_format) {
            ////////////////////////////////////////////////////////////////
            // 8-bit formats
            ////////////////////////////////////////////////////////////////
            case R8: {
              if (channel_mask & 0x01) { // R channel
                auto pixel = this->pixel8(x, y);
                pixel[0] = 255 - pixel[0];
              }
              break;
            }
            case RGB8:
            case BGR8: {
              auto pixel = this->pixel8(x, y);
              if (channel_mask & 0x01) pixel[0] = 255 - pixel[0]; // R or B
              if (channel_mask & 0x02) pixel[1] = 255 - pixel[1]; // G
              if (channel_mask & 0x04) pixel[2] = 255 - pixel[2]; // B or R
              break;
            }
            case RGBA8:
            case BGRA8: {
              auto pixel = this->pixel8(x, y);
              if (channel_mask & 0x01) pixel[0] = 255 - pixel[0]; // R or B
              if (channel_mask & 0x02) pixel[1] = 255 - pixel[1]; // G
              if (channel_mask & 0x04) pixel[2] = 255 - pixel[2]; // B or R
              if (channel_mask & 0x08) pixel[3] = 255 - pixel[3]; // A
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit integer formats
            ////////////////////////////////////////////////////////////////
            case R16UI: {
              if (channel_mask & 0x01) { // R channel
                auto pixel = this->pixel16(x, y);
                pixel[0] = 65535 - pixel[0];
              }
              break;
            }
            case RGB16: {
              auto pixel = this->pixel16(x, y);
              if (channel_mask & 0x01) pixel[0] = 65535 - pixel[0]; // R
              if (channel_mask & 0x02) pixel[1] = 65535 - pixel[1]; // G
              if (channel_mask & 0x04) pixel[2] = 65535 - pixel[2]; // B
              break;
            }
            case RGBA16: {
              auto pixel = this->pixel16(x, y);
              if (channel_mask & 0x01) pixel[0] = 65535 - pixel[0]; // R
              if (channel_mask & 0x02) pixel[1] = 65535 - pixel[1]; // G
              if (channel_mask & 0x04) pixel[2] = 65535 - pixel[2]; // B
              if (channel_mask & 0x08) pixel[3] = 65535 - pixel[3]; // A
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 32-bit float formats
            ////////////////////////////////////////////////////////////////
            case R32F: {
              if (channel_mask & 0x01) { // R channel
                auto data = (float*)this->_data->data();
                size_t pixel_index = y * this->_width + x;
                data[pixel_index] = 1.0f - data[pixel_index];
              }
              break;
            }
            case RGB32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 3;
              if (channel_mask & 0x01) data[pixel_index + 0] = 1.0f - data[pixel_index + 0]; // R
              if (channel_mask & 0x02) data[pixel_index + 1] = 1.0f - data[pixel_index + 1]; // G
              if (channel_mask & 0x04) data[pixel_index + 2] = 1.0f - data[pixel_index + 2]; // B
              break;
            }
            case RGBA32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 4;
              if (channel_mask & 0x01) data[pixel_index + 0] = 1.0f - data[pixel_index + 0]; // R
              if (channel_mask & 0x02) data[pixel_index + 1] = 1.0f - data[pixel_index + 1]; // G
              if (channel_mask & 0x04) data[pixel_index + 2] = 1.0f - data[pixel_index + 2]; // B
              if (channel_mask & 0x08) data[pixel_index + 3] = 1.0f - data[pixel_index + 3]; // A
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit half float format
            ////////////////////////////////////////////////////////////////
            case RGBA16F: {
              auto pixel = this->pixel16(x, y);
              // Convert half to float, invert, convert back
              // Using simple bit manipulation for half float
              // Half format: sign(1) exponent(5) mantissa(10)
              for (size_t c = 0; c < 4; c++) {
                if (channel_mask & (1 << c)) {
                  uint16_t h = pixel[c];
                  // Convert half to float
                  uint32_t sign = (h & 0x8000) << 16;
                  uint32_t exp_half = (h & 0x7C00) >> 10;
                  uint32_t mant_half = h & 0x03FF;

                  uint32_t f_bits;
                  if (exp_half == 0) {
                    if (mant_half == 0) {
                      // Zero -> invert to one
                      f_bits = 0x3F800000; // 1.0f
                    } else {
                      // Denorm - convert properly
                      uint32_t exp_f = 127 - 15;
                      uint32_t mant_f = mant_half << 13;
                      // Normalize
                      while ((mant_f & 0x00800000) == 0) {
                        mant_f <<= 1;
                        exp_f--;
                      }
                      mant_f &= 0x007FFFFF;
                      f_bits = sign | (exp_f << 23) | mant_f;
                    }
                  } else if (exp_half == 31) {
                    // Inf/NaN - preserve
                    f_bits = sign | 0x7F800000 | (mant_half << 13);
                  } else {
                    // Normal
                    uint32_t exp_f = exp_half - 15 + 127;
                    uint32_t mant_f = mant_half << 13;
                    f_bits = sign | (exp_f << 23) | mant_f;
                  }

                  float val = *reinterpret_cast<float*>(&f_bits);
                  val = 1.0f - val;
                  f_bits = *reinterpret_cast<uint32_t*>(&val);

                  // Convert float back to half
                  sign = f_bits & 0x80000000;
                  uint32_t exp_f = (f_bits >> 23) & 0xFF;
                  uint32_t mant_f = f_bits & 0x007FFFFF;

                  uint16_t h_result;
                  if (exp_f == 0) {
                    h_result = sign >> 16; // Zero
                  } else if (exp_f == 255) {
                    // Inf/NaN
                    h_result = (sign >> 16) | 0x7C00 | (mant_f >> 13);
                  } else {
                    int exp_h = int(exp_f) - 127 + 15;
                    if (exp_h <= 0) {
                      // Underflow to zero
                      h_result = sign >> 16;
                    } else if (exp_h >= 31) {
                      // Overflow to inf
                      h_result = (sign >> 16) | 0x7C00;
                    } else {
                      // Normal
                      h_result = (sign >> 16) | (exp_h << 10) | (mant_f >> 13);
                    }
                  }

                  pixel[c] = h_result;
                }
              }
              break;
            }

            default:
              // Unsupported format - skip
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
