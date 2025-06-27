////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgi = logger()->createChannel("VKRTGI", fvec3(0.8, 0.2, 0.5), true);
///////////////////////////////////////////////////////////////////////////////


VkRtGroupImpl::VkRtGroupImpl(vkcontext_rawptr_t ctxVK)
    : _contextVK(ctxVK) {
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
  _cmdbufRTG = nullptr; // Clear the command buffer to avoid dangling pointers
}
///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_updateClearParams(rtgroup_rawptr_t rtg) {
  _autoclear = rtg->_autoclear;
  for(int i = 0; i < rtg->numImageBuffers(); i++) {
    auto rtb      = rtg->buffer(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_clear_color = rtb->_clearColor;
  }
}

///////////////////////////////////////////////////////

void VkRtGroupImpl::_updateMainSurface(VkFrameBufferInterface* fbi) {
  auto ctxVK = fbi->_contextVK;
  int w = ctxVK->mainSurfaceWidth();
  int h = ctxVK->mainSurfaceHeight();
  if ( (_width != w) or (_height != h) ) {
    logchan_rtgi->log("resize main surface to w<%d> h<%d>", w, h);
    //SetSizeDirty(false);
    _width  = w;
    _height = h;
  }  
}

///////////////////////////////////////////////////////////////////////////////

vkrenderinfo_ptr_t VkRtGroupImpl::renderinfo() {
  auto rinfo = std::make_shared<VulkanRenderInfo>(this); 
  _renderinfo_set.insert(rinfo);
  return rinfo;
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
    __attachments->_imageviews.push_back(imgobj->_vkimageview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);

    if (imgobj->_vkimageview == VK_NULL_HANDLE) {
      //printf("rtg<%s> has null imageview\n", _rtg->_name.c_str());
      OrkAssert(false);
    }
  }
  if (_depth_buffer_impl) {
    auto imgobj     = _depth_buffer_impl->_imgobj;
    __attachments->_descriptions.push_back(_depth_buffer_impl->_attachmentDesc);
    __attachments->_references.push_back(_depth_buffer_impl->_attachmentRef);
    __attachments->_imageviews.push_back(imgobj->_vkimageview);
    __attachments->descimginfos.push_back(_depth_buffer_impl->_descriptorInfo);
    OrkAssert(imgobj->_vkimageview != VK_NULL_HANDLE);
  }
  return __attachments;
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb) {
  //logchan_rtgi->log("_transitionToRenderTarget<%p>", (void*)cb.get());
  
  // DEBUG: Log the transition
  logchan_rtgi->log("_transitionToRenderTarget: Transitioning depth buffer to VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL");
  
  for (int i = 0; i < _color_buffer_impls.size(); i++) {
    auto rtb_impl = _color_buffer_impls[i];
    logchan_rtgi->log("Color buffer %d before transition: layout %d", i, rtb_impl->_currentLayout);
    rtb_impl->_transitionToRenderTarget(cb);
    logchan_rtgi->log("Color buffer %d after transition: layout %d", i, rtb_impl->_currentLayout);
  }
  if (_depth_buffer_impl) {
    logchan_rtgi->log("Depth buffer before transition: layout %d", _depth_buffer_impl->_currentLayout);
    _depth_buffer_impl->_transitionToRenderTarget(cb);
    logchan_rtgi->log("Depth buffer after transition: layout %d", _depth_buffer_impl->_currentLayout);
  }
  
  // DEBUG: Log that transition is complete
  logchan_rtgi->log("_transitionToRenderTarget: Depth buffer transition complete");
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_invalidateAttachments() {
  __attachments = nullptr;
  _renderinfo_set.clear();
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToTexture(vkpricmdbufimpl_ptr_t cb){
  int numrt     = _color_buffer_impls.size();
  for (int i = 0; i < numrt; i++) {
    auto rtb_impl = _color_buffer_impls[i];
    rtb_impl->_transitionToTexture(cb);
    logchan_rtgi->log("RTG transitioning color buffer %p from %d to %d in CB %p", (void*)rtb_impl->_imgobj->_vkimage, rtb_impl->_currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  if (_depth_buffer_impl) {
    _depth_buffer_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    _depth_buffer_impl->_transitionToTexture(cb);
    logchan_rtgi->log("RTG transitioning depth buffer %p from %d to %d in CB %p", (void*)_depth_buffer_impl->_imgobj->_vkimage, _depth_buffer_impl->_currentLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToHostRead(vkpricmdbufimpl_ptr_t cb){
  int numrt     = _color_buffer_impls.size();
  for (int i = 0; i < numrt; i++) {
    auto rtb_impl = _color_buffer_impls[i];
    rtb_impl->_transitionToHostRead(cb);
    logchan_rtgi->log("RTG transitioning color buffer %p from %d to %d in CB %p", (void*)rtb_impl->_imgobj->_vkimage, rtb_impl->_currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  if (_depth_buffer_impl) {
    _depth_buffer_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    _depth_buffer_impl->_transitionToHostRead(cb);
    logchan_rtgi->log("RTG transitioning depth buffer %p from %d to %d in CB %p", (void*)_depth_buffer_impl->_imgobj->_vkimage, _depth_buffer_impl->_currentLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, (void*)cb->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::assignToRtGroup(vkrtgrpimpl_ptr_t rtgimpl, rtgroup_rawptr_t rtgroup){
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
