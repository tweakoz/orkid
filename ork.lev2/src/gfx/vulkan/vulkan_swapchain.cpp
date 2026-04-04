////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_captureasync.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static auto logchan_swapchain = logger()->configureChannel("VKSWAP", fvec3(0.5, 0.5, 0.5), true);

////////////////////////////////////////////////////////////////////////////////

bool VkSwapChainCaps::supportsPresentationMode(VkPresentModeKHR mode) const {
  auto it = _presentModes.find(mode);
  return (it != _presentModes.end());
}

////////////////////////////////////////////////////////////////////////////////

VkSwapChain::VkSwapChain(vkcontext_rawptr_t ctxVK)
    : _contextVK(ctxVK) {
  logchan_swapchain->log("new VkSwapChain");
  _buildup();
}

//////////////////////////////////////////////////////

VkSwapChain::~VkSwapChain() {
  logchan_swapchain->log("delete VkSwapChain");
  _teardown();
}

////////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_buildup() {
  auto& vkdev = _contextVK->_vkdevice;
  _contextVK->_vkpresentation_caps = _contextVK->_swapChainCapsForSurface(_contextVK->_vkpresentationsurface);
  
  auto pres_caps = _contextVK->_vkpresentation_caps;

  
  // NOTE: Synchronization objects are now cleared in _teardown() after proper waiting
  // Only create new ones if the arrays are empty (first time or after teardown)
  if (_frame_fences[0] == nullptr) {
    logchan_swapchain->log("_buildup: Creating synchronization objects");
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      _imageAcquiredSemaphores[i] = std::make_shared<VulkanBinarySemaphore>(_contextVK);
      _renderCompleteSemaphores[i] = std::make_shared<VulkanBinarySemaphore>(_contextVK);
      _frame_fences[i] = std::make_shared<VulkanFenceObject>(_contextVK);
    }
  } else {
    logchan_swapchain->log("_buildup: Reusing existing synchronization objects");
  }

  VkSurfaceFormatKHR surfaceFormat = pres_caps->_formats[0];
  for (const auto& format : pres_caps->_formats) {
    // Prefer BGRA8 non-SRGB if available
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT) {
      surfaceFormat = format;
      break;
    }
    /*// Prefer BGRA8 SRGB if available
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = format;
      break;
    }*/
  }

  VkSurfaceTransformFlagsKHR preTransform;
  if (pres_caps->_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    preTransform = pres_caps->_capabilities.currentTransform;
  }

  VkSwapchainCreateInfoKHR SCINFO{};
  initializeVkStruct(SCINFO, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  SCINFO.surface = _contextVK->_vkpresentationsurface;
  SCINFO.presentMode = VK_PRESENT_MODE_FIFO_KHR; // default, overridden below if MAILBOX available

  auto ctx_glfw = _contextVK->_impl.getShared<VkPlatformObject>()->_ctxbase;
  auto window   = ctx_glfw->_glfwWindow;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  logchan_swapchain->log("_buildup: glfwGetFramebufferSize returned %dx%d for window %p", width, height, window);

  auto& caps = pres_caps->_capabilities;
  logchan_swapchain->log("_buildup: surface caps currentExtent=%ux%u min=%ux%u max=%ux%u",
                         caps.currentExtent.width, caps.currentExtent.height,
                         caps.minImageExtent.width, caps.minImageExtent.height,
                         caps.maxImageExtent.width, caps.maxImageExtent.height);

  ////////////////////////////////////////////////////
  // Check if extent is defined by surface (required on some platforms)
  ////////////////////////////////////////////////////

  if (caps.currentExtent.width != 0xFFFFFFFF) {
    logchan_swapchain->log("_buildup: using surface currentExtent %ux%u instead of fb size", caps.currentExtent.width, caps.currentExtent.height);
    width  = caps.currentExtent.width;
    height = caps.currentExtent.height;
  }

  // Clamp to surface capabilities
  width  = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, uint32_t(width)));
  height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, uint32_t(height)));

  // Ensure we have valid dimensions
  if (width == 0 || height == 0) {
    // Window is minimized, use minimum valid size
    width  = std::max(1u, caps.minImageExtent.width);
    height = std::max(1u, caps.minImageExtent.height);
  }
  bool dimensions_changed = (_width != width) || (_height != height);
  _width                  = width;
  _height                 = height;
  
  if (dimensions_changed) {

    if(0)printf("Swap chain dimensions: requested=%dx%d, clamped=%ux%u\n", width, height, uint32_t(width), uint32_t(height));
    if(0)printf(
        "Surface caps: min=%ux%u, max=%ux%u, current=%ux%u\n",
        caps.minImageExtent.width,
        caps.minImageExtent.height,
        caps.maxImageExtent.width,
        caps.maxImageExtent.height,
        caps.currentExtent.width,
        caps.currentExtent.height);
  }

  // image properties
  // Ensure minImageCount is within capabilities
  u32 minImageCount = std::max((u32)MAX_FRAMES_IN_FLIGHT, (u32)caps.minImageCount);
  OrkAssertI(caps.maxImageCount == 0 || minImageCount <= caps.maxImageCount, "minImageCount exceeds caps.maxImageCount");
  SCINFO.minImageCount    = minImageCount;
  SCINFO.imageFormat      = surfaceFormat.format;                // Chosen from VkSurfaceFormatKHR, after querying supported formats
  SCINFO.imageColorSpace  = surfaceFormat.colorSpace;            // Chosen from VkSurfaceFormatKHR
  SCINFO.imageExtent      = {uint32_t(width), uint32_t(height)}; // The width and height of the swap chain images
  SCINFO.imageArrayLayers = 1;                                   // Always 1 unless developing a stereoscopic 3D application

  // Only use supported image usage flags
  SCINFO.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    SCINFO.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    SCINFO.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  if(0)printf("Supported usage flags: 0x%x, requesting: 0x%x\n", caps.supportedUsageFlags, SCINFO.imageUsage);

  // Ensure we're not requesting unsupported usage
  SCINFO.imageUsage &= caps.supportedUsageFlags;

  SCINFO.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;

  ///////////////////////////////////////////////////
  // image view properties
  ///////////////////////////////////////////////////

  SCINFO.imageSharingMode =
      VK_SHARING_MODE_EXCLUSIVE;          // Can be VK_SHARING_MODE_CONCURRENT if sharing between multiple queue families
  SCINFO.queueFamilyIndexCount = 0;       // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT
  SCINFO.pQueueFamilyIndices   = nullptr; // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT

  ///////////////////////////////////////////////////
  // Choose a supported composite alpha mode
  ///////////////////////////////////////////////////

  SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
    // Find first supported composite alpha
    if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
      SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
      SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
      SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }
  }

  ///////////////////////////////////////////////////
  // declare old swapchain
  //  this allows us to reuse resources from the previous swapchain (if applicable)
  ///////////////////////////////////////////////////

  SCINFO.clipped      = VK_TRUE;
  SCINFO.oldSwapchain = _vkSwapChain; // Use previous swapchain if available

  ///////////////////////////////////////////////////
  // Choose a supported present mode
  ///////////////////////////////////////////////////

  SCINFO.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always supported (fallback)
  for (const auto& mode : pres_caps->_presentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      SCINFO.presentMode = mode;
      break;
    }
  }
  const char* mode_name = "UNKNOWN";
  switch(SCINFO.presentMode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR: mode_name = "IMMEDIATE"; break;
    case VK_PRESENT_MODE_MAILBOX_KHR: mode_name = "MAILBOX"; break;
    case VK_PRESENT_MODE_FIFO_KHR: mode_name = "FIFO"; break;
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: mode_name = "FIFO_RELAXED"; break;
    default: break;
  }
  logchan_swapchain->log("_buildup: selected present mode: %s", mode_name);
  SCINFO.clipped = VK_TRUE;

  ///////////////////////////////////////////////////
  // create new swapchain impl
  ///////////////////////////////////////////////////

  logchan_swapchain->log("_buildup: creating swapchain with extent %ux%u surface=%p",
                         SCINFO.imageExtent.width, SCINFO.imageExtent.height,
                         (void*)SCINFO.surface);
  VkResult OK = vkCreateSwapchainKHR(vkdev, &SCINFO, nullptr, &_vkSwapChain);
  logchan_swapchain->log("_buildup: vkCreateSwapchainKHR returned %d swapchain=%p", OK, (void*)_vkSwapChain);
  OrkAssert(OK == VK_SUCCESS);

  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(vkdev, _vkSwapChain, &imageCount, nullptr);
  std::vector<VkImage> swapChainImages;
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vkdev, _vkSwapChain, &imageCount, swapChainImages.data());

  if (imageCount != minImageCount)
    logchan_swapchain->log("WARNING: got extra swapchain images (requested %d, got %d)", minImageCount, imageCount);

  ///////////////////////////////////////////////////
  // register new swapchain images / image views
  ///////////////////////////////////////////////////

  for (size_t i = 0; i < swapChainImages.size(); i++) {

    _contextVK->_setObjectDebugName(swapChainImages[i], VK_OBJECT_TYPE_IMAGE, FormatString("swapchain-image-%d", i).c_str());

    auto IVCI = createImageViewInfo2D(
        swapChainImages[i],   //
        surfaceFormat.format, //
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageView imgview;
    OK = vkCreateImageView(vkdev, IVCI.get(), nullptr, &imgview);
    OrkAssert(OK == VK_SUCCESS);

    ////////////////////////////////////////////
    auto imgobj               = std::make_shared<VulkanImageObject>(_contextVK, swapChainImages[i], imgview, surfaceFormat.format);
    imgobj->_delete_image     = false; // Don't delete the image, it's managed by the swapchain
    imgobj->_delete_imageview = false; // Delete the image view, it's managed by the swapchain
    _swapChainImages.push_back(imgobj);
  }

  ///////////////////////////////////////////////////
  // Create/Update render target group impl for swapchain
  ///////////////////////////////////////////////////

  VkRtbCreateOption depth_option;
  depth_option._format       = VK_FORMAT_D32_SFLOAT; // Use D32_SFLOAT for depth buffer
  depth_option._usage        = "depth"_crcu; // Use "depth" usage
  depth_option._with_texture = false; // No texture for depth buffer

  auto rtg = _contextVK->_fbi->_ensureMainRtg();

  vkrtgrpimpl_ptr_t rtg_impl;
  if (auto existing = rtg->_impl.tryAsShared<VkRtGroupImpl>()) {
    rtg_impl = existing.value();


    // In VkSwapChain::_buildup(), after checking dimensions_changed
    if (dimensions_changed) {
      logchan_swapchain->log("Dimensions changed from %dx%d to %dx%d", rtg_impl->_width, rtg_impl->_height, width, height);

      // Update dimensions
      rtg_impl->_width  = width;
      rtg_impl->_height = height;
      rtg->miW          = width;
      rtg->miH          = height;

      // update abstract depthbuffer dimensions
      if (rtg->_depthBuffer) {
        rtg->_depthBuffer->_width  = width;
        rtg->_depthBuffer->_height = height;
      }

      // Recreate depth buffer with new dimensions
      if (rtg_impl->_depth_buffer_impl) {
        logchan_swapchain->log("Recreating depth buffer with dimensions %dx%d", width, height);
        _vkCreateImageForBuffer(_contextVK, rtg_impl->_depth_buffer_impl, depth_option);
        logchan_swapchain->log(
            "Depth buffer image: %p, view: %p",
            rtg_impl->_depth_buffer_impl->_imgobj->_vkimage,
            rtg_impl->_depth_buffer_impl->_imgobj->_vkimageview);
        // No explicit transition here; handled in framebuffer interface
      }

      // Invalidate attachments cache
      rtg_impl->_invalidateAttachments();
    }
  } else {
    // First time creation
    rtg_impl          = _contextVK->_fbi->_createRtGroupImpl(rtg.get());
    rtg_impl->_width  = width;
    rtg_impl->_height = height;
    VkRtGroupImpl::assignToRtGroup(rtg_impl, rtg.get());
    _vkCreateImageForBuffer(_contextVK, rtg_impl->_depth_buffer_impl, depth_option);
    // No explicit transition here; handled in framebuffer interface
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_teardown() {
    if(nullptr==_contextVK->_vkdevice){
        return;
    }

  // Wait ONLY for in-flight fences - do NOT reset them!
  // Let waitPresentFrame() handle fence resets before the next submit
  logchan_swapchain->log("_teardown: Waiting for in-flight fences");

  // Collect fences that are in-flight (NOT_READY = submitted but not signaled yet)
  std::vector<VkFence> inflight_fences;
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    auto& fence = _frame_fences[i];
    if (fence) {
      VkResult status = vkGetFenceStatus(_contextVK->_vkdevice, fence->_vkfence);
      if (status == VK_SUCCESS) {
        // Already signaled - GPU work done, safe to destroy
        logchan_swapchain->log("  Fence %zu already signaled (GPU idle)", i);
        // DO NOT RESET - leave it signaled for next frame to handle
      } else if (status == VK_NOT_READY) {
        // In-flight - GPU still working on it
        logchan_swapchain->log("  Fence %zu in-flight, will wait", i);
        inflight_fences.push_back(fence->_vkfence);
      } else {
        logchan_swapchain->log("  WARNING: Fence %zu in error state: %d", i, status);
      }
    }
  }

  // Wait for in-flight fences to complete (GPU work using old swapchain images)
  // At 60 FPS with 2 frames in flight, should take ~33ms max
  if (!inflight_fences.empty()) {
    logchan_swapchain->log("_teardown: Waiting for %zu in-flight fences", inflight_fences.size());
    // 50ms timeout - plenty for 2 frames at 60fps (33ms)
    VkResult wait_result = vkWaitForFences(
        _contextVK->_vkdevice,
        inflight_fences.size(),
        inflight_fences.data(),
        VK_TRUE,  // Wait for all
        50000000  // 50ms in nanoseconds
    );

    if (wait_result == VK_SUCCESS) {
      logchan_swapchain->log("_teardown: All in-flight fences signaled, GPU work complete");
      // DO NOT RESET - fences are now signaled, leave them for next frame
    } else if (wait_result == VK_TIMEOUT) {
      logchan_swapchain->log("_teardown: WARNING - Fence wait timed out after 50ms, forcing device wait");
      // Fences still in-flight after 50ms - must ensure GPU idle before destroying resources
      vkDeviceWaitIdle(_contextVK->_vkdevice);
      logchan_swapchain->log("_teardown: Device idle after timeout");
    } else {
      logchan_swapchain->log("_teardown: ERROR - fence wait returned error: %d, forcing device wait", wait_result);
      vkDeviceWaitIdle(_contextVK->_vkdevice);
    }
  }

  // NO fence resets - let waitPresentFrame() handle that before next submit
  // The fences guaranteed swapchain images aren't in use

  if (_vkSwapChain != VK_NULL_HANDLE) {

    // Clean up image views
    for (auto& imgobj : _swapChainImages) {
      if (imgobj && imgobj->_vkimageview != VK_NULL_HANDLE) {
        vkDestroyImageView(_contextVK->_vkdevice, imgobj->_vkimageview, nullptr);
      }
    }
    _swapChainImages.clear();

    vkDestroySwapchainKHR(_contextVK->_vkdevice, _vkSwapChain, nullptr);
  }
  _vkSwapChain = VK_NULL_HANDLE;

  // Explicitly clear synchronization objects AFTER everything is idle
  // The shared_ptr destructors will properly destroy the Vulkan objects
  logchan_swapchain->log("_teardown: Clearing synchronization objects");
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frame_fences[i] = nullptr;
    _imageAcquiredSemaphores[i] = nullptr;
    _renderCompleteSemaphores[i] = nullptr;
  }
  logchan_swapchain->log("_teardown: Complete");
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_reinit() {
  _teardown();
  _buildup();
  _current_frame      = 0;          // Reset frame index after reinitialization
  _curSwapWriteImage = 0xffffffff; // Reset current swap image index
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_acquireImage(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_GPU, "gpu_acquireImage");
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:acquireImage");

  // Ensure we have a valid swapchain
  size_t sub_index = _sub_index;
  
  // DEBUG: Log current frame state
  if(0)logchan_swapchain->log("acquireImage: frame %zu, sub_index %zu, curSwapWriteImage %u", 
                         _current_frame, sub_index, _curSwapWriteImage);

  // Waiting on frame fence at this location is unnecessary.
  // vkAcquireNextImageKHR will handle the synchronization

  ///////////////////////////////////////////////////
  // Get SwapChain Image
  ///////////////////////////////////////////////////

  // It is necessary to continually retry to prevent a window drag
  // resize ending up without a swapchain and potentially crashing.
  // This is a heavy way to do deal with. If we want smooth resizing 
  // of windows there is a better solution. 
  bool got_swapchain_image = false;
  while (not got_swapchain_image) {

    // DEBUG: Log semaphore state before acquire
    auto semaphore = _imageAcquiredSemaphores[sub_index]->_vksema;
    if(0)logchan_swapchain->log("acquireImage: attempting vkAcquireNextImageKHR with semaphore %p (sub_index %zu)", 
                          (void*)semaphore, sub_index);
    
    VkResult status = vkAcquireNextImageKHR(
      ctxVK->_vkdevice,
      _vkSwapChain,
      std::numeric_limits<uint64_t>::max(),
      semaphore, // Use current frame's semaphore
      VK_NULL_HANDLE,
      &_curSwapWriteImage);

    // DEBUG: Log acquire result
    if(0)logchan_swapchain->log("acquireImage: vkAcquireNextImageKHR returned %d, image index %u", 
                          status, _curSwapWriteImage);

    switch (status) {
      case VK_SUCCESS:
        if(0)logchan_swapchain->log("acquireImage: SUCCESS - acquired image %u", _curSwapWriteImage);
        got_swapchain_image = true;
        break;
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR: {
        logchan_swapchain->log("acquireImage: SWAPCHAIN OUT OF DATE - status %s", string_VkResult(status));
        ctxVK->_gfxqueue->queueWaitIdle();
        _reinit();
        break;
      }
      case VK_ERROR_DEVICE_LOST:
      default:
        logchan_swapchain->error("waacquireImageit: UNEXPECTED STATUS %s", string_VkResult(status));
        OrkAssert(false);
        break;
    }
  }
  OrkAssert(_curSwapWriteImage >= 0);

  if(0)logchan_swapchain->log("acquireImage: COMPLETE - image %u ready for rendering", _curSwapWriteImage);
}


///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_enqueueFrame(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:enqueueFrame");
  
  _allWaitSemaphores.clear();
  _allWaitValues.clear();
  _allSignalSemaphores.clear();
  _allSignalValues.clear();
  _allWaitStages.clear();

  // Collect all semaphores and their signal values
  
  size_t sub_index = _sub_index;
  auto imageAcquiredSem = _imageAcquiredSemaphores[sub_index];
  _allWaitSemaphores.push_back(imageAcquiredSem->_vksema);
  _allWaitValues.push_back(0);  // Binary semaphore, value 0

  // Add one-shot timeline semaphores (texture uploads, etc.)
  _allSignalSemaphores.insert(_allSignalSemaphores.end(), ctxVK->_oneShotSignalSemaphores.begin(), ctxVK->_oneShotSignalSemaphores.end());
  _allSignalValues.insert(_allSignalValues.end(), ctxVK->_oneShotSignalValues.begin(), ctxVK->_oneShotSignalValues.end());


  // Add binary semaphore (render complete) with value 0
  auto& renderCompleteSem = _renderCompleteSemaphores[sub_index];
  _allSignalSemaphores.push_back(renderCompleteSem->_vksema);
  _allSignalValues.push_back(0);  // Binary semaphores use value 0
  
  // Timeline info must match ALL semaphores
  VkTimelineSemaphoreSubmitInfo timelineInfo{};
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timelineInfo.signalSemaphoreValueCount = _allSignalValues.size();  // Must match signalSemaphoreCount
  timelineInfo.pSignalSemaphoreValues = _allSignalValues.data();
  
  // Wait semaphore (binary) also needs a value
  timelineInfo.waitSemaphoreValueCount = _allWaitValues.size();
  timelineInfo.pWaitSemaphoreValues = _allWaitValues.data();
  
  auto CB = ctxVK->primary_cb();
  if(0)logchan_swapchain->log("SUBMIT priCB<%p> vkimpl<%p> with %zu wait semaphores and %zu signalsemas",
         (void*)CB.get(),
         (void*)CB->_vkcmdbuf,
         _allWaitSemaphores.size(),
         _allSignalSemaphores.size());
  // Submit info
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pNext = &timelineInfo;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &(CB->_vkcmdbuf);
  submitInfo.signalSemaphoreCount = _allSignalSemaphores.size();
  submitInfo.pSignalSemaphores = _allSignalSemaphores.data();
  

  // Wait for image acquisition
  for(int i=0; i < _allWaitSemaphores.size(); i++) {
    _allWaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  }

  submitInfo.waitSemaphoreCount = _allWaitSemaphores.size();
  submitInfo.pWaitSemaphores = _allWaitSemaphores.data();  
  submitInfo.pWaitDstStageMask = _allWaitStages.data();
  
  auto fence = _frame_fences[sub_index];
  fence->reset();
  ctxVK->_gfxqueue->queueSubmit(&submitInfo, fence->_vkfence);

  if(0)logchan_swapchain->log("vkQueueSubmit: CB %p", (void*)ctxVK->primary_cb()->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_enqueuePresentFrame(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:enqueuePresentFrame");

  size_t sub_index = _sub_index;
  
  // DEBUG: Log present operation
  if(0)logchan_swapchain->log("enqueuePresentFrame: frame %zu, sub_index %zu, image %u", 
                         _current_frame, sub_index, _curSwapWriteImage);

  _semaOkToPresent = _renderCompleteSemaphores[sub_index]->_vksema;

  VkPresentInfoKHR PRESI{};
  initializeVkStruct(PRESI, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  PRESI.waitSemaphoreCount = 1;
  PRESI.pWaitSemaphores    = &_semaOkToPresent;
  PRESI.swapchainCount     = 1;
  PRESI.pSwapchains        = &_vkSwapChain;
  PRESI.pImageIndices      = &_curSwapWriteImage;

  if(0)logchan_swapchain->log("enqueuePresentFrame: calling vkQueuePresentKHR for image %u", _curSwapWriteImage);
  VkResult status = ctxVK->_gfxqueue->queuePresent(&PRESI);
  if(0)logchan_swapchain->log("enqueuePresentFrame: vkQueuePresentKHR returned %d", status);
  
  // printf("vkQueuePresentKHR returned status: %d (0x%x)\n", status, status);

  switch (status) {
    case VK_SUCCESS:
      if(0)logchan_swapchain->log("enqueuePresentFrame: SUCCESS - frame %zu presented", _current_frame);
      break;
    case VK_SUBOPTIMAL_KHR: {
      logchan_swapchain->log("enqueuePresentFrame: VK_SUBOPTIMAL_KHR - Swap chain is suboptimal");
      printf("VK_SUBOPTIMAL_KHR: Swap chain is suboptimal\n");
      // Swap chain is still usable, but may not be optimal
      // Consider recreating it on next frame
      break;
    }
    case VK_ERROR_OUT_OF_DATE_KHR: {
      logchan_swapchain->log("enqueuePresentFrame: VK_ERROR_OUT_OF_DATE_KHR - Swap chain needs recreation");
      printf("VK_ERROR_OUT_OF_DATE_KHR: Swap chain needs recreation\n");
      _contextVK->_gfxqueue->queueWaitIdle();
      _reinit();
      break;
    }
    case VK_ERROR_DEVICE_LOST: {
      logchan_swapchain->log("enqueuePresentFrame: VK_ERROR_DEVICE_LOST - Device has been lost!");
      printf("VK_ERROR_DEVICE_LOST: Device has been lost!\n");
      // This is a fatal error - the device is no longer usable
      // All Vulkan objects are now invalid
      // The application needs to recreate everything from scratch
      OrkAssert(false);
      break;
    }
    case VK_ERROR_SURFACE_LOST_KHR: {
      logchan_swapchain->log("enqueuePresentFrame: VK_ERROR_SURFACE_LOST_KHR - Surface was lost");
      printf("VK_ERROR_SURFACE_LOST_KHR: Surface was lost\n");
      // Surface is no longer available, need to recreate
      // This is fatal for now
      OrkAssert(false);
      break;
    }
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: {
      logchan_swapchain->log("enqueuePresentFrame: VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT");
      printf("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT\n");
      // Can try to reacquire exclusive full-screen access
      //  need to recreate swap chain
      break;
    }
    default:
      logchan_swapchain->log("enqueuePresentFrame: UNEXPECTED STATUS %d", status);
      OrkAssert(false);
      break;
  } // switch (status)
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_waitFrame() {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:waitFrame");
  size_t sub_index = _sub_index;
  if(0)logchan_swapchain->log("waitPresentFrame: frame %zu, sub_index %zu", _current_frame, sub_index);
  auto& fence = _frame_fences[sub_index];
  fence->wait();
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// VkSwapChain - VkFramebufferOutput implementation (GLFW / Vulkan-surface swapchain)
///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::beginFrame(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:swapchainBeginFrame");
  OrkAssertI(!_acquired, "beginFrame called twice without a submit in between");
  _acquireImage(ctxVK);

  // Inject the acquired swapchain image directly into the main RTG color buffer.
  // _replaceImage resets the layout to UNDEFINED, so the subsequent _transitionToRenderTarget
  // in _pushRtGroup correctly barriers UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL.
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_is_surface = true;
  main_rtbi->_replaceImage(_swapChainImages[_curSwapWriteImage]);
  _acquired = true;
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::endFrame(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:swapchainEndFrame");
  // Rendering went directly into the swapchain image (injected via beginFrame).
  // Just transition COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR.
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_transitionToPresent(ctxVK->primary_cb());
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::submit(vkcontext_rawptr_t ctxVK) {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:swapchainSubmit");
  _enqueueFrame(ctxVK);
  _enqueuePresentFrame(ctxVK);
  _waitFrame();
  _incrementFrame();
  _acquired = false;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
