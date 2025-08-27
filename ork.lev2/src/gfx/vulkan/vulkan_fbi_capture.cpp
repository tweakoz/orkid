////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_captureasync.h"
#include "headers/vulkan_ctx.h"
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
    EBufferFormat source_format = async_impl->format;
    bool needs_conversion = (source_format != capbuf_impl->_desired_format);
    
    if (needs_conversion) {
      // Copy to temp image for conversion
      // if same capturebuffer is reused for same RtBuffer, this avoids reallocation
      
      auto temp_img = async_impl->capture_buffer->_raw_image;
      temp_img->initWithFormat(async_impl->width, async_impl->height, source_format);
      size_t bufsize = async_impl->capture_buffer->length();
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
      size_t bufsize = async_impl->capture_buffer->length();
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

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan