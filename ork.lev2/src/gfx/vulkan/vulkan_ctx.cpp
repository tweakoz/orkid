////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#import <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>

ImplementReflectionX(ork::lev2::vulkan::VkContext, "VkContext");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

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
void VkContext::_initVulkanForDevInfo(vkdeviceinfo_ptr_t vk_devinfo){
  _vkphysicaldevice = vk_devinfo->_phydev;
  _vkdeviceinfo     = vk_devinfo;

  ////////////////////////////
  // get queue families
  ////////////////////////////

  using qset_t = std::set<uint32_t>;

  _num_queue_types = vk_devinfo->_queueprops.size();
  std::map<int, qset_t> qids;
  for (size_t i = 0; i < _num_queue_types; i++) {
    auto& qprops = vk_devinfo->_queueprops[i];
    auto qflags  = qprops.queueFlags;
    if (qflags & VK_QUEUE_GRAPHICS_BIT) {
      if (qprops.queueCount > 0) {
        qids[i].insert(VK_QUEUE_GRAPHICS_BIT);
        _vkqfid_graphics = i;
      }
    }
    if (qflags & VK_QUEUE_COMPUTE_BIT) {
      if (qprops.queueCount > 0) {
        // qids[i].insert(VK_QUEUE_COMPUTE_BIT);
        //_vkqfid_compute = i;
      }
    }
    if (qflags & VK_QUEUE_TRANSFER_BIT) {
      if (qprops.queueCount > 0) {
        // qids[i].insert(VK_QUEUE_TRANSFER_BIT);
        //_vkqfid_transfer = i;
      }
    }
  }

  OrkAssert(_vkqfid_graphics != NO_QUEUE);

  // OrkAssert(_vkq_compute!=NO_QUEUE);
  // OrkAssert(_vkq_transfer!=NO_QUEUE);

  _queuePriorities.push_back(1.0f);

  _DQCIs.reserve(qids.size());
  for (auto item : qids) {
    int q_index = item.first;
    for (auto q_type : item.second) {
      VkDeviceQueueCreateInfo& DQCI = _DQCIs.emplace_back();
      initializeVkStruct(DQCI, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
      DQCI.queueFamilyIndex = q_index;
      DQCI.queueCount       = 1;
      DQCI.pQueuePriorities = _queuePriorities.data();
    }
  }

  ////////////////////////////
  // create device
  ////////////////////////////

  _device_extensions.push_back("VK_KHR_swapchain");

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

void VkContext::_initVulkanForWindow(VkSurfaceKHR surface){
  OrkAssert(_GVI != nullptr);
  auto vk_devinfo   = _GVI->findDeviceForSurface(surface);
  if(vk_devinfo!=_GVI->_preferred){
    _GVI->_preferred = vk_devinfo;
  }
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForOffscreen(DisplayBuffer* pBuf){
  // TODO - this may choose a different device than the display device.
  // we need a method to choose the same device as the display device
  //  without having a surface already...
  OrkAssert(false);
  OrkAssert(_GVI != nullptr);
  if( nullptr == _GVI->_preferred ){
    _GVI->_preferred = _GVI->_device_infos.front();
  }
  auto vk_devinfo   = _GVI->_preferred;
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanCommon(){
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
  OK = vkCreateSemaphore(_vkdevice, &SCI, nullptr, &_swapChainImageAcquiredSemaphore);
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
struct VkPlatformObject;
using vkplatformobject_ptr_t = std::shared_ptr<VkPlatformObject>;

struct VkPlatformObject {
  CtxGLFW* _ctxbase     = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop = []() {};
};
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
  auto plato = _impl.getShared<VkPlatformObject>();
  platoMakeCurrent(plato);
}

///////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  makeCurrentContext();

  _cmdbufcurframe_gfx_pri = _defaultCommandBuffer->_impl.getShared<VkCommandBufferImpl>();

  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  //| VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  CBBI_GFX.pInheritanceInfo = nullptr;

  // vkResetCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf, 0); // vkBeginCommandBuffer does an implicit reset
  vkBeginCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf, &CBBI_GFX);

  ///////////////////////////////////////////////////
  // Get SwapChain Image
  ///////////////////////////////////////////////////

  _curSwapWriteImage = 0xffffffff;
  VkResult status    = vkAcquireNextImageKHR(
      _vkdevice,                            //
      _vkSwapChain,                         //
      std::numeric_limits<uint64_t>::max(), // timeout (ns)
      _swapChainImageAcquiredSemaphore,     //
      VK_NULL_HANDLE,                       // no fence
      &_curSwapWriteImage                   //
  );

  switch (status) {
    case VK_SUCCESS:
      break;
    case VK_SUBOPTIMAL_KHR:
      printf("VK_SUBOPTIMAL_KHR\n");
      break;
    case VK_ERROR_OUT_OF_DATE_KHR: {
      OrkAssert(false);
      // need to recreate swap chain
      break;
    }
  }

  auto& swap_image = _vkSwapChainImages[_curSwapWriteImage];

  ///////////////////////////////////////////////////
  // transition SwapChain Image
  ///////////////////////////////////////////////////

  VkImageMemoryBarrier barrier = {};
  initializeVkStruct(barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);

  barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = swap_image; // The image you want to transition.

  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;

  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Adjust as needed.
  barrier.dstAccessMask = 0;

  vkCmdPipelineBarrier(
      _cmdbufcurframe_gfx_pri->_vkcmdbuf,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Adjust as needed.
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      &barrier);
}

///////////////////////////////////////////////////////
void VkContext::_doEndFrame() {
  vkEndCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf);

  VkSemaphore waitStartRenderSemaphores[] = {_swapChainImageAcquiredSemaphore};

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.waitSemaphoreCount   = 1;
  SI.pWaitSemaphores      = waitStartRenderSemaphores;
  SI.commandBufferCount   = 1;
  SI.pCommandBuffers      = &_cmdbufcurframe_gfx_pri->_vkcmdbuf;
  SI.signalSemaphoreCount = 1;
  SI.pSignalSemaphores    = &_renderingCompleteSemaphore;
  ///////////////////////////////////////////////////////

  vkQueueSubmit(_vkqueue_graphics, 1, &SI, _mainGfxSubmitFence);
  // vkQueueWaitIdle(_vkqueue_graphics);

  ///////////////////////////////////////////////////////

  VkSemaphore waitPresentSemaphores[] = {_renderingCompleteSemaphore};

  VkPresentInfoKHR PRESI{};
  initializeVkStruct(PRESI, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  PRESI.waitSemaphoreCount = 1;
  PRESI.pWaitSemaphores    = waitPresentSemaphores;
  PRESI.swapchainCount     = 1;
  PRESI.pSwapchains        = &_vkSwapChain;
  PRESI.pImageIndices      = &_curSwapWriteImage;

  auto status = vkQueuePresentKHR(_vkqueue_graphics, &PRESI); // Non-Blocking
  switch (status) {
    case VK_SUCCESS:
      break;
    case VK_SUBOPTIMAL_KHR:
      printf("VK_SUBOPTIMAL_KHR\n");
      break;
    case VK_ERROR_OUT_OF_DATE_KHR: {
      OrkAssert(false);
      // need to recreate swap chain
      break;
    }
  }

  ///////////////////////////////////////////////////////
  // wait while commandbuffer is "pending"
  ///////////////////////////////////////////////////////

  vkWaitForFences(_vkdevice, 1, &_mainGfxSubmitFence, VK_TRUE, UINT64_MAX);
  vkResetFences(_vkdevice, 1, &_mainGfxSubmitFence);

  ///////////////////////////////////////////////////////

  _cmdbufcurframe_gfx_pri = nullptr;
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
}

void VkContext::_endRenderPass(renderpass_ptr_t renpass) {
  // OrkAssert(false);
}
void VkContext::_beginSubPass(rendersubpass_ptr_t subpass) {
  // OrkAssert(false);
}

void VkContext::_endSubPass(rendersubpass_ptr_t rubpass) {
  // OrkAssert(false);
}

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
      printf( "Qfamily<%u> on surface supports presentation<%d>\n", i, int(presentSupport) );
  }

  _vkpresentation_caps = _swapChainCapsForSurface(_vkpresentationsurface);
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_IMMEDIATE_KHR));
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_MAILBOX_KHR) );
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) );
  // OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR) );

  auto surfaceFormat = _vkpresentation_caps->_formats[0];

  VkSwapchainCreateInfoKHR SCINFO{};
  initializeVkStruct(SCINFO, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  SCINFO.surface = _vkpresentationsurface;

  VkExtent2D extent;
  extent.width  = pWin->miWidth;
  extent.height = pWin->miHeight;

  // image properties
  SCINFO.minImageCount    = 3;
  SCINFO.imageFormat      = surfaceFormat.format;                // Chosen from VkSurfaceFormatKHR, after querying supported formats
  SCINFO.imageColorSpace  = surfaceFormat.colorSpace;            // Chosen from VkSurfaceFormatKHR
  SCINFO.imageExtent      = extent;                              // The width and height of the swap chain images
  SCINFO.imageArrayLayers = 1;                                   // Always 1 unless developing a stereoscopic 3D application
  SCINFO.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Or any other value depending on your needs

  // image view properties
  SCINFO.imageSharingMode =
      VK_SHARING_MODE_EXCLUSIVE;          // Can be VK_SHARING_MODE_CONCURRENT if sharing between multiple queue families
  SCINFO.queueFamilyIndexCount = 0;       // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT
  SCINFO.pQueueFamilyIndices   = nullptr; // Only relevant if sharingMode is VK_SHARING_MODE_CONCURRENT

  // misc properties
  SCINFO.preTransform   = _vkpresentation_caps->_capabilities.currentTransform;
  SCINFO.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // todo - support alpha
  SCINFO.clipped        = VK_TRUE;                           // clip pixels that are obscured by other windows
  SCINFO.oldSwapchain   = VK_NULL_HANDLE;
  SCINFO.presentMode    = VK_PRESENT_MODE_IMMEDIATE_KHR;

  OK = vkCreateSwapchainKHR(_vkdevice, &SCINFO, nullptr, &_vkSwapChain);
  OrkAssert(OK == VK_SUCCESS);

  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(_vkdevice, _vkSwapChain, &imageCount, nullptr);
  _vkSwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(_vkdevice, _vkSwapChain, &imageCount, _vkSwapChainImages.data());

  _vkSwapChainImageViews.resize(_vkSwapChainImages.size());

  for (size_t i = 0; i < _vkSwapChainImages.size(); i++) {
    VkImageViewCreateInfo IVCI{};
    initializeVkStruct(IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    IVCI.image                           = _vkSwapChainImages[i];
    IVCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    IVCI.format                          = surfaceFormat.format;
    IVCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    IVCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    IVCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    IVCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    IVCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    IVCI.subresourceRange.baseMipLevel   = 0;
    IVCI.subresourceRange.levelCount     = 1;
    IVCI.subresourceRange.baseArrayLayer = 0;
    IVCI.subresourceRange.layerCount     = 1;

    OK = vkCreateImageView(_vkdevice, &IVCI, nullptr, &_vkSwapChainImageViews[i]);
    OrkAssert(OK == VK_SUCCESS);
  }

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

void VkContext::debugPushGroup(const std::string str) {
}

///////////////////////////////////////////////////////

void VkContext::debugPopGroup() {
}

///////////////////////////////////////////////////////

void VkContext::debugMarker(const std::string str) {
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
  // loadctx->makeCurrentContext();
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
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
