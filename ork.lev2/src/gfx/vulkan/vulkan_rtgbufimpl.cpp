
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
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgi = logger()->createChannel("VKRTGI", fvec3(0.8, 0.2, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::VklRtBufferImpl(VkRtGroupImpl* par, RtBuffer* rtb) //
    : _rtg_impl(par)
    , _rtb(rtb) { //

  initializeVkStruct(_attachmentDesc);
  initializeVkStruct(_vkimgview);

  _attachmentDesc.samples       = VK_SAMPLE_COUNT_1_BIT;        // No multisampling for this example.
  _attachmentDesc.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear the color/depth buffer before rendering.
  _attachmentDesc.storeOp       = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered color/depth for presentation.
  _attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  _attachmentDesc.finalLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
  switch (rtb->format()) {
    case EBufferFormat::DEPTH:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    default:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't care about stencil.
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
  }

  _vkfmt                 = VkFormatConverter::convertBufferFormat(rtb->format());
  _attachmentDesc.format = _vkfmt;

}
void VklRtBufferImpl::setLayout(VkImageLayout layout) {
  auto previousLayout           = _attachmentDesc.finalLayout;
  _currentLayout                = layout;
  _attachmentDesc.initialLayout = previousLayout;
  _attachmentDesc.finalLayout   = layout;
  OrkAssert(_rtg_impl);
  _rtg_impl->__attachments = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    EBufferFormat ork_fmt,
    uint64_t usage) {               //
  auto VKICI = makeVKICI(           //
      bufferimpl->_rtg_impl->_width,  // width
      bufferimpl->_rtg_impl->_height, // height
      1,                            // depth
      ork_fmt,                      // format
      1);                           // miplevels
  switch (usage) {
    case "depth"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      break;
    case "color"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      // Use as texture and allow data transfer to it
      VKICI->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      break;
    case "swapchain"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      break;
    default:
      OrkAssert(false);
      break;
  }
  ///////////////////////////////////////////////////
  bufferimpl->_imgobj = std::make_shared<VulkanImageObject>(ctxVK, VKICI);
  auto& vkimage       = bufferimpl->_imgobj->_vkimage;
  bufferimpl->_vkimg  = vkimage;
  ///////////////////////////////////////////////////
  auto IVCI = createImageViewInfo2D(
      vkimage,            //
      bufferimpl->_vkfmt, //
      VkFormatConverter::_instance.aspectForUsage(usage));
  VkResult OK = vkCreateImageView(ctxVK->_vkdevice, IVCI.get(), nullptr, &bufferimpl->_vkimgview);
  OrkAssert(OK == VK_SUCCESS);
  ///////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_replaceImage(
    VkFormat new_fmt,     //
    VkImageView new_view, //
    VkImage new_img) {    //

  //auto old_img  = _imgobj->_vkimage;
  //auto old_view = _vkimgview;

  ////////////////////
  // delete old image
  ////////////////////

  // todo

  ////////////////////
  // assign new image
  ////////////////////

  _init      = false;
  _vkimg     = new_img;
  _vkimgview = new_view;
  _vkfmt     = new_fmt;
}

///////////////////////////////////////////////////////////////////////////////

static constexpr VkTransitionParams kToRenderTargetColor = {
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,      // layout
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccess
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // dstAccess
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT  // dstStage
};
static constexpr VkTransitionParams kToRenderTargetDepth = {
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,      // layout
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // srcAccess
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // dstAccess
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,    // srcStage
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT     // dstStage
};

static constexpr VkTransitionParams kToTextureColor = {
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,      // layout
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccess
    VK_ACCESS_SHADER_READ_BIT,                     // dstAccess  
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage 
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT          // dstStage
};

static constexpr VkTransitionParams kToTextureDepth = {
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,      // layout
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // srcAccess
    VK_ACCESS_SHADER_READ_BIT,                     // dstAccess  
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,    // srcStage
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT          // dstStage
};

static constexpr VkTransitionParams kToHostReadColor = {
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,          // layout
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccess
    VK_ACCESS_TRANSFER_READ_BIT,                   // dstAccess
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
    VK_PIPELINE_STAGE_TRANSFER_BIT                 // dstStage
};
static constexpr VkTransitionParams kToHostReadDepth = {
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,          // layout
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // srcAccess
    VK_ACCESS_TRANSFER_READ_BIT,                   // dstAccess
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,    // srcStage
    VK_PIPELINE_STAGE_TRANSFER_BIT                 // dstStage
};

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionImage(vkpricmdbufimpl_ptr_t cb, const VkTransitionParams& p) {

    VkImage img = _imgobj ? _imgobj->_vkimage : _vkimg;
    OrkAssert(img != VK_NULL_HANDLE);
    
    auto barrier = createImageBarrier(img, _currentLayout, p.layout, p.srcAccess, p.dstAccess);
    barrier->subresourceRange.aspectMask = VkFormatConverter::_instance.aspectForUsage(_rtb->_usage);
    
    vkCmdPipelineBarrier(cb->_vkcmdbuf,                 // command buffer
                         p.srcStage,                    // source stage
                         p.dstStage,                    // destination stage    
                         VK_DEPENDENCY_BY_REGION_BIT,   // dependency flags
                         0, nullptr,                    // memory barriers
                         0, nullptr,                    // buffer memory barriers
                         1, barrier.get());             // image memory barriers

    setLayout(p.layout);
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb) { //
  switch( _rtb->_usage) {
    case "color"_crcu: // color attachment
      _transitionImage(cb, kToRenderTargetColor);
      break;
    case "depth"_crcu: // depth attachment
      _transitionImage(cb, kToRenderTargetDepth);
      break;
    case "swapchain"_crcu: // present attachment
      _transitionImage(cb, kToRenderTargetColor);
      break;
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionToTexture(vkpricmdbufimpl_ptr_t cb)      { //
  switch( _rtb->_usage) {
    case "color"_crcu: // color attachment
      _transitionImage(cb, kToTextureColor);
      break;
    case "depth"_crcu: // depth attachment
      _transitionImage(cb, kToTextureDepth);
      break;
    case "swapchain"_crcu: // present attachment
      OrkAssert(false); // swapchain should not be used as a texture
      break;
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionToHostRead(vkpricmdbufimpl_ptr_t cb)     { //
  switch( _rtb->_usage) {
    case "color"_crcu: // color attachment
      _transitionImage(cb, kToHostReadColor);
      break;
    case "depth"_crcu: // depth attachment
      _transitionImage(cb, kToHostReadDepth);
      break;
    case "swapchain"_crcu: // present attachment
      _transitionImage(cb, kToHostReadColor);
      break;
    default:
      OrkAssert(false);
      break;
  }
}

void VklRtBufferImpl::_transitionToPresent(vkpricmdbufimpl_ptr_t cb) {
  OrkAssert(_rtb->_usage == "swapchain"_crcu);
  auto new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  auto imgbar = createImageBarrier(
      _vkimg,
      _currentLayout,            // oldLayout (dont care)
      new_layout,                           // newLayout
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
      VK_ACCESS_MEMORY_READ_BIT);           // dstAccessMask

  vkCmdPipelineBarrier(
      cb->_vkcmdbuf,           // cmdbuf
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // dstStageMask
      0,                                             // dependencyFlags
      0,
      nullptr, // memoryBarrierCount, pMemoryBarriers
      0,
      nullptr, // bufferMemoryBarrierCount, pBufferMemoryBarriers
      1,
      imgbar.get()); // imageMemoryBarrierCount, pImageMemoryBarriers

  setLayout(new_layout);
}

  ///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
