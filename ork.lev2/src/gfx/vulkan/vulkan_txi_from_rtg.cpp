
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txirtg = logger()->configureChannel("VKTXIRTG", fvec3(0.8, 0.2, 0.5), false);
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_initTextureFromRtBuffer(RtBuffer* rtbuffer) {
  auto ptex = rtbuffer->texture();
  OrkAssert(ptex);
  ptex->_source = ETextureSource::FROM_RTG;
  auto vk_tex = ptex->_impl.makeShared<VulkanTextureObject>(_contextVK->_txi.get());

  auto format  = rtbuffer->format();
  int iwidth   = rtbuffer->_width;
  int iheight  = rtbuffer->_height;
  int num_mips = 1;
  auto fmt_str = EBufferFormatToName(format);
  
  // Check if this is a depth buffer
  bool is_depth = (rtbuffer->_usage == "depth"_crcu);

  if (0) {
    logchan_txirtg->log(
        "_initTextureFromRtBuffer ptex<%p:%s> w<%d> h<%d> fmt<%s> is_depth<%d>",
        (void*)ptex,
        ptex->_debugName.c_str(),
        iwidth,
        iheight,
        fmt_str.c_str(),
        is_depth);
  }

  /////////////////////////////////////
  // create image object
  /////////////////////////////////////

  auto img_info   = makeVKICI(iwidth, iheight, 1, format, num_mips);
  if (is_depth) {
    img_info->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  } else {
    img_info->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  std::string debug_name = rtbuffer->_debugName.empty() ? "rtbuffer_texture" : rtbuffer->_debugName;
  // RTG textures use slot [0] only (no double-buffering needed)
  vk_tex->_imgobj[0] = std::make_shared<VulkanImageObject>(_contextVK, img_info, debug_name);
  vk_tex->_vksampler = _contextVK->_sampler_base;

  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  VkImageAspectFlagBits aspect_mask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

  auto IVCI = createImageViewInfo2D(
      vk_tex->_imgobj[0]->_vkimage,                     //
      VkFormatConverter::convertBufferFormat(format), //
      aspect_mask);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(vk_tex->_imgobj[0]->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vk_tex->_imgobj[0]->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  // Set debug name for image view
  if (!rtbuffer->_debugName.empty()) {
    std::string view_name = rtbuffer->_debugName + "_view";
    _contextVK->_setObjectDebugName(vk_tex->_imgobj[0]->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
  }

  OrkAssert(vk_tex->_imgobj[0]->_vkimageview != VK_NULL_HANDLE);

  /////////////////////////////////////
  // create descriptor image info
  /////////////////////////////////////

  vk_tex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vk_tex->_vkdescriptor_info.imageView   = vk_tex->_imgobj[0]->_vkimageview;
  vk_tex->_vkdescriptor_info.sampler     = vk_tex->_vksampler->_vksampler;

  auto rtb_impl        = rtbuffer->_impl.getShared<VklRtBufferImpl>();
  rtb_impl->_imgobj = vk_tex->_imgobj[0];
  // Initialize layout to UNDEFINED since this is a new image
  rtb_impl->setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  rtb_impl->_teximpl = vk_tex;

  vk_tex->_imgview_hash.init();
  vk_tex->_imgview_hash.accumulateItem(vk_tex->_imgobj[0]->_serial_number);
  vk_tex->_imgview_hash.finish();

  /////////////////////////////////////
  // transition to appropriate attachment layout
  /////////////////////////////////////

  // Suspend render pass if active - we need to execute barriers
  bool was_active = _contextVK->_renderPassActive;
  if (was_active) {
    _contextVK->suspendRenderPass();
  }

  auto cmdbuf = _contextVK->beginRecordCommandBuffer("VkTextureInterface::_initTextureFromRtBuffer");

  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  VkImageLayout target_layout;
  VkAccessFlagBits access_flags;
  VkPipelineStageFlags stage_flags;
  
  if (is_depth) {
    target_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    access_flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    stage_flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    target_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    access_flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  }

  // First transition to TRANSFER_DST for clearing
  auto clear_barrier = createImageBarrier(
      vk_tex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);
  clear_barrier->subresourceRange.aspectMask = aspect_mask;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr, 1, clear_barrier.get());

  // Clear the image
  if (is_depth) {
    VkClearDepthStencilValue clear_value = {1.0f, 0};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    vkCmdClearDepthStencilImage(vk_cmdbuf, vk_tex->_imgobj[0]->_vkimage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
  } else {
    VkClearColorValue clear_color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(vk_cmdbuf, vk_tex->_imgobj[0]->_vkimage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);
  }

  // Now transition to the target attachment layout
  auto attach_barrier = createImageBarrier(
      vk_tex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      target_layout,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      access_flags);
  attach_barrier->subresourceRange.aspectMask = aspect_mask;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      stage_flags,
      0, 0, nullptr, 0, nullptr, 1, attach_barrier.get());

  // Update the buffer's current layout to match what we transitioned to
  rtb_impl->setLayout(target_layout);
  // Also update the image object's layout
  if (vk_tex->_imgobj[0]) {
    vk_tex->_imgobj[0]->_currentLayout = target_layout;
  }

  /////////////////////////////////////

  // RTG texture is now ready for sampling
  vk_tex->_img_sampling = vk_tex->_imgobj[0];

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueSecondaryCommandBuffer(cmdbuf);
  //_contextVK->enqueueDeferredOneShotCommand(cmdbuf);
  
  // Resume render pass if it was active
  if (was_active) {
    _contextVK->resumeRenderPass();
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
