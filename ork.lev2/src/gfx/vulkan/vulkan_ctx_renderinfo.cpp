////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VulkanRenderInfo::VulkanRenderInfo(VkRtGroupImpl* rtg) {
  initializeVkStruct(_renderinfo, VK_STRUCTURE_TYPE_RENDERING_INFO);
  _rainfos_color.clear();
  size_t num_image_buffers = rtg->_color_buffer_impls.size();
  for (int i = 0; i < num_image_buffers; i++) {
    auto bufimpl = rtg->_color_buffer_impls[i];
    auto vkfmt   = bufimpl->_vkfmt;
    VkRenderingAttachmentInfo rai;
    initializeVkStruct(rai, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    // Use slice view from descriptor if available, otherwise use image view
    rai.imageView   = bufimpl->_descriptorInfo.imageView != VK_NULL_HANDLE 
                      ? bufimpl->_descriptorInfo.imageView 
                      : bufimpl->_imgobj->_vkimageview;
    rai.imageLayout = bufimpl->_currentLayout;
    rai.resolveMode = VK_RESOLVE_MODE_NONE;
    // rai.resolveImageView = VkImageView();
    // rai.resolveImageLayout = VkImageLayout();
    rai.loadOp           = rtg->_autoclear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    rai.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    auto cc = bufimpl->_clear_color;
    // Debug logging for filtered environment maps
    rai.clearValue.color = {{cc.x,cc.y,cc.z,cc.w}};
    _rainfos_color.push_back(rai);
  }
  _renderinfo.viewMask                 = 0;
  _renderinfo.layerCount               = 1;
  _renderinfo.flags                    = VkRenderingFlags();
  _renderinfo.renderArea.offset.x      = 0;
  _renderinfo.renderArea.offset.y      = 0;
  _renderinfo.renderArea.extent.width  = rtg->_width;
  _renderinfo.renderArea.extent.height = rtg->_height;
  _renderinfo.colorAttachmentCount     = _rainfos_color.size();
  _renderinfo.pColorAttachments        = _rainfos_color.data();
  _renderinfo.pStencilAttachment       = nullptr;

  //printf("rtg->_width<%d> rtg->_height<%d>\n", rtg->_width, rtg->_height);
  auto dbuf_impl                       = rtg->_depth_buffer_impl;
  if (dbuf_impl) {
    initializeVkStruct(_rainfo_depth, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    // Use slice view from descriptor if available, otherwise use image view
    _rainfo_depth.imageView   = dbuf_impl->_descriptorInfo.imageView != VK_NULL_HANDLE 
                                ? dbuf_impl->_descriptorInfo.imageView 
                                : dbuf_impl->_imgobj->_vkimageview;
    _rainfo_depth.imageLayout = dbuf_impl->_currentLayout;
    _rainfo_depth.resolveMode = VK_RESOLVE_MODE_NONE;
    //_rainfo_depth.resolveImageView = VkImageView();
    //_rainfo_depth.resolveImageLayout = VkImageLayout();
    _rainfo_depth.loadOp                        = rtg->_autoclear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    _rainfo_depth.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    _rainfo_depth.clearValue.depthStencil.depth = 1.0f;
    _rainfo_depth.clearValue.depthStencil.stencil = 0;
    _renderinfo.pDepthAttachment                = &_rainfo_depth;
  }
}

VulkanRenderInfo::~VulkanRenderInfo() {
}

///////////////////////////////////////////////////

VulkanPipelineRenderInfo::VulkanPipelineRenderInfo(rtgroup_rawptr_t rtg)
    : _rtg(rtg) {

  initializeVkStruct(_createInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
  for (int i = 0; i < rtg->numImageBuffers(); i++) {
    auto buf     = rtg->buffer(i);
    auto fmt = VkFormatConverter::convertBufferFormat(buf->format());
    _colorFormats.push_back(fmt);
  }

  _createInfo.colorAttachmentCount    = _colorFormats.size();
  _createInfo.pColorAttachmentFormats = _colorFormats.empty() ? nullptr : _colorFormats.data();

  if (rtg->_depthBuffer) {
    _depthFormat                      = VkFormatConverter::convertBufferFormat(rtg->_depthBuffer->format());
    _createInfo.depthAttachmentFormat = _depthFormat;

    // Check if format has stencil
    if (_depthFormat == VK_FORMAT_D24_UNORM_S8_UINT || _depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
      _createInfo.stencilAttachmentFormat = _depthFormat;
    }
  }
}

VulkanPipelineRenderInfo::~VulkanPipelineRenderInfo() {
}

///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
