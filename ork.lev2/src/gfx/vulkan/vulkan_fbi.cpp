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


VkMsaaState::VkMsaaState(){
  initializeVkStruct(_VKSTATE, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  _VKSTATE.sampleShadingEnable = VK_FALSE; // Enable/Disable sample shading
  _VKSTATE.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
  _VKSTATE.minSampleShading = 1.0f; // Minimum fraction for sample shading; closer to 1 is smoother
  _VKSTATE.pSampleMask = nullptr; // Optional
  _VKSTATE.alphaToCoverageEnable = VK_FALSE; // Enable/Disable alpha to coverage
  _VKSTATE.alphaToOneEnable = VK_FALSE; // Enable/Disable alpha to one

  _pipeline_bits = 0;
}

VkFrameBufferInterface::VkFrameBufferInterface(vkcontext_rawptr_t ctx)
    : FrameBufferInterface(*ctx)
    , _contextVK(ctx) {
}

///////////////////////////////////////////////////////

VkFrameBufferInterface::~VkFrameBufferInterface() {
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setViewport(int iX, int iY, int iW, int iH) {

  auto tracker = std::make_shared<VkViewportTracker>();
  tracker->_width = iW;
  tracker->_height = iH;
  tracker->_x = iX;
  tracker->_y = iY;
  _viewportTracker = tracker;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setScissor(int iX, int iY, int iW, int iH) {
   auto tracker = std::make_shared<VkViewportTracker>();
  tracker->_width = iW;
  tracker->_height = iH;
  tracker->_x = iX;
  tracker->_y = iY;
   _scissorTracker = tracker;
}

///////////////////////////////////////////////////////
void VkFrameBufferInterface::_doBeginFrame() {
  _acquireSwapChainForFrame();
  _active_rtgroup = _main_rtg.get();
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_doEndFrame() {
  //popViewport();
  //popScissor();
  //OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::capture(const RtBuffer* inpbuf, const file::Path& pth) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

bool VkFrameBufferInterface::captureToTexture(const CaptureBuffer& capbuf, Texture& tex) {
  OrkAssert(false);
  return false;
}

///////////////////////////////////////////////////////

bool VkFrameBufferInterface::captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) {
  OrkAssert(false);
    return false;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

renderpass_ptr_t createRenderPassForRtGroup(vkcontext_rawptr_t ctxVK, vkrtgrpimpl_ptr_t rtg_impl){
  auto renpass = std::make_shared<RenderPass>();
  auto vk_renpass = renpass->_impl.makeShared<VulkanRenderPass>(renpass.get());

  auto attachments = rtg_impl->attachments();

    VkSubpassDescription SUBPASS = {};
    initializeVkStruct(SUBPASS);
    SUBPASS.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SUBPASS.colorAttachmentCount    = attachments->_references.size();
    SUBPASS.pColorAttachments       = attachments->_references.data();
    //SUBPASS.pDepthStencilAttachment = &DATR;

    VkRenderPassCreateInfo RPI = {};
    initializeVkStruct(RPI, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RPI.attachmentCount = attachments->_descriptions.size();
    RPI.pAttachments    = attachments->_descriptions.data();
    RPI.subpassCount    = 1;
    RPI.pSubpasses      = &SUBPASS;
    // RPI.dependencyCount = 1;
    // RPI.pDependencies = &dependency;
    VkResult OK = vkCreateRenderPass(ctxVK->_vkdevice, &RPI, nullptr, &vk_renpass->_vkrp);
    OrkAssert(OK == VK_SUCCESS);

    initializeVkStruct(vk_renpass->_vkfbinfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    vk_renpass->_vkfbinfo.attachmentCount = attachments->_imageviews.size();
    vk_renpass->_vkfbinfo.pAttachments = attachments->_imageviews.data();
    vk_renpass->_vkfbinfo.width = rtg_impl->_width;
    vk_renpass->_vkfbinfo.height = rtg_impl->_height;
    vk_renpass->_vkfbinfo.layers = 1;
    vk_renpass->_vkfbinfo.renderPass = vk_renpass->_vkrp; 

    vkCreateFramebuffer( ctxVK->_vkdevice, // device
                         &vk_renpass->_vkfbinfo, // pCreateInfo
                         nullptr, // pAllocator
                         &vk_renpass->_vkfb); // pFramebuffer

    // Optionally, you can also define subpass dependencies for layout transitions
    //VkSubpassDependency dependency{};
    //dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    //dependency.dstSubpass = 0;
    //dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.srcAccessMask = 0;
    //dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //RPI.dependencyCount = 1;
    //RPI.pDependencies = &dependency;

    return renpass;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::rtGroupClear(RtGroup* rtg) {
  auto rtgimpl = rtg->_impl.getShared<VkRtGroupImpl>();
  //VkRenderPass& VKRP = rtgimpl->_vkrp;
  VkRenderPass& VKRP = rtgimpl->_rpass_clear->_impl.getShared<VulkanRenderPass>()->_vkrp;
  VkFramebuffer& VKFB = rtgimpl->_rpass_clear->_impl.getShared<VulkanRenderPass>()->_vkfb;
  /////////////////////////////////////////////////////////////
  // clear in a renderpass
  /////////////////////////////////////////////////////////////
  auto color = rtg->_clearColor;
  VkClearValue clearValues[2];
  clearValues[0].color        = {{color.x,color.y,color.z,color.w}};
  clearValues[1].depthStencil = {1.0f, 0};
  VkRenderPassBeginInfo RPBI = {};
  initializeVkStruct(RPBI, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
  //  clear-rect-region
  RPBI.renderArea.offset = {0, 0};
  RPBI.renderArea.extent.width  = rtg->width();
  RPBI.renderArea.extent.height = rtg->height();
  //  clear-targets
  RPBI.clearValueCount = 1;
  RPBI.pClearValues    = clearValues;
  //  clear-misc
  RPBI.renderPass  = VKRP;
  RPBI.framebuffer = VKFB;
  /////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////
  rtgimpl->_cmdbuf = _contextVK->_beginRecordCommandBuffer(rtgimpl->_rpass_clear);
  auto cmdbuf_impl = rtgimpl->_cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  // CLEAR!
  
  vkCmdBeginRenderPass(
      cmdbuf_impl->_vkcmdbuf, //
      &RPBI,                   //
      VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);  

  vkCmdEndRenderPass( cmdbuf_impl->_vkcmdbuf);  
  
  _contextVK->_endRecordCommandBuffer(rtgimpl->_cmdbuf);
  _contextVK->enqueueSecondaryCommandBuffer(rtgimpl->_cmdbuf);

}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::rtGroupMipGen(RtGroup* rtg) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////
void VkFrameBufferInterface::downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_initializeContext(DisplayBuffer* pBuf){
  OrkAssert(false);
}

///////////////////////////////////////////////////////

freestyle_mtl_ptr_t VkFrameBufferInterface::utilshader() {
  OrkAssert(false);
  return nullptr;
}

void implementRenderPassForRtGroup(vkcontext_rawptr_t ctxVK, renderpass_ptr_t renpass, vkrtgrpimpl_ptr_t rtg_impl){
  auto vk_renpass = renpass->_impl.makeShared<VulkanRenderPass>(renpass.get());

    auto attachments = rtg_impl->attachments();

    VkSubpassDescription SUBPASS = {};
    initializeVkStruct(SUBPASS);
    SUBPASS.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SUBPASS.colorAttachmentCount    = attachments->_references.size();
    SUBPASS.pColorAttachments       = attachments->_references.data();
    //SUBPASS.pDepthStencilAttachment = &DATR;

    VkRenderPassCreateInfo RPI = {};
    initializeVkStruct(RPI, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RPI.attachmentCount = attachments->_descriptions.size();
    RPI.pAttachments    = attachments->_descriptions.data();
    RPI.subpassCount    = 1;
    RPI.pSubpasses      = &SUBPASS;
    // RPI.dependencyCount = 1;
    // RPI.pDependencies = &dependency;
    VkResult OK = vkCreateRenderPass(ctxVK->_vkdevice, &RPI, nullptr, &vk_renpass->_vkrp);
    OrkAssert(OK == VK_SUCCESS);

    initializeVkStruct(vk_renpass->_vkfbinfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    vk_renpass->_vkfbinfo.attachmentCount = attachments->_imageviews.size();
    vk_renpass->_vkfbinfo.pAttachments = attachments->_imageviews.data();
    vk_renpass->_vkfbinfo.width = rtg_impl->_width;
    vk_renpass->_vkfbinfo.height = rtg_impl->_height;
    vk_renpass->_vkfbinfo.layers = 1;
    vk_renpass->_vkfbinfo.renderPass = vk_renpass->_vkrp; 

    vkCreateFramebuffer( ctxVK->_vkdevice, // device
                         &vk_renpass->_vkfbinfo, // pCreateInfo
                         nullptr, // pAllocator
                         &vk_renpass->_vkfb); // pFramebuffer

    // Optionally, you can also define subpass dependencies for layout transitions
    //VkSubpassDependency dependency{};
    //dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    //dependency.dstSubpass = 0;
    //dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.srcAccessMask = 0;
    //dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //RPI.dependencyCount = 1;
    //RPI.pDependencies = &dependency;
}

renderpass_ptr_t createRenderPassForRtGroup(vkcontext_rawptr_t ctxVK, rtgroup_ptr_t rtg){
  auto renpass = std::make_shared<RenderPass>();
  auto rtg_impl = ctxVK->_fbi->_createRtGroupImpl(rtg.get());
  implementRenderPassForRtGroup(ctxVK,renpass,rtg_impl);
  return renpass;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
