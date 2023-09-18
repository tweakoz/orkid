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

  auto& vkdev = _contextVK->_vkdevice;
  auto& fence = _contextVK->_mainGfxSubmitFence;
  auto& cmdbuf = _contextVK->primary_cb()->_vkcmdbuf;
  auto pres_caps = _contextVK->_vkpresentation_caps;

  if (_swapchain) {
    _old_swapchains.insert(_swapchain);
    size_t num_images = _swapchain->_vkSwapChainImages.size();
    vkWaitForFences(vkdev, 1, &fence, VK_TRUE, UINT64_MAX);
    for (size_t i = 0; i < num_images; i++) {
      auto img = _swapchain->_vkSwapChainImages[i];

      // barrier - complete all ops before destroying
      if (0)
        _imageBarrier(
            cmdbuf,            // cmdbuf
            img,                                           // image
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
            VK_ACCESS_MEMORY_WRITE_BIT,                    // dstAccessMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,            // dstStageMask
            VK_IMAGE_LAYOUT_UNDEFINED,                     // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);         // newLayout

      auto imgview = _swapchain->_vkSwapChainImageViews[i];
      vkDestroyImageView(vkdev, imgview, nullptr);
      // vkDestroyImage(vkdev, img, nullptr);
    }
    vkDestroySwapchainKHR(vkdev, _swapchain->_vkSwapChain, nullptr);
  }

  auto swap_chain = std::make_shared<VkSwapChain>();

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
  swap_chain->_vkSwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vkdev, swap_chain->_vkSwapChain, &imageCount, swap_chain->_vkSwapChainImages.data());

  swap_chain->_vkSwapChainImageViews.resize(swap_chain->_vkSwapChainImages.size());

  for (size_t i = 0; i < swap_chain->_vkSwapChainImages.size(); i++) {
    VkImageViewCreateInfo IVCI{};
    initializeVkStruct(IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    IVCI.image                           = swap_chain->_vkSwapChainImages[i];
    IVCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    IVCI.format                          = surfaceFormat.format;
    IVCI.components.r                    = VK_COMPONENT_SWIZZLE_R;
    IVCI.components.g                    = VK_COMPONENT_SWIZZLE_G;
    IVCI.components.b                    = VK_COMPONENT_SWIZZLE_B;
    IVCI.components.a                    = VK_COMPONENT_SWIZZLE_A;
    IVCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    IVCI.subresourceRange.baseMipLevel   = 0;
    IVCI.subresourceRange.levelCount     = 1;
    IVCI.subresourceRange.baseArrayLayer = 0;
    IVCI.subresourceRange.layerCount     = 1;

    OK = vkCreateImageView(vkdev, &IVCI, nullptr, &swap_chain->_vkSwapChainImageViews[i]);
    OrkAssert(OK == VK_SUCCESS);

    auto rtg       = std::make_shared<RtGroup>(_contextVK, width, height, MsaaSamples::MSAA_1X, false);
    auto rtb_color = rtg->createRenderTarget(EBufferFormat::RGBA8, "present"_crcu);
    auto rtb_depth = rtg->createRenderTarget(DEPTH_FORMAT, "depth"_crcu);
    auto rtg_impl  = _createRtGroupImpl(rtg.get());
    rtg->_name     = FormatString("vk-swapchain-%d", i);
    ////////////////////////////////////////////
    auto rtb_impl_color = rtb_color->_impl.getShared<VklRtBufferImpl>();
    auto rtb_impl_depth = rtb_depth->_impl.getShared<VklRtBufferImpl>();
    ////////////////////////////////////////////
    // create depth image
    _vkCreateImageForBuffer(_contextVK, rtb_impl_depth, "depth"_crcu);
    ////////////////////////////////////////////
    // link to swap chain color image
    rtb_impl_color->_init      = false;
    rtb_impl_color->_vkimg     = swap_chain->_vkSwapChainImages[i];
    rtb_impl_color->_vkimgview = swap_chain->_vkSwapChainImageViews[i];
    // rtb_impl_color->_vkfmt = true;
    ////////////////////////////////////////////

    swap_chain->_rtgs.push_back(rtg);
    swap_chain->_color_rtbs.push_back(rtb_color);
    swap_chain->_depth_rtbs.push_back(rtb_depth);
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

  vkCmdClearColorImage(gfxcb->_vkcmdbuf, _swapchain->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &ISRR);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_enq_transitionSwapChainForPresent() {

  auto gfxcb = _contextVK->primary_cb();

  _imageBarrier(
      gfxcb->_vkcmdbuf,                              // cmdbuf
      _swapchain->image(),                           // image
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
      0,                                             // dstAccessMask
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,             // dstStageMask
      VK_IMAGE_LAYOUT_UNDEFINED,                     // oldLayout
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);              // newLayout
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_transitionSwapChainForClear() {

  auto gfxcb = _contextVK->primary_cb();

  _imageBarrier(
      gfxcb->_vkcmdbuf,                      // cmdbuf
      _swapchain->image(),                   // image
      0,                                     // srcAccessMask
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // dstAccessMask
      VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // dstStageMask
      VK_IMAGE_LAYOUT_UNDEFINED,             // oldLayout
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); // newLayout
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
  OrkAssert(_swapchain->_curSwapWriteImage < _swapchain->_vkSwapChainImages.size());
  auto swap_rtg = _swapchain->_rtgs[_swapchain->_curSwapWriteImage];

  _main_rtg = swap_rtg;
  // printf( "_swapchain->_curSwapWriteImage<%u>\n", _swapchain->_curSwapWriteImage );
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_bindSwapChainToRenderPass(vkrenderpass_ptr_t rpass) {

  if(nullptr==_swapchain->_mainRenderPass){
    _swapchain->_mainRenderPass = rpass;

    _swapchain->_vkFrameBuffers.resize(_swapchain->_vkSwapChainImages.size());
    for (size_t i = 0; i < _swapchain->_vkSwapChainImages.size(); i++) {

        auto& VKFRB = _swapchain->_vkFrameBuffers[i];
        std::vector<VkImageView> fb_attachments;
        auto& IMGVIEW = _swapchain->_vkSwapChainImageViews[i];

        auto depth_rtb     = _swapchain->_depth_rtbs[i];
        auto depth_rtbimpl = depth_rtb->_impl.getShared<VklRtBufferImpl>();
        fb_attachments.push_back(IMGVIEW);
        fb_attachments.push_back(depth_rtbimpl->_vkimgview);

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
  }
  else{
    OrkAssert(_swapchain->_mainRenderPass == rpass);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
