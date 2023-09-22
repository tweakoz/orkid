////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

ImplementReflectionX(ork::lev2::vulkan::VkContext, "VkContext");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkImage VkSwapChain::image(){
  return _vkSwapChainImages[_curSwapWriteImage];
}
VkImageView VkSwapChain::imageView(){
  return _vkSwapChainImageViews[_curSwapWriteImage];
}
VkFramebuffer VkSwapChain::framebuffer(){
  return _vkFrameBuffers[_curSwapWriteImage];
}

void VkContext::describeX(class_t* clazz) {

  clazz->annotateTyped<context_factory_t>("context_factory", []() { return VkContext::makeShared(); });
}

///////////////////////////////////////////////////////

bool VkContext::HaveExtension(const std::string& extname) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

vkcontext_ptr_t VkContext::makeShared() {
  struct VkContextX : public VkContext {
    VkContextX()
        : VkContext() {
    }
  };
  auto ctx = std::make_shared<VkContextX>();
  return ctx;
}

///////////////////////////////////////////////////////////////////////////////
void VkContext::_initVulkanForDevInfo(vkdeviceinfo_ptr_t vk_devinfo) {
  _vkphysicaldevice = vk_devinfo->_phydev;
  _vkdeviceinfo     = vk_devinfo;

  ////////////////////////////
  // get queue families
  ////////////////////////////

  _vkqfid_graphics = NO_QUEUE;
  _vkqfid_compute  = NO_QUEUE;
  _vkqfid_transfer = NO_QUEUE;

  _num_queue_types = vk_devinfo->_queueprops.size();
  std::vector<float> queuePriorities(_num_queue_types, 1.0f);

  for (uint32_t i = 0; i < _num_queue_types; i++) {
    const auto& QPROP = vk_devinfo->_queueprops[i];
    if (QPROP.queueCount == 0)
      continue;

    VkDeviceQueueCreateInfo DQCI;
    initializeVkStruct(DQCI, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);

    DQCI.queueFamilyIndex = i;
    DQCI.queueCount       = 1; // Just one queue from each family for now
    DQCI.pQueuePriorities = queuePriorities.data();

    bool add = false;
    if (QPROP.queueFlags & VK_QUEUE_GRAPHICS_BIT && _vkqfid_graphics == NO_QUEUE) {
      _vkqfid_graphics = i;
      add              = true;
    }

    if (QPROP.queueFlags & VK_QUEUE_COMPUTE_BIT && _vkqfid_compute == NO_QUEUE) {
      _vkqfid_compute = i;
      add             = true;
    }

    if (QPROP.queueFlags & VK_QUEUE_TRANSFER_BIT && _vkqfid_transfer == NO_QUEUE) {
      _vkqfid_transfer = i;
      add              = true;
    }
    if (add) {
      _DQCIs.push_back(DQCI);
    }
  }

  OrkAssert(_vkqfid_graphics != NO_QUEUE);
  OrkAssert(_vkqfid_compute != NO_QUEUE);
  OrkAssert(_vkqfid_transfer != NO_QUEUE);

  ////////////////////////////
  // create device
  ////////////////////////////

  _device_extensions.push_back("VK_KHR_swapchain");
  if( _GVI->_debugEnabled ) {
    _device_extensions.push_back("VK_EXT_debug_marker");
  }
  
  //_device_extensions.push_back("VK_EXT_debug_utils");

  VkDeviceCreateInfo DCI = {};
  initializeVkStruct(DCI, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  DCI.queueCreateInfoCount    = _DQCIs.size();
  DCI.pQueueCreateInfos       = _DQCIs.data();
  DCI.enabledExtensionCount   = _device_extensions.size();
  DCI.ppEnabledExtensionNames = _device_extensions.data();
  vkCreateDevice(_vkphysicaldevice, &DCI, nullptr, &_vkdevice);

  vkGetDeviceQueue(
      _vkdevice,        //
      _vkqfid_graphics, //
      0,                //
      &_vkqueue_graphics);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForWindow(VkSurfaceKHR surface) {
  OrkAssert(_GVI != nullptr);
  auto vk_devinfo = _GVI->findDeviceForSurface(surface);
  if (vk_devinfo != _GVI->_preferred) {
    _GVI->_preferred = vk_devinfo;
  }
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();

  if( _GVI->_debugEnabled ) {
    _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName,"vkSetDebugUtilsObjectNameEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT,"vkCmdDebugMarkerBeginEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT,"vkCmdDebugMarkerEndEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT,"vkCmdDebugMarkerInsertEXT");
  }

  // UGLY!!!


  for (auto ctx : _GVI->_contexts) {
    if (ctx != this) {
      ctx->_vkdevice         = _vkdevice;
      ctx->_vkphysicaldevice = _vkphysicaldevice;
      ctx->_vkqueue_graphics = _vkqueue_graphics;
      ctx->_vkqfid_graphics  = _vkqfid_graphics;
      ctx->_vkqfid_transfer  = _vkqfid_transfer;
      ctx->_vkqfid_compute   = _vkqfid_compute;

      ctx->_vkSetDebugUtilsObjectName = _vkSetDebugUtilsObjectName;
      ctx->_vkCmdDebugMarkerBeginEXT = _vkCmdDebugMarkerBeginEXT;
      ctx->_vkCmdDebugMarkerEndEXT = _vkCmdDebugMarkerEndEXT;
      ctx->_vkCmdDebugMarkerInsertEXT = _vkCmdDebugMarkerInsertEXT;

    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForOffscreen(DisplayBuffer* pBuf) {
  // TODO - this may choose a different device than the display device.
  // we need a method to choose the same device as the display device
  //  without having a surface already...
  OrkAssert(false);
  OrkAssert(_GVI != nullptr);
  if (nullptr == _GVI->_preferred) {
    _GVI->_preferred = _GVI->_device_infos.front();
  }
  auto vk_devinfo = _GVI->_preferred;
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanCommon() {
  ////////////////////////////
  // create command pools
  ////////////////////////////

  VkCommandPoolCreateInfo CPCI_GFX = {};
  initializeVkStruct(CPCI_GFX, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  CPCI_GFX.queueFamilyIndex = _vkqfid_graphics;
  CPCI_GFX.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT //
                   | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  VkResult OK = vkCreateCommandPool(_vkdevice, &CPCI_GFX, nullptr, &_vkcmdpool_graphics);
  OrkAssert(OK == VK_SUCCESS);

  ////////////////////////////
  // create command buffer impls
  ////////////////////////////

  size_t count = _cmdbuf_pool.capacity();

  for (size_t i = 0; i < count; i++) {
    auto ork_cb                          = _cmdbuf_pool.direct_access(i);
    auto vk_impl                         = ork_cb->_impl.makeShared<VkCommandBufferImpl>();
    VkCommandBufferAllocateInfo CBAI_GFX = {};
    initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CBAI_GFX.commandPool        = _vkcmdpool_graphics;
    CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CBAI_GFX.commandBufferCount = 1;

    VkResult OK = vkAllocateCommandBuffers(
        _vkdevice, //
        &CBAI_GFX, //
        &vk_impl->_vkcmdbuf);
    OrkAssert(OK == VK_SUCCESS);
  }

  VkSemaphoreCreateInfo SCI{};
  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  OK = vkCreateSemaphore(_vkdevice, &SCI, nullptr, &_fbi->_swapChainImageAcquiredSemaphore);
  OrkAssert(OK == VK_SUCCESS);

  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  OK = vkCreateSemaphore(_vkdevice, &SCI, nullptr, &_renderingCompleteSemaphore);
  OrkAssert(OK == VK_SUCCESS);

  ////////////////////////////

  VkFenceCreateInfo fenceCreateInfo = {};
  initializeVkStruct(fenceCreateInfo, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  vkCreateFence(_vkdevice, &fenceCreateInfo, nullptr, &_mainGfxSubmitFence);
}

///////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {

  _GVI->_contexts.insert(this);

  ////////////////////////////
  // create child interfaces
  ////////////////////////////

  _dwi = std::make_shared<VkDrawingInterface>(this);
  _imi = std::make_shared<VkImiInterface>(this);
  //_rsi = std::make_shared<VkRasterStateInterface>(this);
  _msi = std::make_shared<VkMatrixStackInterface>(this);
  _fbi = std::make_shared<VkFrameBufferInterface>(this);
  _gbi = std::make_shared<VkGeometryBufferInterface>(this);
  _txi = std::make_shared<VkTextureInterface>(this);
  _fxi = std::make_shared<VkFxInterface>(this);
#if defined(ENABLE_COMPUTE_SHADERS)
  _ci = std::make_shared<VkComputeInterface>(this);
#endif
}

///////////////////////////////////////////////////////

VkContext::~VkContext() {
}

///////////////////////////////////////////////////////

uint32_t VkContext::_findMemoryType(    //
    uint32_t typeFilter,                //
    VkMemoryPropertyFlags properties) { //
  VkPhysicalDeviceMemoryProperties memProperties;
  initializeVkStruct(memProperties);
  vkGetPhysicalDeviceMemoryProperties(_vkphysicaldevice, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  OrkAssert(false);
  return 0;
}

///////////////////////////////////////////////////////

void VkContext::FxInit() {
}

///////////////////////////////////////////////////////

ctx_platform_handle_t VkContext::_doClonePlatformHandle() const {
  OrkAssert(false);
  return ctx_platform_handle_t();
}

//////////////////////////////////////////////
// Interfaces

FxInterface* VkContext::FXI() {
  return _fxi.get();
}

///////////////////////////////////////////////////////

ImmInterface* VkContext::IMI() {
  return _imi.get();
}

///////////////////////////////////////////////////////
/*RasterStateInterface* VkContext::RSI() {

  return _rsi.get();
}*/
///////////////////////////////////////////////////////

MatrixStackInterface* VkContext::MTXI() {
  return _msi.get();
}
///////////////////////////////////////////////////////

GeometryBufferInterface* VkContext::GBI() {
  return _gbi.get();
}
///////////////////////////////////////////////////////

FrameBufferInterface* VkContext::FBI() {
  return _fbi.get();
}
///////////////////////////////////////////////////////

TextureInterface* VkContext::TXI() {
  return _txi.get();
}
///////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)
ComputeInterface* VkContext::CI() {
  return _ci.get();
};
#endif
///////////////////////////////////////////////////////

DrawingInterface* VkContext::DWI() {
  return _dwi.get();
}

///////////////////////////////////////////////////////////////////////

struct VkOneTimeInit {

  VkOneTimeInit() {
    _gplato             = std::make_shared<VkPlatformObject>();
    auto global_ctxbase = CtxGLFW::globalOffscreenContext();
    _gplato->_ctxbase   = global_ctxbase;
    global_ctxbase->makeCurrent();
  }
  vkplatformobject_ptr_t _gplato;
};

static vkplatformobject_ptr_t global_plato() {
  static VkOneTimeInit _ginit;
  return _ginit._gplato;
}
static vkplatformobject_ptr_t _current_plato;
static void platoMakeCurrent(vkplatformobject_ptr_t plato) {
  _current_plato = plato;
  if (plato->_ctxbase) {
    plato->_ctxbase->makeCurrent();
  }
  plato->_bindop();
}
static void platoPresent(vkplatformobject_ptr_t plato) {
  platoMakeCurrent(plato);
  if (plato->_ctxbase) {
    plato->_ctxbase->present();
  }
}

///////////////////////////////////////////////////////////////////////

void VkContext::makeCurrentContext() {
  //auto plato = _impl.getShared<VkPlatformObject>();
  //platoMakeCurrent(plato);
}

///////////////////////////////////////////////////////
 void _imageBarrier(VkCommandBuffer cmdbuf, //
                    VkImage image, //
                    VkAccessFlags srcAccessMask, //
                    VkAccessFlags dstAccessMask, //
                    VkPipelineStageFlags srcStageMask, //
                    VkPipelineStageFlags dstStageMask, //
                    VkImageLayout oldLayout, //
                    VkImageLayout newLayout) { //

  VkImageMemoryBarrier barrier = {};
  initializeVkStruct(barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);

  barrier.oldLayout           = oldLayout;
  barrier.newLayout           = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = image; // The image you want to transition.

  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;

  barrier.srcAccessMask = srcAccessMask; // Adjust as needed.
  barrier.dstAccessMask = dstAccessMask;

  vkCmdPipelineBarrier(
      cmdbuf,
      srcStageMask, 
      dstStageMask,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      &barrier);
}

///////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {

  _renderpass_index = -1;

  makeCurrentContext();

  if( _fbi->_main_rtg ){
    miW = _fbi->_main_rtg->miW;
    miH = _fbi->_main_rtg->miH;
  }

  auto mainrect = mainSurfaceRectAtOrigin();

  _fbi->setViewport(mainrect);
  _fbi->setScissor(mainrect);

  _fbi->BeginFrame();
  _gbi->BeginFrame();
  _fxi->BeginFrame();

  PushModColor(fcolor4::White());
  _msi->PushMMatrix(fmtx4::Identity());
  _msi->PushVMatrix(fmtx4::Identity());
  _msi->PushPMatrix(fmtx4::Identity());

  mpCurrentObject = 0;

  mRenderContextInstData = 0;

  ////////////////////////
  _defaultCommandBuffer = _cmdbuf_pool.allocate();

  ////////////////////////

  for (auto l : _onBeginFrameCallbacks)
    l();

  _cmdbufcurframe_gfx_pri = _defaultCommandBuffer->_impl.getShared<VkCommandBufferImpl>();

  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CBBI_GFX.pInheritanceInfo = nullptr;

  if(_first_frame){
    //beginRenderPass(_main_render_pass);
    //endRenderPass(_main_render_pass);
  }

  if( not _first_frame){
    // technically we should wait for a fence associated with the
    //  current _cmdbufcurframe_gfx_pri
    vkWaitForFences(_vkdevice, 1, &_mainGfxSubmitFence, VK_TRUE, UINT64_MAX);
  }

  vkBeginCommandBuffer(primary_cb()->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

  _cmdbufcur_gfx = primary_cb();
  _cmdbufprv_gfx = primary_cb();

  beginRenderPass(_main_render_pass);

  //FBI()->pushMainSurface();

}

vkcmdbufimpl_ptr_t VkContext::primary_cb(){
  OrkAssert(_current_subpass==nullptr);
  return _cmdbufcurframe_gfx_pri;
}

///////////////////////////////////////////////////////
void VkContext::_doEndFrame() {

  for (auto l : _onEndFrameCallbacks)
    l();

  GBI()->EndFrame();
  MTXI()->PopMMatrix();
  MTXI()->PopVMatrix();
  MTXI()->PopPMatrix();
  FBI()->EndFrame();

  //FBI()->popMainSurface();

  PopModColor();
  mbPostInitializeContext = false;
  ////////////////////////
  // intermediate subpasses
  ////////////////////////
  for( auto sp : _main_render_pass->_subpasses ){
    // todo : in parallel ?
    _beginExecuteSubPass(sp);
    _endExecuteSubPass(sp);
  }
  ////////////////////////
  endRenderPass(_main_render_pass);
  ////////////////////////

  _fbi->_enq_transitionSwapChainForPresent();

  vkEndCommandBuffer(primary_cb()->_vkcmdbuf);

  std::vector<VkSemaphore> waitStartRenderSemaphores = {_fbi->_swapChainImageAcquiredSemaphore};
  std::vector<VkPipelineStageFlags> waitStages       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.waitSemaphoreCount   = waitStartRenderSemaphores.size();
  SI.pWaitSemaphores      = waitStartRenderSemaphores.data();
  SI.pWaitDstStageMask    = waitStages.data();
  SI.commandBufferCount   = 1;
  SI.pCommandBuffers      = &primary_cb()->_vkcmdbuf;
  SI.signalSemaphoreCount = 1;
  SI.pSignalSemaphores    = &_renderingCompleteSemaphore;
  ///////////////////////////////////////////////////////

  vkResetFences(_vkdevice, 1, &_mainGfxSubmitFence);
  vkQueueSubmit(_vkqueue_graphics, 1, &SI, _mainGfxSubmitFence);

  ///////////////////////////////////////////////////////
  
  auto swapchain = _fbi->_swapchain;

  std::vector<VkSemaphore> waitPresentSemaphores = {_renderingCompleteSemaphore};

  VkPresentInfoKHR PRESI{};
  initializeVkStruct(PRESI, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  PRESI.waitSemaphoreCount = waitPresentSemaphores.size();
  PRESI.pWaitSemaphores    = waitPresentSemaphores.data();
  PRESI.swapchainCount     = 1;
  PRESI.pSwapchains        = &swapchain->_vkSwapChain;
  PRESI.pImageIndices      = &swapchain->_curSwapWriteImage;

  auto status = vkQueuePresentKHR(_vkqueue_graphics, &PRESI); // Non-Blocking
  switch (status) {
    case VK_SUCCESS:
      break;
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR: {
      // OrkAssert(false);
      //  need to recreate swap chain
      break;
    }
  }

  ///////////////////////////////////////////////////////

  _cmdbufcurframe_gfx_pri = nullptr;
   _first_frame = false;

  ///////////////////////////////////////////////////////

  _cmdbuf_pool.deallocate(_defaultCommandBuffer);
  _defaultCommandBuffer = nullptr;
  ////////////////////////

  miTargetFrame++;
 _renderpass_index = -1;
}

///////////////////////////////////////////////////////

void VkContext::present(CTXBASE* ctxbase) {
  auto plato = _impl.getShared<VkPlatformObject>();
  // platoPresent(plato);
  //_fbi->_present();
}

///////////////////////////////////////////////////////

void VkContext::_beginRenderPass(renderpass_ptr_t renpass) {

  if (renpass->_immutable) {
    OrkAssert(false);
  }

  auto main_rtg = _fbi->_main_rtg;
  auto rtg_impl = main_rtg->_impl.getShared<VkRtGroupImpl>();

  vkrenderpass_ptr_t vk_rpass;
  if( auto as_vkrpass = renpass->_impl.tryAsShared<VulkanRenderPass>() ){
    vk_rpass = as_vkrpass.value();
  }
  else{
    /////////////////////////////////////////
    // create the renderpass
    /////////////////////////////////////////

    vk_rpass = renpass->_impl.makeShared<VulkanRenderPass>(renpass.get());

    // todo - use vk_rpass->_toposorted_subpasses

    auto surfaceFormat = _vkpresentation_caps->_formats[0];

    VkAttachmentDescription CATC = {};
    initializeVkStruct(CATC);
    CATC.format = surfaceFormat.format; // Must match the format of the swap chain images.
    CATC.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling for this example.
    CATC.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear the color buffer before rendering.
    CATC.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered content for presentation.
    CATC.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't care about stencil.
    CATC.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    CATC.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    CATC.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Prepare for presentation after rendering.

    VkAttachmentDescription DATC = {};
    DATC.format = VkFormatConverter::_instance.convertBufferFormat(DEPTH_FORMAT); 
    DATC.samples = VK_SAMPLE_COUNT_1_BIT;
    DATC.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear the depth buffer before rendering.
    DATC.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DATC.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DATC.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    DATC.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DATC.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference CATR = {};
    initializeVkStruct(CATR);
    CATR.attachment = 0;
    CATR.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DATR = {};
    DATR.attachment = 1;
    DATR.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription SUBPASS = {};
    initializeVkStruct(SUBPASS);
    SUBPASS.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SUBPASS.colorAttachmentCount = 1;
    SUBPASS.pColorAttachments = &CATR;
    SUBPASS.colorAttachmentCount = 1;
    SUBPASS.pDepthStencilAttachment = &DATR;

    std::array<VkAttachmentDescription, 2> rp_attachments = { CATC, DATC };

    VkRenderPassCreateInfo RPI = {};
    initializeVkStruct(RPI,VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RPI.attachmentCount = static_cast<uint32_t>(rp_attachments.size());
    RPI.pAttachments = rp_attachments.data();
    RPI.subpassCount = 1;
    RPI.pSubpasses = &SUBPASS;
    //RPI.dependencyCount = 1;
    //RPI.pDependencies = &dependency;
    VkResult OK = vkCreateRenderPass(_vkdevice, &RPI, nullptr, &vk_rpass->_vkrp);
    OrkAssert(OK == VK_SUCCESS);
    /////////////////////////////////////////
  }
  /////////////////////////////////////////
  // is swapchain backed by a framebuffer ?
  /////////////////////////////////////////

  _fbi->_bindSwapChainToRenderPass(vk_rpass);

  /////////////////////////////////////////
  // perform the clear
  /////////////////////////////////////////
  static float phi = 0.0f;

  float R = sinf(phi * 0.1f) * 0.5f + 0.5f;
  float G = sinf(phi * 0.3f) * 0.5f + 0.5f;
  float B = sinf(phi * 0.77f) * 0.5f + 0.5f;

  VkClearValue clearValues[2];
  clearValues[0].color        = {{R,G,B, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  phi += 0.001f;

  VkRenderPassBeginInfo RPBI = {};
  initializeVkStruct(RPBI, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
  //  clear-rect-region
  RPBI.renderArea.offset = {0, 0};
  RPBI.renderArea.extent = _fbi->_swapchain->_extent;
  //  clear-targets
  RPBI.clearValueCount   = 2;
  RPBI.pClearValues      = clearValues;
  //  clear-misc
  RPBI.renderPass = vk_rpass->_vkrp;
  RPBI.framebuffer = _fbi->_swapchain->framebuffer();
  // CLEAR!
  vkCmdBeginRenderPass(
      primary_cb()->_vkcmdbuf, //
      &RPBI,                              //
      VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  /////////////////////////////////////////
  _renderpass_index++;
}

///////////////////////////////////////////////////////

void VkContext::_endRenderPass(renderpass_ptr_t renpass) {
  vkCmdEndRenderPass(primary_cb()->_vkcmdbuf);
  // OrkAssert(false);
}

///////////////////////////////////////////////////////
// begin subpass recording
///////////////////////////////////////////////////////

void VkContext::_beginSubPass(rendersubpass_ptr_t subpass) {
  OrkAssert(_current_subpass==nullptr); // no nesting...
  _current_subpass = subpass;
  //_exec_subpasses.push_back(subpass);
  // OrkAssert(false);
}

///////////////////////////////////////////////////////
// end subpass recording
///////////////////////////////////////////////////////

void VkContext::_endSubPass(rendersubpass_ptr_t subpass) {
  OrkAssert(_current_subpass==subpass); // no nesting...
  _current_subpass = nullptr;
}

///////////////////////////////////////////////////////
// begin subpass execution
///////////////////////////////////////////////////////

void VkContext::_beginExecuteSubPass(rendersubpass_ptr_t subpass){

}

///////////////////////////////////////////////////////
// end subpass execution
///////////////////////////////////////////////////////

void VkContext::_endExecuteSubPass(rendersubpass_ptr_t subpass){

}

///////////////////////////////////////////////////////

commandbuffer_ptr_t VkContext::_beginRecordCommandBuffer() {
  OrkAssert(false);
  auto cmdbuf          = std::make_shared<CommandBuffer>();
  auto vkcmdbuf        = cmdbuf->_impl.makeShared<VkCommandBufferImpl>();
  _recordCommandBuffer = cmdbuf;
  return cmdbuf;
}
void VkContext::_endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) {
  OrkAssert(false);
  OrkAssert(cmdbuf == _recordCommandBuffer);
  auto vkcmdbuf        = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  _recordCommandBuffer = nullptr;
}

///////////////////////////////////////////////////////

bool VkSwapChainCaps::supportsPresentationMode(VkPresentModeKHR mode) const {
  auto it = _presentModes.find(mode);
  return (it != _presentModes.end());
}


///////////////////////////////////////////////////////

void VkContext::initializeWindowContext(
    Window* pWin,        //
    CTXBASE* pctxbase) { //
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  vkplatformobject_ptr_t plato = std::make_shared<VkPlatformObject>();
  plato->_ctxbase              = glfw_container;
  mCtxBase                     = pctxbase;
  _impl.setShared<VkPlatformObject>(plato);
  ///////////////////////
  platoMakeCurrent(plato);
  _fbi->SetThisBuffer(pWin);
  VkResult OK = glfwCreateWindowSurface(_GVI->_instance, glfw_window, nullptr, &_vkpresentationsurface);
  OrkAssert(OK == VK_SUCCESS);

  _initVulkanForWindow(_vkpresentationsurface);

  for (uint32_t i = 0; i < _num_queue_types; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, i, _vkpresentationsurface, &presentSupport);
    printf("Qfamily<%u> on surface supports presentation<%d>\n", i, int(presentSupport));
  }

  _vkpresentation_caps = _swapChainCapsForSurface(_vkpresentationsurface);
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_IMMEDIATE_KHR));
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
  //OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_MAILBOX_KHR) );
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) );
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR) );

  _fbi->_initSwapChain();

} // make a window

///////////////////////////////////////////////////////

void VkContext::initializeOffscreenContext(DisplayBuffer* pbuffer) {
  meTargetType = TargetType::OFFSCREEN;
  miW          = pbuffer->GetBufferW();
  miH          = pbuffer->GetBufferH();
  ///////////////////////
  auto glfw_container = (CtxGLFW*)global_plato()->_ctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  vkplatformobject_ptr_t plato = std::make_shared<VkPlatformObject>();
  plato->_ctxbase              = glfw_container;
  mCtxBase                     = glfw_container;
  _impl.setShared<VkPlatformObject>(plato);
  ///////////////////////
  _initVulkanForOffscreen(pbuffer);
  ///////////////////////
  platoMakeCurrent(plato);
  _fbi->SetThisBuffer(pbuffer);
  ///////////////////////
  plato->_ctxbase   = global_plato()->_ctxbase;
  plato->_needsInit = false;
  _defaultRTG       = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb          = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture      = rtb->texture();
  _fbi->SetBufferTexture(texture);
  ///////////////////////

} // make a pbuffer

///////////////////////////////////////////////////////
void VkContext::initializeLoaderContext() {
  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  auto plato = std::make_shared<VkPlatformObject>();
  _impl.setShared<VkPlatformObject>(plato);

  plato->_ctxbase   = global_plato()->_ctxbase;
  plato->_needsInit = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
  };
}

///////////////////////////////////////////////////////

void VkContext::debugPushGroup(const std::string str, const fvec4& color) {
  if(vkCmdDebugMarkerBeginEXT){
    OrkAssert(_current_cmdbuf!=nullptr);
    auto cmdbuf_impl = _current_cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    initializeVkStruct(markerInfo,VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.color[0] = color.x;  // R
    markerInfo.color[1] = color.y;  // G
    markerInfo.color[2] = color.z;  // B
    markerInfo.color[3] = color.w;  // A
    markerInfo.pMarkerName = str.c_str();
    _vkCmdDebugMarkerBeginEXT(cmdbuf_impl->_vkcmdbuf, &markerInfo);
  }
}

///////////////////////////////////////////////////////

void VkContext::debugPopGroup() {
  if(vkCmdDebugMarkerEndEXT){
    OrkAssert(_current_cmdbuf!=nullptr);
    auto cmdbuf_impl = _current_cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    _vkCmdDebugMarkerEndEXT(cmdbuf_impl->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////

void VkContext::debugMarker(const std::string named, const fvec4& color){
  if(_vkCmdDebugMarkerInsertEXT){
    OrkAssert(_current_cmdbuf!=nullptr);
    auto cmdbuf_impl = _current_cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    initializeVkStruct(markerInfo,VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.color[0] = color.x;  // R
    markerInfo.color[1] = color.y;  // G
    markerInfo.color[2] = color.z;  // B
    markerInfo.color[3] = color.w;  // A
    markerInfo.pMarkerName = named.c_str();
    _vkCmdDebugMarkerInsertEXT(cmdbuf_impl->_vkcmdbuf, &markerInfo);
  }
}

///////////////////////////////////////////////////////

void VkContext::TakeThreadOwnership() {
}

///////////////////////////////////////////////////////

bool VkContext::SetDisplayMode(DisplayMode* mode) {
  return false;
}
///////////////////////////////////////////////////////
load_token_t VkContext::_doBeginLoad() {
  load_token_t rval = nullptr;

  while (false == _GVI->_loadTokens.try_pop(rval)) {
    usleep(1 << 10);
  }
  auto save_data = rval.getShared<VkLoadContext>();

  GLFWwindow* current_window = glfwGetCurrentContext();
  save_data->_pushedWindow   = current_window;
  // todo make global loading ctx current..
  return rval;
}
///////////////////////////////////////////////////////
void VkContext::_doEndLoad(load_token_t ploadtok) {
  auto loadctx = ploadtok.getShared<VkLoadContext>();
  auto pushed  = loadctx->_pushedWindow;
  glfwMakeContextCurrent(pushed);
  _GVI->_loadTokens.push(loadctx);
}
///////////////////////////////////////////////////////////////////////////////////////////////

vkswapchaincaps_ptr_t VkContext::_swapChainCapsForSurface(VkSurfaceKHR surface) {

  auto rval = std::make_shared<VkSwapChainCaps>();

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      _vkphysicaldevice, //
      surface,           //
      &rval->_capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      _vkphysicaldevice, //
      surface,           //
      &formatCount,      //
      nullptr);
  if (formatCount != 0) {
    rval->_formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        _vkphysicaldevice, //
        surface,           //
        &formatCount,      //
        rval->_formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      _vkphysicaldevice, //
      surface,           //
      &presentModeCount, //
      nullptr);

  printf("presentModeCount<%d>\n", presentModeCount);
  if (presentModeCount != 0) {
    std::vector<VkPresentModeKHR> presentModes;
    presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        _vkphysicaldevice, //
        surface,           //
        &presentModeCount, //
        presentModes.data());
    for (auto item : presentModes) {
      rval->_presentModes.insert(item);
    }
  }
  VkBool32 presentSupport = false;
  vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, _vkqfid_graphics, surface, &presentSupport);
  OrkAssert(presentSupport);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, //
                                     rtgroup_ptr_t rtg ) { //

  _cmdbufprv_gfx = _cmdbufcur_gfx;

  OrkAssert(_current_cmdbuf==cmdbuf);
  vkcmdbufimpl_ptr_t impl;
  if( auto as_impl = cmdbuf->_impl.tryAsShared<VkCommandBufferImpl>() ){
    impl = as_impl.value();
  }
  else{
    impl = cmdbuf->_impl.makeShared<VkCommandBufferImpl>();
    VkCommandBufferAllocateInfo CBAI_GFX = {};
    initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CBAI_GFX.commandPool        = _vkcmdpool_graphics;
    CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    CBAI_GFX.commandBufferCount = 1;

    VkResult OK = vkAllocateCommandBuffers(
        _vkdevice, //
        &CBAI_GFX, //
        &impl->_vkcmdbuf);
    OrkAssert(OK == VK_SUCCESS);

    _setObjectDebugName(impl->_vkcmdbuf, VK_OBJECT_TYPE_COMMAND_BUFFER, cmdbuf->_debugName.c_str());
  }
  VkCommandBufferInheritanceInfo INHINFO = {};
  initializeVkStruct(INHINFO,VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  auto rpimpl = _main_render_pass->_impl.getShared<VulkanRenderPass>();
  INHINFO.renderPass = rpimpl->_vkrp;  // The render pass the secondary command buffer will be executed within.
  INHINFO.subpass = 0;  // The index of the subpass in the render pass.
  if( rtg ){
    INHINFO.subpass = 0;  // The index of the subpass in the render pass.
    OrkAssert(false);
  }
  INHINFO.framebuffer = VK_NULL_HANDLE;  // Optional: The framebuffer targeted by the render pass. Can be VK_NULL_HANDLE if not provided.
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
  auto impl = _current_cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  vkEndCommandBuffer(impl->_vkcmdbuf);
  _cmdbufcur_gfx = _cmdbufprv_gfx;
}

void VkContext::_doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) {
  auto impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  vkCmdExecuteCommands(_cmdbufcurframe_gfx_pri->_vkcmdbuf, 1, &impl->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doResizeMainSurface(int iw, int ih) {
  scheduleOnBeginFrame([this,iw,ih](){
    _fbi->_main_rtg->Resize(iw, ih);
  });
}

VulkanRenderPass::VulkanRenderPass(RenderPass* rpass){
  // topological sort of renderpass's subpasses
  //  to determine execution order

  std::set<rendersubpass_ptr_t> subpass_set;

  std::function<void(rendersubpass_ptr_t)> visit_subpass;

  visit_subpass = [&](rendersubpass_ptr_t subp){
    for( auto dep : subp->_subpass_dependencies ){
      if( subpass_set.find(dep) == subpass_set.end() ){
        subpass_set.insert(dep);
        visit_subpass(dep);
      }
    }
    _toposorted_subpasses.push_back(subp.get());
  };

  // visit top
  for(auto subp : rpass->_subpasses){
    visit_subpass(subp);
  }

}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
