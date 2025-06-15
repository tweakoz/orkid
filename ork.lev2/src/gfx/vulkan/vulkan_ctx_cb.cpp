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
static logchannel_ptr_t logchan_vkcb = logger()->createChannel("VKCB", fvec3(1,1,.9));

secondary_commandbuffer_ptr_t VkContext::_beginRecordCommandBuffer(renderpass_ptr_t rpass,std::string name) {
  auto cmdbuf          = std::make_shared<SecondaryCommandBuffer>();
  if( name == "" ){
    cmdbuf->_debugName = "_beginRecordCommandBuffer";
  }
  else {
    cmdbuf->_debugName = name;
  }
  logchan_vkcb->log("_beginRecordCommandBuffer<%p:%s>", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str());
  auto vkcmdbuf = _createSecondaryVkCommandBuffer(cmdbuf.get());
  _recordCommandBuffer = cmdbuf;

  _setObjectDebugName(vkcmdbuf->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, cmdbuf->_debugName.c_str());

  VkCommandBufferBeginInfo CBBI_GFX      = {};
  VkCommandBufferInheritanceInfo INHINFO = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  initializeVkStruct(INHINFO, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (rpass) {
    auto rpimpl        = rpass->_impl.getShared<VulkanRenderPass>();
    INHINFO.renderPass = rpimpl->_vkrp; // The render pass the secondary command buffer will be executed within.
    INHINFO.subpass    = 0;             // The index of the subpass in the render pass.
    INHINFO.framebuffer =
        rpimpl->_vkfb; // Optional: The framebuffer targeted by the render pass. Can be VK_NULL_HANDLE if not provided.
    CBBI_GFX.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  }
  CBBI_GFX.pInheritanceInfo = &INHINFO;
  vkBeginCommandBuffer(vkcmdbuf->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

  return cmdbuf;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  OrkAssert(cmdbuf == _recordCommandBuffer);
  auto vkcmdbuf        = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  _recordCommandBuffer = nullptr;
  vkcmdbuf->_recorded = true;
  vkEndCommandBuffer(vkcmdbuf->_vkcmdbuf);
  logchan_vkcb->log("_endRecordCommandBuffer<%p:%s>", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str());
}

///////////////////////////////////////////////////////////////////////////////

/*
void VkContext::_doPushCommandBuffer(
    secondary_commandbuffer_ptr_t cmdbuf, //
    rtgroup_ptr_t rtg) {        //
  
  _vk_cmdbufstack.push(_cmdbufcur_gfx);

  OrkAssert(_current_cmdbuf == cmdbuf);
  vkcmdbufimpl_ptr_t impl;
  if (auto as_impl = cmdbuf->_impl.tryAsShared<VkCommandBufferImpl>()) {
    impl = as_impl.value();
  } else {
    impl = _createVkCommandBuffer(cmdbuf.get());
  }

  //printf( "pushCB<%p:%s> impl<%p>\n", (void*) cmdbuf.get(), cmdbuf->_debugName.c_str(), (void*) impl.get() );
  VkCommandBufferInheritanceInfo INHINFO = {};
  initializeVkStruct(INHINFO, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  auto rpass         = _renderpasses.back();
  auto rpimpl        = rpass->_impl.getShared<VulkanRenderPass>();
  INHINFO.renderPass = rpimpl->_vkrp; // The render pass the secondary command buffer will be executed within.
  INHINFO.subpass    = 0;             // The index of the subpass in the render pass.
  INHINFO.framebuffer = rpimpl->_vkfb; // Optional: The framebuffer targeted by the render pass. Can be VK_NULL_HANDLE if not provided.
  ////////////////////////////////////////////
  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT //
                   | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  CBBI_GFX.pInheritanceInfo = &INHINFO;
  vkBeginCommandBuffer(impl->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

  _cmdbufcur_gfx = impl;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doPopCommandBuffer() {
  _cmdbufcur_gfx->_recorded = true;
  //printf( "popCB<%p:%s> impl<%p>\n", (void*) _cmdbufcur_gfx->_parent, _cmdbufcur_gfx->_parent->_debugName.c_str(), (void*) _cmdbufcur_gfx.get() );
  vkEndCommandBuffer(_cmdbufcur_gfx->_vkcmdbuf);
  _cmdbufcur_gfx = _vk_cmdbufstack.top();
  _vk_cmdbufstack.pop();
}
*/
///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEnqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  logchan_vkcb->log("_doEnqueueSecondaryCommandBuffer<%p:%s>", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str());
  auto impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  if(not impl->_recorded){
    printf( "CB<%p:%s> impl<%p> not recorded!\n", (void*) cmdbuf.get(), cmdbuf->_debugName.c_str(), (void*) impl.get() );
    OrkAssert(false);
  }
  vkCmdExecuteCommands(primary_cb()->_vkcmdbuf, 1, &impl->_vkcmdbuf);
  primary_cb()->_secondary_cmdbuffers.push_back(cmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::enqueueDeferredOneShotCommand(secondary_commandbuffer_ptr_t cmdbuf) {
  auto impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  OrkAssert(impl->_recorded);
  _pendingOneShotCommands.push_back(cmdbuf);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

vkpricmdbufimpl_ptr_t VkContext::_createPrimaryVkCommandBuffer(PrimaryCommandBuffer* ork_cb){

  vkpricmdbufimpl_ptr_t rval = ork_cb->_impl.makeShared<VkPrimaryCommandBufferImpl>(this);
  rval->_orkCB = ork_cb;
  VkCommandBufferAllocateInfo CBAI_GFX = {};
  initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  CBAI_GFX.commandPool        = _vkcmdpool_graphics;
  CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  CBAI_GFX.commandBufferCount = 1;
  VkResult OK = vkAllocateCommandBuffers(
      _vkdevice, //
      &CBAI_GFX, //
      &rval->_vkcmdbuf);
  OrkAssert(OK == VK_SUCCESS);
  _setObjectDebugName(rval->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, ork_cb->_debugName.c_str());
  return rval;
}
vkseccmdbufimpl_ptr_t VkContext::_createSecondaryVkCommandBuffer(SecondaryCommandBuffer* ork_cb){

  vkseccmdbufimpl_ptr_t rval = ork_cb->_impl.makeShared<VkSecondaryCommandBufferImpl>(this);
  rval->_orkCB = ork_cb;
  VkCommandBufferAllocateInfo CBAI_GFX = {};
  initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  CBAI_GFX.commandPool        = _vkcmdpool_graphics;
  CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  CBAI_GFX.commandBufferCount = 1;
  VkResult OK = vkAllocateCommandBuffers(
      _vkdevice, //
      &CBAI_GFX, //
      &rval->_vkcmdbuf);
  OrkAssert(OK == VK_SUCCESS);
  _setObjectDebugName(rval->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, ork_cb->_debugName.c_str());
  return rval;
}
///////////////////////////////////////////////////

std::atomic<int> VkPrimaryCommandBufferImpl::_cmdbufcount(0);

VkPrimaryCommandBufferImpl::VkPrimaryCommandBufferImpl(VkContext* ctx)
    : _contextVK(ctx) {
    int count = _cmdbufcount.fetch_add(1);
    logchan_vkcb->log("VkPrimaryCommandBufferImpl<%p> count<%d>", (void*)this, count);
}

VkPrimaryCommandBufferImpl::~VkPrimaryCommandBufferImpl(){
  //printf ("DESTROY CB<%p>\n", (void*) _vkcmdbuf );
  vkFreeCommandBuffers(_contextVK->_vkdevice, _contextVK->_vkcmdpool_graphics, 1, &_vkcmdbuf);
  _cmdbufcount.fetch_sub(1);
}

///////////////////////////////////////////////////

std::atomic<int> VkSecondaryCommandBufferImpl::_cmdbufcount(0);

VkSecondaryCommandBufferImpl::VkSecondaryCommandBufferImpl(VkContext* ctx)
    : _contextVK(ctx) {
    int count = _cmdbufcount.fetch_add(1);
    logchan_vkcb->log("VkSecondaryCommandBufferImpl<%p> count<%d>", (void*)this, count);
}

VkSecondaryCommandBufferImpl::~VkSecondaryCommandBufferImpl(){
  //printf ("DESTROY CB<%p>\n", (void*) _vkcmdbuf );
  vkFreeCommandBuffers(_contextVK->_vkdevice, _contextVK->_vkcmdpool_graphics, 1, &_vkcmdbuf);
  _cmdbufcount.fetch_sub(1);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
