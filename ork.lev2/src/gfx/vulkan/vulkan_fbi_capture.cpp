////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_captureasync.h"
#include "headers/vulkan_ctx.h"
#include "headers/vk_misc.h"
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/pickbuffer.h>

namespace ork::lev2::vulkan {

////////////////////////////////////////////////////////////////

VkCaptureAsyncImpl::VkCaptureAsyncImpl(vkcontext_rawptr_t ctx) 
    : _contextVK(ctx) {
}

////////////////////////////////////////////////////////////////

VkCaptureAsyncImpl::~VkCaptureAsyncImpl() {
  // Ensure we've waited for any pending operations
  if (_fence && !_dataRetrieved) {
    vkWaitForFences(_contextVK->_vkdevice, 1, &_fence->_vkfence, VK_TRUE, UINT64_MAX);
  }
}

///////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::capture(const RtBuffer* inpbuf, const file::Path& pth, void_lambda_t on_capture_complete) {
  // For now, return a simple future that completes after one frame
  // The actual GPU transfer happens in captureAsFormat which records the commands
  // After endFrame submits the command buffer, the data will be available
  
  auto future = std::make_shared<CaptureAsync>();
  future->_width = inpbuf->_width;
  future->_height = inpbuf->_height;
  future->_format = EBufferFormat::RGBA8;
  
  // Capture to a buffer (this records the GPU commands)
  auto capbuf = std::make_shared<CaptureBuffer>();
  if (!captureAsFormat(inpbuf, capbuf, EBufferFormat::RGBA8)) {
    future->_failed = true;
    return future;
  }
  
  // Verify staging buffer was created
  if (!capbuf->_impl.isSet()) {
    future->_failed = true;
    return future;
  }
  
  // Store capture data in the future's implementation using VkCaptureAsyncImpl
  auto async_impl = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
  async_impl->capture_buffer = capbuf;
  async_impl->path = pth;
  async_impl->width = inpbuf->_width;
  async_impl->height = inpbuf->_height;
  async_impl->format = EBufferFormat::RGBA8;
  async_impl->frame_submitted = false;
  
  future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
  future->_on_capture_complete = on_capture_complete;
  
  // After one frame iteration, the command buffer will be submitted and executed
  // So we'll mark as ready after that
  _contextVK->_pending_captures.push_back(future);
  
  return future;
}

///////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::captureToTexture(const RtBuffer* inpbuf, Texture& tex, void_lambda_t on_capture_complete) {
  auto future = std::make_shared<CaptureAsync>();
  future->_width = inpbuf->_width;
  future->_height = inpbuf->_height;
  future->_format = inpbuf->format();
  
  // Create temporary capture buffer
  auto capbuf = std::make_shared<CaptureBuffer>();
  
  // Use captureAsFormat to do the actual capture
  auto base_future = captureAsFormat(inpbuf, capbuf, inpbuf->format());
  if (!base_future || base_future->_failed) {
    future->_failed = true;
    return future;
  }
  
  OrkAssert(false); // captureToTexture not implemented yet
  
  future->_captureTexture = nullptr;
  future->_failed = true;
  future->_on_capture_complete = on_capture_complete;
  
  return future;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_processPendingCaptures() {
  if (_pending_captures.empty()) {
    return;
  }
  
  // Process all pending captures
  auto captures_to_process = _pending_captures;
  _pending_captures.clear();
  
  for (auto capture_async : captures_to_process) {
    auto async_impl = capture_async->_impl.getShared<VkCaptureAsyncImpl>();
    
    // Get the capture buffer implementation
    auto capbuf_impl = async_impl->capture_buffer->_impl.getShared<VkCaptureBufferImpl>();
    auto staging_buffer = capbuf_impl->staging_buffer;
    
    // Determine if conversion is needed
    EBufferFormat source_format = VkFormatConverter::convertBufferFormat(capbuf_impl->_actual_format);
    bool needs_conversion = (source_format != capbuf_impl->_desired_format);
    
    if (needs_conversion) {
      // Copy to temp image for conversion
      // if same capturebuffer is reused for same RtBuffer, this avoids reallocation
      
      auto temp_img = async_impl->capture_buffer->_raw_image;
      temp_img->initWithFormat(async_impl->width, async_impl->height, source_format);
      // Use actual buffer size from staging buffer (source format size)
      size_t bufsize = staging_buffer->_length;
      staging_buffer->copyToHost((void*)temp_img->_data->data(), bufsize);
      
      // Do conversion async on opq
      opq::concurrentQueue()->enqueue([capture_async]() {
        auto async_impl = capture_async->_impl.getShared<VkCaptureAsyncImpl>();
        auto capbuf = async_impl->capture_buffer;
        auto capbuf_impl = capbuf->_impl.getShared<VkCaptureBufferImpl>();
        capbuf->_image->convertFromImageToFormat(*capbuf->_raw_image, capbuf_impl->_desired_format);
        
        // Signal completion
        capture_async->_completed = true;
        if (capture_async->_on_capture_complete) {
          capture_async->_on_capture_complete();
        }
      });
    } else {
      // No conversion needed - copy directly to main image
      // if same capturebuffer is reused for same RtBuffer, this avoids reallocation
      
      auto img = async_impl->capture_buffer->_image;
      img->initWithFormat(async_impl->width, async_impl->height, source_format);
      size_t bufsize = staging_buffer->_length;
      staging_buffer->copyToHost((void*)img->_data->data(), bufsize);
      
      // Signal immediately
      capture_async->_completed = true;
      if (capture_async->_on_capture_complete) {
        capture_async->_on_capture_complete();
      }
    }
    
    // Store capture results in the future
    capture_async->_captureBuffer = async_impl->capture_buffer;
    capture_async->_captureTexture = async_impl->capture_texture;
    capture_async->_capturePath = async_impl->path;
    capture_async->_width = async_impl->width;
    capture_async->_height = async_impl->height;
    capture_async->_format = async_impl->format;
    
    // If this is a single pixel capture, populate the PixelFetchContext
    if (capture_async->_pixelFetchContext && capture_async->_width == 1 && capture_async->_height == 1) {
      auto pixfetch_ctx = capture_async->_pixelFetchContext;
      auto img = async_impl->capture_buffer->_image;
      
      if (img && img->_data && pixfetch_ctx->_pickvalues.size() > 0) {
        // Get the usage mode for the first (and only) MRT
        auto usage_mode = pixfetch_ctx->_usage.size() > 0 ? pixfetch_ctx->_usage[0] : PixelFetchContext::EPixelUsage::FVEC4;
        
        // Extract the pixel value based on format and usage mode
        switch (usage_mode) {
          case PixelFetchContext::EPixelUsage::SVARIANT: {
            // Store raw data as svariant based on format
            switch (capture_async->_format) {
              case EBufferFormat::RGBA32F: {
                auto pixel_data = reinterpret_cast<const float*>(img->_data->data());
                fvec4 rgba(pixel_data[0], pixel_data[1], pixel_data[2], pixel_data[3]);
                pixfetch_ctx->_pickvalues[0] = pixfetch_ctx->decodePixel(rgba);
                break;
              }
              case EBufferFormat::RGBA16UI: {
                auto pixel_data = reinterpret_cast<const uint16_t*>(img->_data->data());
                // Pack as u32vec4 for decodePixel (will extend 16-bit to 32-bit)
                u32vec4 value(pixel_data[0], pixel_data[1], pixel_data[2], pixel_data[3]);
                pixfetch_ctx->_pickvalues[0] = pixfetch_ctx->decodePixel(value);
                break;
              }
              case EBufferFormat::RGBA32UI: {
                auto pixel_data = reinterpret_cast<const uint32_t*>(img->_data->data());
                u32vec4 value(pixel_data[0], pixel_data[1], pixel_data[2], pixel_data[3]);
                pixfetch_ctx->_pickvalues[0] = pixfetch_ctx->decodePixel(value);
                break;
              }
              default:
                pixfetch_ctx->_pickvalues[0] = nullptr;
                break;
            }
            break;
          }
          case PixelFetchContext::EPixelUsage::PTR64: {
            // Pack data into 64-bit pointer
            switch (capture_async->_format) {
              case EBufferFormat::RGBA16UI: {
                auto pixel_data = reinterpret_cast<const uint16_t*>(img->_data->data());
                // Swizzle so hex appears as xxxxyyyyzzzzwwww (same as GL implementation)
                uint64_t a = uint64_t(pixel_data[0]);
                uint64_t b = uint64_t(pixel_data[1]);
                uint64_t c = uint64_t(pixel_data[2]);
                uint64_t d = uint64_t(pixel_data[3]);
                uint64_t value = (d << 48) | (c << 32) | (b << 16) | a;
                pixfetch_ctx->_pickvalues[0].set<uint64_t>(value);
                break;
              }
              case EBufferFormat::RGBA32UI: {
                auto pixel_data = reinterpret_cast<const uint32_t*>(img->_data->data());
                // Pack two 32-bit values into 64-bit (using R and G channels)
                uint64_t low = uint64_t(pixel_data[0]);
                uint64_t high = uint64_t(pixel_data[1]);
                uint64_t value = (high << 32) | low;
                pixfetch_ctx->_pickvalues[0].set<uint64_t>(value);
                break;
              }
              default:
                pixfetch_ctx->_pickvalues[0].set<uint64_t>(0);
                break;
            }
            break;
          }
          case PixelFetchContext::EPixelUsage::FVEC4:
          default: {
            // Convert to fvec4 (default behavior)
            switch (capture_async->_format) {
              case EBufferFormat::RGBA8: {
                auto pixel_data = img->_data->data();
                float r = pixel_data[0] / 255.0f;
                float g = pixel_data[1] / 255.0f;
                float b = pixel_data[2] / 255.0f;
                float a = pixel_data[3] / 255.0f;
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(r, g, b, a));
                break;
              }
              case EBufferFormat::RGBA16F: {
                auto pixel_data = reinterpret_cast<const uint16_t*>(img->_data->data());
                // Convert half-float to float using bit manipulation
                auto half_to_float = [](uint16_t h) -> float {
                  uint32_t sign = (h & 0x8000) << 16;
                  uint32_t exponent = ((h & 0x7C00) >> 10);
                  uint32_t mantissa = (h & 0x03FF) << 13;
                  
                  if (exponent == 0) {
                    // Denormalized number or zero
                    if (mantissa == 0) return 0.0f;
                    // Convert denormalized half to normalized float
                    exponent = 1;
                    while (!(mantissa & 0x00800000)) {
                      mantissa <<= 1;
                      exponent--;
                    }
                    mantissa &= ~0x00800000;
                    exponent += 127 - 15;
                  } else if (exponent == 0x1F) {
                    // Infinity or NaN
                    exponent = 0xFF;
                  } else {
                    // Normalized number
                    exponent += 127 - 15;
                  }
                  
                  uint32_t result = sign | (exponent << 23) | mantissa;
                  return *reinterpret_cast<float*>(&result);
                };
                
                float r = half_to_float(pixel_data[0]);
                float g = half_to_float(pixel_data[1]);
                float b = half_to_float(pixel_data[2]);
                float a = half_to_float(pixel_data[3]);
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(r, g, b, a));
                break;
              }
              case EBufferFormat::RGBA32F: {
                auto pixel_data = reinterpret_cast<const float*>(img->_data->data());
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(pixel_data[0], pixel_data[1], pixel_data[2], pixel_data[3]));
                break;
              }
              case EBufferFormat::RGBA16UI: {
                auto pixel_data = reinterpret_cast<const uint16_t*>(img->_data->data());
                // Convert uint16 values to float
                float r = static_cast<float>(pixel_data[0]);
                float g = static_cast<float>(pixel_data[1]);
                float b = static_cast<float>(pixel_data[2]);
                float a = static_cast<float>(pixel_data[3]);
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(r, g, b, a));
                break;
              }
              case EBufferFormat::RGBA32UI: {
                auto pixel_data = reinterpret_cast<const uint32_t*>(img->_data->data());
                // Convert uint32 values to float (note: may lose precision for large values)
                float r = static_cast<float>(pixel_data[0]);
                float g = static_cast<float>(pixel_data[1]);
                float b = static_cast<float>(pixel_data[2]);
                float a = static_cast<float>(pixel_data[3]);
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(r, g, b, a));
                break;
              }
              default:
                pixfetch_ctx->_pickvalues[0].set<fvec4>(fvec4(0, 0, 0, 0));
                break;
            }
            break;
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::captureAsFormat(
    const RtBuffer* inpbuf,
    capturebuffer_ptr_t capbuf,
    EBufferFormat destfmt,
    void_lambda_t on_capture_complete) {

  auto future     = std::make_shared<CaptureAsync>();
  future->_width  = inpbuf->_width;
  future->_height = inpbuf->_height;
  future->_format = destfmt;

  OrkAssert(inpbuf->_impl.isShared<VklRtBufferImpl>()); // must have been implemented for vulkan already
  auto rtbi = inpbuf->_impl.getShared<VklRtBufferImpl>();
  if (nullptr == capbuf) {
    future->_failed = true;
    return future;
  }
  int x = 0;
  int y = 0;
  int w = inpbuf->_width;
  int h = inpbuf->_height;

  if (capbuf->_captureW != 0) {
    x = capbuf->_captureX;
    y = capbuf->_captureY;
    w = capbuf->_captureW;
    h = capbuf->_captureH;
  }

  // Capture must be called during a frame when command buffer is active
  auto cb = _contextVK->primary_cb();
  OrkAssert(cb != nullptr); // capture must be called during frame recording

  /*
  printf("VkFrameBufferInterface::captureAsFormat rtb<%p> w<%d> h<%d> has_impl<%d>\n",
         inpbuf, w, h, inpbuf->_impl.isSet());
  printf("  rtb->_impl.isShared<VklRtBufferImpl>() = %d\n",
         inpbuf->_impl.isShared<VklRtBufferImpl>());
  */

  rtbi->_transitionToHostRead(cb);

  // printf("captureAsFormat w<%d> h<%d>\n", w, h);

  bool fmtmatch = (capbuf->format() == destfmt);
  bool sizmatch = (capbuf->width() == w);
  sizmatch &= (capbuf->height() == h);

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(destfmt, w, h);

  auto vkimg     = rtbi->_imgobj->_vkimage;
  auto vkfmt     = rtbi->_vkfmt;
  auto imgobj    = rtbi->_imgobj;
  auto vkimgview = imgobj->_vkimageview;

  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0; // 0 means tightly packed
  region.bufferImageHeight = 0; // 0 means tightly packed
  region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageOffset       = {int32_t(x), int32_t(y), 0}; // Specify where to copy from in the source image
  region.imageExtent       = {uint32_t(w), uint32_t(h), 1};

  // GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      break;
    }
    case EBufferFormat::RGBA8: {
      // Handle both 8-bit and 32-bit float formats
      bool is_float_format = (vkfmt == VK_FORMAT_R32G32B32A32_SFLOAT);
      bool is_8bit_format  = (vkfmt == VK_FORMAT_R8G8B8A8_UNORM || vkfmt == VK_FORMAT_B8G8R8A8_UNORM);

      OrkAssert(is_float_format || is_8bit_format);

      size_t staging_bufsize = is_float_format ? (w * h * 16) : (w * h * 4); // 16 bytes per pixel for RGBA32F

      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);

      // Create staging buffer for GPU to CPU transfer
      auto staging_buffer =
          std::make_shared<VulkanBuffer>(_contextVK, staging_bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging");

      // Copy image to staging buffer (image is already in TRANSFER_SRC_OPTIMAL from transition)
      vkCmdCopyImageToBuffer(cb->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      // Transition back to render target for continued rendering
      rtbi->_transitionToRenderTarget(cb);

      // Store staging buffer with metadata about conversion requirements
      auto capbuf_impl             = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer  = staging_buffer;
      capbuf_impl->_actual_format  = vkfmt;
      capbuf_impl->_desired_format = destfmt;

      auto async_impl            = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width          = w;
      async_impl->height         = h;
      async_impl->format         = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true; // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted

      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer       = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;

      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);

      // printf("VkFrameBufferInterface::captureAsFormat - copy command recorded, data will be available after frame submit\n");
      break;
    }
    case EBufferFormat::RGB8: {
      //////////////////////////////////////
      // RGB8 not implemented for Vulkan yet
      //////////////////////////////////////
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      // TODO: Implement RGB8 capture for Vulkan
      OrkAssert(false);
      //////////////////////////////////////
      break;
    }
    case EBufferFormat::RGBA16F: {
      OrkAssert(vkfmt == VK_FORMAT_R16G16B16A16_SFLOAT);
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);

      // Create staging buffer for GPU to CPU transfer
      size_t bufsize = w * h * 8; // 8 bytes per pixel for RGBA16F (2 bytes per channel)
      auto staging_buffer =
          std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging_f16");

      // Copy image to staging buffer
      vkCmdCopyImageToBuffer(cb->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      // Transition back to render target
      rtbi->_transitionToRenderTarget(cb);

      // Store staging buffer with metadata (use same struct for consistency)
      auto capbuf_impl             = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer  = staging_buffer;
      capbuf_impl->_actual_format  = vkfmt;
      capbuf_impl->_desired_format = destfmt;

      // Store capture data in the future
      auto async_impl            = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width          = w;
      async_impl->height         = h;
      async_impl->format         = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true; // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted

      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer       = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;

      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);

      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA32F: {
      OrkAssert(vkfmt == VK_FORMAT_R32G32B32A32_SFLOAT);
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);

      // Create staging buffer for GPU to CPU transfer
      size_t bufsize = w * h * 16; // 16 bytes per pixel for RGBA32F
      auto staging_buffer =
          std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging_f32");

      // Copy image to staging buffer
      vkCmdCopyImageToBuffer(cb->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      // Transition back to render target
      rtbi->_transitionToRenderTarget(cb);

      // Store staging buffer with metadata (use same struct for consistency)
      auto capbuf_impl             = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer  = staging_buffer;
      capbuf_impl->_actual_format  = vkfmt;
      capbuf_impl->_desired_format = destfmt;

      // Store capture data in the future
      auto async_impl            = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width          = w;
      async_impl->height         = h;
      async_impl->format         = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true; // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted

      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer       = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;

      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);
      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::R32F:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32UI:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RED_INTEGER, GL_UNSIGNED_INT, capbuf->_data);
      break;
    case EBufferFormat::RG32F:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RG, GL_FLOAT, capbuf->_data);
      break;
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA16UI: {
      OrkAssert(vkfmt == VK_FORMAT_R16G16B16A16_UINT);
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);

      // Create staging buffer for GPU to CPU transfer
      size_t bufsize = w * h * 8; // 8 bytes per pixel for RGBA16UI (2 bytes per channel)
      auto staging_buffer =
          std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging_ui16");

      // Copy image to staging buffer
      vkCmdCopyImageToBuffer(cb->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      // Transition back to render target
      rtbi->_transitionToRenderTarget(cb);

      // Store staging buffer with metadata (use same struct for consistency)
      auto capbuf_impl             = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer  = staging_buffer;
      capbuf_impl->_actual_format  = vkfmt;
      capbuf_impl->_desired_format = destfmt;

      // Store capture data in the future
      auto async_impl            = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width          = w;
      async_impl->height         = h;
      async_impl->format         = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true; // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted

      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer       = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;

      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);
      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA32UI: {
      OrkAssert(vkfmt == VK_FORMAT_R32G32B32A32_UINT);
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);

      // Create staging buffer for GPU to CPU transfer
      size_t bufsize = w * h * 16; // 16 bytes per pixel for RGBA32UI (4 bytes per channel)
      auto staging_buffer =
          std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging_ui32");

      // Copy image to staging buffer
      vkCmdCopyImageToBuffer(cb->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      // Transition back to render target
      rtbi->_transitionToRenderTarget(cb);

      // Store staging buffer with metadata (use same struct for consistency)
      auto capbuf_impl             = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer  = staging_buffer;
      capbuf_impl->_actual_format  = vkfmt;
      capbuf_impl->_desired_format = destfmt;

      // Store capture data in the future
      auto async_impl            = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width          = w;
      async_impl->height         = h;
      async_impl->format         = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true; // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted

      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer       = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;

      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);
      break;
    }
    ///////////////////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
  }
  // GL_ERRORCHECK();

  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  //   glReadBuffer( readbuffer ); // restore read buffer
  // GL_ERRORCHECK();
  return future;
}

////////////////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::capturePixelAsync(
    pixelfetchctx_ptr_t pfc,
    int x, 
    int y, 
    void_lambda_t on_capture_complete) {
  
  // For now, capture from the first buffer (color buffer)
  // TODO: Support capturing from multiple buffers for deep pixels
  auto rtg = pfc->_rtgroup;
  auto rtb = rtg->buffer(0);
  if (!rtb) {
    auto future = std::make_shared<CaptureAsync>();
    future->_failed = true;
    return future;
  }
  
  // Create a capture buffer for the single pixel
  auto capbuf = std::make_shared<CaptureBuffer>();
  capbuf->_captureX = x;
  capbuf->_captureY = y;
  capbuf->_captureW = 1;
  capbuf->_captureH = 1;
  
  // Use captureAsFormat to do the actual capture of the 1x1 region
  auto future = captureAsFormat(rtb.get(), capbuf, rtb->format(), on_capture_complete);
  if (!future || future->_failed) {
    return future;
  }
  
  // Attach PixelFetchContext for deep pixel support
  pfc->_rtgroup = rtg;
  future->_pixelFetchContext = pfc;
  
  // The pixel data will be available after frame submit
  // The PixelFetchContext will be populated when the data is retrieved
  
  return future;
}

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan