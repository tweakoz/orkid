
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


VkRtGroupImpl::VkRtGroupImpl(vkcontext_rawptr_t ctxVK, rtgroup_rawptr_t rtg)
    : _rtg(rtg)
    , _contextVK(ctxVK) {
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

///////////////////////////////////////////////////////////////////////////////

vkrenderinfo_ptr_t VkRtGroupImpl::renderinfo() {
  auto rinfo = std::make_shared<VulkanRenderInfo>(_rtg); 
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
  int numrt     = _rtg->numImageBuffers();
  for (int i = 0; i < numrt; i++) {
    auto rtbuffer   = _rtg->buffer(i);
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);

    if (bufferimpl->_vkimgview == VK_NULL_HANDLE) {
      printf("rtg<%s> has null imageview\n", _rtg->_name.c_str());
      OrkAssert(false);
    }
  }
  if (_rtg->_depthBuffer) {
    auto rtbuffer   = _rtg->_depthBuffer;
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);
    OrkAssert(bufferimpl->_vkimgview != VK_NULL_HANDLE);
  }
  return __attachments;
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb){
  int inumtargets = _rtg->numImageBuffers();
  for (int i = 0; i < inumtargets; i++) {
    auto rtb      = _rtg->buffer(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_transitionToRenderTarget(cb);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  }
  auto depthbuffer = _rtg->_depthBuffer;
  if (depthbuffer) {
    auto rtb_impl = depthbuffer->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    rtb_impl->_transitionToRenderTarget(cb);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToTexture(vkpricmdbufimpl_ptr_t cb){
  int inumtargets = _rtg->numImageBuffers();
  for (int i = 0; i < inumtargets; i++) {
    auto rtb      = _rtg->buffer(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_transitionToTexture(cb);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  auto depthbuffer = _rtg->_depthBuffer;
  if (depthbuffer) {
    auto rtb_impl = depthbuffer->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    rtb_impl->_transitionToTexture(cb);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkRtGroupImpl::_transitionToHostRead(vkpricmdbufimpl_ptr_t cb){
  int inumtargets = _rtg->numImageBuffers();
  for (int i = 0; i < inumtargets; i++) {
    auto rtb      = _rtg->buffer(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_transitionToHostRead(cb);
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  auto depthbuffer = _rtg->_depthBuffer;
  if (depthbuffer) {
    auto rtb_impl = depthbuffer->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    rtb_impl->_transitionToHostRead(cb);
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
