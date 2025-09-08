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
  
  // Store capture data in the future's implementation
  struct VulkanCaptureData {
    capturebuffer_ptr_t capture_buffer;
    texture_ptr_t capture_texture;
    file::Path path;
    int width;
    int height;
    EBufferFormat format;
    bool frame_submitted = false;
  };
  
  auto capture_data = std::make_shared<VulkanCaptureData>();
  capture_data->capture_buffer = capbuf;
  capture_data->path = pth;
  capture_data->width = inpbuf->_width;
  capture_data->height = inpbuf->_height;
  capture_data->format = EBufferFormat::RGBA8;
  
  future->_impl.setShared<VulkanCaptureData>(capture_data);
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
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      // TODO: Implement RGBA16F capture for Vulkan
      OrkAssert(false);
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

} // namespace ork::lev2::vulkan