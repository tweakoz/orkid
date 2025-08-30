
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
static logchannel_ptr_t logchan_txirtg = logger()->configureChannel("VKTXIRTG", fvec3(0.8, 0.2, 0.5), true);
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

  if (0) {
    logchan_txirtg->log(
        "_initTextureFromRtBuffer ptex<%p:%s> w<%d> h<%d> fmt<%s>",
        (void*)ptex,
        ptex->_debugName.c_str(),
        iwidth,
        iheight,
        fmt_str.c_str());
  }

  /////////////////////////////////////
  // create image object
  /////////////////////////////////////

  auto img_info   = makeVKICI(iwidth, iheight, 1, format, num_mips);
  img_info->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

  std::string debug_name = rtbuffer->_debugName.empty() ? "rtbuffer_texture" : rtbuffer->_debugName;
  teximpl->_imgobj    = std::make_shared<VulkanImageObject>(_contextVK, img_info, debug_name);
  teximpl->_vksampler = _contextVK->_sampler_base;

  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      teximpl->_imgobj->_vkimage,                     //
      VkFormatConverter::convertBufferFormat(format), //
      VK_IMAGE_ASPECT_COLOR_BIT);
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

  /////////////////////////////////////
  // transition to transfer dst (for copy)
  /////////////////////////////////////

  auto cmdbuf = _contextVK->beginRecordCommandBuffer("VkTextureInterface::_initTextureFromRtBuffer");

  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto barrier = createImageBarrier(
      teximpl->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
