////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>
#include <ork/lev2/gfx/gfxenv.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgi = logger()->configureChannel("VKRTGI", fvec3(0.8, 0.2, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::VklRtBufferImpl(vkcontext_rawptr_t ctxVK, VkRtGroupImpl* par, uint64_t usage, VkFormat fmt) //
    : _contextVK(ctxVK)
    , _rtg_impl(par)
    , _usage(usage)
    , _vkfmt(fmt) { //

  logchan_rtgi->log("VklRtBufferImpl constructor - usage=0x%zx (%zu)", _usage, _usage);

  initializeVkStruct(_attachmentDesc);

  _attachmentDesc.samples       = VK_SAMPLE_COUNT_1_BIT;        // No multisampling for this example.
  _attachmentDesc.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear the color/depth buffer before rendering.
  _attachmentDesc.storeOp       = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered color/depth for presentation.
  _attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  _attachmentDesc.finalLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
  switch (_vkfmt) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    default:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't care about stencil.
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
  }

  _attachmentDesc.format = _vkfmt;

}

///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::~VklRtBufferImpl() {
  // Capture resources that need cleanup
  auto imgobj = _imgobj;
  
  if (imgobj) {
    // Enqueue cleanup to main thread with proper Vulkan context
    GfxEnv::GetRef().enqueueDeferredContextOp(
      [=](Context* ctx) {
        // Resources will be released when shared_ptr goes out of scope
        // This ensures it happens on the main thread with valid Vulkan context
        auto temp_img = imgobj;
        _teximpl.clear(); // Clear the texture implementation variant
      });
  }
  
  _imgobj = nullptr; // Clear the image object to avoid dangling pointers
}

///////////////////////////////////////////////////////////////////////////////

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
  auto vk_fmt = VkFormatConverter::convertBufferFormat(ork_fmt);
  VkRtbCreateOption options;
  options._format = vk_fmt;
  options._usage = usage;
  options._with_texture = false; // No texture for this buffer
  _vkCreateImageForBuffer(ctxVK, bufferimpl, options);
}

///////////////////////////////////////////////////////////////////////////////

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    VkRtbCreateOption options) {               //

    auto old_imgobj = bufferimpl->_imgobj;

    auto VKICI = makeVKICI(           //
      bufferimpl->_rtg_impl->_width,  // width
      bufferimpl->_rtg_impl->_height, // height
      1,                              // depth
      options._format,                // format
      1);                             // miplevels
  
  logchan_rtgi->log("_vkCreateImageForBuffer: usage=0x%zx (%zu) format=%d", options._usage, options._usage, options._format);
  
  // Defensive check: convert usage=0 to "color"_crcu
  uint64_t effective_usage = options._usage;
  if (effective_usage == 0) {
    logchan_rtgi->log("WARNING: _vkCreateImageForBuffer received usage=0, defaulting to 'color'");
    effective_usage = "color"_crcu;
    // Also update the buffer's usage to the corrected value
    bufferimpl->_usage = effective_usage;
  }
  
  switch (effective_usage) {
    case "depth"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; // Allow rendering D/S to this image
      break;
    case "color"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Allow rendering Color to this image      
      break;
    case "swapchain"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Allow rendering Color to this image
      break;
    default:
      logchan_rtgi->log("ERROR: Unknown usage value 0x%zx in _vkCreateImageForBuffer", effective_usage);
      OrkAssert(false);
      break;
  }
  if(options._with_texture) {
    // Use as texture
    VKICI->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;       // Allow sampling from this image
    VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Allow data transfer to it
    VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // Allow data transfer from it (for readback/save)
  }
  ///////////////////////////////////////////////////
  auto imgobj = std::make_shared<VulkanImageObject>(ctxVK, VKICI);
  auto& vkimage       = imgobj->_vkimage;
  bufferimpl->_imgobj = imgobj;
  ///////////////////////////////////////////////////
  auto IVCI = createImageViewInfo2D(
      vkimage,            //
      bufferimpl->_vkfmt, //
      VkFormatConverter::_instance.aspectForUsage(effective_usage));
  VkResult OK = vkCreateImageView(ctxVK->_vkdevice, IVCI.get(), nullptr, &imgobj->_vkimageview);
  OrkAssert(OK == VK_SUCCESS);
  bufferimpl->_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Reset layout to undefined after creation
  ///////////////////////////////////////////////////
  logchan_rtgi->log("IMAGE: Created image %p, initial layout %d", (void*)vkimage, bufferimpl->_currentLayout);
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_replaceImage(vkimageobj_ptr_t imgobj) { //
  _imgobj = imgobj;
  _vkfmt = imgobj->_format;
  _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
  _rtg_impl->_invalidateAttachments();
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

    VkImage img = _imgobj->_vkimage;
    OrkAssert(img != VK_NULL_HANDLE);
    
    logchan_rtgi->log("IMAGE: Transition requested for image %p: current layout %d, target layout %d, CB %p", (void*)img, _currentLayout, p.layout, (void*)cb->_vkcmdbuf);
    if (_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED || _currentLayout != p.layout) {
      logchan_rtgi->log("IMAGE: Performing transition for image %p from %d to %d", (void*)img, _currentLayout, p.layout);
    auto barrier = createImageBarrier(img, _currentLayout, p.layout, p.srcAccess, p.dstAccess);
    barrier->subresourceRange.aspectMask = VkFormatConverter::_instance.aspectForUsage(_usage);
    
    vkCmdPipelineBarrier(cb->_vkcmdbuf,                 // command buffer
                         p.srcStage,                    // source stage
                         p.dstStage,                    // destination stage    
                         VK_DEPENDENCY_BY_REGION_BIT,   // dependency flags
                         0, nullptr,                    // memory barriers
                         0, nullptr,                    // buffer memory barriers
                         1, barrier.get());             // image memory barriers

    setLayout(p.layout);
      logchan_rtgi->log("IMAGE: Transition complete for image %p, new layout %d", (void*)img, _currentLayout);
    } else {
      logchan_rtgi->log("IMAGE: Skipping transition for image %p, already in layout %d", (void*)img, _currentLayout);
    }
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb) { //
  switch( _usage) {
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
  switch( _usage) {
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
  switch(_usage) {
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

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_transitionToPresent(vkpricmdbufimpl_ptr_t cb) {
  OrkAssert(_usage == "swapchain"_crcu);
  auto new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  auto imgbar = createImageBarrier(
      _imgobj->_vkimage,                    // image
      _currentLayout,                       // oldLayout (dont care)
      new_layout,                           // newLayout
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
      VK_ACCESS_MEMORY_READ_BIT);           // dstAccessMask

  vkCmdPipelineBarrier(
      cb->_vkcmdbuf,                                 // cmdbuf
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
