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
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/memcpy.inl>
#include <math.h>
#include <ork/util/logger.h>

namespace ork::lev2 {
logchannel_ptr_t logchan_image = logger()->configureChannel("IMAGE", fvec3(0.1, 0.2, 0.3), false);

static std::atomic<int> g_ImageInstanceCounter = 0;

  ///////////////////////////////////////////////////////////////////////////////

Image::Image() {
  int numimages = g_ImageInstanceCounter.fetch_add(1);
  if(0)printf("Image<%p> numimages<%d>\n", (void*)this, numimages+1);
}
Image::~Image(){
  int numimages = g_ImageInstanceCounter.fetch_sub(1);
  if(0)printf("~Image<%p> <%dx%d> numimages<%d>\n", (void*)this, _width, _height, numimages-1);
}

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

void Image::initWithFormat(size_t w, size_t h, EBufferFormat fmt) {
  _width = w;
  _height = h;
  _format = fmt;
  
  // Determine components and bytes per channel from format
  switch(fmt) {
    case EBufferFormat::R8:
      _numcomponents = 1;
      _bytesPerChannel = 1;
      break;
    case EBufferFormat::R16UI:
      _numcomponents = 1;
      _bytesPerChannel = 2;
      break;
    case EBufferFormat::R32F:
      _numcomponents = 1;
      _bytesPerChannel = 4;
      break;
    case EBufferFormat::R32UI:
      _numcomponents = 1;
      _bytesPerChannel = 4;
      break;
    case EBufferFormat::RGB8:
      _numcomponents = 3;
      _bytesPerChannel = 1;
      break;
    case EBufferFormat::BGRA8:
      _numcomponents = 4;
      _bytesPerChannel = 1;
    break;
    case EBufferFormat::RGBA8:
      _numcomponents = 4;
      _bytesPerChannel = 1;
      break;
    case EBufferFormat::RGBA16F:
      _numcomponents = 4;
      _bytesPerChannel = 2;
      break;
    case EBufferFormat::RGB32F:
      _numcomponents = 3;
      _bytesPerChannel = 4;
      break;
    case EBufferFormat::RGBA32F:
      _numcomponents = 4;
      _bytesPerChannel = 4;
      break;
    case EBufferFormat::RG32F:
      _numcomponents = 2;
      _bytesPerChannel = 4;
      break;
    case EBufferFormat::RGBA16UI:
      _numcomponents = 4;
      _bytesPerChannel = 2;
      break;
    case EBufferFormat::RGBA32UI:
      _numcomponents = 4;
      _bytesPerChannel = 4;
      break;
    default:
      OrkAssert(false); // Unsupported format
      break;
  }
  
  // Ensure data block exists and resize it
  if (!_data) {
    _data = std::make_shared<DataBlock>();
  }
  size_t bufsize = _width * _height * _numcomponents * _bytesPerChannel;
  _data->resize(bufsize); // no-ops if same size
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

///////////////////////////////////////////////////////////////////////////////

uint8_t* Image::pixel8(int x, int y) {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((uint8_t*)_data->data()) + index;
}
const uint8_t* Image::pixel8(int x, int y) const {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((const uint8_t*)_data->data()) + index;
}
uint16_t* Image::pixel16(int x, int y) {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((uint16_t*)_data->data()) + index;
}
const uint16_t* Image::pixel16(int x, int y) const {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((const uint16_t*)_data->data()) + index;
}
float* Image::pixel32f(int x, int y) {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((float*)_data->data()) + index;
}
const float* Image::pixel32f(int x, int y) const {
  if(x>=_width) x=_width-1;
  if(y>=_height) y=_height-1;
  int index = (y * _width + x) * _numcomponents;
  return ((const float*)_data->data()) + index;
}

///////////////////////////////////////////////////////////////////////////////

void Image::initRGB8WithColor(size_t w, size_t h, fvec3 color) {
  uint8_t r        = uint8_t(color.x * 255.0f);
  uint8_t g        = uint8_t(color.y * 255.0f);
  uint8_t b        = uint8_t(color.z * 255.0f);
  _format          = EBufferFormat::RGB8;
  _bytesPerChannel = 1;
  init(w, h, 3, 1);
  auto outptr = (uint8_t*)_data->data();
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

void Image::initRGBA8WithColor(size_t w, size_t h, fvec4 color) {
  uint8_t r        = uint8_t(color.x * 255.0f);
  uint8_t g        = uint8_t(color.y * 255.0f);
  uint8_t b        = uint8_t(color.z * 255.0f);
  uint8_t a        = uint8_t(color.w * 255.0f);
  _format          = EBufferFormat::RGBA8;
  _bytesPerChannel = 1;
  init(w, h, 4, 1);
  auto outptr = (uint8_t*)_data->data();
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
  using enum EBufferFormat;
  switch (numc) {
    case 1:
      _format = R8;
      break;
    case 3:
      _format = RGB8;
      break;
    case 4:
      _format = RGBA8;
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
#if 1 //defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
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
  logchan_image->log(
      "// Image::uncompressed(%s) w<%zu> h<%zu> BPC<%d> _format<%x>",
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
  imgout._bytesPerChannel = _bytesPerChannel;
  imgout._data           = std::make_shared<DataBlock>();
  size_t data_size       = _width * _height * _numcomponents * _bytesPerChannel;
  ork::Timer timer;
  timer.Start();
  std::atomic<int> pending = 0;
  size_t src_stride        = _width * _numcomponents * _bytesPerChannel;
  size_t dst_stride        = src_stride;
  auto src_base            = (uint8_t*)this->_data->data();
  auto dst_base            = (uint8_t*)imgout._data->allocateBlock(data_size);
  memcpy_fast(dst_base, src_base, data_size);

  float time = timer.SecsSinceStart();
  float MPPS = float(_width * _height) * 1e-6 / time;
  logchan_image->log("// compression time<%g> MPPS<%g>", time, MPPS);
}

///////////////////////////////////////////////////////////////////////////////

void Image::gaussianBlur(Image& imgout, float kernel_size) const {
  imgout._format = _format;
  imgout._bytesPerChannel = _bytesPerChannel;
  imgout._numcomponents = _numcomponents;
  imgout._width = _width;
  imgout._height = _height;
  imgout._data = std::make_shared<DataBlock>();
  imgout._data->allocateBlock(_width * _height * _numcomponents * _bytesPerChannel);
  
  // Compute Gaussian kernel
  int radius = int(kernel_size * 2.0f);
  float sigma = kernel_size / 2.0f;
  float sigma2 = sigma * sigma;
  float coeff = 1.0f / (2.0f * M_PI * sigma2);
  
  // Create kernel weights
  std::vector<float> kernel;
  std::vector<float> weights;
  float totalWeight = 0.0f;
  
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      float dist2 = float(x*x + y*y);
      float weight = coeff * expf(-dist2 / (2.0f * sigma2));
      kernel.push_back(weight);
      weights.push_back(weight);
      totalWeight += weight;
    }
  }
  
  // Normalize weights
  for (auto& w : weights) {
    w /= totalWeight;
  }
  
  // Apply filter
  int kernel_dim = 2 * radius + 1;
  uint8_t* dst_data = (uint8_t*)imgout._data->data();
  auto conq = opq::concurrentQueue();
  auto group = opq::createCompletionGroup(conq, "GaussianBlur");
  for (int y = 0; y < _height; y++) {
    group->enqueue([=](){
      for (int x = 0; x < _width; x++) {
        std::vector<float> sum(_numcomponents, 0.0f);
        int weight_idx = 0;
        
        for (int ky = -radius; ky <= radius; ky++) {
          for (int kx = -radius; kx <= radius; kx++) {
            int sx = std::max(0, std::min(int(_width) - 1, x + kx));
            int sy = std::max(0, std::min(int(_height) - 1, y + ky));
            
            const uint8_t* src_pixel = pixel8(sx, sy);
            float weight = weights[weight_idx++];
            
            for (size_t c = 0; c < _numcomponents; c++) {
              sum[c] += float(src_pixel[c]) * weight;
            }
          }
        }
        
        // Write blurred pixel
        uint8_t* dst_pixel = dst_data + ((y * _width + x) * _numcomponents);
        for (size_t c = 0; c < _numcomponents; c++) {
          dst_pixel[c] = uint8_t(std::min(255.0f, std::max(0.0f, sum[c])));
        }
      }
    });
  }
  group->join();
}

void Image::separableConvolve(Image& output, const std::vector<float>& kernel, fvec4 threshold) const {
  if (kernel.empty() || (kernel.size() % 2) == 0) {
    OrkAssert(false && "Kernel must have odd size");
    return;
  }

  // Initialize output image
  output.initWithFormat(_width, _height, _format);

  int radius = int(kernel.size() / 2);

  // Temporary buffer for horizontal pass
  auto temp_data = std::make_shared<DataBlock>();
  temp_data->allocateBlock(_width * _height * _numcomponents * sizeof(float));
  float* temp = (float*)temp_data->data();

  auto conq = opq::concurrentQueue();

  // Horizontal pass
  auto group_h = opq::createCompletionGroup(conq, "SeparableConvolveH");
  for (size_t y = 0; y < _height; y++) {
    group_h->enqueue([=](){
      for (size_t x = 0; x < _width; x++) {
        std::vector<float> sum(_numcomponents, 0.0f);
        float total_weight = 0.0f;

        // Convolve with neighbors that pass threshold
        for (int k = -radius; k <= radius; k++) {
          int sx = std::max(0, std::min(int(_width) - 1, int(x) + k));
          const float* neighbor_pixel = pixel32f(sx, y);

          // Check threshold on NEIGHBOR pixel - all components must be > threshold
          bool neighbor_passes = true;
          for (size_t c = 0; c < std::min(_numcomponents, size_t(4)); c++) {
            if (neighbor_pixel[c] <= threshold[c]) {
              neighbor_passes = false;
              break;
            }
          }

          if (neighbor_passes) {
            float weight = kernel[k + radius];
            total_weight += weight;

            for (size_t c = 0; c < _numcomponents; c++) {
              sum[c] += neighbor_pixel[c] * weight;
            }
          }
        }

        // Normalize by actual weight used and write to temp buffer
        size_t idx = (y * _width + x) * _numcomponents;
        if (total_weight > 0.0f) {
          for (size_t c = 0; c < _numcomponents; c++) {
            temp[idx + c] = sum[c] / total_weight;
          }
        } else {
          // No neighbors passed threshold - copy original
          const float* src_pixel = pixel32f(x, y);
          for (size_t c = 0; c < _numcomponents; c++) {
            temp[idx + c] = src_pixel[c];
          }
        }
      }
    });
  }
  group_h->join();

  // Vertical pass
  auto group_v = opq::createCompletionGroup(conq, "SeparableConvolveV");
  for (size_t y = 0; y < _height; y++) {
    group_v->enqueue([=, &output, &temp](){
      for (size_t x = 0; x < _width; x++) {
        std::vector<float> sum(_numcomponents, 0.0f);
        float total_weight = 0.0f;

        // Convolve with neighbors that pass threshold
        for (int k = -radius; k <= radius; k++) {
          int sy = std::max(0, std::min(int(_height) - 1, int(y) + k));
          size_t neighbor_idx = (sy * _width + x) * _numcomponents;

          // Check threshold on NEIGHBOR pixel - all components must be > threshold
          bool neighbor_passes = true;
          for (size_t c = 0; c < std::min(_numcomponents, size_t(4)); c++) {
            if (temp[neighbor_idx + c] <= threshold[c]) {
              neighbor_passes = false;
              break;
            }
          }

          if (neighbor_passes) {
            float weight = kernel[k + radius];
            total_weight += weight;

            for (size_t c = 0; c < _numcomponents; c++) {
              sum[c] += temp[neighbor_idx + c] * weight;
            }
          }
        }

        // Normalize by actual weight used and write to output
        float* dst_pixel = output.pixel32f(x, y);
        if (total_weight > 0.0f) {
          for (size_t c = 0; c < _numcomponents; c++) {
            dst_pixel[c] = sum[c] / total_weight;
          }
        } else {
          // No neighbors passed threshold - copy from temp
          size_t idx_src = (y * _width + x) * _numcomponents;
          for (size_t c = 0; c < _numcomponents; c++) {
            dst_pixel[c] = temp[idx_src + c];
          }
        }
      }
    });
  }
  group_v->join();
}

void Image::lerp(const Image& a, const Image& b, float index) {
  // Clamp interpolation index to [0,1]
  float t = std::max(0.0f, std::min(1.0f, index));
  
  // Verify images have same dimensions and format
  if (a._width != b._width || a._height != b._height || 
      a._numcomponents != b._numcomponents || 
      a._bytesPerChannel != b._bytesPerChannel) {
    OrkAssert(false && "Images must have same dimensions and format");
    return;
  }
  
  // Initialize this image with same properties
  _format = a._format;
  _bytesPerChannel = a._bytesPerChannel;
  _numcomponents = a._numcomponents;
  _width = a._width;
  _height = a._height;
  _data = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents * _bytesPerChannel);
  
  // Only handle 8-bit per channel case for simplicity
  if (_bytesPerChannel == 1) {
    uint8_t* dst_data = (uint8_t*)_data->data();
    const uint8_t* src_a = (const uint8_t*)a._data->data();
    const uint8_t* src_b = (const uint8_t*)b._data->data();
    
    size_t total_pixels = _width * _height;
    size_t total_elements = total_pixels * _numcomponents;
    
    for (size_t i = 0; i < total_elements; i++) {
      float val_a = float(src_a[i]);
      float val_b = float(src_b[i]);
      float blended = val_a * (1.0f - t) + val_b * t;
      dst_data[i] = uint8_t(std::min(255.0f, std::max(0.0f, blended)));
    }
  }
  // Handle 16-bit per channel case if needed
  else if (_bytesPerChannel == 2) {
    uint16_t* dst_data = (uint16_t*)_data->data();
    const uint16_t* src_a = (const uint16_t*)a._data->data();
    const uint16_t* src_b = (const uint16_t*)b._data->data();
    
    size_t total_pixels = _width * _height;
    size_t total_elements = total_pixels * _numcomponents;
    
    for (size_t i = 0; i < total_elements; i++) {
      float val_a = float(src_a[i]);
      float val_b = float(src_b[i]);
      float blended = val_a * (1.0f - t) + val_b * t;
      dst_data[i] = uint16_t(std::min(65535.0f, std::max(0.0f, blended)));
    }
  }
  else {
    OrkAssert(false && "Unsupported bytes per channel");
  }
}

void Image::fullBlurOf(const Image& a) {
  // Make sure this image has the same dimensions and format as input
  _format = a._format;
  _bytesPerChannel = a._bytesPerChannel;
  _numcomponents = a._numcomponents;
  _width = a._width;
  _height = a._height;
  _data = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents * _bytesPerChannel);
  
  // Compute average color
  std::vector<float> avg_color(_numcomponents, 0.0f);
  size_t total_pixels = a._width * a._height;
  
  // For 8-bit per channel
  if (a._bytesPerChannel == 1) {
    const uint8_t* src_data = (const uint8_t*)a._data->data();
    
    // Sum up all values for each component
    for (size_t y = 0; y < a._height; y++) {
      for (size_t x = 0; x < a._width; x++) {
        const uint8_t* pixel = a.pixel8(x, y);
        for (size_t c = 0; c < _numcomponents; c++) {
          avg_color[c] += float(pixel[c]);
        }
      }
    }
    
    // Calculate average
    for (size_t c = 0; c < _numcomponents; c++) {
      avg_color[c] /= float(total_pixels);
    }
    
    // Fill this image with the average color
    uint8_t* dst_data = (uint8_t*)_data->data();
    for (size_t i = 0; i < total_pixels; i++) {
      for (size_t c = 0; c < _numcomponents; c++) {
        dst_data[i * _numcomponents + c] = uint8_t(avg_color[c]);
      }
    }
  }
  // For 16-bit per channel
  else if (a._bytesPerChannel == 2) {
    const uint16_t* src_data = (const uint16_t*)a._data->data();
    
    // Sum up all values for each component
    for (size_t y = 0; y < a._height; y++) {
      for (size_t x = 0; x < a._width; x++) {
        const uint16_t* pixel = a.pixel16(x, y);
        for (size_t c = 0; c < _numcomponents; c++) {
          avg_color[c] += float(pixel[c]);
        }
      }
    }
    
    // Calculate average
    for (size_t c = 0; c < _numcomponents; c++) {
      avg_color[c] /= float(total_pixels);
    }
    
    // Fill this image with the average color
    uint16_t* dst_data = (uint16_t*)_data->data();
    for (size_t i = 0; i < total_pixels; i++) {
      for (size_t c = 0; c < _numcomponents; c++) {
        dst_data[i * _numcomponents + c] = uint16_t(avg_color[c]);
      }
    }
  }
  else {
    OrkAssert(false && "Unsupported bytes per channel");
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
