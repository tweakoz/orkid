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

VulkanRenderInfo::VulkanRenderInfo(rtgroup_rawptr_t rtg)
    : _rtg(rtg) {
  initializeVkStruct(_renderinfo, VK_STRUCTURE_TYPE_RENDERING_INFO);
  _rainfos_color.clear();
  size_t num_image_buffers = rtg->numImageBuffers();
  for (int i = 0; i < num_image_buffers; i++) {
    auto buf     = rtg->buffer(i);
    auto vkfmt   = VkFormatConverter::convertBufferFormat(buf->format());
    auto bufimpl = buf->_impl.getShared<VklRtBufferImpl>();
    VkRenderingAttachmentInfo rai;
    initializeVkStruct(rai, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    rai.imageView   = bufimpl->_vkimgview;
    rai.imageLayout = bufimpl->_currentLayout;
    rai.resolveMode = VK_RESOLVE_MODE_NONE;
    // rai.resolveImageView = VkImageView();
    // rai.resolveImageLayout = VkImageLayout();
    bool clear = buf->_autoclear;
    rai.loadOp           = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    rai.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    auto cc = buf->_clearColor;
    rai.clearValue.color = {{cc.x,cc.y,cc.z,cc.w}};
    _rainfos_color.push_back(rai);
  }
  auto dbuf                            = rtg->_depthBuffer;
  _renderinfo.viewMask                 = 0;
  _renderinfo.layerCount               = 1;
  _renderinfo.flags                    = VkRenderingFlags();
  _renderinfo.renderArea.offset.x      = 0;
  _renderinfo.renderArea.offset.y      = 0;
  _renderinfo.renderArea.extent.width  = rtg->miW;
  _renderinfo.renderArea.extent.height = rtg->miH;
  _renderinfo.colorAttachmentCount     = _rainfos_color.size();
  _renderinfo.pColorAttachments        = _rainfos_color.data();
  _renderinfo.pStencilAttachment       = nullptr;

  if (dbuf) {
    auto dbuf_impl = dbuf->_impl.getShared<VklRtBufferImpl>();
    initializeVkStruct(_rainfo_depth, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    _rainfo_depth.imageView   = dbuf_impl->_vkimgview;
    _rainfo_depth.imageLayout = dbuf_impl->_currentLayout;
    _rainfo_depth.resolveMode = VK_RESOLVE_MODE_NONE;
    //_rainfo_depth.resolveImageView = VkImageView();
    //_rainfo_depth.resolveImageLayout = VkImageLayout();
    _rainfo_depth.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    _rainfo_depth.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    _rainfo_depth.clearValue.depthStencil.depth = 0.0f;
    _renderinfo.pDepthAttachment                = &_rainfo_depth;
  }
}

VulkanRenderInfo::~VulkanRenderInfo() {
}

///////////////////////////////////////////////////

VulkanPipelineRenderInfo::VulkanPipelineRenderInfo(rtgroup_rawptr_t rtg)
    : _rtg(rtg) {

  initializeVkStruct(_createInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
  std::vector<VkFormat> colorFormats;
  for (int i = 0; i < rtg->numImageBuffers(); i++) {
    auto fmt = VkFormatConverter::convertBufferFormat(rtg->buffer(i)->format());
    colorFormats.push_back(fmt);
  }

  _createInfo.colorAttachmentCount    = colorFormats.size();
  _createInfo.pColorAttachmentFormats = colorFormats.data();

  if (rtg->_depthBuffer) {
    auto depthFmt                     = VkFormatConverter::convertBufferFormat(rtg->_depthBuffer->format());
    _createInfo.depthAttachmentFormat = depthFmt;

    // Check if format has stencil
    if (depthFmt == VK_FORMAT_D24_UNORM_S8_UINT || depthFmt == VK_FORMAT_D32_SFLOAT_S8_UINT) {
      _createInfo.stencilAttachmentFormat = depthFmt;
    }
  }
}

VulkanPipelineRenderInfo::~VulkanPipelineRenderInfo() {
}

///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
