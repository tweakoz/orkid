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
    _swapchain->_fence->wait();
    size_t num_images = _swapchain->_rtgs.size();
    for (size_t i = 0; i < num_images; i++) {
      auto rtg = _swapchain->_rtgs[i];
      auto rtb_color = rtg->GetMrt(0);
      auto rtb_depth = rtg->_depthBuffer;
      auto rtb_impl_color = rtb_color->_impl.getShared<VklRtBufferImpl>();
      auto rtb_impl_depth = rtb_depth->_impl.getShared<VklRtBufferImpl>();
      //auto img = _swapchain->_vkSwapChainImages[i];

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
            0, nullptr,                                    // memoryBarrierCount, pMemoryBarriers
            0, nullptr,                                    // bufferMemoryBarrierCount, pBufferMemoryBarriers
            1, imgbar.get());                              // imageMemoryBarrierCount, pImageMemoryBarriers
      }

      vkDestroyImageView(vkdev, rtb_impl_color->_vkimgview, nullptr);
      // vkDestroyImage(vkdev, img, nullptr);
    }
    vkDestroySwapchainKHR(vkdev, _swapchain->_vkSwapChain, nullptr);
  }

  auto swap_chain = std::make_shared<VkSwapChain>();
  swap_chain->_fence = std::make_shared<VulkanFenceObject>(_contextVK);
  auto surfaceFormat = pres_caps->_formats[0];

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

  swap_chain->_extent.width  = width;
  swap_chain->_extent.height = height;
  swap_chain->_width         = width;
  swap_chain->_height        = height;

  // image properties
  SCINFO.minImageCount    = 3;
  SCINFO.imageFormat      = surfaceFormat.format;                // Chosen from VkSurfaceFormatKHR, after querying supported formats
  SCINFO.imageColorSpace  = surfaceFormat.colorSpace;            // Chosen from VkSurfaceFormatKHR
  SCINFO.imageExtent      = swap_chain->_extent;                 // The width and height of the swap chain images
  SCINFO.imageArrayLayers = 1;                                   // Always 1 unless developing a stereoscopic 3D application
  SCINFO.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Or any other value depending on your needs
  SCINFO.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  SCINFO.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  SCINFO.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;

  // image view properties
  SCINFO.imageSharingMode =
      VK_SHARING_MODE_EXCLUSIVE;          // Can be VK_SHARING_MODE_CONCURRENT if sharing between multiple queue families
  SCINFO.queueFamilyIndexCount = 0;       // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT
  SCINFO.pQueueFamilyIndices   = nullptr; // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT

  // misc properties
  SCINFO.preTransform   = pres_caps->_capabilities.currentTransform;
  SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // todo - support alpha
  SCINFO.clipped        = VK_TRUE;                           // clip pixels that are obscured by other windows
  SCINFO.oldSwapchain   = VK_NULL_HANDLE;
  SCINFO.presentMode    = VK_PRESENT_MODE_IMMEDIATE_KHR;
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
    auto IVCI = createImageViewInfo2D( swapChainImages[i], //
                                       surfaceFormat.format, //
                                       VK_IMAGE_ASPECT_COLOR_BIT );

    VkImageView imgview;
    OK = vkCreateImageView(vkdev, IVCI.get(), nullptr, &imgview);
    OrkAssert(OK == VK_SUCCESS);

    auto rtg       = std::make_shared<RtGroup>(_contextVK, width, height, MsaaSamples::MSAA_1X, true);
    auto rtb_color = rtg->createRenderTarget(EBufferFormat::RGBA8, "present"_crcu);
    auto rtb_depth = rtg->createRenderTarget(DEPTH_FORMAT, "depth"_crcu);
    auto rtg_impl  = _createRtGroupImpl(rtg.get());
    rtg->_name     = FormatString("vk-swapchain-%d", i);
    ////////////////////////////////////////////
    // link rtb_color to swap chain color image
    ////////////////////////////////////////////
    auto rtb_impl_color = rtb_color->_impl.getShared<VklRtBufferImpl>();
    rtb_impl_color->_is_surface = true;
    rtb_impl_color->_replaceImage( surfaceFormat.format, //
                                   imgview, //
                                   swapChainImages[i] );
    ////////////////////////////////////////////
    swap_chain->_rtgs.push_back(rtg);
  }

  _swapchain = swap_chain;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_clearSwapChainBuffer() {

  auto gfxcb = _contextVK->primary_cb();
  VkClearColorValue clearColor = {{0.0f, 1.0f, 0.0f, 1.0f}}; // Clear to black color
  VkImageSubresourceRange ISRR = {};
  ISRR.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
  ISRR.baseMipLevel            = 0;
  ISRR.levelCount              = 1;
  ISRR.baseArrayLayer          = 0;
  ISRR.layerCount              = 1;

  auto rtg  = _swapchain->currentRTG();
  auto rtbi = rtg->GetMrt(0)->_impl.getShared<VklRtBufferImpl>();

  vkCmdClearColorImage(gfxcb->_vkcmdbuf, rtbi->_vkimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &ISRR);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_enq_transitionMainRtgToPresent() {

  auto main_rtb = _main_rtg->GetMrt(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();

  auto imgbar = createImageBarrier(
      main_rtbi->_vkimg,
      main_rtbi->_currentLayout,            // oldLayout (dont care)
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,      // newLayout
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
      VK_ACCESS_MEMORY_READ_BIT);           // dstAccessMask

  vkCmdPipelineBarrier(
      _contextVK->primary_cb()->_vkcmdbuf,           // cmdbuf
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // dstStageMask
      0,                                             // dependencyFlags
      0, nullptr,                                    // memoryBarrierCount, pMemoryBarriers
      0, nullptr,                                    // bufferMemoryBarrierCount, pBufferMemoryBarriers
      1, imgbar.get());                              // imageMemoryBarrierCount, pImageMemoryBarriers

   main_rtbi->_currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_transitionSwapChainForClear() {

  auto main_rtb = _main_rtg->GetMrt(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();

   main_rtbi->_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  auto imgbar = createImageBarrier(
      main_rtbi->_vkimg,
      VK_IMAGE_LAYOUT_UNDEFINED,             // oldLayout
      main_rtbi->_currentLayout,             // newLayout
      VkAccessFlagBits(0),                   // srcAccessMask
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT); // dstAccessMask

  vkCmdPipelineBarrier(
      _contextVK->primary_cb()->_vkcmdbuf,  // cmdbuf
      VK_PIPELINE_STAGE_TRANSFER_BIT,       // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
      0,                                    // dependencyFlags
      0, nullptr,                           // memoryBarrierCount, pMemoryBarriers
      0, nullptr,                           // bufferMemoryBarrierCount, pBufferMemoryBarriers
      1, imgbar.get());                     // imageMemoryBarrierCount, pImageMemoryBarriers
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_acquireSwapChainForFrame() {

  ///////////////////////////////////////////////////
  // Get SwapChain Image
  ///////////////////////////////////////////////////

  bool ok_to_transition = false;

  while (not ok_to_transition) {
    _swapchain->_curSwapWriteImage = 0xffffffff;
    VkResult status                = vkAcquireNextImageKHR(
        _contextVK->_vkdevice,                //
        _swapchain->_vkSwapChain,             //
        std::numeric_limits<uint64_t>::max(), // timeout (ns)
        _swapChainImageAcquiredSemaphore,     //
        VK_NULL_HANDLE,                       // no fence
        &_swapchain->_curSwapWriteImage       //
    );

    switch (status) {
      case VK_SUCCESS:
        ok_to_transition = true;
        break;
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR: {
        _initSwapChain();
        // printf("VK_ERROR_OUT_OF_DATE_KHR\n");
        //  OrkAssert(false);
        //   need to recreate swap chain
        break;
      }
    }
  }

  OrkAssert(_swapchain->_curSwapWriteImage >= 0);
  OrkAssert(_swapchain->_curSwapWriteImage < _swapchain->_rtgs.size());
  _main_rtg = _swapchain->currentRTG();
  // printf( "_swapchain->_curSwapWriteImage<%u>\n", _swapchain->_curSwapWriteImage );
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_bindSwapChainToRenderPass(vkrenderpass_ptr_t rpass) {

  if (nullptr == _swapchain->_mainRenderPass) {
    _swapchain->_mainRenderPass = rpass;

    _swapchain->_vkFrameBuffers.resize(_swapchain->_rtgs.size());
    for (size_t i = 0; i < _swapchain->_rtgs.size(); i++) {
      auto rtg = _swapchain->_rtgs[i];
      auto rtb_color = rtg->GetMrt(0);
      auto rtb_depth = rtg->_depthBuffer;
      auto rtb_impl_color = rtb_color->_impl.getShared<VklRtBufferImpl>();
      auto rtb_impl_depth = rtb_depth->_impl.getShared<VklRtBufferImpl>();

      auto& VKFRB = _swapchain->_vkFrameBuffers[i];
      std::vector<VkImageView> fb_attachments;

      fb_attachments.push_back(rtb_impl_color->_vkimgview);
      fb_attachments.push_back(rtb_impl_depth->_vkimgview);

      VkFramebufferCreateInfo CFBI = {};
      CFBI.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      CFBI.renderPass              = rpass->_vkrp; // The VkRenderPass you created earlier
      CFBI.attachmentCount         = fb_attachments.size();
      CFBI.pAttachments            = fb_attachments.data();
      CFBI.width                   = _swapchain->_extent.width; // Typically the size of your swap chain images or offscreen buffer
      CFBI.height                  = _swapchain->_extent.height;
      CFBI.layers                  = 1;

      VkResult OK = vkCreateFramebuffer(_contextVK->_vkdevice, &CFBI, nullptr, &VKFRB);
      OrkAssert(OK == VK_SUCCESS);
    }
  } else {
    //OrkAssert(_swapchain->_mainRenderPass == rpass);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
