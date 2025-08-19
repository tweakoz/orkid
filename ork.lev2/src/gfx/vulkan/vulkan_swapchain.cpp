////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static auto logchan_swapchain = logger()->configureChannel("VKSWAP", fvec3(0.5, 0.5, 0.5), false);

VkSwapChain::VkSwapChain(vkcontext_rawptr_t ctxVK)
    : _contextVK(ctxVK) {
  logchan_swapchain->log("new VkSwapChain");
  _buildup();
}

///////////////////////////////////////////////////////////////////////////////

VkSwapChain::~VkSwapChain() {
  logchan_swapchain->log("delete VkSwapChain");
  _teardown();
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_buildup() {
  auto& vkdev    = _contextVK->_vkdevice;
  auto& cmdbuf   = _contextVK->primary_cb()->_vkcmdbuf;
  
  _contextVK->_vkpresentation_caps = _contextVK->_swapChainCapsForSurface(_contextVK->_vkpresentationsurface);
  
  auto pres_caps = _contextVK->_vkpresentation_caps;

  
  _semasOkToRender.resize(1);
  _waitOnPipelineStages.resize(1);
  _semasOkToPresent.resize(1);
  _imageAcquiredSemaphores.clear();
  _renderCompleteSemaphores.clear();
  _frameFences.clear();
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

    auto bin_sema_imgacq = std::make_shared<VulkanBinarySemaphore>(_contextVK);
    auto bin_sema_rencom = std::make_shared<VulkanBinarySemaphore>(_contextVK);
    auto fence           = std::make_shared<VulkanFenceObject>(_contextVK);
    _imageAcquiredSemaphores.push_back(bin_sema_imgacq);
    _renderCompleteSemaphores.push_back(bin_sema_rencom);
    _frameFences.push_back(fence);
    fence->reset();
  }

  VkSurfaceFormatKHR surfaceFormat = pres_caps->_formats[0];
  for (const auto& format : pres_caps->_formats) {
    // Prefer BGRA8 SRGB if available
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = format;
      break;
    }
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
  // SCINFO.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // No vsync
  SCINFO.presentMode = VK_PRESENT_MODE_FIFO_KHR; // No vsync

  auto ctx_glfw = _contextVK->_impl.getShared<VkPlatformObject>()->_ctxbase;
  auto window   = ctx_glfw->_glfwWindow;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  auto& caps = pres_caps->_capabilities;

  ////////////////////////////////////////////////////
  // Check if extent is defined by surface (required on some platforms)
  ////////////////////////////////////////////////////

  if (caps.currentExtent.width != 0xFFFFFFFF) {
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

    printf("Swap chain dimensions: requested=%dx%d, clamped=%ux%u\n", width, height, uint32_t(width), uint32_t(height));
    printf(
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
  uint32_t minImageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && minImageCount > caps.maxImageCount) {
    minImageCount = caps.maxImageCount;
  }
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

  printf("Supported usage flags: 0x%x, requesting: 0x%x\n", caps.supportedUsageFlags, SCINFO.imageUsage);

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

  auto old_swapchain = _contextVK->_fbi->_swapchain;   
  SCINFO.clipped      = VK_TRUE; // clip pixels that are obscured by other windows
  SCINFO.oldSwapchain = old_swapchain ? old_swapchain->_vkSwapChain : VK_NULL_HANDLE; // Use previous swapchain if available

  ///////////////////////////////////////////////////
  // Choose a supported present mode
  ///////////////////////////////////////////////////

  SCINFO.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always supported
  for (const auto& mode : pres_caps->_presentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      // SCINFO.presentMode = mode;
      break;
    }
  }
  // SCINFO.presentMode    = VK_PRESENT_MODE_FIFO_KHR;
  SCINFO.clipped = VK_TRUE;

  ///////////////////////////////////////////////////
  // create new swapchain impl
  ///////////////////////////////////////////////////

  VkResult OK = vkCreateSwapchainKHR(vkdev, &SCINFO, nullptr, &_vkSwapChain);
  OrkAssert(OK == VK_SUCCESS);

  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(vkdev, _vkSwapChain, &imageCount, nullptr);
  std::vector<VkImage> swapChainImages;
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vkdev, _vkSwapChain, &imageCount, swapChainImages.data());

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

  auto rtg = _contextVK->_fbi->_main_rtg;

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
  vkDeviceWaitIdle(_contextVK->_vkdevice);
  vkQueueWaitIdle(_contextVK->_vkqueue_graphics);
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
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_reinit() {
  _teardown();
  _buildup();
  _currentFrame      = 0;          // Reset frame index after reinitialization
  _curSwapWriteImage = 0xffffffff; // Reset current swap image index
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::_update() {
  bool got_swapchain_image = false;
  while (not got_swapchain_image) {
    VkResult status     = acquireImage(_contextVK);
    got_swapchain_image = (status == VK_SUCCESS);
    if (not got_swapchain_image) {
      _reinit();
    }
  }
  // return _rtgs[_curSwapWriteImage];
}

///////////////////////////////////////////////////////////////////////////////

size_t VkSwapChain::subIndex() const {
  // Return the current frame index modulo MAX_FRAMES_IN_FLIGHT
  // This gives us the index of the current frame in the circular buffer
  return _currentFrame % MAX_FRAMES_IN_FLIGHT;
}

///////////////////////////////////////////////////////////////////////////////

VkResult VkSwapChain::acquireImage(vkcontext_rawptr_t ctxVK) {

  // Ensure we have a valid swapchain
  size_t sub_index = subIndex();
  
  // DEBUG: Log current frame state
  logchan_swapchain->log("acquireImage: frame %zu, sub_index %zu, curSwapWriteImage %u", 
                         _currentFrame, sub_index, _curSwapWriteImage);

  // REMOVED: Fence waiting logic that was causing hang
  // The semaphores are properly managed by the swapchain
  // and vkAcquireNextImageKHR will handle the synchronization

  ///////////////////////////////////////////////////
  // Get SwapChain Image
  ///////////////////////////////////////////////////

  bool ok_to_transition = false;

  while (not ok_to_transition) {

    // Ensure we're using the correct frame's semaphore
    // and that any previous signal has been consumed
    if (_curSwapWriteImage != 0xffffffff) {
      // Previous acquire might have failed mid-operation
      // Wait for device idle to ensure clean state
      logchan_swapchain->log("acquireImage: previous acquire failed, waiting for device idle");
      vkDeviceWaitIdle(ctxVK->_vkdevice);
    }

    _curSwapWriteImage = 0xffffffff;
    
    // DEBUG: Log semaphore state before acquire
    auto semaphore = _imageAcquiredSemaphores[sub_index]->_vksema;
    logchan_swapchain->log("acquireImage: attempting vkAcquireNextImageKHR with semaphore %p (sub_index %zu)", 
                          (void*)semaphore, sub_index);
    
    VkResult status    = vkAcquireNextImageKHR(
        ctxVK->_vkdevice,
        _vkSwapChain,
        std::numeric_limits<uint64_t>::max(),
        semaphore, // Use current frame's semaphore
        VK_NULL_HANDLE,
        &_curSwapWriteImage);

    // DEBUG: Log acquire result
    logchan_swapchain->log("acquireImage: vkAcquireNextImageKHR returned %d, image index %u", 
                          status, _curSwapWriteImage);

    switch (status) {
      case VK_SUCCESS:
        ok_to_transition = true;
        logchan_swapchain->log("acquireImage: SUCCESS - acquired image %u", _curSwapWriteImage);
        break;
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR: {
        logchan_swapchain->log("acquireImage: SWAPCHAIN OUT OF DATE - status %d", status);
        vkDeviceWaitIdle(ctxVK->_vkdevice);
        return status;
        break;
      }
      default:
        logchan_swapchain->log("acquireImage: UNEXPECTED STATUS %d", status);
        OrkAssert(false);
        break;
    }
  }
  OrkAssert(_curSwapWriteImage >= 0);

  auto rtg              = _contextVK->_fbi->_main_rtg;
  auto rtg_impl         = rtg->_impl.getShared<VkRtGroupImpl>();
  auto rtb_color        = rtg->buffer(0);
  auto rtb_impl         = rtb_color->_impl.getShared<VklRtBufferImpl>();
  rtb_impl->_is_surface = true;
  rtb_impl->_replaceImage(_swapChainImages[_curSwapWriteImage]);

  logchan_swapchain->log("acquireImage: COMPLETE - image %u ready for rendering", _curSwapWriteImage);
  return VK_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::enqueueFrame(vkcontext_rawptr_t ctxVK) {

  size_t sub_index = subIndex();
  
  // DEBUG: Log frame submission
  logchan_swapchain->log("enqueueFrame: frame %zu, sub_index %zu", _currentFrame, sub_index);

  _semasOkToRender[0]      = _imageAcquiredSemaphores[sub_index]->_vksema;
  _waitOnPipelineStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  // DEBUG: Log semaphore setup
  logchan_swapchain->log("enqueueFrame: wait semaphore %p (image acquired), signal semaphore %p (render complete)", 
                        (void*)_semasOkToRender[0], (void*)_renderCompleteSemaphores[sub_index]->_vksema);

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.waitSemaphoreCount   = _semasOkToRender.size();
  SI.pWaitSemaphores      = _semasOkToRender.data();
  SI.pWaitDstStageMask    = _waitOnPipelineStages.data();
  SI.commandBufferCount   = 1;
  SI.pCommandBuffers      = &ctxVK->primary_cb()->_vkcmdbuf;
  SI.signalSemaphoreCount = 1;
  SI.pSignalSemaphores    = &(_renderCompleteSemaphores[sub_index]->_vksema);

  // Submit with this frame's fence
  if (sub_index < _frameFences.size()) {
    auto& fence = _frameFences[sub_index];
    fence->reset();
    logchan_swapchain->log("enqueueFrame: submitting with fence %p (sub_index %zu)", (void*)fence->_vkfence, sub_index);
    vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, fence->_vkfence);
    logchan_swapchain->log("enqueueFrame: queue submit complete with fence %p", (void*)fence->_vkfence);
  } else {
    logchan_swapchain->log("enqueueFrame: WARNING - submitting without fence (sub_index %zu >= fence count %zu)", 
                          sub_index, _frameFences.size());
    vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, VK_NULL_HANDLE);
  }
  
  logchan_swapchain->log("enqueueFrame: COMPLETE - frame %zu submitted", _currentFrame);
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::enqueuePresentFrame(vkcontext_rawptr_t ctxVK) {

  size_t sub_index = subIndex();
  
  // DEBUG: Log present operation
  logchan_swapchain->log("enqueuePresentFrame: frame %zu, sub_index %zu, image %u", 
                         _currentFrame, sub_index, _curSwapWriteImage);

  _semasOkToPresent[0] = (_renderCompleteSemaphores[sub_index]->_vksema);

  // DEBUG: Log present semaphore
  logchan_swapchain->log("enqueuePresentFrame: waiting for render complete semaphore %p", (void*)_semasOkToPresent[0]);

  VkPresentInfoKHR PRESI{};
  initializeVkStruct(PRESI, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  PRESI.waitSemaphoreCount = _semasOkToPresent.size();
  PRESI.pWaitSemaphores    = _semasOkToPresent.data();
  PRESI.swapchainCount     = 1;
  PRESI.pSwapchains        = &_vkSwapChain;
  PRESI.pImageIndices      = &_curSwapWriteImage;

  logchan_swapchain->log("enqueuePresentFrame: calling vkQueuePresentKHR for image %u", _curSwapWriteImage);
  VkResult status = vkQueuePresentKHR(ctxVK->_vkqueue_graphics, &PRESI);
  logchan_swapchain->log("enqueuePresentFrame: vkQueuePresentKHR returned %d", status);
  
  // printf("vkQueuePresentKHR returned status: %d (0x%x)\n", status, status);

  switch (status) {
    case VK_SUCCESS:
      logchan_swapchain->log("enqueuePresentFrame: SUCCESS - frame %zu presented", _currentFrame);
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
      // Need to recreate swap chain immediately
      vkDeviceWaitIdle(ctxVK->_vkdevice);
      ctxVK->_fbi->_swapchain->_reinit();
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

void VkSwapChain::waitPresentFrame(vkcontext_rawptr_t ctxVK) {
  size_t sub_index = subIndex();
  
  // DEBUG: Log frame waiting
  logchan_swapchain->log("waitPresentFrame: frame %zu, sub_index %zu", _currentFrame, sub_index);
  
  // Wait for the current frame's fence to ensure rendering is complete
  auto& fence = _frameFences[sub_index];

  float pre_time                = ctxVK->_present_timer.SecsSinceStart();
  float time_since_last_present = pre_time - ctxVK->_prev_time;
  ctxVK->_prev_time             = pre_time;

  if (fence) {
    // DEBUG: Log fence waiting
    logchan_swapchain->log("waitPresentFrame: waiting for fence %p (frame %zu, sub_index %zu)", 
                          (void*)fence->_vkfence, _currentFrame, sub_index);
    
    // printf("  VkSwapChain<%p> Waiting for fence from frame %zu...\n", (void*) this, _currentFrame);
    fence->wait();
    
    logchan_swapchain->log("waitPresentFrame: fence %p wait complete, resetting fence", (void*)fence->_vkfence);
    fence->reset();
    
    logchan_swapchain->log("waitPresentFrame: fence %p reset complete", (void*)fence->_vkfence);
  } else {
    logchan_swapchain->log("waitPresentFrame: WARNING - no fence for sub_index %zu", sub_index);
  }

  ctxVK->_total_frame_time += time_since_last_present;

  float pos_time   = ctxVK->_present_timer.SecsSinceStart();
  float delta_time = pos_time - pre_time;
  ctxVK->_present_wait_time += delta_time;
  ctxVK->_total_wait_time = ctxVK->_present_timer.SecsSinceStart();

  if ((_currentFrame & 0x1ff) == 0) {
    float average_frame_time = ctxVK->_total_frame_time / (_currentFrame + 1);
    float average_wait_time  = ctxVK->_present_wait_time / (_currentFrame + 1);
    printf(
        "waittime<%g> total_time<%g>. average_wait_time<%g s> average_frame_time<%g>\n",
        ctxVK->_present_wait_time,
        ctxVK->_total_wait_time,
        average_wait_time,
        average_frame_time);

    ctxVK->_total_frame_time = 0.0f;
    ctxVK->_total_wait_time  = 0.0f;
  }
  
  // DEBUG: Log frame completion and increment
  logchan_swapchain->log("waitPresentFrame: frame %zu complete, incrementing to frame %zu", 
                         _currentFrame, _currentFrame + 1);
  _currentFrame++;
  
  logchan_swapchain->log("waitPresentFrame: COMPLETE - frame counter now %zu", _currentFrame);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
