////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/math/misc_math.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtg_texarray = logger()->configureChannel("VKRTGTEXARRAY", fvec3(0.5, 0.8, 0.2), false);
///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_buildRtgImplFromTextureArraySlice(rtgroup_rawptr_t rtgroup) {
  auto slice = rtgroup->_slice;
  int slice_index = slice->_slice;
  auto texarray = slice->_array;
  auto tex = texarray->_tex;
  int iw = texarray->_width;
  int ih = texarray->_height;
  
  OrkAssert(iw >= 8);
  OrkAssert(ih >= 8);
  OrkAssert(slice_index < texarray->_maxslices);
  
  logchan_rtg_texarray->log("Building RTG from texture array slice %d (w=%d h=%d)", slice_index, iw, ih);
  
  /////////////////////////////////////////////
  // Get or initialize texture object
  /////////////////////////////////////////////
  vktexobj_ptr_t vktex;
  if (auto as_vktex = tex->_impl.tryAsShared<VulkanTextureObject>()) {
    vktex = as_vktex.value();
  } else {
    // Initialize texture array if needed
    auto txi = _contextVK->_txi;
    txi->initTextureArray2D(texarray);
    vktex = tex->_impl.getShared<VulkanTextureObject>();
  }
  
  OrkAssert(vktex);
  OrkAssert(vktex->_imgobj);
  OrkAssert(vktex->_imgobj->_vkimage != VK_NULL_HANDLE);
  
  /////////////////////////////////////////////
  // Create RTG implementation
  /////////////////////////////////////////////
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(_contextVK);
  RTGIMPL->_width = iw;
  RTGIMPL->_height = ih;
  RTGIMPL->_pipeline_bits = 0;
  
  /////////////////////////////////////////////
  // Determine if depth or color
  /////////////////////////////////////////////
  bool is_depth = (texarray->_format == EBufferFormat::Z32F) || 
                  (texarray->_format == EBufferFormat::Z24S8);
  
  VkFormat vk_fmt = VkFormatConverter::convertBufferFormat(texarray->_format);
  
  /////////////////////////////////////////////
  // Create per-layer image view
  /////////////////////////////////////////////
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = vktex->_imgobj->_vkimage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Single layer 2D view
  viewInfo.format = vk_fmt;
  
  if (is_depth) {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (texarray->_format == EBufferFormat::Z24S8) {
      viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = slice_index; // Specific slice
  viewInfo.subresourceRange.layerCount = 1; // Single layer
  
  // Swizzle defaults
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  
  VkImageView slice_view;
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &slice_view);
  OrkAssert(VK_SUCCESS == ok);
  
  logchan_rtg_texarray->log("Created slice view %p for array layer %d", (void*)slice_view, slice_index);
  
  /////////////////////////////////////////////
  // Setup RTG for the slice
  /////////////////////////////////////////////
  if (is_depth) {
    // Create depth buffer impl for this slice
    uint64_t usage = "depth"_crcu;
    auto rtb = rtgroup->createDepthBuffer(texarray->_format, false);
    rtb->_mipgen = RtBuffer::EMipGen::EMG_NONE;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(), usage, vk_fmt);
    rtb->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_imgobj = vktex->_imgobj;
    
    // Store the slice view in the descriptor info
    bufferimpl->_descriptorInfo.imageView = slice_view;
    bufferimpl->_descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Setup attachment description
    bufferimpl->_attachmentDesc.format = vk_fmt;
    bufferimpl->_attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    bufferimpl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    bufferimpl->_attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    bufferimpl->_attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    bufferimpl->_attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    bufferimpl->_attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bufferimpl->_attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    RTGIMPL->_depth_buffer_impl = bufferimpl;
    
  } else {
    // Color attachment case
    uint64_t usage = "color"_crcu;
    auto rtb = rtgroup->createRenderTarget(texarray->_format, "arrayslice"_crcu, false);
    rtb->_mipgen = RtBuffer::EMipGen::EMG_NONE;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(), usage, vk_fmt);
    rtb->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_imgobj = vktex->_imgobj;
    
    // Store the slice view in the descriptor info
    bufferimpl->_descriptorInfo.imageView = slice_view;
    bufferimpl->_descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Setup attachment description
    bufferimpl->_attachmentDesc.format = vk_fmt;
    bufferimpl->_attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    bufferimpl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    bufferimpl->_attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    bufferimpl->_attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    bufferimpl->_attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    bufferimpl->_attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bufferimpl->_attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Setup attachment reference
    bufferimpl->_attachmentRef.attachment = 0;
    bufferimpl->_attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    RTGIMPL->_color_buffer_impls.push_back(bufferimpl);
  }
  
  // Assign the implementation to the rtgroup
  rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
  
  // Store reference to the slice for cleanup
  // Note: We store the slice view in a custom field that may need to be added to VkRtGroupImpl
  // For now, we're using the existing structure
  
  logchan_rtg_texarray->log("RTG from texture array slice created successfully");
  
  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////