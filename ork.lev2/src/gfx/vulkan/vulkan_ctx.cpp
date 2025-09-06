////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_captureasync.h"
#include "vulkan_ubo_dynamic.h"
#include <ork/lev2/gfx/image.h>

#define USE_OIIO
#if defined(USE_OIIO)
#include <OpenImageIO/imageio.h>
OIIO_NAMESPACE_USING
#endif

ImplementReflectionX(ork::lev2::vulkan::VkContext, "VkContext");

namespace ork::lev2 {
  extern appinitdata_ptr_t _ginitdata;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkctx = logger()->configureChannel("VKCTX", fvec3(1,1,.9),false);
static logchannel_ptr_t logchan_vkcap = logger()->configureChannel("VKCAPTURE", fvec3(1,1,.9),true);

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
  
  // Load device function pointers needed for rendering
  // These are needed for both window and offscreen contexts
  if (_GVI->_debugEnabled) {
    _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT, "vkCmdDebugMarkerBeginEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT, "vkCmdDebugMarkerEndEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT, "vkCmdDebugMarkerInsertEXT");
    _fetchDeviceProcAddr(_vkCmdInsertDebugUtilsLabelEXT, "vkCmdInsertDebugUtilsLabelEXT");
  }

  _fetchDeviceProcAddr(_vkCmdBeginRenderingKHR, "vkCmdBeginRenderingKHR");
  _fetchDeviceProcAddr(_vkCmdEndRenderingKHR, "vkCmdEndRenderingKHR");
  OrkAssertI(_vkCmdBeginRenderingKHR != nullptr, "_vkCmdBeginRenderingKHR function pointer is null!");
  OrkAssertI(_vkCmdEndRenderingKHR != nullptr, "_vkCmdEndRenderingKHR function pointer is null!");
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForWindow(VkSurfaceKHR surface) {
  OrkAssert(_GVI != nullptr);
  auto vk_devinfo = _GVI->findDeviceForSurface(surface);
  if (vk_devinfo != _GVI->_preferred) {
    _GVI->_preferred = vk_devinfo;
  }

  // UGLY!!!
  if(_GVI->_contexts.size()>=1){
    auto context0 = *_GVI->_contexts.begin();
    _vkdevice = context0->_vkdevice;
    _vkdeviceinfo = context0->_vkdeviceinfo;
    _vkphysicaldevice = context0->_vkphysicaldevice;
    _vkqueue_graphics = context0->_vkqueue_graphics;
    _vkqfid_graphics = context0->_vkqfid_graphics;
    _vkqfid_transfer = context0->_vkqfid_transfer;
    _vkqfid_compute = context0->_vkqfid_compute;
    _vkSetDebugUtilsObjectName = context0->_vkSetDebugUtilsObjectName;
    _vkCmdDebugMarkerBeginEXT = context0->_vkCmdDebugMarkerBeginEXT;
    _vkCmdDebugMarkerEndEXT = context0->_vkCmdDebugMarkerEndEXT;
    _vkCmdDebugMarkerInsertEXT = context0->_vkCmdDebugMarkerInsertEXT;
    _vkCmdBeginRenderingKHR = context0->_vkCmdBeginRenderingKHR;
    _vkCmdEndRenderingKHR = context0->_vkCmdEndRenderingKHR;

    _device_extensions = context0->_device_extensions;
    _num_queue_types = context0->_num_queue_types;
    _DQCIs = context0->_DQCIs;
    _initVulkanCommon();
  }
  else{
    _initVulkanForDevInfo(vk_devinfo);
    _initVulkanCommon();
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForOffscreen(DisplayBuffer* pBuf) {
  // TODO - this may choose a different device than the display device.
  // we need a method to choose the same device as the display device
  //  without having a surface already...
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

  // Initialize sampler cache
  _sampler_cache.clear();

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
  
  ////////////////////////////
  // create default texture implementations
  ////////////////////////////
  
  _initDefaultTextures();
  
  ////////////////////////////
  // Initialize dynamic UBO system
  ////////////////////////////
  
  extern VkDynamicUBOSystem* g_dynamic_ubo_system;
  if (!g_dynamic_ubo_system) {
    g_dynamic_ubo_system = new VkDynamicUBOSystem();
    g_dynamic_ubo_system->init(this);
    printf("VkContext: Initialized dynamic UBO system\n");
  }
}

  void VkContext::_beginAssetProcessing() {
    beginFrame();
  }
  void VkContext::_endAssetProcessing(){
    endFrame();
  }

  ///////////////////////////////////////////////////////////////////////////////

void VkContext::_initDefaultTextures() {
  // Create black default textures for each type
  auto create_default_texture = [this](ETextureType tex_type, int width, int height, int depth = 1, int num_layers = 1) -> vktexobj_ptr_t {
    auto tex_obj = std::make_shared<VulkanTextureObject>(_txi.get());
    
    // Calculate mip levels based on dimensions
    int num_mips = 1;
    if (tex_type == ETEXTYPE_3D) {
      num_mips = 1 + static_cast<int>(std::floor(std::log2(std::min({width, height, depth}))));
    } else {
      num_mips = 1 + static_cast<int>(std::floor(std::log2(std::min(width, height))));
    }
    
    // Create image create info
    auto imageInfo = std::make_shared<VkImageCreateInfo>();
    initializeVkStruct(*imageInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    imageInfo->format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo->extent.width = width;
    imageInfo->extent.height = height;
    imageInfo->extent.depth = depth;
    imageInfo->mipLevels = num_mips;
    imageInfo->arrayLayers = num_layers;
    imageInfo->samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo->tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    switch (tex_type) {
      case ETEXTYPE_2D:
        imageInfo->imageType = VK_IMAGE_TYPE_2D;
        break;
      case ETEXTYPE_CUBE:
        imageInfo->imageType = VK_IMAGE_TYPE_2D;
        imageInfo->flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo->arrayLayers = 6;
        break;
      case ETEXTYPE_2D_ARRAY:
        imageInfo->imageType = VK_IMAGE_TYPE_2D;
        imageInfo->arrayLayers = num_layers;
        break;
      case ETEXTYPE_3D:
        imageInfo->imageType = VK_IMAGE_TYPE_3D;
        break;
      default:
        OrkAssert(false);
    }
    
    // Create the image object
    tex_obj->_imgobj = std::make_shared<VulkanImageObject>(this, imageInfo, "default_texture");
    
    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    viewInfo.image = tex_obj->_imgobj->_vkimage;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = num_mips;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    
    // Set view type and layer count based on texture type
    switch (tex_type) {
      case ETEXTYPE_2D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.layerCount = 1;
        break;
      case ETEXTYPE_CUBE:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.subresourceRange.layerCount = 6;
        break;
      case ETEXTYPE_2D_ARRAY:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.subresourceRange.layerCount = num_layers;
        break;
      case ETEXTYPE_3D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.subresourceRange.layerCount = 1;
        break;
      default:
        OrkAssert(false);
    }
    
    VkResult ok = vkCreateImageView(_vkdevice, &viewInfo, nullptr, &tex_obj->_imgobj->_vkimageview);
    OrkAssert(VK_SUCCESS == ok);
    
    // Initialize with black data (we'll need to transition and fill the texture)
    // For now, just transition to shader read optimal
    auto cmdbuf = _beginRecordCommandBuffer("init_default_texture", nullptr);
    auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
    
    VkImageMemoryBarrier barrier = {};
    initializeVkStruct(barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex_obj->_imgobj->_vkimage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = num_mips;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = (tex_type == ETEXTYPE_CUBE) ? 6 : num_layers;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(
        cmdbuf_impl->_vkcmdbuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    
    _endRecordCommandBuffer(cmdbuf);
    enqueueDeferredOneShotCommand(cmdbuf);
    
    // Set up descriptor info
    tex_obj->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    tex_obj->_vkdescriptor_info.imageView = tex_obj->_imgobj->_vkimageview;
    tex_obj->_vkdescriptor_info.sampler = _sampler_base->_vksampler;
    
    return tex_obj;
  };
  
  // Create default textures with agreed-upon sizes
  _defaultTexImpl2D = create_default_texture(ETEXTYPE_2D, 64, 64);
  _defaultTexImplCube = create_default_texture(ETEXTYPE_CUBE, 64, 64);
  _defaultTexImpl2DArray = create_default_texture(ETEXTYPE_2D_ARRAY, 64, 64, 1, 4); // 4 layers
  _defaultTexImpl3D = create_default_texture(ETEXTYPE_3D, 16, 16, 16);
}

///////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {
  _present_timer.Start();
  _prev_time = 0.0f;
  _GVI->_contexts.push_back(this);

  ////////////////////////////
  // Setup rendering conventions for Vulkan
  ////////////////////////////
  _renderingConventions._isRightHanded = true;      // Orkid uses RH like GL
  _renderingConventions._isLogicalYUp = true;       // Emulating GL Y-up
  _renderingConventions._isNativeYUp = false;       // Vulkan native is Y-down
  _renderingConventions._ndcZRange01 = true;        // Vulkan native [0,1]
  _renderingConventions._defaultWindingCCW = true;  // Default CCW like GL
  // When FLIP_Y_LIKE_OPENGL is true, Y-flip reverses winding order
  _renderingConventions._frontFaceWindingCCW = !FLIP_Y_LIKE_OPENGL;
  
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

void VkContext::_doBeginPrimaryCommandBuffer() {
  ////////////////////////
  // Check if command buffer pool is healthy
  ////////////////////////
  // logchan_vkctx->log("  Allocating command buffer from pool (available: %zu)", _pri_cmdbuf_pool.available());
  _defaultCommandBuffer = _pri_cmdbuf_pool.allocate();

  ////////////////////////
  _defaultCommandBufferImpl = _defaultCommandBuffer->_impl.getShared<VkPrimaryCommandBufferImpl>();
  _cmdbufcurpri_gfx         = _defaultCommandBufferImpl;
  _vkcmdbuffer_current      = _cmdbufcurpri_gfx->_vkcmdbuf; // Initialize current command buffer to primary
  ////////////////////////
  logchan_vkctx->log("CMDBUF: _doPreBeginFrame: setting primary CB to %p", _cmdbufcurpri_gfx ? (void*)_cmdbufcurpri_gfx->_vkcmdbuf : nullptr);
  //logchan_vkctx->log("VkContext<%p> begin primaryCB", (void*)this );
  ////////////////////////
  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CBBI_GFX.pInheritanceInfo = nullptr;
  vkBeginCommandBuffer(primary_cb()->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndPrimaryCommandBuffer() {
  primary_cb()->_recorded = true;
  vkEndCommandBuffer(primary_cb()->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doSubmitPrimaryCommandBuffer(){

  auto swapchain = _fbi->_swapchain;
  
  if (swapchain) {
    // Onscreen rendering with swapchain
    bool semas_empty = false;
    _pendingOneShotSemas.atomicOp([&](vkcompsema_set_t& unlocked) {
      semas_empty = unlocked.empty();
    });
    
    // Associate frame fence with pending captures
    size_t sub_index = swapchain->subIndex();
    if (sub_index < swapchain->_frameFences.size() && !_pending_captures.empty()) {
      auto frame_fence = swapchain->_frameFences[sub_index];
      for (auto& capture : _pending_captures) {
        if (auto async_impl = capture->_impl.getShared<VkCaptureAsyncImpl>()) {
          async_impl->_fence = frame_fence;
        }
      }
    }
    
    if ( not semas_empty) {
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
    
    // Process pending captures after swapchain frame completion
    _processPendingCaptures();
  } else {
    // Offscreen rendering - just submit command buffers without presentation
    // We need to submit the command buffer to complete the frame
    VkSubmitInfo SI = {};
    initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
    SI.commandBufferCount = 1;
    SI.pCommandBuffers = &_cmdbufcurpri_gfx->_vkcmdbuf;
    
    // Create fence for captures if needed
    vkfence_obj_ptr_t capture_fence;
    if (!_pending_captures.empty()) {
      capture_fence = std::make_shared<VulkanFenceObject>(this);
      capture_fence->reset(); // Start unsignaled
      
      // Associate fence with pending captures
      for (auto& capture : _pending_captures) {
        if (auto async_impl = capture->_impl.getShared<VkCaptureAsyncImpl>()) {
          async_impl->_fence = capture_fence;
        }
      }
      
      vkQueueSubmit(_vkqueue_graphics, 1, &SI, capture_fence->_vkfence);
      capture_fence->wait(); // Wait for fence to be signaled
    } else {
      vkQueueSubmit(_vkqueue_graphics, 1, &SI, VK_NULL_HANDLE);
      vkQueueWaitIdle(_vkqueue_graphics);
    }
    
    logchan_vkctx->log("Offscreen frame submitted");
    
    // Process pending captures after offscreen frame completion
    _processPendingCaptures();
  }
}

///////////////////////////////////////////////////////////////////////////////

vkpricmdbufimpl_ptr_t VkContext::primary_cb() {
  return _cmdbufcurpri_gfx;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doPreBeginFrame() {
  //logchan_vkctx->log("VkContext<%p> _doPreBeginFrame", (void*)this );

  mpCurrentObject        = 0;
  mRenderContextInstData = 0;
  _doBeginPrimaryCommandBuffer();

  /////////////////////////////////////////
  _pendingOneShotCommands.atomicOp([&](vkseccmdbufarray_t& unlocked) {
    for (auto one_shot : unlocked) {
      enqueueSecondaryCommandBuffer(one_shot);
    }  
    unlocked.clear();
  });
  /////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  //logchan_vkctx->log("VkContext<%p> _doBeginFrame w<%d> h<%d>", (void*)this, miW, miH);
  auto main_rtg = _fbi->_ensureMainRtg();
  if (main_rtg) {
    miW = main_rtg->miW;
    miH = main_rtg->miH;
  }
  // Poll timeline semaphores
  _pendingOneShotSemas.atomicOp([&](vkcompsema_set_t& unlocked) {
    for (auto semaphore : unlocked) {
      if(semaphore->isSignalled()){
        // If the semaphore is signalled, execute its completion callback      
        if(semaphore->_onComplete!=nullptr){
          // If the semaphore has a completion callback, execute it
          semaphore->_onComplete();
          semaphore->_onComplete = nullptr; // Clear the callback after execution
        }
      }
    }
    std::erase_if(          //
      unlocked, //
      [](auto sema) { //
        return sema->_onComplete==nullptr; //
    });
  });
  
  // Clean up completed semaphores
  _txi->_beginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndFrame() {
  
  auto main_rtg = _fbi->_ensureMainRtg();
  ////////////////////////
  // main_rtg -> presentation or readable layout
  ////////////////////////

  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();

  // Only transition to present for window targets with swapchain
  // For offscreen, transition to texture-readable state
  logchan_vkctx->log("_doEndFrame: meTargetType=%d (WINDOW=%d), buffer usage=0x%zx", (int)meTargetType, (int)TargetType::WINDOW, main_rtbi->_usage);
  if (meTargetType == TargetType::WINDOW) {
    main_rtbi->_transitionToPresent(primary_cb());
  } else {
    // For offscreen/loader contexts, transition to texture-readable state
    // This allows the rendered image to be read back or used as a texture
    main_rtbi->_transitionToTexture(primary_cb());
  }

  ////////////////////////
  // done with primary command buffer for this frame
  ////////////////////////

  _doEndPrimaryCommandBuffer();

  ////////////////////////

  // logchan_vkctx->log( "num renderpasses<%zu>", _renderpasses.size() );

  ///////////////////////////////////////////////////////
  // submit primary command buffer for this frame
  ///////////////////////////////////////////////////////

  submitPrimaryCommandBuffer();

  ///////////////////////////////////////////////////////

  logchan_vkctx->log("CMDBUF: _doEndFrame: clearing primary CB (was %p)", _cmdbufcurpri_gfx ? (void*)_cmdbufcurpri_gfx->_vkcmdbuf : nullptr);

  _pri_cmdbuf_pool.deallocate(_defaultCommandBuffer);
  _cmdbufcurpri_gfx->_secondary_cmdbuffers.clear();

  ////////////////////////


  ///////////////////////////////////////////////////////
  //logchan_vkctx->log("VkContext<%p> clear renderpasses", (void*)this );

  _defaultCommandBuffer = nullptr;
  //_cmdbufcurpri_gfx = nullptr;
  _vkcmdbuffer_current = nullptr; // Clear current command buffer
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

  // Check if we're in offscreen mode
  bool is_offscreen = (_ginitdata && _ginitdata->_offscreen);
  
  if (is_offscreen) {
    // Create headless surface for offscreen rendering
    logchan_vkctx->log("Creating headless surface for offscreen rendering");
    
    VkHeadlessSurfaceCreateInfoEXT headlessInfo = {};
    headlessInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    headlessInfo.pNext = nullptr;
    headlessInfo.flags = 0;
    
    auto vkCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)
      vkGetInstanceProcAddr(_GVI->_instance, "vkCreateHeadlessSurfaceEXT");
    
    if (vkCreateHeadlessSurfaceEXT) {
      VkResult OK = vkCreateHeadlessSurfaceEXT(
        _GVI->_instance, 
        &headlessInfo, 
        nullptr, 
        &_vkpresentationsurface
      );
      OrkAssert(OK == VK_SUCCESS);
      logchan_vkctx->log("Headless surface created successfully");
    } else {
      logchan_vkctx->log("ERROR: vkCreateHeadlessSurfaceEXT not available");
      OrkAssert(false);
    }
  } else {
    // Original onscreen path
    VkResult OK = glfwCreateWindowSurface(_GVI->_instance, glfw_window, nullptr, &_vkpresentationsurface);
    OrkAssert(OK == VK_SUCCESS);
  }

  // Initialize Vulkan device - for offscreen, just use the first available device
  if (is_offscreen) {
    // For offscreen rendering, select the first available device (or preferred if set)
    auto vk_devinfo = _GVI->_preferred ? _GVI->_preferred : (_GVI->_device_infos.empty() ? nullptr : _GVI->_device_infos[0]);
    OrkAssert(vk_devinfo != nullptr);
    logchan_vkctx->log("Offscreen mode: using device <%s>", vk_devinfo->_devprops.deviceName);
    _initVulkanForDevInfo(vk_devinfo);
    _initVulkanCommon();
    
    // Initialize debug functions if needed
    if (_GVI->_debugEnabled) {
      _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");
      _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT, "vkCmdDebugMarkerBeginEXT");
      _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT, "vkCmdDebugMarkerEndEXT");
      _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT, "vkCmdDebugMarkerInsertEXT");
      _fetchDeviceProcAddr(_vkCmdInsertDebugUtilsLabelEXT, "vkCmdInsertDebugUtilsLabelEXT");
    }
  } else {
    // Original path for windowed rendering
    _initVulkanForWindow(_vkpresentationsurface);
    
    for (uint32_t i = 0; i < _num_queue_types; i++) {
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, i, _vkpresentationsurface, &presentSupport);
      logchan_vkctx->log("Qfamily<%u> on surface supports presentation<%d>", i, int(presentSupport));
    }
  }

  // Only get presentation capabilities if we have a surface
  if (_vkpresentationsurface) {
    _vkpresentation_caps = _swapChainCapsForSurface(_vkpresentationsurface);
  }
  
  // Reuse is_offscreen variable from above
  
  if (!is_offscreen && _vkpresentation_caps) {
    // Only create swapchain for onscreen rendering
    OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_IMMEDIATE_KHR));
    OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
    // OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
    //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_MAILBOX_KHR) );
    //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) );
    //  OrkAssert( _vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR) );
    
    _fbi->_swapchain = std::make_shared<VkSwapChain>(this);
    logchan_vkctx->log("Swapchain created for onscreen rendering");
  } else {
    // For offscreen, we'll render to framebuffer objects instead
    _fbi->_swapchain = nullptr;
    logchan_vkctx->log("Offscreen mode: no swapchain created");
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

  // Initialize Vulkan device for loader context (always offscreen)
  // Create headless surface
  VkHeadlessSurfaceCreateInfoEXT headlessInfo = {};
  headlessInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
  headlessInfo.pNext = nullptr;
  headlessInfo.flags = 0;
  
  auto vkCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)
    vkGetInstanceProcAddr(_GVI->_instance, "vkCreateHeadlessSurfaceEXT");
  
  if (vkCreateHeadlessSurfaceEXT) {
    VkResult OK = vkCreateHeadlessSurfaceEXT(
      _GVI->_instance, 
      &headlessInfo, 
      nullptr, 
      &_vkpresentationsurface
    );
    OrkAssert(OK == VK_SUCCESS);
  }
  
  // Initialize device without requiring surface support
  auto vk_devinfo = _GVI->_preferred ? _GVI->_preferred : _GVI->_device_infos[0];
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // logchan_vkctx->log("resizing defaultRTG<%p>", _defaultRTG);
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
    auto main_rtg = _fbi->_ensureMainRtg();
    if (main_rtg) {
      main_rtg->Resize(iw, ih);
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

// Helper functions for texture sampling mode conversion
namespace {

VkFilter orkidMagFilterToVulkan(ETextureMagnifyFilterMode mode) {
  switch(mode) {
    case ETextureMagnifyFilterMode::NEAREST:
      return VK_FILTER_NEAREST;
    case ETextureMagnifyFilterMode::LINEAR:
      return VK_FILTER_LINEAR;
    default:
      return VK_FILTER_LINEAR;
  }
}

VkFilter orkidMinFilterToVulkan(ETextureMinifyFilterMode mode) {
  switch(mode) {
    case ETextureMinifyFilterMode::NEAREST:
    case ETextureMinifyFilterMode::NEAREST_MIPMAP_NEAREST:
    case ETextureMinifyFilterMode::NEAREST_MIPMAP_LINEAR:
      return VK_FILTER_NEAREST;
    case ETextureMinifyFilterMode::LINEAR:
    case ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR:
    case ETextureMinifyFilterMode::LINEAR_MIPMAP_NEAREST:
      return VK_FILTER_LINEAR;
    default:
      return VK_FILTER_LINEAR;
  }
}

VkSamplerMipmapMode orkidMipModeToVulkan(ETextureMinifyFilterMode mode) {
  switch(mode) {
    case ETextureMinifyFilterMode::NEAREST:
    case ETextureMinifyFilterMode::LINEAR:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST; // No mipmapping
    case ETextureMinifyFilterMode::NEAREST_MIPMAP_NEAREST:
    case ETextureMinifyFilterMode::LINEAR_MIPMAP_NEAREST:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case ETextureMinifyFilterMode::NEAREST_MIPMAP_LINEAR:
    case ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

VkSamplerAddressMode orkidWrapToVulkan(TextureAddressMode mode) {
  switch(mode) {
    case TextureAddressMode::WRAP:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TextureAddressMode::CLAMP:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    default:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

uint64_t hashSamplingMode(const TextureSamplingModeData& mode) {
  boost::Crc64 hasher;
  hasher.init();
  hasher.accumulateItem(mode._texFiltModeMag);
  hasher.accumulateItem(mode._texFiltModeMin);
  hasher.accumulateItem(mode._texAddrModeS);
  hasher.accumulateItem(mode._texAddrModeT);
  hasher.accumulateItem(mode._texAddrModeR);
  hasher.accumulateItem(mode._maxAnisotropy);
  hasher.finish();
  return hasher.result();
}

} // namespace

vksampler_obj_ptr_t VkContext::_getOrCreateSampler(const TextureSamplingModeData& sampling_mode) {
  // Calculate hash
  SamplerCacheKey key;
  key._hash = hashSamplingMode(sampling_mode);
  
  // Check cache
  {
    std::lock_guard<std::mutex> lock(_sampler_cache_mutex);
    auto it = _sampler_cache.find(key);
    if (it != _sampler_cache.end()) {
      return it->second;
    }
  }
  
  // Create new sampler
  auto sci = std::make_shared<VkSamplerCreateInfo>();
  initializeVkStruct(*sci, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  
  // Filter modes
  sci->magFilter = orkidMagFilterToVulkan(sampling_mode._texFiltModeMag);
  sci->minFilter = orkidMinFilterToVulkan(sampling_mode._texFiltModeMin);
  sci->mipmapMode = orkidMipModeToVulkan(sampling_mode._texFiltModeMin);
  
  // Address modes
  sci->addressModeU = orkidWrapToVulkan(sampling_mode._texAddrModeS);
  sci->addressModeV = orkidWrapToVulkan(sampling_mode._texAddrModeT);
  sci->addressModeW = orkidWrapToVulkan(sampling_mode._texAddrModeR);
  
  // Anisotropy
  float max_aniso = sampling_mode._maxAnisotropy;
  if (max_aniso > 1.0f) {
    sci->anisotropyEnable = VK_TRUE;
    sci->maxAnisotropy = max_aniso;
  } else {
    sci->anisotropyEnable = VK_FALSE;
    sci->maxAnisotropy = 1.0f;
  }
  
  // LOD settings
  sci->mipLodBias = 0.0f;
  sci->minLod = 0.0f;
  sci->maxLod = VK_LOD_CLAMP_NONE; // Or texture's max mip level
  
  // Border color for CLAMP_TO_BORDER mode
  // Default to opaque black (most common)
  sci->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  
  // Comparison for depth textures
  sci->compareEnable = VK_FALSE;
  sci->compareOp = VK_COMPARE_OP_ALWAYS;
  
  // Unnormalized coordinates (false for normal textures)
  sci->unnormalizedCoordinates = VK_FALSE;
  
  // Create sampler object
  auto sampler = std::make_shared<VulkanSamplerObject>(this, sci);
  
  // Cache it
  {
    std::lock_guard<std::mutex> lock(_sampler_cache_mutex);
    _sampler_cache[key] = sampler;
  }
  
  return sampler;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
//////////////////////////////////////////////////////////////////////////////
