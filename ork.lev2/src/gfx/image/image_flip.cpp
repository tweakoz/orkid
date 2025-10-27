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
// Flip image horizontally (returns new image)
// (x, y) → (width - 1 - x, y)
////////////////////////////////////////////////////////////////

image_ptr_t Image::hFlipped() const {
  auto result = std::make_shared<Image>();
  result->init(_width, _height, _numcomponents, _bytesPerChannel);
  result->_format = _format;
  result->_debugName = _debugName + "_hFlip";

  size_t bytes_per_pixel = _numcomponents * _bytesPerChannel;

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, result, bytes_per_pixel, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        for (size_t x = 0; x < this->_width; x++) {
          size_t new_x = this->_width - 1 - x;

          size_t old_offset = (y * this->_width + x) * bytes_per_pixel;
          size_t new_offset = (y * result->_width + new_x) * bytes_per_pixel;

          memcpy(
            (uint8_t*)result->_data->data() + new_offset,
            (uint8_t*)this->_data->data() + old_offset,
            bytes_per_pixel
          );
        }
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }

  return result;
}

////////////////////////////////////////////////////////////////
// Flip image vertically (returns new image)
// (x, y) → (x, height - 1 - y)
////////////////////////////////////////////////////////////////

image_ptr_t Image::vFlipped() const {
  auto result = std::make_shared<Image>();
  result->init(_width, _height, _numcomponents, _bytesPerChannel);
  result->_format = _format;
  result->_debugName = _debugName + "_vFlip";

  size_t bytes_per_pixel = _numcomponents * _bytesPerChannel;

  // Parallelize by chunking rows
  size_t num_chunks = (_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, result, bytes_per_pixel, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, this->_height);

      for (size_t y = y_start; y < y_end; y++) {
        size_t new_y = this->_height - 1 - y;

        size_t old_row_offset = y * this->_width * bytes_per_pixel;
        size_t new_row_offset = new_y * result->_width * bytes_per_pixel;

        // Can copy entire row at once
        memcpy(
          (uint8_t*)result->_data->data() + new_row_offset,
          (uint8_t*)this->_data->data() + old_row_offset,
          this->_width * bytes_per_pixel
        );
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }

  return result;
}

////////////////////////////////////////////////////////////////
// Mutating wrappers
////////////////////////////////////////////////////////////////

void Image::hFlip() {
  *this = *hFlipped();
}

void Image::vFlip() {
  *this = *vFlipped();
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2
