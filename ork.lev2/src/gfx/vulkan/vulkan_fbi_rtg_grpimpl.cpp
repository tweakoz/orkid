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


VkRtGroupImpl::VkRtGroupImpl(vkcontext_rawptr_t ctxVK, rtgroup_rawptr_t rtgroup)
    : _contextVK(ctxVK)
    , _rtgroup(rtgroup) {
  std::string name = "rtg";
  _cmdbufRTG = std::make_shared<SecondaryCommandBuffer>();
  _cmdbufRTG->_debugName = name;
  auto vkcmdbuf = _contextVK->_createSecondaryVkCommandBuffer(_cmdbufRTG.get());
  _contextVK->_setObjectDebugName(vkcmdbuf->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, name.c_str());
  initializeVkStruct(_cmdBufCBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  _cmdBufCBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  initializeVkStruct(_cmdBufII, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  _cmdBufCBBI_GFX.pInheritanceInfo = &_cmdBufII;
}
VkRtGroupImpl::~VkRtGroupImpl() {
  // Capture resources that need cleanup
  auto cmdbuf = _cmdbufRTG;
  auto color_buffers = _color_buffer_impls;
  auto depth_buffer = _depth_buffer_impl;
  auto attachments = __attachments;
  
  if (!color_buffers.empty() || depth_buffer || cmdbuf || attachments) {
    // FRAME-DELAYED destroy, same hazard as ~VklRtBufferImpl: a resize drops
    // this group while its secondary command buffer may still be recorded into
    // an in-flight primary (vkFreeCommandBuffers is a use-after-submit at the
    // next beginFrame), and releasing the buffer impls here is what fires their
    // own delayed teardown. Cleanup still runs on the context-owning thread —
    // the delayed queue is drained from beginFrame too, just N frames later.
    constexpr int kDelayFrames = 3; // > MAX_FRAMES_IN_FLIGHT
    _contextVK->enqueueDelayedDestroy(
      [=]() {
        // Release shared_ptrs - their destructors will handle cleanup
        auto temp_colors = color_buffers;
        auto temp_depth = depth_buffer;
        auto temp_cmd = cmdbuf;
        auto temp_attach = attachments;
      },
      kDelayFrames);
  }
  
  _cmdbufRTG = nullptr; // Clear the command buffer to avoid dangling pointers
}
///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_updateClearParams(::ork::lev2::rtgroup_rawptr_t rtg) {
  _autoclear = rtg->_autoclear;
  for(int i = 0; i < rtg->numImageBuffers(); i++) {
    auto rtb      = rtg->buffer(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_clear_color = rtb->_clearColor;
  }
}

///////////////////////////////////////////////////////

void VkRtGroupImpl::_updateMainSurface(VkFrameBufferInterface* fbi) {
  int w = fbi->_output->_width;
  int h = fbi->_output->_height;
  if ( (_width != w) or (_height != h) ) {
    logchan_rtgi->log("resize main surface to w<%d> h<%d>", w, h);
    _width  = w;
    _height = h;
    // Invalidate cached render info since size changed
    _rinfo_retain = nullptr;
    _rinfo_resume_retain = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////

vkrenderinfo_ptr_t VkRtGroupImpl::renderinfo() {
  if (!_rinfo_retain) {
    _rinfo_retain = std::make_shared<VulkanRenderInfo>(this);
    _renderinfo_set.insert(_rinfo_retain);
  }
  return _rinfo_retain;
}

///////////////////////////////////////////////////////////////////////////////

vkrenderinfo_ptr_t VkRtGroupImpl::renderinfoForResume() {
  if (!_rinfo_resume_retain) {
    // Create resume render info based on this RTG
    _rinfo_resume_retain = std::make_shared<VulkanRenderInfo>(this);
    
    // Modify it for resume semantics - force all attachments to LOAD
    for(auto& colorAttachment : _rinfo_resume_retain->_rainfos_color) {
      colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    
    // Force depth attachment to LOAD if present
    if(_rinfo_resume_retain->_renderinfo.pDepthAttachment) {
      _rinfo_resume_retain->_rainfo_depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    
    // Always set the RESUMING bit
    _rinfo_resume_retain->_renderinfo.flags |= VK_RENDERING_RESUMING_BIT;
    
    _renderinfo_set.insert(_rinfo_resume_retain);
  }
  return _rinfo_resume_retain;
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_attachments_ptr_t VkRtGroupImpl::attachments() {
  if (__attachments) {
    return __attachments;
  }
  __attachments = std::make_shared<RtGroupAttachments>();
  auto at       = std::make_shared<RtGroupAttachments>();
  int numrt     = _color_buffer_impls.size();
  for (int i = 0; i < numrt; i++) {
    auto bufferimpl = _color_buffer_impls[i];
    auto imgobj     = bufferimpl->_imgobj;
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    // Use slice view from descriptor if available, otherwise use image view
    VkImageView view_to_use = bufferimpl->_descriptorInfo.imageView != VK_NULL_HANDLE 
                               ? bufferimpl->_descriptorInfo.imageView 
                               : imgobj->_vkimageview;
    __attachments->_imageviews.push_back(view_to_use);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);

    if (view_to_use == VK_NULL_HANDLE) {
      //printf("rtg<%s> has null imageview\n", _rtg->_name.c_str());
      OrkAssert(false);
    }
  }
  if (_depth_buffer_impl) {
    auto imgobj     = _depth_buffer_impl->_imgobj;
    __attachments->_descriptions.push_back(_depth_buffer_impl->_attachmentDesc);
    __attachments->_references.push_back(_depth_buffer_impl->_attachmentRef);
    // Use slice view from descriptor if available, otherwise use image view
    VkImageView view_to_use = _depth_buffer_impl->_descriptorInfo.imageView != VK_NULL_HANDLE 
                               ? _depth_buffer_impl->_descriptorInfo.imageView 
                               : imgobj->_vkimageview;
    __attachments->_imageviews.push_back(view_to_use);
    __attachments->descimginfos.push_back(_depth_buffer_impl->_descriptorInfo);
    OrkAssert(view_to_use != VK_NULL_HANDLE);
  }
  return __attachments;
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb) {
  //logchan_rtgi->log("_transitionToRenderTarget<%p>", (void*)cb.get());
  
  // DEBUG: Log the transition
  if(0)logchan_rtgi->log("_transitionToRenderTarget: Transitioning depth buffer to VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL");
  
  for (int i = 0; i < _color_buffer_impls.size(); i++) {
    auto rtb_impl = _color_buffer_impls[i];
    if(0)logchan_rtgi->log("Color buffer %d before transition: layout %d", i, rtb_impl->_currentLayout);
    rtb_impl->_transitionToRenderTarget(cb);
    if(0)logchan_rtgi->log("Color buffer %d after transition: layout %d", i, rtb_impl->_currentLayout);
  }
  if (_depth_buffer_impl) {
    if(0)logchan_rtgi->log("Depth buffer before transition: layout %d", _depth_buffer_impl->_currentLayout);
    _depth_buffer_impl->_transitionToRenderTarget(cb);
    if(0)logchan_rtgi->log("Depth buffer after transition: layout %d", _depth_buffer_impl->_currentLayout);
  }
  
  // DEBUG: Log that transition is complete
  if(0)logchan_rtgi->log("_transitionToRenderTarget: Depth buffer transition complete");
}

///////////////////////////////////////////////////////////////////////////////
// MULTIVIEW MSAA DEPTH RESOLVE.
//
// Under multiview one pass rasterizes every view, and the depth TEST reads the
// multisample attachment — so the rendered image is depth-correct in every eye whether or
// not any resolve happens. What no shader can sample is that attachment: every
// sampler-side depth consumer (the stereo occlusion pyramid, DEPTH_MAP for SSAO/water)
// reads the SINGLE-SAMPLE copy instead. Filling that copy is the resolve's whole job, and
// the render pass's own pResolveAttachment cannot do it for more than one view: Metal's
// resolve names ONE array slice per pass, and MoltenVK's instanced multiview is a single
// Metal pass. Measured on this backend at 4 samples, that in-pass resolve filled NEITHER
// eye — the single-sample copy sat at the 1.0 far clear across both layers, so the stereo
// occlusion pyramid read max(far, far) and rejected nothing at all (the safe direction,
// and a silent one). With the per-layer passes below, both layers carry scene depth and
// the same scene rejects as it does with multisampling off.
//
// So the resolve is driven here instead, ONE LAYER AT A TIME: a single-layer render pass
// per view, no draws, whose only work is the load/resolve/store the driver performs at
// pass end. That is the portable expression of a per-view depth resolve — there is no
// depth equivalent of vkCmdResolveImage (it is color-only), and sampling the multisample
// depth in a kernel would need a sampled multisample image this backend does not create.
// Same path on every driver: a resolve that is only correct where the implementation
// happens to loop over views is not one we can reason about.
///////////////////////////////////////////////////////////////////////////////

bool VkRtGroupImpl::_needsManualDepthResolve() const {
  if (not _depth_buffer_impl)
    return false;
  if (not _depth_buffer_impl->_msaa_imgobj)
    return false; // no MSAA -> the sampled image IS the rendered one
  if (not _rtgroup)
    return false;
  return _rtgroup->_multiview and (_rtgroup->_numLayers > 1);
}

void VkRtGroupImpl::_resolveMultiviewDepth(vkpricmdbufimpl_ptr_t cb) {
  if (not _needsManualDepthResolve())
    return;
  auto dbuf = _depth_buffer_impl;
  int  nlay = _rtgroup->_numLayers;

  // Both images are in an attachment layout here: the pass that just ended put them
  // there, and _transitionToRenderTarget keeps the pair in step.
  VkImageLayout msaa_layout    = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  VkImageLayout resolve_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  for (int l = 0; l < nlay; l++) {
    VkRenderingAttachmentInfo dai;
    initializeVkStruct(dai, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
    VkImageView msaa_view    = dbuf->layerView(l, true);
    VkImageView resolve_view = dbuf->layerView(l, false);
    OrkAssertI(
        msaa_view != VK_NULL_HANDLE and resolve_view != VK_NULL_HANDLE,
        "multiview MSAA depth has no per-layer attachment view — nothing could resolve this eye");
    dai.imageView   = msaa_view;
    dai.imageLayout = msaa_layout;
    // SAMPLE_ZERO is the only depth resolve mode required of every implementation, and
    // it is what the single-view path has always used — one sample of the depth, not an
    // average across samples that lie on different surfaces.
    dai.resolveMode        = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    dai.resolveImageView   = resolve_view;
    dai.resolveImageLayout = resolve_layout;
    // LOAD/STORE, never clear: this pass draws nothing, so a clear would erase the depth
    // it exists to copy, and the color pass still depth-tests against the multisample
    // image afterwards.
    dai.loadOp                        = VK_ATTACHMENT_LOAD_OP_LOAD;
    dai.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    dai.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo ri;
    initializeVkStruct(ri, VK_STRUCTURE_TYPE_RENDERING_INFO);
    ri.viewMask               = 0; // NOT multiview: one view, named explicitly
    ri.layerCount             = 1;
    ri.renderArea.offset      = {0, 0};
    ri.renderArea.extent      = {uint32_t(_width), uint32_t(_height)};
    ri.colorAttachmentCount   = 0;
    ri.pColorAttachments      = nullptr;
    ri.pDepthAttachment       = &dai;
    ri.pStencilAttachment     = nullptr;

    _contextVK->_vkCmdBeginRenderingKHR(cb->_vkcmdbuf, &ri);
    _contextVK->_vkCmdEndRenderingKHR(cb->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_invalidateAttachments() {
  __attachments = nullptr;
  _renderinfo_set.clear();
  _rinfo_retain = nullptr;
  _rinfo_resume_retain = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// pipeline-hash contribution keyed by the ATTACHMENT LAYOUT (ordered color
// formats + depth format + msaa) — a VkPipeline built with dynamic rendering
// is only valid against passes of MATCHING attachment layout. Historically
// every impl reported 0, so pipelines leaked ACROSS layouts: a pipeline built
// for the (color+depth) main depth-prepass RTG also ran against the Z32F-only
// shadow slices, where its fragment stage's discard was silently dropped
// (surfaced by the A3 masked depth prepass — alpha-tested casters shadowed
// solid). Same-layout RTGs still share pipelines (correct and cheap).
///////////////////////////////////////////////////////////////////////////////

int VkRtGroupImpl::layoutBits() {
  if (_pipeline_bits >= 0)
    return _pipeline_bits;
  uint64_t sig = 0xf0e1d2c3;
  for (auto& cbi : _color_buffer_impls)
    sig = sig * 31 + uint64_t(cbi->_vkfmt) + 1;
  sig = sig * 31 + (_depth_buffer_impl ? uint64_t(_depth_buffer_impl->_vkfmt) + 1 : 0);
  sig = sig * 31 + uint64_t(msaaEnumToInt(_rtgroup->_msaa_samples));
  // VIEW MASK is part of the attachment layout for pipeline purposes: a VkPipeline built
  // with VkPipelineRenderingCreateInfo.viewMask=0 is INVALID inside a pass whose
  // VkRenderingInfo.viewMask is non-zero (VUID-vkCmdDraw-viewMask), and the same key also
  // separates the base-module pipelines from the multiview-module ones, which are chosen
  // from this exact field. Mono is unaffected: every legacy RTG reports 0 here, so the
  // existing partition is preserved (times 31, plus a constant) and no new layout appears.
  sig = sig * 31 + uint64_t(_rtgroup->viewMask());
  static std::unordered_map<uint64_t, int> _layout_registry;
  static std::mutex _layout_mutex;
  std::lock_guard<std::mutex> lock(_layout_mutex);
  auto it = _layout_registry.find(sig);
  if (it == _layout_registry.end())
    it = _layout_registry.emplace(sig, int(_layout_registry.size())).first;
  _pipeline_bits = it->second;
  OrkAssert(_pipeline_bits < 16); // 4-bit budget in the pipeline hash
  return _pipeline_bits;
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_setupCubeFaceRendering(int face_index) {
  OrkAssert(face_index >= 0 && face_index < 6);

  // Setup per-face views for color buffers
  for (auto& bufimpl : _color_buffer_impls) {
    if (!bufimpl->_imgobj) continue;

    // Create per-face views if they don't exist
    if (!bufimpl->_hasCubeFaceViews) {
      for (int i = 0; i < 6; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = bufimpl->_imgobj->_vkimage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 2D view of single layer
        viewInfo.format = bufimpl->_vkfmt;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;  // Face index
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &bufimpl->_cubeFaceViews[i]);
        OrkAssert(VK_SUCCESS == ok);

        // Set debug name for the face view
        std::string name = FormatString("cubeFace%d_color", i);
        _contextVK->_setObjectDebugName(bufimpl->_cubeFaceViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, name.c_str());
      }
      bufimpl->_hasCubeFaceViews = true;
    }

    // Set the descriptor info to point to this face's view
    bufimpl->_descriptorInfo.imageView = bufimpl->_cubeFaceViews[face_index];
    bufimpl->_descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  // Setup per-face view for depth buffer if present
  if (_depth_buffer_impl && _depth_buffer_impl->_imgobj) {
    auto& dbuf = _depth_buffer_impl;

    // Create per-face depth views if they don't exist
    if (!dbuf->_hasCubeFaceViews) {
      for (int i = 0; i < 6; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = dbuf->_imgobj->_vkimage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = dbuf->_vkfmt;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &dbuf->_cubeFaceViews[i]);
        OrkAssert(VK_SUCCESS == ok);

        std::string name = FormatString("cubeFace%d_depth", i);
        _contextVK->_setObjectDebugName(dbuf->_cubeFaceViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, name.c_str());
      }
      dbuf->_hasCubeFaceViews = true;
    }

    // Set the descriptor info to point to this face's view
    dbuf->_descriptorInfo.imageView = dbuf->_cubeFaceViews[face_index];
    dbuf->_descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  }

  // Invalidate cached render info so it rebuilds with the new face views
  _invalidateAttachments();
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToTexture(vkpricmdbufimpl_ptr_t cb){
  int numrt     = _color_buffer_impls.size();
  for (int i = 0; i < numrt; i++) {
    auto rtb_impl = _color_buffer_impls[i];
    rtb_impl->_transitionToTexture(cb);
    if(0)logchan_rtgi->log("RTG transitioning color buffer %p from %d to %d in CB %p", (void*)rtb_impl->_imgobj->_vkimage, rtb_impl->_currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  if (_depth_buffer_impl) {
    _depth_buffer_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    _depth_buffer_impl->_transitionToTexture(cb);
    if(0)logchan_rtgi->log("RTG transitioning depth buffer %p from %d to %d in CB %p", (void*)_depth_buffer_impl->_imgobj->_vkimage, _depth_buffer_impl->_currentLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToHostRead(vkpricmdbufimpl_ptr_t cb){
  int numrt     = _color_buffer_impls.size();
  for (int i = 0; i < numrt; i++) {
    auto rtb_impl = _color_buffer_impls[i];
    rtb_impl->_transitionToHostRead(cb);
    if(0)logchan_rtgi->log("RTG transitioning color buffer %p from %d to %d in CB %p", (void*)rtb_impl->_imgobj->_vkimage, rtb_impl->_currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  if (_depth_buffer_impl) {
    _depth_buffer_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    _depth_buffer_impl->_transitionToHostRead(cb);
    if(0)logchan_rtgi->log("RTG transitioning depth buffer %p from %d to %d in CB %p", (void*)_depth_buffer_impl->_imgobj->_vkimage, _depth_buffer_impl->_currentLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::assignToRtGroup(vkrtgrpimpl_ptr_t rtgimpl, ::ork::lev2::rtgroup_rawptr_t rtgroup){
  rtgroup->_impl.setShared<VkRtGroupImpl>(rtgimpl);
  int inumtargets = rtgroup->numImageBuffers();
  int inumimpls = rtgimpl->_color_buffer_impls.size();
  OrkAssert(inumtargets == inumimpls);
  for(int i=0; i < inumtargets; i++) {
    auto rtb = rtgroup->buffer(i);
    auto rtb_impl = rtgimpl->_color_buffer_impls[i];
    rtb->_impl.setShared<VklRtBufferImpl>(rtb_impl);
  }
  if(rtgroup->_depthBuffer) {
    auto rtb_impl = rtgimpl->_depth_buffer_impl;
    rtgroup->_depthBuffer->_impl.setShared<VklRtBufferImpl>(rtb_impl);
  }
  rtgimpl->_updateClearParams(rtgroup);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
