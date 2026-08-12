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

VulkanRenderInfo::VulkanRenderInfo(VkRtGroupImpl* rtgi) {
  bool log = (rtgi->_rtgroup->_usage!="swapchain"_crcu);

  if(log){
    //OrkAssert(false);
  }

  initializeVkStruct(_renderinfo, VK_STRUCTURE_TYPE_RENDERING_INFO);
  _rainfos_color.clear();
  size_t num_image_buffers = rtgi->_color_buffer_impls.size();
  for (int i = 0; i < num_image_buffers; i++) {
    auto bufimpl = rtgi->_color_buffer_impls[i];
    auto vkfmt   = bufimpl->_vkfmt;
    VkRenderingAttachmentInfo rai;
    initializeVkStruct(rai, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    // The single-sample image view (descriptor slice if present, else the resolve-target _imgobj).
    VkImageView single_view = bufimpl->_descriptorInfo.imageView != VK_NULL_HANDLE
                              ? bufimpl->_descriptorInfo.imageView
                              : bufimpl->_imgobj->_vkimageview;
    if (bufimpl->_msaa_imgobj) {
      // MSAA: render INTO the multisample image, resolve DOWN to the single-sample image at
      // endRendering (the resolve is free; everything downstream samples the resolved _imgobj).
      rai.imageView          = bufimpl->_msaa_imgobj->_vkimageview;
      rai.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      rai.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
      rai.resolveImageView   = single_view;
      rai.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
      rai.imageView   = single_view;
      rai.imageLayout = bufimpl->_currentLayout;
      rai.resolveMode = VK_RESOLVE_MODE_NONE;
    }
    rai.loadOp           = rtgi->_autoclear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    rai.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    auto cc = bufimpl->_clear_color;
    // Debug logging for filtered environment maps
    rai.clearValue.color = {{cc.x,cc.y,cc.z,cc.w}};
    _rainfos_color.push_back(rai);
  }
  // viewMask and layerCount are DIFFERENTLY SCOPED, not two spellings of the same number:
  //  a non-zero viewMask makes the pass multiview (one bit per view, driver broadcasts the
  //  draws across layers); layerCount is the NON-multiview layered-rendering field and is
  //  ignored whenever viewMask != 0. Setting them to match is the classic multiview bug.
  _renderinfo.viewMask                 = rtgi->_rtgroup ? rtgi->_rtgroup->viewMask() : 0;
  _renderinfo.layerCount               = 1;
  _renderinfo.flags                    = VkRenderingFlags();
  _renderinfo.renderArea.offset.x      = 0;
  _renderinfo.renderArea.offset.y      = 0;
  _renderinfo.renderArea.extent.width  = rtgi->_width;
  _renderinfo.renderArea.extent.height = rtgi->_height;
  _renderinfo.colorAttachmentCount     = _rainfos_color.size();
  _renderinfo.pColorAttachments        = _rainfos_color.data();
  _renderinfo.pStencilAttachment       = nullptr;

  auto dbuf_impl = rtgi->_depth_buffer_impl;
  if (dbuf_impl) {
    initializeVkStruct(_rainfo_depth, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    // Single-sample depth view (descriptor slice if present, else the resolve-target _imgobj).
    VkImageView single_depth = dbuf_impl->_descriptorInfo.imageView != VK_NULL_HANDLE //
                             ? dbuf_impl->_descriptorInfo.imageView //
                             : dbuf_impl->_imgobj->_vkimageview; //
    if (dbuf_impl->_msaa_imgobj) {
      // MSAA depth: depth-test against the multisample depth. Resolve to the single-sample copy
      // ONLY in a depth-WRITE pass (the prepass) so DEPTH_MAP samplers (SSAO/water) read a valid
      // single-sample depth; in the read-only color pass we don't re-resolve (prepass already did).
      _rainfo_depth.imageView   = dbuf_impl->_msaa_imgobj->_vkimageview;
      _rainfo_depth.imageLayout = dbuf_impl->_currentLayout;
      if (rtgi->_needsManualDepthResolve()) {
        // MULTIVIEW: this resolve names ONE resolve-target subresource for a pass covering
        // SEVERAL views, and what each view's share of it receives is left to the
        // implementation — Metal (so MoltenVK, whose instanced multiview is a single Metal
        // pass) can only resolve one array slice, and measured here it resolved NEITHER
        // eye: the single-sample copy every sampler reads stayed at the far clear. So this
        // pass resolves nothing and VkRtGroupImpl::_resolveMultiviewDepth does all the
        // layers explicitly once it ends. Uniform across drivers on purpose: a resolve
        // that works only where the driver happens to loop over views is not one we can
        // reason about.
        _rainfo_depth.resolveMode = VK_RESOLVE_MODE_NONE;
      } else if (not rtgi->_depthReadOnlyMode) {
        _rainfo_depth.resolveMode        = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        _rainfo_depth.resolveImageView   = single_depth;
        _rainfo_depth.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
      } else {
        _rainfo_depth.resolveMode = VK_RESOLVE_MODE_NONE;
      }
    } else {
      _rainfo_depth.imageView   = single_depth;
      _rainfo_depth.imageLayout = dbuf_impl->_currentLayout;
      _rainfo_depth.resolveMode = VK_RESOLVE_MODE_NONE;
    }
    // A read-only-depth pass may not clear: the clear IS a depth write, and it
    // would discard the very depth the pass exists to read (the prepass result
    // that fills this attachment, plus the DEPTH_MAP the pass samples). Only
    // colour keeps honouring _autoclear there.
    bool depth_clear                            = rtgi->_autoclear and not rtgi->_depthReadOnlyMode;
    _rainfo_depth.loadOp                        = depth_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
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

  // Attachment formats come from the IMPL (the buffers the render pass ACTUALLY
  // attaches — see VulkanRenderInfo), NOT the ork-level RtGroup: texture-array
  // slice RTGs (shadow cascades / spot cookies) carry no ork-level buffers, so
  // reading rtg->_depthBuffer here produced depthAttachmentFormat=UNDEFINED
  // for pipelines rendering into a real Z32F pass — a dynamic-rendering
  // mismatch (UB) under which the driver's depth path bypassed the fragment
  // shader entirely (masked depth-prepass discard was silently dropped, A3).
  auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();
  OrkAssert(rtg_impl != nullptr);

  initializeVkStruct(_createInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
  for (auto& cbi : rtg_impl->_color_buffer_impls) {
    _colorFormats.push_back(cbi->_vkfmt);
  }

  _createInfo.colorAttachmentCount    = _colorFormats.size();
  _createInfo.pColorAttachmentFormats = _colorFormats.empty() ? nullptr : _colorFormats.data();
  // must agree with VulkanRenderInfo's viewMask for every pass this pipeline is used in —
  //  a mismatch is invalid at draw time, not at pipeline creation.
  _createInfo.viewMask                = rtg->viewMask();

  if (rtg_impl->_depth_buffer_impl) {
    _depthFormat                      = rtg_impl->_depth_buffer_impl->_vkfmt;
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
