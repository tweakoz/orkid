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
    
    _createInfo.colorAttachmentCount = colorFormats.size();
    _createInfo.pColorAttachmentFormats = colorFormats.data();
    
    if (rtg->_depthBuffer) {
      auto depthFmt = VkFormatConverter::convertBufferFormat(rtg->_depthBuffer->format());
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
