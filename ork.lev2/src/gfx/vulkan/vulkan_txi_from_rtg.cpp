
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
  auto teximpl = ptex->_impl.makeShared<VulkanTextureObject>(_contextVK->_txi.get());

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
    img_info->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  } else {
    img_info->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  std::string debug_name = rtbuffer->_debugName.empty() ? "rtbuffer_texture" : rtbuffer->_debugName;
  teximpl->_imgobj    = std::make_shared<VulkanImageObject>(_contextVK, img_info, debug_name);
  teximpl->_vksampler = _contextVK->_sampler_base;

  /////////////////////////////////////
  // create image view
  /////////////////////////////////////
  
  VkImageAspectFlagBits aspect_mask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

  auto IVCI = createImageViewInfo2D(
      teximpl->_imgobj->_vkimage,                     //
      VkFormatConverter::convertBufferFormat(format), //
      aspect_mask);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(teximpl->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &teximpl->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);
  
  // Set debug name for image view
  if (!rtbuffer->_debugName.empty()) {
    std::string view_name = rtbuffer->_debugName + "_view";
    _contextVK->_setObjectDebugName(teximpl->_imgobj->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
  }

  OrkAssert(teximpl->_imgobj->_vkimageview != VK_NULL_HANDLE);

  /////////////////////////////////////
  // create descriptor image info
  /////////////////////////////////////

  teximpl->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  teximpl->_vkdescriptor_info.imageView   = teximpl->_imgobj->_vkimageview;
  teximpl->_vkdescriptor_info.sampler     = teximpl->_vksampler->_vksampler;

  auto rtb_impl        = rtbuffer->_impl.getShared<VklRtBufferImpl>();
  rtb_impl->_imgobj = teximpl->_imgobj;
  rtb_impl->setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  rtb_impl->_teximpl = teximpl;

  teximpl->_imgview_hash.init();
  teximpl->_imgview_hash.accumulateItem(teximpl);
  teximpl->_imgview_hash.accumulateItem(teximpl->_imgobj);
  teximpl->_imgview_hash.accumulateItem(teximpl->_imgobj->_vkimageview);
  teximpl->_imgview_hash.finish();

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

  auto barrier = createImageBarrier(
      teximpl->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      target_layout,
      VkAccessFlagBits(0),
      access_flags);

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      stage_flags,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

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
