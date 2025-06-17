////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////

void VkFrameBufferInterface::_initSwapChain() {

  auto& vkdev    = _contextVK->_vkdevice;
  auto& cmdbuf   = _contextVK->primary_cb()->_vkcmdbuf;
  auto pres_caps = _contextVK->_vkpresentation_caps;

  if (_swapchain) {
    _old_swapchains.insert(_swapchain);

    // Wait for all frames in flight to complete before destroying
    vkDeviceWaitIdle(_contextVK->_vkdevice);

    for (auto& fence : _swapchain->_frameFences) {
      if (fence) {
        fence->wait();
      }
    }

    size_t num_images = _swapchain->_rtgs.size();
    for (size_t i = 0; i < num_images; i++) {
      auto rtg            = _swapchain->_rtgs[i];
      auto rtb_color      = rtg->buffer(0);
      auto rtb_depth      = rtg->_depthBuffer;
      auto rtb_impl_color = rtb_color->_impl.getShared<VklRtBufferImpl>();
      auto rtb_impl_depth = rtb_depth ? rtb_depth->_impl.getShared<VklRtBufferImpl>() : nullptr;
      // auto img = _swapchain->_vkSwapChainImages[i];

      // barrier - complete all ops before destroying
      if (0) {
        auto imgbar = createImageBarrier(
            rtb_impl_color->_vkimg,
            VK_IMAGE_LAYOUT_UNDEFINED,            // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newLayout
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
            VK_ACCESS_MEMORY_WRITE_BIT);          // dstAccessMask
        vkCmdPipelineBarrier(
            cmdbuf,                                        // cmdbuf
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,            // dstStageMask
            0,                                             // dependencyFlags
            0,
            nullptr, // memoryBarrierCount, pMemoryBarriers
            0,
            nullptr, // bufferMemoryBarrierCount, pBufferMemoryBarriers
            1,
            imgbar.get()); // imageMemoryBarrierCount, pImageMemoryBarriers
      }

      vkDestroyImageView(vkdev, rtb_impl_color->_vkimgview, nullptr);
      // vkDestroyImage(vkdev, img, nullptr);
    }
    vkDestroySwapchainKHR(vkdev, _swapchain->_vkSwapChain, nullptr);
  }

  // Clear old swapchains after destroying current one
  for (auto& old_swap : _old_swapchains) {
    vkDestroySwapchainKHR(vkdev, old_swap->_vkSwapChain, nullptr);
  }
  _old_swapchains.clear();

  auto swap_chain = std::make_shared<VkSwapChain>();

  // Create per-frame synchronization objects
  swap_chain->_imageAcquiredSemaphores.clear();
  swap_chain->_renderCompleteSemaphores.clear();
  swap_chain->_frameFences.clear();

  for (size_t i = 0; i < VkSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {

    auto bin_sema_imgacq = std::make_shared<VulkanBinarySemaphore>(_contextVK);
    auto bin_sema_rencom = std::make_shared<VulkanBinarySemaphore>(_contextVK);
    auto fence = std::make_shared<VulkanFenceObject>(_contextVK);
    swap_chain->_imageAcquiredSemaphores.push_back(bin_sema_imgacq);
    swap_chain->_renderCompleteSemaphores.push_back(bin_sema_rencom);
    swap_chain->_frameFences.push_back(fence);
    fence->reset();
  }

  // auto surfaceFormat = pres_caps->_formats[0];
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

  auto ctx_glfw = _contextVK->_impl.getShared<VkPlatformObject>()->_ctxbase;
  auto window   = ctx_glfw->_glfwWindow;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  auto& caps = pres_caps->_capabilities;

  // Check if extent is defined by surface (required on some platforms)
  if (caps.currentExtent.width != 0xFFFFFFFF) {
    width  = caps.currentExtent.width;
    height = caps.currentExtent.height;
  }

  // Clamp to surface capabilities
  width  = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, uint32_t(width)));
  height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, uint32_t(height)));

  printf("Swap chain dimensions: requested=%dx%d, clamped=%ux%u\n", width, height, uint32_t(width), uint32_t(height));
  printf(
      "Surface caps: min=%ux%u, max=%ux%u, current=%ux%u\n",
      caps.minImageExtent.width,
      caps.minImageExtent.height,
      caps.maxImageExtent.width,
      caps.maxImageExtent.height,
      caps.currentExtent.width,
      caps.currentExtent.height);

  // Ensure we have valid dimensions
  if (width == 0 || height == 0) {
    // Window is minimized, use minimum valid size
    width  = std::max(1u, caps.minImageExtent.width);
    height = std::max(1u, caps.minImageExtent.height);
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

  // image view properties
  SCINFO.imageSharingMode =
      VK_SHARING_MODE_EXCLUSIVE;          // Can be VK_SHARING_MODE_CONCURRENT if sharing between multiple queue families
  SCINFO.queueFamilyIndexCount = 0;       // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT
  SCINFO.pQueueFamilyIndices   = nullptr; // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT

  // misc properties
  // SCINFO.preTransform already set above, don't override

  // Choose a supported composite alpha mode
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

  SCINFO.clipped = VK_TRUE; // clip pixels that are obscured by other windows
  // SCINFO.oldSwapchain   = _swapchain ? _swapchain->_vkSwapChain : VK_NULL_HANDLE;
  SCINFO.oldSwapchain = VK_NULL_HANDLE; // _swapchain ? _swapchain->_vkSwapChain : VK_NULL_HANDLE;

  // Choose a supported present mode
  SCINFO.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always supported
  for (const auto& mode : pres_caps->_presentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      SCINFO.presentMode = mode;
      break;
    }
  }
  // SCINFO.presentMode    = VK_PRESENT_MODE_FIFO_KHR;
  SCINFO.clipped = VK_TRUE;

  VkResult OK = vkCreateSwapchainKHR(vkdev, &SCINFO, nullptr, &swap_chain->_vkSwapChain);
  OrkAssert(OK == VK_SUCCESS);

  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(vkdev, swap_chain->_vkSwapChain, &imageCount, nullptr);
  std::vector<VkImage> swapChainImages;
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vkdev, swap_chain->_vkSwapChain, &imageCount, swapChainImages.data());

  for (size_t i = 0; i < swapChainImages.size(); i++) {

    _contextVK->_setObjectDebugName(swapChainImages[i], VK_OBJECT_TYPE_IMAGE, FormatString("swapchain-image-%d", i).c_str());

    auto IVCI = createImageViewInfo2D(
        swapChainImages[i],   //
        surfaceFormat.format, //
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageView imgview;
    OK = vkCreateImageView(vkdev, IVCI.get(), nullptr, &imgview);
    OrkAssert(OK == VK_SUCCESS);

    auto ork_color_format = VkFormatConverter::convertBufferFormat(surfaceFormat.format);

    auto rtg       = std::make_shared<RtGroup>(_contextVK, width, height, MsaaSamples::MSAA_1X, true);
    auto rtb_color = rtg->createRenderTarget(ork_color_format, "present"_crcu);
    auto rtg_impl  = _createRtGroupImpl(rtg.get());
    rtg->_name     = FormatString("vk-swapchain-%d", i);
    ////////////////////////////////////////////
    // link rtb_color to swap chain color image
    ////////////////////////////////////////////
    auto rtb_impl_color         = rtb_color->_impl.getShared<VklRtBufferImpl>();
    rtb_impl_color->_is_surface = true;
    rtb_impl_color->_replaceImage(
        surfaceFormat.format, //
        imgview,              //
        swapChainImages[i]);
    ////////////////////////////////////////////
    swap_chain->_rtgs.push_back(rtg);
  }

  _swapchain = swap_chain;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_enq_transitionMainRtgToPresent() {

  auto main_rtb  = _main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();

  auto new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  auto imgbar = createImageBarrier(
      main_rtbi->_vkimg,
      main_rtbi->_currentLayout,            // oldLayout (dont care)
      new_layout,                           // newLayout
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
      VK_ACCESS_MEMORY_READ_BIT);           // dstAccessMask

  vkCmdPipelineBarrier(
      _contextVK->primary_cb()->_vkcmdbuf,           // cmdbuf
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // dstStageMask
      0,                                             // dependencyFlags
      0,
      nullptr, // memoryBarrierCount, pMemoryBarriers
      0,
      nullptr, // bufferMemoryBarrierCount, pBufferMemoryBarriers
      1,
      imgbar.get()); // imageMemoryBarrierCount, pImageMemoryBarriers

  main_rtbi->setLayout(new_layout);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
