////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif
#if defined(__linux__)
#include "headers/vk_swapchain_drm.h"
#endif
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
static logchannel_ptr_t logchan_vkcap = logger()->configureChannel("VKCAPTURE", fvec3(1,1,.9),false);
static logchannel_ptr_t logchan_vkprof = logger()->configureChannel("VKPROF", fvec3(0.1, 0.5, 0.9), true);

////////////////////////////////////////////////////////////////////////////////

void VkContext::describeX(class_t* clazz) {
  clazz->annotateTyped<context_factory_t>("context_factory", []() { //
    return std::make_shared<VkContext>();
  });
}

///////////////////////////////////////////////////////////////////////////////

bool VkContext::HaveExtension(const std::string& extname) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Vulkan Context Internal Init
///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForDevInfo(vkdeviceinfo_ptr_t vk_devinfo) {

  logchan_vkctx->log("VkContext: using device <%s>", vk_devinfo->_devprops.deviceName);

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

  // Only request swapchain extension for windowed (non-offscreen) rendering
  // In headless/offscreen mode, swapchain is not needed and may not be available
  bool is_offscreen = (_ginitdata && _ginitdata->_offscreen);
  if (!is_offscreen) {
    _device_extensions.push_back("VK_KHR_swapchain");
  }
  if (_GVI->_debugEnabled) {
    _device_extensions.push_back("VK_EXT_debug_marker");
  }

  // VK_KHR_portability_subset is macOS/MoltenVK specific
#if defined(__APPLE__)
  _device_extensions.push_back("VK_KHR_portability_subset");

  // GPU-direct video decode (VideoToolbox → IOSurface → Vulkan)
  _device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  _device_extensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

  // Check if VK_EXT_metal_objects is available before enabling
  if (vk_devinfo->_extension_set.count("VK_EXT_metal_objects") > 0) {
    _device_extensions.push_back("VK_EXT_metal_objects");
    logchan_vkctx->log("Added VK_EXT_metal_objects for GPU-direct video");
  } else {
    logchan_vkctx->log("WARNING: VK_EXT_metal_objects NOT available - GPU-direct video will not work!");
  }
#endif

  // DRM-specific extensions (Linux only)
#if defined(__linux__)
  if(_ginitdata && _ginitdata->_use_drm) {
    _device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    _device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    _device_extensions.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    _device_extensions.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    _device_extensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    _device_extensions.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    logchan_vkctx->log("Added DRM-specific Vulkan device extensions");
  }
#endif

  _device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

  // YCbCr sampler support for GPU-direct video (NV12, P010 formats)
  _device_extensions.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
  logchan_vkctx->log("Added YCbCr sampler conversion extension for video decode");

  VkDeviceCreateInfo DCI = {};
  initializeVkStruct(DCI, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  DCI.queueCreateInfoCount    = _DQCIs.size();
  DCI.pQueueCreateInfos       = _DQCIs.data();
  DCI.enabledExtensionCount   = _device_extensions.size();
  DCI.ppEnabledExtensionNames = _device_extensions.data();

  // add features (not extensions)

  VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
  VkPhysicalDeviceDynamicRenderingFeatures dynrenderfeat{};
  VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures{};

  initializeVkStruct(timelineFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
  initializeVkStruct(dynrenderfeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
  initializeVkStruct(ycbcrFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);

  timelineFeatures.timelineSemaphore = VK_TRUE;
  dynrenderfeat.dynamicRendering = VK_TRUE;
  ycbcrFeatures.samplerYcbcrConversion = VK_TRUE;

  DCI.pNext = (void*) & timelineFeatures;
  timelineFeatures.pNext = (void*) & dynrenderfeat;
  dynrenderfeat.pNext = (void*) & ycbcrFeatures;
  ycbcrFeatures.pNext = (void*) nullptr;


  VkResult result = vkCreateDevice(_vkphysicaldevice, &DCI, nullptr, &_vkdevice);
  if (result != VK_SUCCESS) {
    printf("vkCreateDevice FAILED with result: %d\n", result);
    printf("is_offscreen: %d\n", int(is_offscreen));
    printf("device extensions count: %zu\n", _device_extensions.size());
    for(size_t i = 0; i < _device_extensions.size(); i++) {
      printf("  ext[%zu]: %s\n", i, _device_extensions[i]);
    }
    logchan_vkctx->log("vkCreateDevice FAILED with result: %d", result);
    OrkAssert(false);
  }

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

    // Validate that the existing device can present to this surface
    // This is critical for multi-GPU systems and when loader context was created first
    bool can_present = false;
    auto devinfo = context0->_vkdeviceinfo;
    if (devinfo) {
      for (uint32_t qf = 0; qf < devinfo->_queueprops.size(); qf++) {
        VkBool32 support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(context0->_vkphysicaldevice, qf, surface, &support);
        if (support) {
          can_present = true;
          break;
        }
      }
    }

    if (!can_present) {
      // Reused device can't present to this surface, need to create new device
      logchan_vkctx->log("Existing device cannot present to surface, creating new device for window");
      OrkAssert(vk_devinfo != nullptr);
      _initVulkanForDevInfo(vk_devinfo);
      _initVulkanCommon();
      return;
    }

    // Existing device is compatible, reuse it
    logchan_vkctx->log("Reusing existing device for window (validated presentation support)");
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
    if (nullptr == _GVI->_preferred) {
      // Prefer discrete GPU if available
      vkdeviceinfo_ptr_t discrete_device = nullptr;
      for (auto devinfo : _GVI->_device_infos) {
        if (devinfo->_is_discrete) {
          discrete_device = devinfo;
          break;
        }
      }
      // Use discrete GPU if found, otherwise fall back to first device
      _GVI->_preferred = discrete_device ? discrete_device : _GVI->_device_infos.front();
    }
    auto vk_devinfo = _GVI->_preferred;
    _initVulkanForDevInfo(vk_devinfo);
    _initVulkanCommon();
  }
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

  OrkVkAssert(vkCreateCommandPool(_vkdevice, &CPCI_GFX, nullptr, &_vkcmdpool_graphics));

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

  // Initialize synchronous transfer resources
  initSyncTransfer();

  // create descriptor pool
  std::vector<VkDescriptorPoolSize> poolSizes;

  constexpr size_t DESCRIPTORSET_COUNT = 262144;

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

  auto& poolsize_uniform_buffers_dynamic           = poolSizes.emplace_back();
  poolsize_uniform_buffers_dynamic.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  poolsize_uniform_buffers_dynamic.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  auto& poolsize_storage_buffers           = poolSizes.emplace_back();
  poolsize_storage_buffers.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolsize_storage_buffers.descriptorCount = DESCRIPTORSET_COUNT; // Number of descriptors of this type to allocate

  VkDescriptorPoolCreateInfo poolInfo = {};
  initializeVkStruct(poolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
  poolInfo.poolSizeCount = poolSizes.size();
  poolInfo.pPoolSizes    = poolSizes.data();
  poolInfo.maxSets       = DESCRIPTORSET_COUNT; // Maximum number of descriptor sets to allocate from this pool

  OrkVkAssert(vkCreateDescriptorPool(_vkdevice, &poolInfo, nullptr, &_vkDescriptorPool));
  
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
    if(0)printf("VkContext: Initialized dynamic UBO system\n");
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
    // Default textures use slot [0] only
    tex_obj->_imgobj[0] = std::make_shared<VulkanImageObject>(this, imageInfo, "default_texture");

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    viewInfo.image = tex_obj->_imgobj[0]->_vkimage;
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
    
    OrkVkAssert(vkCreateImageView(_vkdevice, &viewInfo, nullptr, &tex_obj->_imgobj[0]->_vkimageview));

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
    barrier.image = tex_obj->_imgobj[0]->_vkimage;
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
    cmdbuf_impl->_referenced_images.push_back(tex_obj->_imgobj[0]);
    enqueueDeferredOneShotCommand(cmdbuf);

    // Set up descriptor info (default textures only use slot [0])
    tex_obj->_vkdescriptor_info[0] = std::make_shared<VkDescriptorImageInfo>();
    tex_obj->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    tex_obj->_vkdescriptor_info[0]->imageView = tex_obj->_imgobj[0]->_vkimageview;
    tex_obj->_vkdescriptor_info[0]->sampler = _sampler_base->_vksampler;
    tex_obj->_descset_sampling = tex_obj->_vkdescriptor_info[0];

    tex_obj->_imgview_hash.init();
    tex_obj->_imgview_hash.accumulateItem(tex_obj->_imgobj[0]->_serial_number);
    tex_obj->_imgview_hash.finish();

    // Default texture is now ready for sampling
    tex_obj->_img_sampling = tex_obj->_imgobj[0];
    return tex_obj;
  };
  
  // Create default textures with agreed-upon sizes
  _defaultTexImpl2D = create_default_texture(ETEXTYPE_2D, 64, 64);
  _defaultTexImplCube = create_default_texture(ETEXTYPE_CUBE, 64, 64);
  _defaultTexImpl2DArray = create_default_texture(ETEXTYPE_2D_ARRAY, 64, 64, 1, 4); // 4 layers
  _defaultTexImpl3D = create_default_texture(ETEXTYPE_3D, 16, 16, 16);
}

////////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {
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
#if defined(__APPLE__)
    _stopDisplayLink();
#endif
    if (_vkpresentationsurface != VK_NULL_HANDLE && _GVI) {
      printf("VkContext::~VkContext: destroying VkSurface %p\n", (void*)_vkpresentationsurface);
      vkDestroySurfaceKHR(_GVI->_instance, _vkpresentationsurface, nullptr);
      _vkpresentationsurface = VK_NULL_HANDLE;
    } else {
      printf("VkContext::~VkContext: no surface to destroy (surface=%p GVI=%p)\n",
             (void*)_vkpresentationsurface, (void*)_GVI.get());
    }
    _vkdevice = nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void VkContext::FxInit() {
}

///////////////////////////////////////////////////////

ctx_platform_handle_t VkContext::_doClonePlatformHandle() const {
  OrkAssert(false);
  return ctx_platform_handle_t();
}

////////////////////////////////////////////////////////////////////////////////
// Interfaces
////////////////////////////////////////////////////////////////////////////////

FxInterface* VkContext::FXI() {
  return _fxi.get();
}

///////////////////////////////////////////////////////

ImmInterface* VkContext::IMI() {
  return _imi.get();
}

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

////////////////////////////////////////////////////////////////////////////////
// Vk Static Global State
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

void VkContext::makeCurrentContext() {
  // auto plato = _impl.getShared<VkPlatformObject>();
  // platoMakeCurrent(plato);
}

////////////////////////////////////////////////////////////////////////////////
// Vulkan Context Frame And Command Buffer
////////////////////////////////////////////////////////////////////////////////

void VkContext::_doBeginPrimaryCommandBuffer() {
  ////////////////////////
  // Check if command buffer pool is healthy
  ////////////////////////
  // logchan_vkctx->log("  Allocating command buffer from pool (available: %zu)", _pri_cmdbuf_pool.available());
  _defaultCommandBuffer = _pri_cmdbuf_pool.allocate();

  ////////////////////////
  _defaultCommandBufferImpl = _defaultCommandBuffer->_impl.getShared<VkPrimaryCommandBufferImpl>();
  _cmdbufcurpri_gfx         = _defaultCommandBufferImpl;
  ////////////////////////

  // Invoke cleanup callbacks (e.g., return pooled CBs to pool) before clearing
  // Safe now because vkBeginCommandBuffer will reset the VkCommandBuffer handle
  if(0)printf( "BEGIN priCB<%p> impl<%p> vkhandle<%p> pending_cleanup=%zu\n",
    (void*)_defaultCommandBuffer.get(),
    (void*)_defaultCommandBufferImpl.get(),
    (void*)_cmdbufcurpri_gfx->_vkcmdbuf,
    _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup.size() );
  for (auto& cb : _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup) {
    auto impl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();

    if(0)printf( "  cleanup CB<%p> cleanupCB<%p>\n", (void*)cb.get(), (void*)impl->_onCleanupCallback.target<void>() );

    if (impl->_onCleanupCallback) {
      impl->_onCleanupCallback();
    }
  }
  _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup.clear();

  ////////////////////////
  // cleanup _vkbuffers_pending_cleanup
  ////////////////////////

  _cmdbufcurpri_gfx->_vkbuffers_pending_cleanup.clear();

  ////////////////////////
  // Move context-level pending cleanup to CB (buffers destroyed when no CB was active)
  ////////////////////////
  {
    std::lock_guard<std::mutex> lock(_vkbuffers_pending_cleanup_mutex);
    for (auto& buf : _vkbuffers_pending_cleanup) {
      _cmdbufcurpri_gfx->_vkbuffers_pending_cleanup.push_back(std::move(buf));
    }
    _vkbuffers_pending_cleanup.clear();
  }

  ////////////////////////
  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CBBI_GFX.pInheritanceInfo = nullptr;
  vkBeginCommandBuffer(primary_cb()->_vkcmdbuf, &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset

}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndPrimaryCommandBuffer() {
  auto CB = primary_cb();
  if(0)printf( "END priCB<%p> impl<%p> vkhandle<%p>\n", (void*)_defaultCommandBuffer.get(), (void*)CB.get(), (void*)CB->_vkcmdbuf );
  CB->_recorded = true;
  vkEndCommandBuffer(CB->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doSubmitPrimaryCommandBuffer(){
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:doSubmitPrimaryCommandBuffer");

  _oneShotSignalSemaphores.clear();
  _oneShotSignalValues.clear();
  _pendingOneShotSemas.atomicOp([&](vkcompsema_set_t& unlocked) {
    for (auto& semaphore : unlocked) {
      _oneShotSignalSemaphores.push_back(semaphore->_vksema);
      _oneShotSignalValues.push_back(1);
    }
  });

  // Associate captures with the current frame fence before submitting to the GPU
  auto frame_fence = _fbi->_output->currentFrameFence();
  for (auto& capture : _pending_captures) {
    if (auto async_impl = capture->_impl.getShared<VkCaptureAsyncImpl>())
      async_impl->_fence = frame_fence;
  }

  // VkFramebufferOutput deals with output specific submission and waiting.
  _fbi->_output->submit(this);

  _processPendingCaptures();
}

///////////////////////////////////////////////////////////////////////////////

vkpricmdbufimpl_ptr_t VkContext::primary_cb() {
  return _cmdbufcurpri_gfx;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::initSyncTransfer() {
  // Allocate a primary command buffer for synchronous transfers
  _syncTransfer.command_buffer = _pri_cmdbuf_pool.allocate();
  _syncTransfer.command_buffer_impl = _syncTransfer.command_buffer->_impl.getShared<VkPrimaryCommandBufferImpl>();

  // Start with 16MB staging buffer (reasonable default)
  _syncTransfer.staging_size = 16 * 1024 * 1024;
  _syncTransfer.staging_buffer = std::make_shared<VulkanBuffer>(
    this,
    _syncTransfer.staging_size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    "syncTransferStaging");
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::ensureSyncStagingSize(size_t needed) {
  if (needed > _syncTransfer.staging_size) {
    logchan_vkctx->log("Growing sync staging buffer: %zu -> %zu bytes",
                       _syncTransfer.staging_size, needed);
    _syncTransfer.staging_buffer = std::make_shared<VulkanBuffer>(
      this,
      needed,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      "syncTransferStaging");
    _syncTransfer.staging_size = needed;
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::beginSyncTransferCB() {
  // Lock is acquired by caller

  VkCommandBufferBeginInfo beginInfo{};
  initializeVkStruct(beginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Implicit reset

  vkBeginCommandBuffer(_syncTransfer.command_buffer_impl->_vkcmdbuf, &beginInfo);
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::endAndSubmitSyncTransferCB() {
  vkEndCommandBuffer(_syncTransfer.command_buffer_impl->_vkcmdbuf);

  // Create fence for precise waiting (better than vkQueueWaitIdle)
  VkFenceCreateInfo fenceInfo{};
  initializeVkStruct(fenceInfo, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  VkFence fence;
  vkCreateFence(_vkdevice, &fenceInfo, nullptr, &fence);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  initializeVkStruct(submitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_syncTransfer.command_buffer_impl->_vkcmdbuf;

  vkQueueSubmit(_vkqueue_graphics, 1, &submitInfo, fence);

  // Wait for this specific submit to complete
  vkWaitForFences(_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(_vkdevice, fence, nullptr);

  // Command buffer is now in INVALID state (ONE_TIME_SUBMIT)
  // Next beginSyncTransferCB will implicitly reset it

  // Lock is released by caller
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doPreBeginFrame() {
  if(0)logchan_vkctx->log("VkContext<%p> _doPreBeginFrame", (void*)this );

  mpCurrentObject        = 0;
  mRenderContextInstData = 0;
  _doBeginPrimaryCommandBuffer();

  // begin gpu profiler frame after we have setup commandbuffer
  // must use beginProfilerFrame overload to set cmdbuf for frame
  VkProfilerChannel::BeginParams profiler_params = {
    .device           = _vkdevice,
    .timestamp_period = _vkdeviceinfo->_devprops.limits.timestampPeriod,
    .cmdbuf           = primary_cb()->_vkcmdbuf,
  };
  OrkProfilerFrameBegin(CHANNEL_GPU, VkProfilerChannel, profiler_params);
  OrkProfilerSampleBegin(CHANNEL_GPU, SERIES_GPU_FRAME_ALL);

  /////////////////////////////////////////
  _pendingOneShotCommands.atomicOp([&](vkseccmdbufarray_t& unlocked) {
    //size_t num_one_shot = unlocked.size();
    //printf("VkContext<%p> executing %zu one-shot secondary command buffers\n", (void*)this, num_one_shot);
    for (auto one_shot : unlocked) {
      enqueueSecondaryCommandBuffer(one_shot);
    }  
    unlocked.clear();
  });
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  //logchan_vkctx->log("VkContext<%p> _doBeginFrame w<%d> h<%d>", (void*)this, miW, miH);
  auto main_rtg = _fbi->_ensureMainRtg();
  if (main_rtg) {
    miW = main_rtg->miW;
    miH = main_rtg->miH;
  }

  // Poll completion semaphores
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

void VkContext::_onGpuPreInit() {
  // Called before application gpuInit
  // Currently nothing special needed here
}

///////////////////////////////////////////////////////

void VkContext::_onGpuPostInit() {
  // Submit the primary command buffer that was recorded during gpuPreInit
  // This ensures all texture array transitions are executed before first frame

  //printf("VkContext::_onGpuPostInit: Submitting gpuPreInit command buffer\n");

  // During init, we haven't started a frame yet, so we can't use the swapchain submit path
  // Do a simple direct submit without presentation semaphores

  VkSubmitInfo SI;
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.commandBufferCount = 1;
  SI.pCommandBuffers = &_cmdbufcurpri_gfx->_vkcmdbuf;

  // Create fence to wait for completion
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence;
  vkCreateFence(_vkdevice, &fenceInfo, nullptr, &fence);

  // Submit and wait
  vkQueueSubmit(_vkqueue_graphics, 1, &SI, fence);
  vkWaitForFences(_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(_vkdevice, fence, nullptr);

  // Clear the command buffer pointers
  // The pool will reuse this command buffer on the next beginFrame
  _defaultCommandBuffer = nullptr;
  _defaultCommandBufferImpl = nullptr;
  _cmdbufcurpri_gfx = nullptr;

  //printf("VkContext::_onGpuPostInit: gpuPreInit transitions complete\n");

#if defined(__APPLE__)
  if (_vkpresentationsurface != VK_NULL_HANDLE) {
    auto* self = this;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      self->_startDisplayLink();
    });
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndFrame() {
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:doEndFrame");

  ////////////////////////////////////////
  // Done with primary command buffer for this frame
  ////////////////////////////////////////

  // Output endFrame is called from VkFrameBufferInterface::_doEndFrame (FBI()->EndFrame()).

  // End the sample for all GPU work.
  OrkProfilerSampleEnd(CHANNEL_GPU, SERIES_GPU_FRAME_ALL);

  // End and submit primary command buffer. Currently this also waits.
  _doEndPrimaryCommandBuffer();
  _doSubmitPrimaryCommandBuffer();

  // TODO turn into OrkProfiler macro
  // {
  //   double now = Timer::getEpochMS();
  //   if (_last_submit_epoch_ms > 0.0) {
  //     double delta = now - _last_submit_epoch_ms;
  //     _submit_delta_stats.pollValue(delta);
  //     _submit_delta_stats.printStats("primary cmdbuf submit delta");
  //   }
  //   _last_submit_epoch_ms = now;
  // }
  // [RunningStats] primary cmdbuf submit delta last:9.8372 count:7538 min:5.2849 max:18.7739 mean:11.1116 stddev:1.2483

  // read back GPU timestamps now that the GPU has finished executing
  OrkProfilerFrameEnd(CHANNEL_GPU);

#if defined(__APPLE__)
  // Apple marks the prediction target from vulkan_displaylink.mm
  // if the context was created with a surface
  if (_vkpresentationsurface == VK_NULL_HANDLE)
    _render_timing_estimator->markPredictionTarget();
#else
  _render_timing_estimator->markPredictionTarget();
#endif

  ////////////////////////////////////////

  if(0)logchan_vkctx->log("CMDBUF: _doEndFrame: deallocating priCB<%p> impl<%p> vkhandle<%p>, pending_cleanup=%zu",
    (void*)_defaultCommandBuffer.get(),
    (void*)_cmdbufcurpri_gfx.get(),
    (void*)(_cmdbufcurpri_gfx ? _cmdbufcurpri_gfx->_vkcmdbuf : nullptr),
    _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup.size());

  ////////////////////////////////////////
  // Append secondary command buffers to pending cleanupπ
  // They will be destroyed when this primary CB is reallocated and reset
  // (3 frames later due to pool size 3 in practice)
  ////////////////////////////////////////

  // APPEND to pending_cleanup, don't replace! Multiple frames may add to it.
  _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup.insert(
    _cmdbufcurpri_gfx->_secondary_cmdbuffers_pending_cleanup.end(),
    std::make_move_iterator(_cmdbufcurpri_gfx->_secondary_cmdbuffers.begin()),
    std::make_move_iterator(_cmdbufcurpri_gfx->_secondary_cmdbuffers.end())
  );
  _cmdbufcurpri_gfx->_secondary_cmdbuffers.clear();

  _pri_cmdbuf_pool.deallocate(_defaultCommandBuffer);

  ////////////////////////////////////////
  
  _defaultCommandBuffer = nullptr;
  _first_frame            = false;
}

////////////////////////////////////////////////////////////////////////////////
// Vulkan Context Initialization Paths
////////////////////////////////////////////////////////////////////////////////

#if defined(__linux__)
void VkContext::initializeDRMContext(Window* pWin, CTXBASE* pctxbase) {
  logchan_vkctx->log("Detected DRM context, using VkPlatformObjectDRM");
  auto ctxdrm  = dynamic_cast<CtxDRM*>(pctxbase);
  OrkAssert(ctxdrm != nullptr);
  miW          = pWin->miWidth;
  miH          = pWin->miHeight;
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  auto plato_drm      = std::make_shared<VkPlatformObjectDRM>();
  plato_drm->_ctxbase = ctxdrm;
  plato_drm->_drmctx  = ctxdrm->_drmctx.get();
  mCtxBase            = ctxdrm;
  _impl.setShared<VkPlatformObjectDRM>(plato_drm);
  plato_drm->_bindop(); // Call bind operation directly for DRM
  _fbi->SetThisBuffer(pWin);
  // CRITICAL: Use actual DRM mode dimensions, not window request size
  miW = ctxdrm->_drmctx->imageExtent.width;
  miH = ctxdrm->_drmctx->imageExtent.height;
  logchan_vkctx->log("DRM: dimensions set to actual mode: %dx%d", miW, miH);
  _vkpresentationsurface = VK_NULL_HANDLE;
  ///////////////////////
  auto vk_devinfo = _GVI->_preferred ? _GVI->_preferred : (_GVI->_device_infos.empty() ? nullptr : _GVI->_device_infos[0]);
  OrkAssert(vk_devinfo != nullptr);
  logchan_vkctx->log("DRM mode: using device <%s>", vk_devinfo->_devprops.deviceName);
  _initVulkanForDevInfo(vk_devinfo);
  _initVulkanCommon();
  ///////////////////////
  if (_GVI->_debugEnabled) {
    _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT, "vkCmdDebugMarkerBeginEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT, "vkCmdDebugMarkerEndEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT, "vkCmdDebugMarkerInsertEXT");
    _fetchDeviceProcAddr(_vkCmdInsertDebugUtilsLabelEXT, "vkCmdInsertDebugUtilsLabelEXT");
  }
  ///////////////////////
  auto drm_sc = std::make_shared<VkSwapChainDRM>(this, plato_drm->_drmctx);
  drm_sc->_buildup();
  _fbi->_output = drm_sc;
  logchan_vkctx->log("DRM context initialized");
}
#endif

///////////////////////////////////////////////////////

void VkContext::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
  miW = pWin->miWidth;
  miH = pWin->miHeight;
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  logchan_vkctx->log("initializeWindowContext called: pctxbase=%p, type=%s",
                     pctxbase, pctxbase ? typeid(*pctxbase).name() : "null");
  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto plato          = std::make_shared<VkPlatformObject>();
  plato->_ctxbase     = glfw_container;
  mCtxBase            = pctxbase;
  _impl.setShared<VkPlatformObject>(plato);
  platoMakeCurrent(plato);
  _fbi->SetThisBuffer(pWin);
  ///////////////////////
  auto glfw_window = glfw_container->_glfwWindow;
  uint32_t count;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);
  logchan_vkctx->log("GLFW requires %u extensions for surface:", count);
  for (uint32_t i = 0; i < count; i++) {
    logchan_vkctx->log("  - %s", extensions[i]);
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  OrkVkAssert(glfwCreateWindowSurface(_GVI->_instance, glfw_window, nullptr, &_vkpresentationsurface));
  ///////////////////////
  _initVulkanForWindow(_vkpresentationsurface);
  ///////////////////////
  // Validate presentation support on queue families
  bool has_presentation_support = false;
  for (uint32_t i = 0; i < _num_queue_types; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, i, _vkpresentationsurface, &presentSupport);
    logchan_vkctx->log("Qfamily<%u> on surface supports presentation<%d>", i, int(presentSupport));
    if (presentSupport) has_presentation_support = true;
  }
  if (!has_presentation_support) {
    logchan_vkctx->log("ERROR: No queue family supports presentation on device %s", _vkdeviceinfo->_devprops.deviceName);
  }
  OrkAssert(has_presentation_support);
  ///////////////////////
  _vkpresentation_caps = _swapChainCapsForSurface(_vkpresentationsurface);
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_IMMEDIATE_KHR));
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
  ///////////////////////
  _fbi->_output = std::make_shared<VkSwapChain>(this);
  logchan_vkctx->log("Window context initialized");
} 

///////////////////////////////////////////////////////

void VkContext::initializeOffscreenContext(DisplayBuffer* pbuffer) {
  meTargetType = TargetType::OFFSCREEN;
  miW          = pbuffer->GetBufferW();
  miH          = pbuffer->GetBufferH();
  ///////////////////////
  auto glfw_container          = (CtxGLFW*)global_plato()->_ctxbase;
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
  plato->_ctxbase =  global_plato()->_ctxbase;
  plato->_needsInit = false;
  _fbi->_output    = std::make_shared<VkOffscreen>(this);
  _defaultRTG      = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb         = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture     = rtb->texture();
  _fbi->SetBufferTexture(texture);
  logchan_vkctx->log("Offscreen context initialized");
}

///////////////////////////////////////////////////////

void VkContext::initializeLoaderContext() {
  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  auto plato = std::make_shared<VkPlatformObject>();
  _impl.setShared<VkPlatformObject>(plato);
  plato->_ctxbase = global_plato()->_ctxbase;
  plato->_needsInit = false;

  // Select device. In DRM or headless (GLFW_PLATFORM_NULL) skip findPresentableDevice() 
  bool use_drm        = (_ginitdata && _ginitdata->_use_drm);
  bool glfw_null_plat = (glfwGetPlatform() == GLFW_PLATFORM_NULL);
  if (nullptr == _GVI->_preferred) {
    if (!use_drm && !glfw_null_plat) {
      auto vk_devinfo = _GVI->findPresentableDevice();
      if (vk_devinfo) {
        _GVI->_preferred = vk_devinfo;
        logchan_vkctx->log("Loader context: Selected presentation-capable device for share group");
      }
    }

    // Fallback: prefer discrete GPU
    if (nullptr == _GVI->_preferred) {
      vkdeviceinfo_ptr_t discrete_device = nullptr;
      for (auto devinfo : _GVI->_device_infos) {
        if (devinfo->_is_discrete) {
          discrete_device = devinfo;
          break;
        }
      }
      _GVI->_preferred = discrete_device ? discrete_device : _GVI->_device_infos.front();
      logchan_vkctx->log("Loader context: falling back to device <%s>", _GVI->_preferred->_devprops.deviceName);
    }
  }
  logchan_vkctx->log("Loader context: using device <%s>", _GVI->_preferred->_devprops.deviceName);

  auto vk_devinfo = _GVI->_preferred;
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
  
  _fbi->_output = std::make_shared<VkOffscreen>(this);
  logchan_vkctx->log("Loader context initialized");
}

////////////////////////////////////////////////////////////////////////////////
// Vulkan Context Debug Utils
////////////////////////////////////////////////////////////////////////////////

void VkContext::debugPushGroup(const std::string str, const fvec4& color) {
  if (_vkCmdDebugMarkerBeginEXT) {
    OrkAssert(_cmdbufcurpri_gfx);
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
    OrkAssert(_cmdbufcurpri_gfx);
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

////////////////////////////////////////////////////////////////////////////////

void VkContext::TakeThreadOwnership() {
}

////////////////////////////////////////////////////////////////////////////////

bool VkContext::SetDisplayMode(DisplayMode* mode) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

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
      const char* pname = "UNKNOWN";
      switch(item) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: pname = "IMMEDIATE"; break;
        case VK_PRESENT_MODE_MAILBOX_KHR: pname = "MAILBOX"; break;
        case VK_PRESENT_MODE_FIFO_KHR: pname = "FIFO"; break;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: pname = "FIFO_RELAXED"; break;
        default: break;
      }
      logchan_vkctx->log("  presentMode available: %s (%d)", pname, int(item));
    }
  }
  VkBool32 presentSupport = false;
  vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, _vkqfid_graphics, surface, &presentSupport);
  if (!presentSupport) {
    logchan_vkctx->log("ERROR: Graphics queue family %u does not support presentation to this surface!", _vkqfid_graphics);
    logchan_vkctx->log("       Device: %s", _vkdeviceinfo->_devprops.deviceName);
    logchan_vkctx->log("       This indicates device selection or queue family selection is incorrect.");
  }
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

////////////////////////////////////////////////////////////////////////////////
// Helper functions for texture sampling mode conversion
////////////////////////////////////////////////////////////////////////////////

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
    float max_aniso = 1.0;//sampling_mode._maxAnisotropy;
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

////////////////////////////////////////////////////////////////////////////////
// Vulkan Context Render Pass
////////////////////////////////////////////////////////////////////////////////

void VkContext::suspendRenderPass() {
  if (!_renderPassActive) {
    // No render pass to suspend
    return;
  }

  // Invariant: if render pass is active, RTG must be set
  OrkAssert(_activeRenderPassRTG != nullptr);

  // End the current render pass
  auto& CB = primary_cb()->_vkcmdbuf;
  _vkCmdEndRenderingKHR(CB);

  // Mark render pass as inactive but keep the RTG reference
  // so we know what to resume
  _renderPassActive = false;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::resumeRenderPass() {
  if (_renderPassActive) {
    // Already in a render pass, nothing to do
    return;
  }
  
  if (!_activeRenderPassRTG) {
    // No RTG to resume to
    return;
  }
  
  // Resume the render pass using the cached RTG
  auto& CB = primary_cb()->_vkcmdbuf;
  
  // Use renderinfoForResume() which has LOAD ops and RESUMING bit
  auto rinfo = _activeRenderPassRTG->renderinfoForResume();
  _vkCmdBeginRenderingKHR(CB, &rinfo->_renderinfo);
  
  // Mark render pass as active again
  _renderPassActive = true;
}

////////////////////////////////////////////////////////////////////////////////
// GPU Profiler Implementation
////////////////////////////////////////////////////////////////////////////////

void VkProfilerChannel::frameBegin(BeginParams params) {
  _recording = Profiler::enabled();
  if (!_recording) [[unlikely]] return;
  if (_device == VK_NULL_HANDLE) {
    _device = params.device;
    _tick_to_ms = double(params.timestamp_period) * 1e-6; // milliseconds per GPU tick

    VkQueryPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = MAX_GPU_PERF_QUERIES * 2, // 2 timestamps per block (begin + end)
    };
    OrkVkAssert(vkCreateQueryPool(_device, &info, nullptr, &_query_pool));
  }

  if (!_vk_span_stack.empty()) [[unlikely]] {
    logchan_vkprof->log("frameBegin(%s) but _vk_span_stack not empty (size=%zu)! Ensure sampleEnd called. Or use sampleScope.",
        _name.c_str(), _vk_span_stack.size());
    while (!_vk_span_stack.empty()) {
      logchan_vkprof->log("   stale entry: %s", _vk_span_stack.top().series->_name.c_str());
      _vk_span_stack.pop();
    }
    _current_level = 0;
  }

  _cmdbuf = params.cmdbuf;
  vkCmdResetQueryPool(_cmdbuf, _query_pool, 0, MAX_GPU_PERF_QUERIES * 2);
}

void VkProfilerChannel::frameEnd() {
  if (!_recording) [[unlikely]] return;
  if (_cmdbuf == VK_NULL_HANDLE) [[unlikely]]{
    logchan_vkprof->log("frameEnd(%s) called but frameBegin was never called! Skipping.", _name.c_str());
    return;
  }
  if (!_vk_span_stack.empty()) [[unlikely]] {
    logchan_vkprof->log("frameEnd(%s) but _vk_span_stack not empty (size=%zu)! Ensure sampleEnd called! Or use sampleScope!",
        _name.c_str(), _vk_span_stack.size());
    while (!_vk_span_stack.empty()) {
      logchan_vkprof->log("   leaked entry: %s", _vk_span_stack.top().series->_name.c_str());
      _vk_span_stack.pop();
    }
    _current_level = 0;
  }

  _cmdbuf = VK_NULL_HANDLE;

  // Readback timestamp queries
  _timestamps.resize(_query_index);
  VkResult ok = vkGetQueryPoolResults(_device, _query_pool, 0, _query_index, _query_index * sizeof(u64),
      _timestamps.data(), sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  OrkAssert(VK_SUCCESS == ok);
  _query_index = 0;

    // Accumulate isolated ticks from per-segment spans
  for (auto& span : _vk_spans)
      span.series->_isolated_ticks += _timestamps[span.end_query] - _timestamps[span.begin_query];
  _vk_spans.clear();

    // Accumulate total ticks from full-duration spans (begin_total_query -> end_query)
  for (auto& span : _vk_total_spans)
      span.series->_total_ticks += _timestamps[span.end_query] - _timestamps[span.begin_total_query];
  _vk_total_spans.clear();

  // accumulate in series through base call
  ProfilerChannel::frameEnd();
}

void VkProfilerChannel::sampleBegin(SampleProfilerSeries* s) {
  if (!_recording) [[unlikely]] return;
  if (_cmdbuf == VK_NULL_HANDLE) [[unlikely]] {
    logchan_vkprof->log("sampleBegin(%s::%s) called but frameBegin was never called! Skipping.", _name.c_str(), s->_name.c_str());
    return;
  }
  if (_query_index >= MAX_GPU_PERF_QUERIES) [[unlikely]] {
    logchan_vkprof->log("sampleBegin(%s::%s) queries exhausted! Increase MAX_GPU_PERF_QUERIES or reduce samples per frame. Skipping.", _name.c_str(), s->_name.c_str());
    return;
  }
  if (s->_call_level != -1) [[unlikely]] {
    logchan_vkprof->log("sampleBegin(%s::%s) _call_level=%d already sampling! Ensure sampleEnd was called or use sampleScope. Skipping.",
        _name.c_str(), s->_name.c_str(), s->_call_level);
    return;
  }

  int current_query_index = _query_index++;
  vkCmdWriteTimestamp(_cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _query_pool, current_query_index);

  s->_call_level = _current_level++;
  s->_sampling   = true;
  s->_call_count++;

	// pause parent time by pushing a span which will end at the current query index
  if (!_vk_span_stack.empty()) {
    auto& parent = _vk_span_stack.top();
    _vk_spans.push_back({ .series = parent.series, .begin_query = parent.begin_query, .end_query = current_query_index });
  }

  _vk_span_stack.push({ .series = s, .begin_total_query = current_query_index, .begin_query = current_query_index, .end_query = -1 });
}
 
void VkProfilerChannel::sampleEnd(SampleProfilerSeries* s) {
  if (!_recording) [[unlikely]] return;
  if (_cmdbuf == VK_NULL_HANDLE) [[unlikely]] {
    logchan_vkprof->log("sampleEnd(%s::%s) called but frameBegin was never called! Skipping.", _name.c_str(), s->_name.c_str());
    return;
  }
  if (s->_call_level == -1) [[unlikely]] {
    logchan_vkprof->log("sampleEnd(%s::%s) but _call_level=-1 not sampling! Ensure sampleBegin was called or use sampleScope! Skipping.",
        _name.c_str(), s->_name.c_str());
    return;
  }

  int current_query_index = _query_index++;
  vkCmdWriteTimestamp(_cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _query_pool, current_query_index);

  while (!_vk_span_stack.empty()) {
    auto& top = _vk_span_stack.top();
    auto  top_series = top.series;

    _vk_total_spans.push_back({ .series = s,          .begin_total_query = top.begin_total_query, .end_query = current_query_index });
    _vk_spans.push_back(      { .series = top_series, .begin_query       = top.begin_query,       .end_query = current_query_index });
    top_series->_max_call_level = std::max(top_series->_max_call_level, _current_level);
    top_series->_call_level     = -1;
    top_series->_sampling       = false;
    _current_level--;
    OrkAssertI(_current_level >= 0, "VkProfilerChannel _current_level should never go below 0!");
    _vk_span_stack.pop();

    // resume parent
    if (!_vk_span_stack.empty())
      _vk_span_stack.top().begin_query = current_query_index;

		// pop and end samples for all children of passed in series
    if (top_series == s)
      return;
  }
  logchan_vkprof->log("sampleEnd(%s::%s) beginSample never called for this series!", _name.c_str(), s->_name.c_str());
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
//////////////////////////////////////////////////////////////////////////////
