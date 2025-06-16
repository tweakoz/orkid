////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txi_loadreq = logger()->createChannel("VKTXILOAD", fvec3(0.8, 0.2, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_createFromLoadReq(texloadreq_ptr_t req) {
  auto ptex = req->ptex;
  logchan_txi_loadreq->log("xxx _createFromLoadReq<%p:%s>\n", (void*)ptex.get(), ptex->_debugName.c_str());
  ptex->_debugName = "VkTextureInterface::_createFromLoadReq";

  auto vktex       = ptex->_impl.makeShared<VulkanTextureObject>(this);
  auto chain       = req->_cmipchain;
  size_t num_mips  = chain->_levels.size();
  auto format      = chain->_format;
  int iwidth       = chain->_width;
  int iheight      = chain->_height;

  // Create a single VkImage with all mip levels
  auto imageInfo   = makeVKICI(iwidth, iheight, 1, format, num_mips);
  imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  vktex->_imgobj   = std::make_shared<VulkanImageObject>(_contextVK, imageInfo, "imgmemcfclr");

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer("VkTextureInterface::_createFromLoadReq");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    auto& level         = chain->_levels[ilevel];
    int level_width     = level._width;
    int level_height    = level._height;
    auto level_data     = level._data->data(0);
    size_t level_length = level._data->length();

    // Transition the mip level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    auto barrier = createImageBarrier(
        vktex->_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkAccessFlagBits(0),
        VK_ACCESS_TRANSFER_WRITE_BIT);
    barrier->subresourceRange.baseMipLevel = ilevel;
    barrier->subresourceRange.levelCount   = 1;
    vkCmdPipelineBarrier(
        vk_cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier.get());

    // Copy the mip level data from the staging buffer to the image
    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "_createFromLoadReq");
    staging_buffer->copyFromHost(level_data, level_length);
    vktex->_staging_buffers.insert(staging_buffer);
    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, uint32_t(ilevel), 0, 1};
    region.imageExtent       = {uint32_t(level_width), uint32_t(level_height), 1};
    vkCmdCopyBufferToImage(
        vk_cmdbuf, staging_buffer->_vkbuffer, vktex->_imgobj->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    // Transition the mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        barrier.get());
  }
  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      vktex->_imgobj->_vkimage,                       //
      VkFormatConverter::convertBufferFormat(format), //
      VK_IMAGE_ASPECT_COLOR_BIT);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(vktex->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////
  // descriptor image info
  /////////////////////////////////////

  vktex->_vksampler                     = _contextVK->_sampler_per_maxlod[num_mips];
  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  /////////////////////////////////////

  _contextVK->onFenceCrossed([=]() {
    //vktex->_staging_buffers.clear();
    //vktex->_loadCB = nullptr;
  });

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
