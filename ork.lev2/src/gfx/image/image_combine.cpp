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
// Color matrix transformation
// Each output channel is a linear combination of input channels
// output[i] = sum(input[j] * matrix[i][j])
// For RGB: uses 3x3 portion
// For RGBA: uses full 4x4
// For single channel: uses first row
////////////////////////////////////////////////////////////////

void Image::combine(const fmtx4& matrix) {

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  using enum EBufferFormat;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, &matrix, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        for (size_t x = 0; x < this->_width; x++) {

          switch (this->_format) {
            ////////////////////////////////////////////////////////////////
            // 8-bit formats
            ////////////////////////////////////////////////////////////////
            case R8: {
              auto pixel = this->pixel8(x, y);
              float in_r = float(pixel[0]) / 255.0f;
              float out_r = in_r * matrix.elemXY(0, 0);
              pixel[0] = uint8_t(std::clamp(out_r, 0.0f, 1.0f) * 255.0f);
              break;
            }
            case RGB8:
            case BGR8: {
              auto pixel = this->pixel8(x, y);
              float in[3];
              for (size_t c = 0; c < 3; c++) {
                in[c] = float(pixel[c]) / 255.0f;
              }
              float out[3];
              for (size_t i = 0; i < 3; i++) {
                out[i] = in[0] * matrix.elemXY(i, 0) +
                         in[1] * matrix.elemXY(i, 1) +
                         in[2] * matrix.elemXY(i, 2);
              }
              for (size_t c = 0; c < 3; c++) {
                pixel[c] = uint8_t(std::clamp(out[c], 0.0f, 1.0f) * 255.0f);
              }
              break;
            }
            case RGBA8:
            case BGRA8: {
              auto pixel = this->pixel8(x, y);
              float in[4];
              for (size_t c = 0; c < 4; c++) {
                in[c] = float(pixel[c]) / 255.0f;
              }
              float out[4];
              for (size_t i = 0; i < 4; i++) {
                out[i] = in[0] * matrix.elemXY(i, 0) +
                         in[1] * matrix.elemXY(i, 1) +
                         in[2] * matrix.elemXY(i, 2) +
                         in[3] * matrix.elemXY(i, 3);
              }
              for (size_t c = 0; c < 4; c++) {
                pixel[c] = uint8_t(std::clamp(out[c], 0.0f, 1.0f) * 255.0f);
              }
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit integer formats
            ////////////////////////////////////////////////////////////////
            case R16UI: {
              auto pixel = this->pixel16(x, y);
              float in_r = float(pixel[0]) / 65535.0f;
              float out_r = in_r * matrix.elemXY(0, 0);
              pixel[0] = uint16_t(std::clamp(out_r, 0.0f, 1.0f) * 65535.0f);
              break;
            }
            case RGB16: {
              auto pixel = this->pixel16(x, y);
              float in[3];
              for (size_t c = 0; c < 3; c++) {
                in[c] = float(pixel[c]) / 65535.0f;
              }
              float out[3];
              for (size_t i = 0; i < 3; i++) {
                out[i] = in[0] * matrix.elemXY(i, 0) +
                         in[1] * matrix.elemXY(i, 1) +
                         in[2] * matrix.elemXY(i, 2);
              }
              for (size_t c = 0; c < 3; c++) {
                pixel[c] = uint16_t(std::clamp(out[c], 0.0f, 1.0f) * 65535.0f);
              }
              break;
            }
            case RGBA16: {
              auto pixel = this->pixel16(x, y);
              float in[4];
              for (size_t c = 0; c < 4; c++) {
                in[c] = float(pixel[c]) / 65535.0f;
              }
              float out[4];
              for (size_t i = 0; i < 4; i++) {
                out[i] = in[0] * matrix.elemXY(i, 0) +
                         in[1] * matrix.elemXY(i, 1) +
                         in[2] * matrix.elemXY(i, 2) +
                         in[3] * matrix.elemXY(i, 3);
              }
              for (size_t c = 0; c < 4; c++) {
                pixel[c] = uint16_t(std::clamp(out[c], 0.0f, 1.0f) * 65535.0f);
              }
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 32-bit float formats (no clamping)
            ////////////////////////////////////////////////////////////////
            case R32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = y * this->_width + x;
              float in_r = data[pixel_index];
              data[pixel_index] = in_r * matrix.elemXY(0, 0);
              break;
            }
            case RGB32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 3;
              float in[3];
              for (size_t c = 0; c < 3; c++) {
                in[c] = data[pixel_index + c];
              }
              for (size_t i = 0; i < 3; i++) {
                data[pixel_index + i] = in[0] * matrix.elemXY(i, 0) +
                                        in[1] * matrix.elemXY(i, 1) +
                                        in[2] * matrix.elemXY(i, 2);
              }
              break;
            }
            case RGBA32F: {
              auto data = (float*)this->_data->data();
              size_t pixel_index = (y * this->_width + x) * 4;
              float in[4];
              for (size_t c = 0; c < 4; c++) {
                in[c] = data[pixel_index + c];
              }
              for (size_t i = 0; i < 4; i++) {
                data[pixel_index + i] = in[0] * matrix.elemXY(i, 0) +
                                        in[1] * matrix.elemXY(i, 1) +
                                        in[2] * matrix.elemXY(i, 2) +
                                        in[3] * matrix.elemXY(i, 3);
              }
              break;
            }

            ////////////////////////////////////////////////////////////////
            // 16-bit half float format (no clamping)
            ////////////////////////////////////////////////////////////////
            case RGBA16F: {
              auto pixel = this->pixel16(x, y);
              float in[4];

              // Convert all channels from half to float
              for (size_t c = 0; c < 4; c++) {
                uint16_t h = pixel[c];
                uint32_t sign = (h & 0x8000) << 16;
                uint32_t exp_half = (h & 0x7C00) >> 10;
                uint32_t mant_half = h & 0x03FF;

                uint32_t f_bits;
                if (exp_half == 0) {
                  if (mant_half == 0) {
                    f_bits = sign; // Zero
                  } else {
                    // Denorm
                    uint32_t exp_f = 127 - 15;
                    uint32_t mant_f = mant_half << 13;
                    while ((mant_f & 0x00800000) == 0) {
                      mant_f <<= 1;
                      exp_f--;
                    }
                    mant_f &= 0x007FFFFF;
                    f_bits = sign | (exp_f << 23) | mant_f;
                  }
                } else if (exp_half == 31) {
                  // Inf/NaN
                  f_bits = sign | 0x7F800000 | (mant_half << 13);
                } else {
                  // Normal
                  uint32_t exp_f = exp_half - 15 + 127;
                  uint32_t mant_f = mant_half << 13;
                  f_bits = sign | (exp_f << 23) | mant_f;
                }
                in[c] = *reinterpret_cast<float*>(&f_bits);
              }

              // Apply matrix transformation
              float out[4];
              for (size_t i = 0; i < 4; i++) {
                out[i] = in[0] * matrix.elemXY(i, 0) +
                         in[1] * matrix.elemXY(i, 1) +
                         in[2] * matrix.elemXY(i, 2) +
                         in[3] * matrix.elemXY(i, 3);
              }

              // Convert all channels from float to half
              for (size_t c = 0; c < 4; c++) {
                uint32_t f_bits = *reinterpret_cast<uint32_t*>(&out[c]);
                uint32_t sign = f_bits & 0x80000000;
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
                    h_result = sign >> 16; // Underflow
                  } else if (exp_h >= 31) {
                    h_result = (sign >> 16) | 0x7C00; // Overflow
                  } else {
                    h_result = (sign >> 16) | (exp_h << 10) | (mant_f >> 13);
                  }
                }
                pixel[c] = h_result;
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
