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
static logchannel_ptr_t logchan_vkcb = logger()->configureChannel("VKCB", fvec3(1, 1, .9), true);

void VkContext::_beginRecordCommandBuffer(secondary_commandbuffer_ptr_t cbuf) {
  
}

secondary_commandbuffer_ptr_t VkContext::_beginRecordCommandBuffer(std::string name, rtgroup_rawptr_t rtg) {
  auto cmdbuf = std::make_shared<SecondaryCommandBuffer>();
  cmdbuf->_debugName = name;

  //logchan_vkcb->log("_beginRecordCommandBuffer<%p:%s>", (void*)cmdbuf.get(), name.c_str());
  auto vkcmdbuf        = _createSecondaryVkCommandBuffer(cmdbuf.get());

  _setObjectDebugName(vkcmdbuf->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, name.c_str());

  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkCommandBufferInheritanceInfo INHINFO = {};
  initializeVkStruct(INHINFO, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);

  if (rtg) { // dynamic rendering inheritance?
    CBBI_GFX.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    // Dynamic rendering inheritance
    VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
    initializeVkStruct(inheritanceRenderingInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO);

    // Get current rendering state from FBI

    // Specify the formats being rendered to
    std::vector<VkFormat> colorFormats;
    for (int i = 0; i < rtg->mNumMrts; i++) {
      auto fmt = VkFormatConverter::convertBufferFormat(rtg->buffer(i)->format());
      colorFormats.push_back(fmt);
    }

    inheritanceRenderingInfo.colorAttachmentCount    = colorFormats.size();
    inheritanceRenderingInfo.pColorAttachmentFormats = colorFormats.data();

    if (rtg->_depthBuffer) {
      auto depthFmt                                  = VkFormatConverter::convertBufferFormat(rtg->_depthBuffer->format());
      inheritanceRenderingInfo.depthAttachmentFormat = depthFmt;

      // If format has stencil
      if (depthFmt == VK_FORMAT_D24_UNORM_S8_UINT || depthFmt == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        inheritanceRenderingInfo.stencilAttachmentFormat = depthFmt;
      }
    }

    inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // or from rtg

    INHINFO.pNext             = &inheritanceRenderingInfo;
  } else {
    // Secondary command buffer for compute or transfer operations
    // Still needs basic inheritance info, just no render pass
    INHINFO.renderPass = VK_NULL_HANDLE;
    INHINFO.framebuffer = VK_NULL_HANDLE;
    INHINFO.occlusionQueryEnable = VK_FALSE;
    INHINFO.queryFlags = 0;
    INHINFO.pipelineStatistics = 0;
  }
  CBBI_GFX.pInheritanceInfo = &INHINFO;

  vkBeginCommandBuffer(vkcmdbuf->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

  if(0)logchan_vkcb->log("vkBeginCommandBuffer: %s CB %p", "secondary", (void*)vkcmdbuf->_vkcmdbuf);

  return cmdbuf;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  auto vkcmdbuf        = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  vkcmdbuf->_recorded  = true;
  vkEndCommandBuffer(vkcmdbuf->_vkcmdbuf);
  //logchan_vkcb->log("_endRecordCommandBuffer<%p:%s>", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str());
  if(0)logchan_vkcb->log("vkEndCommandBuffer: %s CB %p", "secondary", (void*)vkcmdbuf->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEnqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  if(0)logchan_vkcb->log("_doEnqueueSecondaryCommandBuffer<%p:%s>", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str());
  auto impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  if (not impl->_recorded) {
    printf("CB<%p:%s> impl<%p> not recorded!\n", (void*)cmdbuf.get(), cmdbuf->_debugName.c_str(), (void*)impl.get());
    OrkAssert(false);
  }

  auto pricb = primary_cb();

  // Invoke pre-enqueue callback if set (pooled CBs add themselves to pending_cleanup)
  if (impl->_onPreEnqueueCallback) {

    impl->_onPreEnqueueCallback();
  } else {
    // Non-pooled CB: add to _secondary_cmdbuffers for lifecycle management
    pricb->_secondary_cmdbuffers.push_back(cmdbuf);
  }

  // DEBUG: Log when secondary command buffer is executed
  if(0)logchan_vkcb->log("DOENQSECCB: exec secCB<%p:%s> in priCB<%p> preenqCB<%d> cleanupCB<%d>",
                    (void*)impl->_vkcmdbuf, 
                    cmdbuf->_debugName.c_str(),
                    (void*)pricb.get(),
                    int(impl->_onPreEnqueueCallback != nullptr),
                    int(impl->_onCleanupCallback != nullptr)
                    );

  vkCmdExecuteCommands(pricb->_vkcmdbuf, 1, &impl->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::enqueueDeferredOneShotCommand(secondary_commandbuffer_ptr_t cmdbuf) {
  auto impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  OrkAssert(impl->_recorded);
  _pendingOneShotCommands.atomicOp([&](vkseccmdbufarray_t& unlocked) {
    unlocked.push_back(cmdbuf);
  });
  // Track semaphores separately for batch submission
  if (impl->_completionSemaphore) {
    _pendingOneShotSemas.atomicOp([&](vkcompsema_set_t& unlocked) {
      unlocked.insert(impl->_completionSemaphore);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

vkpricmdbufimpl_ptr_t VkContext::_createPrimaryVkCommandBuffer(PrimaryCommandBuffer* ork_cb) {

  vkpricmdbufimpl_ptr_t rval           = ork_cb->_impl.makeShared<VkPrimaryCommandBufferImpl>(this);
  rval->_orkCB                         = ork_cb;
  VkCommandBufferAllocateInfo CBAI_GFX = {};
  initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  CBAI_GFX.commandPool        = _vkcmdpool_graphics;
  CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  CBAI_GFX.commandBufferCount = 1;
  VkResult OK                 = vkAllocateCommandBuffers(
      _vkdevice, //
      &CBAI_GFX, //
      &rval->_vkcmdbuf);
  OrkAssert(OK == VK_SUCCESS);
  _setObjectDebugName(rval->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, ork_cb->_debugName.c_str());
  return rval;
}
vkseccmdbufimpl_ptr_t VkContext::_createSecondaryVkCommandBuffer(SecondaryCommandBuffer* ork_cb) {

  OrkAssert(_vkcmdpool_graphics != VK_NULL_HANDLE); // Ensure command pool is created
  
  vkseccmdbufimpl_ptr_t rval           = ork_cb->_impl.makeShared<VkSecondaryCommandBufferImpl>(this);
  rval->_orkCB                         = ork_cb;
  VkCommandBufferAllocateInfo CBAI_GFX = {};
  initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  CBAI_GFX.commandPool        = _vkcmdpool_graphics;
  CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  CBAI_GFX.commandBufferCount = 1;
  VkResult OK                 = vkAllocateCommandBuffers(
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
  //logchan_vkcb->log("VkPrimaryCommandBufferImpl<%p> count<%d>", (void*)this, count);
}

VkPrimaryCommandBufferImpl::~VkPrimaryCommandBufferImpl() {
  _cmdbufcount.fetch_sub(1);
  try {
    if(_contextVK && _contextVK->_vkdevice && _contextVK->_vkcmdpool_graphics) {
      vkFreeCommandBuffers(_contextVK->_vkdevice, _contextVK->_vkcmdpool_graphics, 1, &_vkcmdbuf);
    }
  } catch (...) {
    // Swallow — during static destruction _contextVK may be dangling.
  }
}

///////////////////////////////////////////////////

std::atomic<int> VkSecondaryCommandBufferImpl::_cmdbufcount(0);

VkSecondaryCommandBufferImpl::VkSecondaryCommandBufferImpl(VkContext* ctx)
    : _contextVK(ctx) {
  int count = _cmdbufcount.fetch_add(1);
_vkcmdbuf = nullptr;
  //logchan_vkcb->log("VkSecondaryCommandBufferImpl<%p> count<%d>", (void*)this, count);
}

VkSecondaryCommandBufferImpl::~VkSecondaryCommandBufferImpl() {
  _cmdbufcount.fetch_sub(1);
  try {
    if(_contextVK && _vkcmdbuf) {
      auto device = _contextVK->_vkdevice;
      auto cmdpool = _contextVK->_vkcmdpool_graphics;
      if(device && cmdpool) {
        vkFreeCommandBuffers(device, cmdpool, 1, &_vkcmdbuf);
      }
    }
  } catch (...) {
    // Swallow — during static destruction _contextVK may be dangling.
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
