////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

ImplementReflectionX(ork::lev2::vulkan::VkContext, "VkContext");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkctx = logger()->createChannel("VKCTX", fvec3(1,1,.9));

void VkContext::describeX(class_t* clazz) {

  clazz->annotateTyped<context_factory_t>("context_factory", []() { //
    return std::make_shared<VkContext>();
  });
}

///////////////////////////////////////////////////////

bool VkContext::HaveExtension(const std::string& extname) {
  return false;
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
  if (_GVI->_debugEnabled) {
    _device_extensions.push_back("VK_EXT_debug_marker");
  }
  _device_extensions.push_back("VK_KHR_portability_subset");
  _device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
  //_device_extensions.push_back("VK_EXT_debug_utils");

  VkDeviceCreateInfo DCI = {};
  initializeVkStruct(DCI, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  DCI.queueCreateInfoCount    = _DQCIs.size();
  DCI.pQueueCreateInfos       = _DQCIs.data();
  DCI.enabledExtensionCount   = _device_extensions.size();
  DCI.ppEnabledExtensionNames = _device_extensions.data();

  // add features (not extensions)

  VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
  VkPhysicalDeviceDynamicRenderingFeatures dynrenderfeat{};

  initializeVkStruct(timelineFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
  initializeVkStruct(dynrenderfeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);

  timelineFeatures.timelineSemaphore = VK_TRUE;
  dynrenderfeat.dynamicRendering = VK_TRUE;

  DCI.pNext = (void*) & timelineFeatures;
  timelineFeatures.pNext = (void*) & dynrenderfeat;
  dynrenderfeat.pNext = (void*) nullptr;


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

  if (_GVI->_debugEnabled) {
    _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT, "vkCmdDebugMarkerBeginEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT, "vkCmdDebugMarkerEndEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT, "vkCmdDebugMarkerInsertEXT");
  }

  _fetchDeviceProcAddr( _vkCmdBeginRenderingKHR,"vkCmdBeginRenderingKHR");
  _fetchDeviceProcAddr( _vkCmdEndRenderingKHR,"vkCmdEndRenderingKHR");
   OrkAssertI(_vkCmdBeginRenderingKHR != nullptr, "_vkCmdBeginRenderingKHR function pointer is null!");
   OrkAssertI(_vkCmdEndRenderingKHR != nullptr, "_vkCmdEndRenderingKHR function pointer is null!");

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
      ctx->_vkCmdDebugMarkerBeginEXT  = _vkCmdDebugMarkerBeginEXT;
      ctx->_vkCmdDebugMarkerEndEXT    = _vkCmdDebugMarkerEndEXT;
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
  // create primary command buffer impls
  ////////////////////////////

  size_t count = _pri_cmdbuf_pool.capacity();

  for (size_t i = 0; i < count; i++) {
    auto ork_cb         = _pri_cmdbuf_pool.direct_access(i);
    auto vk_impl        = _createPrimaryVkCommandBuffer(ork_cb.get());
  }

  auto vksci_base = makeVKSCI();
  _sampler_base   = std::make_shared<VulkanSamplerObject>(this, vksci_base);

  _sampler_per_maxlod.resize(16);
  for (size_t maxlod = 0; maxlod < 16; maxlod++) {
    auto vksci                  = makeVKSCI();
    vksci->maxLod               = maxlod;
    _sampler_per_maxlod[maxlod] = std::make_shared<VulkanSamplerObject>(this, vksci);
  }

  // create descriptor pool
  std::vector<VkDescriptorPoolSize> poolSizes;

  constexpr size_t DESCRIPTORSET_COUNT = 4096;

  auto& poolsize_combsamplers           = poolSizes.emplace_back();
  poolsize_combsamplers.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolsize_combsamplers.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  auto& poolsize_samplers           = poolSizes.emplace_back();
  poolsize_samplers.type            = VK_DESCRIPTOR_TYPE_SAMPLER;
  poolsize_samplers.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  auto& poolsize_sampled_images           = poolSizes.emplace_back();
  poolsize_sampled_images.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  poolsize_sampled_images.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  auto& poolsize_uniform_buffers           = poolSizes.emplace_back();
  poolsize_uniform_buffers.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolsize_uniform_buffers.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  auto& poolsize_storage_buffers           = poolSizes.emplace_back();
  poolsize_storage_buffers.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolsize_storage_buffers.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  VkDescriptorPoolCreateInfo poolInfo = {};
  initializeVkStruct(poolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
  poolInfo.poolSizeCount = poolSizes.size();
  poolInfo.pPoolSizes    = poolSizes.data();
  poolInfo.maxSets       = DESCRIPTORSET_COUNT; // Maximum number of descriptor sets to allocate from this pool

  OK = vkCreateDescriptorPool(_vkdevice, &poolInfo, nullptr, &_vkDescriptorPool);
  OrkAssert(OK == VK_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {
  _present_timer.Start();
  _prev_time = 0.0f;
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
  _ci  = std::make_shared<VkComputeInterface>(this);
}

///////////////////////////////////////////////////////

VkContext::~VkContext() {
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

ComputeInterface* VkContext::CI() {
  return _ci.get();
};

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
  // auto plato = _impl.getShared<VkPlatformObject>();
  // platoMakeCurrent(plato);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doPreBeginFrame() {
  //logchan_vkctx->log("VkContext<%p> _doPreBeginFrame", (void*)this );

  mpCurrentObject        = 0;
  mRenderContextInstData = 0;

  ////////////////////////
  // Check if command buffer pool is healthy
  ////////////////////////

  // logchan_vkctx->log("  Allocating command buffer from pool (available: %zu)\n", _pri_cmdbuf_pool.available());
  _defaultCommandBuffer = _pri_cmdbuf_pool.allocate();

  ////////////////////////
  _defaultCommandBufferImpl = _defaultCommandBuffer->_impl.getShared<VkPrimaryCommandBufferImpl>();
  _cmdbufcurpri_gfx         = _defaultCommandBufferImpl;
  ////////////////////////

  //logchan_vkctx->log("VkContext<%p> begin primaryCB", (void*)this );

  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CBBI_GFX.pInheritanceInfo = nullptr;
  vkBeginCommandBuffer(primary_cb()->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

  /////////////////////////////////////////
  for (auto one_shot : _pendingOneShotCommands) {
    enqueueSecondaryCommandBuffer(one_shot);
  }  
  _pendingOneShotCommands.clear();
  /////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  //logchan_vkctx->log("VkContext<%p> _doBeginFrame w<%d> h<%d>", (void*)this, miW, miH);
  if (_fbi->_main_rtg) {
    miW = _fbi->_main_rtg->miW;
    miH = _fbi->_main_rtg->miH;
  }
  // Poll timeline semaphores
  for (auto semaphore : _pendingOneShotSemas) {
    if(semaphore->isSignalled()){
      // If the semaphore is signalled, execute its completion callback      
      if(semaphore->_onComplete!=nullptr){
        // If the semaphore has a completion callback, execute it
        semaphore->_onComplete();
        semaphore->_onComplete = nullptr; // Clear the callback after execution
      }
    }
  }
  
  // Clean up completed semaphores
  std::erase_if(          //
    _pendingOneShotSemas, //
    [](auto sema) { //
      return sema->_onComplete==nullptr; //
  });

}

///////////////////////////////////////////////////////////////////////////////

vkpricmdbufimpl_ptr_t VkContext::primary_cb() {
  return _cmdbufcurpri_gfx;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndFrame() {
  
  ////////////////////////
  // main_rtg -> presentation layout
  ////////////////////////

  auto main_rtb  = _fbi->_main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();

  main_rtbi->_transitionToPresent(primary_cb());

  ////////////////////////
  // done with primary command buffer for this frame
  ////////////////////////

  primary_cb()->_recorded = true;
  vkEndCommandBuffer(primary_cb()->_vkcmdbuf);

  ////////////////////////

  // logchan_vkctx->log( "num renderpasses<%zu>\n", _renderpasses.size() );

  ///////////////////////////////////////////////////////
  // submit primary command buffer for this frame
  ///////////////////////////////////////////////////////

  auto swapchain = _fbi->_swapchain;
  
  if ( not _pendingOneShotSemas.empty()) {
    // Submit with timeline semaphores
    swapchain->_submitFrameWithSemaphores(this);
  } else {
    // Normal submission
    swapchain->enqueueFrame(this);
  }
  
  ///////////////////////////////////////////////////////
  // Present !
  ///////////////////////////////////////////////////////

  swapchain->enqueuePresentFrame(this);
  swapchain->waitPresentFrame(this);

  ///////////////////////////////////////////////////////

  _pri_cmdbuf_pool.deallocate(_defaultCommandBuffer);
  _cmdbufcurpri_gfx->_secondary_cmdbuffers.clear();

  ////////////////////////


  ///////////////////////////////////////////////////////
  //logchan_vkctx->log("VkContext<%p> clear renderpasses", (void*)this );

  _defaultCommandBuffer = nullptr;
  _cmdbufcurpri_gfx = nullptr;
  _first_frame            = false;

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

  uint32_t count;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);
  logchan_vkctx->log("GLFW requires %u extensions for surface:", count);
  for (uint32_t i = 0; i < count; i++) {
    logchan_vkctx->log("  - %s", extensions[i]);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  //logchan_vkctx->log("createWindowSurface with instance<%p>", (void*)&_GVI->_instance);

  VkResult OK = glfwCreateWindowSurface(_GVI->_instance, glfw_window, nullptr, &_vkpresentationsurface);
  OrkAssert(OK == VK_SUCCESS);

  _initVulkanForWindow(_vkpresentationsurface);

  for (uint32_t i = 0; i < _num_queue_types; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, i, _vkpresentationsurface, &presentSupport);
    logchan_vkctx->log("Qfamily<%u> on surface supports presentation<%d>", i, int(presentSupport));
  }

  _vkpresentation_caps = _swapChainCapsForSurface(_vkpresentationsurface);
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_IMMEDIATE_KHR));
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
  // OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
  //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_MAILBOX_KHR) );
  //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) );
  //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR) );

  _fbi->_swapchain = std::make_shared<VkSwapChain>(this);

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
      // logchan_vkctx->log("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
  };
}

///////////////////////////////////////////////////////

void VkContext::debugPushGroup(const std::string str, const fvec4& color) {
  if (_vkCmdDebugMarkerBeginEXT) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    initializeVkStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.color[0]    = color.x; // R
    markerInfo.color[1]    = color.y; // G
    markerInfo.color[2]    = color.z; // B
    markerInfo.color[3]    = color.w; // A
    markerInfo.pMarkerName = str.c_str();
    _vkCmdDebugMarkerBeginEXT(_cmdbufcurpri_gfx->_vkcmdbuf, &markerInfo);
  }
}

///////////////////////////////////////////////////////

void VkContext::debugPopGroup() {
  if (_vkCmdDebugMarkerEndEXT) {
    _vkCmdDebugMarkerEndEXT(_cmdbufcurpri_gfx->_vkcmdbuf);
  }
}
///////////////////////////////////////////////////////

void VkContext::debugPushGroup(secondary_commandbuffer_ptr_t cb, const std::string str, const fvec4& color) {
  if (_vkCmdDebugMarkerBeginEXT) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    initializeVkStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.color[0]    = color.x; // R
    markerInfo.color[1]    = color.y; // G
    markerInfo.color[2]    = color.z; // B
    markerInfo.color[3]    = color.w; // A
    markerInfo.pMarkerName = str.c_str();

    auto cbimpl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();

    _vkCmdDebugMarkerBeginEXT(cbimpl->_vkcmdbuf, &markerInfo);
  }
}

///////////////////////////////////////////////////////

void VkContext::debugPopGroup(secondary_commandbuffer_ptr_t cb) {
  if (_vkCmdDebugMarkerEndEXT) {
    auto cbimpl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();
    _vkCmdDebugMarkerEndEXT(cbimpl->_vkcmdbuf);
  }
}

///////////////////////////////////////////////////////

void VkContext::debugMarker(const std::string named, const fvec4& color) {
  if (_vkCmdDebugMarkerInsertEXT) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    initializeVkStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.color[0]    = color.x; // R
    markerInfo.color[1]    = color.y; // G
    markerInfo.color[2]    = color.z; // B
    markerInfo.color[3]    = color.w; // A
    markerInfo.pMarkerName = named.c_str();
    _vkCmdDebugMarkerInsertEXT(_cmdbufcurpri_gfx->_vkcmdbuf, &markerInfo);
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

  logchan_vkctx->log("presentModeCount<%d>", presentModeCount);
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

void VkContext::_doResizeMainSurface(int iw, int ih) {
  scheduleOnBeginFrame([this, iw, ih]() {
    logchan_vkctx->log("VkContext<%p> _doResizeMainSurface w<%d> h<%d>", (void*)this, iw, ih);
    if (_fbi->_main_rtg) {
      _fbi->_main_rtg->Resize(iw, ih);
    }
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
