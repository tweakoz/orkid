////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vk_gpu_surface.h"  // IoSurfaceTexImpl and GpuExternalSurface
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/kernel/svariant.h>

#if defined(__APPLE__)
#import <IOSurface/IOSurface.h>
#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#include <vulkan/vulkan_metal.h>  // For VK_EXT_metal_objects structures
#endif

namespace ork::lev2 {

namespace vulkan {

///////////////////////////////////////////////////////////////////////////////
// DOUBLE-BUFFER PING-PONG: No VulkanExternalTextureImpl needed!
// VideoToolboxBackend manages ping-pong buffers, Vulkan just imports from stable slot
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GPU-Direct External Memory Import (IOSurface on macOS, DMA-BUF on Linux)
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromGpuExternalSurface(Texture* ptex) {

#if defined(__APPLE__)
  ///////////////////////////////////////////////////////////////////////
  // macOS: Import IOSurface as VkImage via VK_EXT_metal_objects
  ///////////////////////////////////////////////////////////////////////

  auto vkctx = (vulkan::VkContext*)_ctx;

  // DOUBLE-BUFFER PING-PONG: Get frame from VideoToolbox backend
  // We need to get the backend instance - stored in texture's movie context
  // For now, use a simple approach: check _impl_2 for IoSurfaceTexImpl directly

  auto frame_impl_opt = ptex->_impl_2.tryAsShared<IoSurfaceTexImpl>();
  if (!frame_impl_opt) {
    printf("ERROR: initTextureFromGpuExternalSurface called but no IoSurfaceTexImpl available\n");
    return;
  }

  auto frame_impl = frame_impl_opt.value();
  if (!frame_impl) {
    printf("ERROR: IoSurfaceTexImpl is null\n");
    return;
  }

  if (!frame_impl->surface) {
    printf("ERROR: IoSurfaceTexImpl has null surface\n");
    return;
  }

  auto handle_opt = frame_impl->surface->_impl.tryAs<NativeSurfaceHandle>();
  if (!handle_opt || !handle_opt.value().handle) {
    printf("ERROR: IoSurfaceTexImpl has null IOSurface\n");
    return;
  }

  IOSurfaceRef iosurface = (IOSurfaceRef)handle_opt.value().handle;
  OSType pixel_format = frame_impl->pixel_format();
  size_t width = frame_impl->width();
  size_t height = frame_impl->height();

  // Determine Vulkan format based on IOSurface pixel format
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
  switch (pixel_format) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: // NV12
      vk_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
      break;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange: // P010
      vk_format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
      break;
    case kCVPixelFormatType_32BGRA: // BGRA8
      vk_format = VK_FORMAT_B8G8R8A8_UNORM;
      break;
    default:
      printf("ERROR: Unsupported IOSurface pixel format: 0x%x\n", pixel_format);
      return;
  }

  ///////////////////////////////////////////////////////////////////////
  // Import IOSurface directly as VkImage (VK_EXT_metal_objects)
  // 1:1 Mapping: Each IoSurfaceTexImpl owns its VkImage (created once, reused)
  ///////////////////////////////////////////////////////////////////////

  @autoreleasepool {
    // Get Vulkan context
    auto vkctx = (vulkan::VkContext*)_ctx;

    vkimageobj_ptr_t vkimgobj_to_use;

    // Check if VkImage already exists in frame_impl (1:1 mapping)
    if (frame_impl->vkimage) {
      // REUSE: VkImage already created for this IOSurface
      vkimgobj_to_use = std::static_pointer_cast<VulkanImageObject>(frame_impl->vkimage);

    } else {
      // CREATE: First time seeing this IOSurface, create VkImage

      ///////////////////////////////////////////////////////////////////////
      // Step 1: Create VkImage with IOSurface import (let MoltenVK handle memory)
      ///////////////////////////////////////////////////////////////////////

      // Create IOSurface import info
      VkImportMetalIOSurfaceInfoEXT importInfo{};
      importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT;
      importInfo.pNext = nullptr;
      importInfo.ioSurface = iosurface;

      VkImageCreateInfo imageInfo{};
      imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.pNext = &importInfo;  // Import IOSurface
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      imageInfo.format = vk_format;
      imageInfo.extent.width = width;
      imageInfo.extent.height = height;
      imageInfo.extent.depth = 1;
      imageInfo.mipLevels = 1;
      imageInfo.arrayLayers = 1;
      imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      VkImage vkImage = VK_NULL_HANDLE;
      VkResult result = vkCreateImage(vkctx->_vkdevice, &imageInfo, nullptr, &vkImage);

      if (result != VK_SUCCESS) {
        printf("ERROR: vkCreateImage with IOSurface import failed: %d\n", result);
        return;
      }

      ///////////////////////////////////////////////////////////////////////
      // Step 2: Allocate dedicated memory (bookkeeping only, no Metal allocation)
      //         This is REQUIRED in MoltenVK 1.4.1 even for IOSurface-backed images
      ///////////////////////////////////////////////////////////////////////

      VkMemoryRequirements memReq;
      vkGetImageMemoryRequirements(vkctx->_vkdevice, vkImage, &memReq);

      // Use dedicated allocation to avoid Metal heap/buffer creation (zero-copy path)
      VkMemoryDedicatedAllocateInfo dedicatedInfo{};
      dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
      dedicatedInfo.image = vkImage;
      dedicatedInfo.buffer = VK_NULL_HANDLE;

      VkMemoryAllocateInfo memAllocInfo{};
      memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memAllocInfo.pNext = &dedicatedInfo;  // CRITICAL for zero-copy
      memAllocInfo.allocationSize = memReq.size;
      memAllocInfo.memoryTypeIndex = vkctx->_findMemoryType(
          memReq.memoryTypeBits,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      );

      VkDeviceMemory deviceMemory;
      result = vkAllocateMemory(vkctx->_vkdevice, &memAllocInfo, nullptr, &deviceMemory);
      if (result != VK_SUCCESS) {
        printf("ERROR: vkAllocateMemory for IOSurface failed: %d\n", result);
        vkDestroyImage(vkctx->_vkdevice, vkImage, nullptr);
        return;
      }

      ///////////////////////////////////////////////////////////////////////
      // Step 3: Bind memory to image
      ///////////////////////////////////////////////////////////////////////

      result = vkBindImageMemory(vkctx->_vkdevice, vkImage, deviceMemory, 0);
      if (result != VK_SUCCESS) {
        printf("ERROR: vkBindImageMemory for IOSurface failed: %d\n", result);
        vkFreeMemory(vkctx->_vkdevice, deviceMemory, nullptr);
        vkDestroyImage(vkctx->_vkdevice, vkImage, nullptr);
        return;
      }

      ///////////////////////////////////////////////////////////////////////
      // Step 4: Create VkImageView
      ///////////////////////////////////////////////////////////////////////

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.pNext = nullptr;
      viewInfo.image = vkImage;
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = vk_format;
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      VkImageView vkImageView = VK_NULL_HANDLE;
      result = vkCreateImageView(vkctx->_vkdevice, &viewInfo, nullptr, &vkImageView);

      if (result != VK_SUCCESS) {
        printf("ERROR: vkCreateImageView failed: %d\n", result);
        vkFreeMemory(vkctx->_vkdevice, deviceMemory, nullptr);
        vkDestroyImage(vkctx->_vkdevice, vkImage, nullptr);
        return;
      }

      ///////////////////////////////////////////////////////////////////////
      // Step 5: Transition image layout (UNDEFINED → SHADER_READ_ONLY_OPTIMAL)
      ///////////////////////////////////////////////////////////////////////

      // Imported images start in UNDEFINED layout - must transition before use
      VkCommandBufferAllocateInfo cmdAllocInfo{};
      cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdAllocInfo.commandPool = vkctx->_vkcmdpool_graphics;
      cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmdAllocInfo.commandBufferCount = 1;

      VkCommandBuffer cmdBuffer;
      vkAllocateCommandBuffers(vkctx->_vkdevice, &cmdAllocInfo, &cmdBuffer);

      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(cmdBuffer, &beginInfo);

      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = vkImage;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
      barrier.srcAccessMask = 0;  // No previous access
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Will be sampled

      vkCmdPipelineBarrier(
          cmdBuffer,
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // Wait for nothing
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // Before fragment shader samples
          0,
          0, nullptr,
          0, nullptr,
          1, &barrier
      );

      vkEndCommandBuffer(cmdBuffer);

      // Submit layout transition (async - no wait, GPU will synchronize automatically)
      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &cmdBuffer;

      vkQueueSubmit(vkctx->_vkqueue_graphics, 1, &submitInfo, VK_NULL_HANDLE);

      // Wait for layout transition to complete before freeing command buffer
      // TODO: Replace with deferred cleanup to avoid blocking
      vkQueueWaitIdle(vkctx->_vkqueue_graphics);
      vkFreeCommandBuffers(vkctx->_vkdevice, vkctx->_vkcmdpool_graphics, 1, &cmdBuffer);

      ///////////////////////////////////////////////////////////////////////
      // Step 6: Create VulkanImageObject and store in frame_impl (1:1 mapping)
      ///////////////////////////////////////////////////////////////////////

      // Create VulkanImageObject with the imported VkImage and VkImageView
      auto vkimgobj = std::make_shared<VulkanImageObject>(vkctx, vkImage, vkImageView, vk_format);
      vkimgobj->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // Now actually in this layout
      vkimgobj->_delete_image = true;        // We own this image
      vkimgobj->_delete_imageview = true;    // We own this view
      vkimgobj->_vkdevicemem = deviceMemory; // Store device memory handle
      vkimgobj->_delete_devicemem = true;    // We own this memory

      // Note: VkDeviceMemory is a bookkeeping wrapper only (dedicated allocation)
      // MoltenVK creates the Metal texture directly from IOSurface (zero-copy)
      // The IoSurfaceTexImpl keeps the IOSurface alive

      // Store VkImage in frame_impl for reuse (1:1 mapping)
      frame_impl->vkimage = vkimgobj;
      vkimgobj_to_use = vkimgobj;
    }

    ///////////////////////////////////////////////////////////////////////
    // Get or create VulkanTextureObject
    ///////////////////////////////////////////////////////////////////////

    vktexobj_ptr_t vktex;

    if (auto existing_vktex = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
      // EXISTING TEXTURE
      vktex = existing_vktex.value();
    } else {
      // FIRST TIME: Create new VulkanTextureObject
      vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    }

    // Update sampling image to the VkImage for this frame
    vktex->_img_sampling = vkimgobj_to_use;

    // Update hash to invalidate cached descriptor sets when frame changes
    vktex->_imgview_hash.init();
    vktex->_imgview_hash.accumulateItem((uint64_t)vkimgobj_to_use->_vkimageview);
    vktex->_imgview_hash.accumulateItem((uint64_t)iosurface);  // Include IOSurface ref
    vktex->_imgview_hash.finish();
    vktex->_dataVersion++;  // Increment version to signal texture data changed

    // Update descriptor info with VkImageView (reuse descriptor from frame_impl if available)
    if (frame_impl->vkdescriptor) {
      // REUSE: Descriptor already exists for this frame
      vktex->_descset_sampling = std::static_pointer_cast<VkDescriptorImageInfo>(frame_impl->vkdescriptor);
    } else {
      // CREATE: First time, create descriptor and store in frame_impl
      if (!vktex->_vkdescriptor_info[0]) {
        vktex->_vkdescriptor_info[0] = std::make_shared<VkDescriptorImageInfo>();
      }
      vktex->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      vktex->_vkdescriptor_info[0]->imageView = vkimgobj_to_use->_vkimageview;

      // Create sampler on first use only (reuse for subsequent swaps)
      if (!vktex->_vksampler) {
        TextureSamplingModeData sampling_mode;
        sampling_mode._texFiltModeMin = ETextureMinifyFilterMode::LINEAR;
        sampling_mode._texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
        sampling_mode._texAddrModeS = TextureAddressMode::CLAMP;
        sampling_mode._texAddrModeT = TextureAddressMode::CLAMP;
        vktex->_vksampler = _contextVK->_getOrCreateSampler(sampling_mode);
      }
      vktex->_vkdescriptor_info[0]->sampler = vktex->_vksampler->_vksampler;

      // Store descriptor in frame_impl for reuse
      frame_impl->vkdescriptor = vktex->_vkdescriptor_info[0];
      vktex->_descset_sampling = vktex->_vkdescriptor_info[0];
    }

    // Set texture parameters
    ptex->_width = width;
    ptex->_height = height;
    ptex->_depth = 1;
    ptex->_num_mips = 1;

    // DOUBLE-BUFFER PING-PONG: No GPU sync needed!
    // Producer and consumer use different buffers, so no race conditions
  }

#elif defined(__linux__)
  ///////////////////////////////////////////////////////////////////////
  // Linux: Import DMA-BUF as VkImage via VK_EXT_external_memory_dma_buf
  ///////////////////////////////////////////////////////////////////////

  printf("VkTextureInterface: DMA-BUF import not yet implemented\n");

#else
  ///////////////////////////////////////////////////////////////////////
  // Unsupported platform
  ///////////////////////////////////////////////////////////////////////

  printf("ERROR: initTextureFromGpuExternalSurface not supported on this platform\n");

#endif
}

///////////////////////////////////////////////////////////////////////////////
// Check if external texture backing changed (platform-specific)
///////////////////////////////////////////////////////////////////////////////

bool VkTextureInterface::externalTextureChanged(const Texture* ptex) {
#if defined(__APPLE__)
  if (!ptex) {
    return true;
  }

  // DOUBLE-BUFFER PING-PONG: Check if we have a new frame
  // Just check if _impl_2 has a frame - VideoToolbox updates this with stable read buffer
  auto frame_opt = ptex->_impl_2.tryAsShared<IoSurfaceTexImpl>();
  if (!frame_opt) {
    return false;  // No frame data yet, don't try to import
  }

  auto frame = frame_opt.value();
  if (!frame || !frame->surface) {
    return false;  // No valid frame yet
  }
  auto handle_opt = frame->surface->_impl.tryAs<NativeSurfaceHandle>();
  if (!handle_opt || !handle_opt.value().handle) {
    return false;  // No valid IOSurface yet
  }

  // Check if already imported
  if (auto vktex_opt = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    auto vktex = vktex_opt.value();

    // Check if current sampling image matches current frame
    if (vktex->_img_sampling && frame->vkimage) {
      auto current_frame_vkimg = std::static_pointer_cast<VulkanImageObject>(frame->vkimage);
      if (current_frame_vkimg && vktex->_img_sampling->_vkimageview == current_frame_vkimg->_vkimageview) {
        // Already displaying this frame - no change
        return false;
      }
    }
  }

  return true;

#elif defined(__linux__)
  // TODO: Implement for VA-API/DMA-BUF
  return true;

#else
  return true;
#endif
}

///////////////////////////////////////////////////////////////////////////////

} // namespace vulkan
} // namespace ork::lev2
