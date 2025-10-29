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
static logchannel_ptr_t logchan_rtbi = logger()->configureChannel("VKRTBI", fvec3(0.8, 0.2, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::VklRtBufferImpl(vkcontext_rawptr_t ctxVK, VkRtGroupImpl* par, uint64_t usage, VkFormat fmt) //
    : _contextVK(ctxVK)
    , _rtg_impl(par)
    , _usage(usage)
    , _vkfmt(fmt) { //

  //logchan_rtbi->log("VklRtBufferImpl constructor - usage=0x%zx (%zu)", _usage, _usage);

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
  vkimageobj_ptr_t imgobj = _imgobj;
  vktexobj_ptr_t impl = nullptr;
  if (_teximpl.tryAsShared<VulkanTextureObject>()) {
    impl = _teximpl.getShared<VulkanTextureObject>();
  }
  _imgobj = nullptr; // Clear the image object to avoid dangling pointers
  _teximpl.clear(); // Clear the texture implementation variant

  if (imgobj or impl) {
    // Enqueue cleanup to main thread with proper Vulkan context
    GfxEnv::GetRef().enqueueDeferredContextOp(
      [=](Context* ctx) mutable {
        imgobj = nullptr;
        impl = nullptr;
      });
  }  
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::setLayout(VkImageLayout layout) {
  auto previousLayout           = _attachmentDesc.finalLayout;
  _currentLayout                = layout;
  _attachmentDesc.initialLayout = previousLayout;
  _attachmentDesc.finalLayout   = layout;
  OrkAssert(_rtg_impl);
  _rtg_impl->__attachments = nullptr;

  // Sync layout to the image object so we can check it during texture binding
  if (_imgobj) {
    _imgobj->_currentLayout = layout;
  }

  // Also update the associated texture's descriptor if it exists
  if (_teximpl.tryAsShared<VulkanTextureObject>()) {
    auto tex_impl = _teximpl.getShared<VulkanTextureObject>();
    tex_impl->_vkdescriptor_info.imageLayout = layout;
  }
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
    int w = bufferimpl->_rtg_impl->_width;
    int h = bufferimpl->_rtg_impl->_height;

    auto old_imgobj = bufferimpl->_imgobj;

    auto VKICI = makeVKICI(           //
      w,  // width
      h, // height
      1,                              // depth
      options._format,                // format
      1);                             // miplevels
  
  
  // Defensive check: convert usage=0 to "color"_crcu
  uint64_t effective_usage = options._usage;
  if (effective_usage == 0) {
    //logchan_rtbi->log("WARNING: _vkCreateImageForBuffer received usage=0, defaulting to 'color'");
    effective_usage = "color"_crcu;
    // Also update the buffer's usage to the corrected value
    bufferimpl->_usage = effective_usage;
  }


  switch (effective_usage) {
    case "depth"_crcu:
      //logchan_rtbi->log("_vkCreateImageForBuffer: DEPTH format=%d wh<%d %d>", options._format, w, h);
      VKICI->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; // Allow rendering D/S to this image
      break;
    case "color"_crcu:
      //logchan_rtbi->log("_vkCreateImageForBuffer: COLOR format=%d wh<%d %d>", options._format, w, h);
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Allow rendering Color to this image      
      break;
    case "swapchain"_crcu:
      //logchan_rtbi->log("_vkCreateImageForBuffer: SWAPCHAIN format=%d wh<%d %d>", options._format, w, h);
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Allow rendering Color to this image
      break;
    default:
      logchan_rtbi->log("ERROR: Unknown usage value 0x%zx in _vkCreateImageForBuffer", effective_usage);
      OrkAssert(false);
      break;
  }
  if(options._with_texture) {
    // Use as texture
    VKICI->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;       // Allow sampling from this image
    VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Allow data transfer to it
  }
  // Always allow readback for pixel capture
  VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // Allow data transfer from it (for readback/capture)
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
  imgobj->_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Also set on the image object
  ///////////////////////////////////////////////////
  //logchan_rtbi->log("IMAGE: Created image %p, initial layout %d", (void*)vkimage, bufferimpl->_currentLayout);
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_replaceImage(vkimageobj_ptr_t imgobj) { //
  _imgobj = imgobj;
  _vkfmt = imgobj->_format;
  _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (_imgobj) {
    _imgobj->_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }
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
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,            // srcStage (wait for ALL graphics work including tile flush on TBDR/Metal)
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT  // dstStage
};

static constexpr VkTransitionParams kToTextureDepth = {
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,      // layout
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // srcAccess
    VK_ACCESS_SHADER_READ_BIT,                     // dstAccess
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,  // srcStage
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT  // dstStage (conservative for MoltenVK)
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

    // CRITICAL: Cannot call vkCmdPipelineBarrier inside dynamic rendering on MoltenVK
    // Skip transitions if we're inside an active render pass - layout will be managed by dynamic rendering
    if(0)logchan_rtbi->log("IMAGE TRANSITION CHECK: img=%p, layout %d->%d, renderPassActive=%d",
                      (void*)img, _currentLayout, p.layout, _contextVK->_renderPassActive);

    if (_contextVK->_renderPassActive) {
      if(0)logchan_rtbi->log("IMAGE: Skipping transition for image %p while render pass active (layout %d -> %d)",
                        (void*)img, _currentLayout, p.layout);
      // Update our tracking to match what dynamic rendering expects
      setLayout(p.layout);
      return;
    }

    if(0)logchan_rtbi->log("IMAGE: Transition requested for image %p: current layout %d, target layout %d, CB %p", (void*)img, _currentLayout, p.layout, (void*)cb->_vkcmdbuf);
    if (_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED || _currentLayout != p.layout) {
    if(0)logchan_rtbi->log("IMAGE: Performing transition for image %p from %d to %d", (void*)img, _currentLayout, p.layout);
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
    if(0)logchan_rtbi->log("IMAGE: Transition complete for image %p, new layout %d", (void*)img, _currentLayout);
    } else {
      if(0)logchan_rtbi->log("IMAGE: Skipping transition for image %p, already in layout %d", (void*)img, _currentLayout);
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
  if(0)printf("VklRtBufferImpl::_transitionToTexture: current layout = %d (UNDEFINED=%d, COLOR_ATTACH=%d, SHADER_READ=%d)\n",
         _currentLayout, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  // If image is still undefined, we need different transition params
  if (_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
    VkTransitionParams params;
    params.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    params.srcAccess = VkAccessFlagBits(0); // No prior access from UNDEFINED
    params.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    params.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Start of pipeline for UNDEFINED
    params.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    _transitionImage(cb, params);
  } else {
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
