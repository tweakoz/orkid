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
// Rotate image 90 degrees clockwise (returns new image)
// (x, y) → (height - 1 - y, x)
// new_width = old_height, new_height = old_width
////////////////////////////////////////////////////////////////

image_ptr_t Image::rotated90cw() const {
  auto result = std::make_shared<Image>();
  result->init(_height, _width, _numcomponents, _bytesPerChannel);
  result->_format = _format;
  result->_debugName = _debugName + "_rot90cw";

  size_t bytes_per_pixel = _numcomponents * _bytesPerChannel;

  // Parallelize by chunking output rows
  size_t num_chunks = (result->_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, result, bytes_per_pixel, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, result->_height);

      for (size_t new_y = y_start; new_y < y_end; new_y++) {
        for (size_t new_x = 0; new_x < result->_width; new_x++) {
          // Clockwise rotation: (old_x, old_y) → (old_height - 1 - old_y, old_x)
          // Inverse: (new_x, new_y) → (new_y, old_height - 1 - new_x)
          size_t old_x = new_y;
          size_t old_y = this->_height - 1 - new_x;

          size_t old_offset = (old_y * this->_width + old_x) * bytes_per_pixel;
          size_t new_offset = (new_y * result->_width + new_x) * bytes_per_pixel;

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
// Rotate image 90 degrees counter-clockwise (returns new image)
// (x, y) → (y, width - 1 - x)
// new_width = old_height, new_height = old_width
////////////////////////////////////////////////////////////////

image_ptr_t Image::rotated90ccw() const {
  auto result = std::make_shared<Image>();
  result->init(_height, _width, _numcomponents, _bytesPerChannel);
  result->_format = _format;
  result->_debugName = _debugName + "_rot90ccw";

  size_t bytes_per_pixel = _numcomponents * _bytesPerChannel;

  // Parallelize by chunking output rows
  size_t num_chunks = (result->_height + IMG_PROC_CHUNK_SIZE - 1) / IMG_PROC_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, result, bytes_per_pixel, &chunkcounter]() {
      size_t y_start = chunk * IMG_PROC_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_PROC_CHUNK_SIZE, result->_height);

      for (size_t new_y = y_start; new_y < y_end; new_y++) {
        for (size_t new_x = 0; new_x < result->_width; new_x++) {
          // Counter-clockwise rotation: (old_x, old_y) → (old_y, old_width - 1 - old_x)
          // Inverse: (new_x, new_y) → (old_width - 1 - new_y, new_x)
          size_t old_x = this->_width - 1 - new_y;
          size_t old_y = new_x;

          size_t old_offset = (old_y * this->_width + old_x) * bytes_per_pixel;
          size_t new_offset = (new_y * result->_width + new_x) * bytes_per_pixel;

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
// Mutating wrappers
////////////////////////////////////////////////////////////////

void Image::rotate90cw() {
  *this = *rotated90cw();
}

void Image::rotate90ccw() {
  *this = *rotated90ccw();
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2
