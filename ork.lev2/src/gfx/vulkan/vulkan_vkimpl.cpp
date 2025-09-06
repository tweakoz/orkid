////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>
#if defined(ENABLE_VULKAN)
#include "headers/vulkan_ctx.h"
#import <ork/lev2/glfw/ctx_glfw.h>

namespace ork::lev2::vulkan {

vkinstance_ptr_t _GVI = nullptr;
constexpr bool _enable_validate = true;
constexpr bool _enable_renderdoc = false;
constexpr bool _enable_debug = (_enable_validate or _enable_renderdoc);
///////////////////////////////////////////////////////////////////////////////////////////////

using layer_props_t = std::vector<VkLayerProperties>;

static layer_props_t _layerProperties() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  layer_props_t layer_props(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layer_props.data());
  return layer_props;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static bool _hasLayer(layer_props_t& layer_props, std::string layerName) {
  bool has_layer = false;
  for (const auto& lprop : layer_props) {
    if (strcmp(lprop.layerName, layerName.c_str()) == 0) {
     has_layer = true;
    }
  }
  printf("has_layer<%s> : %s\n", layerName.c_str(), has_layer ? "true": "false");
  return has_layer;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(       //
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    //
    VkDebugUtilsMessageTypeFlagsEXT messageType,               //
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, //
    void* pUserData) {                                         //
  //std::cerr << "Vulkan Validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE; // abort ?
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VulkanInstance::_setupDebugMessenger() {
  return;//
  VkDebugUtilsMessengerEXT debugMessenger;
  initializeVkStruct(debugMessenger);

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
  initializeVkStruct(debugCreateInfo,VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);

  debugCreateInfo.messageSeverity   = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT //
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT                  //
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT      //
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT //
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugCreateInfo.pfnUserCallback = vk_debug_callback;
  debugCreateInfo.pUserData       = (void*)this;

  // Note: vkCreateDebugUtilsMessengerEXT is not directly available. You have to fetch its address.
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(_instance, &debugCreateInfo, nullptr, &debugMessenger);
  } else {
    std::cerr << "Could not set up debug messenger!" << std::endl;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

vkdeviceinfo_ptr_t VulkanInstance::findDeviceForSurface(VkSurfaceKHR surface){
  for( auto devinfo : _device_infos ){
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(devinfo->_phydev, 0, surface, &presentSupport);
    if(presentSupport){
      return devinfo;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::VulkanInstance() {

// printf( "VulkanInstance::VulkanInstance() HERE!!!\n");

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  auto yel = fvec3::Yellow();

  std::vector<const char*> validation_layers;

  if( _enable_validate ){
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
  }
  if( _enable_renderdoc ){
    validation_layers.push_back("VK_LAYER_RENDERDOC_Capture");
  }
  auto layer_props = _layerProperties();
  for(size_t i=0; i<layer_props.size(); i++){
   printf("layer<%d:%s>\n", i, layer_props[i].layerName);
  }
    _debugEnabled    = _enable_debug and _hasLayer(layer_props, validation_layers[0]);

  initializeVkStruct(_appdata,VK_STRUCTURE_TYPE_APPLICATION_INFO);
  _appdata.pApplicationName   = "Orkid";
  _appdata.applicationVersion = 1;
  _appdata.pEngineName        = "Orkid";
  _appdata.engineVersion      = 1;
  _appdata.apiVersion         = VK_API_VERSION_1_3;

  initializeVkStruct(_instancedata,VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
  _instancedata.pApplicationInfo        = &_appdata;

  if(_debugEnabled ){
  _instancedata.enabledLayerCount       = uint32_t(validation_layers.size());
  _instancedata.ppEnabledLayerNames     = validation_layers.data();
  }
  else{
  _instancedata.enabledLayerCount       = 0;
  _instancedata.ppEnabledLayerNames     = nullptr;
  }

  _slp_cache = std::make_shared<shadlang::ShadLangParserCache>();

  _instance_extensions.push_back("VK_EXT_debug_utils");
  _instance_extensions.push_back("VK_EXT_debug_report");
 //_instance_extensions.push_back("VK_KHR_dynamic_rendering");

  for( size_t i=0; i<glfwExtensionCount; i++ ){
    _instance_extensions.push_back(glfwExtensions[i]);
  }

  _instance_extensions.push_back("VK_KHR_surface");

#if defined(__APPLE__)
  _instance_extensions.push_back("VK_MVK_macos_surface");
  _instance_extensions.push_back("VK_EXT_metal_surface");
  _instance_extensions.push_back("VK_EXT_headless_surface");
  _instance_extensions.push_back("VK_KHR_portability_enumeration");
  _instancedata.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
 #else 
  _instance_extensions.push_back("VK_KHR_xcb_surface");
  _instance_extensions.push_back("VK_EXT_headless_surface");
#endif

  _instancedata.enabledExtensionCount   = _instance_extensions.size();
  _instancedata.ppEnabledExtensionNames = _instance_extensions.data();

  //printf("num vk instance extensions<%zu>\n", _instance_extensions.size());

  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  printf("Working dir: %s\n", cwd);
  printf("VK_ICD_FILENAMES: %s\n", getenv("VK_ICD_FILENAMES"));
  printf("VK_LAYER_PATH: %s\n", getenv("VK_LAYER_PATH"));
  printf("DYLD_LIBRARY_PATH: %s\n", getenv("DYLD_LIBRARY_PATH"));
  printf("MVK_CONFIG_LOG_LEVEL: %s\n", getenv("MVK_CONFIG_LOG_LEVEL"));

  VkResult res = vkCreateInstance(&_instancedata, nullptr, &_instance);
  OrkAssert(res == 0);

  //deco::printf(yel, "vulkan::_init instance<%p> res<%d>\n", (void*) & _instance, int(res));

  /////////////////////////////////////////////////////////////////////////////
  // check device groups (for later multidevice support)
  /////////////////////////////////////////////////////////////////////////////

  res = vkEnumeratePhysicalDeviceGroups(_instance, &_numgroups, nullptr);
  _phygroups.resize(_numgroups);
  vkEnumeratePhysicalDeviceGroups(_instance, &_numgroups, _phygroups.data());
  deco::printf(yel, "vulkan::_init numgroups<%u>\n", _numgroups);
  int igroup = 0;
  for (auto& group : _phygroups) {
    vkdevgrp_ptr_t dev_group_out = std::make_shared<VulkanDeviceGroup>();
    _devgroups.push_back(dev_group_out);

    dev_group_out->_deviceCount = group.physicalDeviceCount;
    deco::printf(yel, "vulkan::_init grp<%d> numgpus<%zu>\n", igroup, dev_group_out->_deviceCount);
    for (int idev = 0; idev < dev_group_out->_deviceCount; idev++) {
      auto device_info = std::make_shared<VulkanDeviceInfo>();
      dev_group_out->_device_infos.push_back(device_info);
      _device_infos.push_back(device_info);
      device_info->_phydev = group.physicalDevices[idev];
      vkGetPhysicalDeviceProperties(device_info->_phydev, &device_info->_devprops);
      device_info->_is_discrete = (device_info->_devprops.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
      if(1)deco::printf(
          yel,
          "    gouup<%d> gpu<%d:%s> is_discrete<%d>\n",
          igroup,
          device_info->_devprops.deviceID,
          device_info->_devprops.deviceName,
          int(device_info->_is_discrete));

      // Check for Vulkan 1.3 and dynamic rendering support
      initializeVkStruct(device_info->_devfeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            
      VkPhysicalDeviceVulkan13Features vk13Features{};
      initializeVkStruct(vk13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
      vk13Features.dynamicRendering = VK_TRUE;
      device_info->_devfeatures2.pNext = &vk13Features;
      
      vkGetPhysicalDeviceFeatures2(device_info->_phydev, &device_info->_devfeatures2);
      device_info->_devfeatures = device_info->_devfeatures2.features;
      
      //device_info->_supportsDynamicRendering = dynRenderFeatures.dynamicRendering;
      device_info->_supportsVulkan13 = true;
      

    }
    igroup++;
  }

  /////////////////////////////////////////////////////////////////////////////

  res = vkEnumeratePhysicalDevices(_instance, &_numgpus, nullptr);
  OrkAssert(_device_infos.size() == _numgpus);

  // std::vector<VkPhysicalDevice> phydevs(_numgpus);
  // vkEnumeratePhysicalDevices(_instance, &_numgpus, phydevs.data());

  //deco::printf(yel, "vulkan::_init numgpus<%u>\n", _numgpus);
  for (auto device_info : _device_infos) {

    const auto& phy    = device_info->_phydev;
    auto& dev_props    = device_info->_devprops;
    auto& dev_feats    = device_info->_devfeatures;
    auto& dev_memprops = device_info->_devmemprops;

    vkGetPhysicalDeviceProperties(phy, &dev_props);
    vkGetPhysicalDeviceFeatures(phy, &dev_feats);
    bool is_discrete           = (dev_props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    device_info->_maxWkgCountX = dev_props.limits.maxComputeWorkGroupCount[0];
    device_info->_maxWkgCountY = dev_props.limits.maxComputeWorkGroupCount[1];
    device_info->_maxWkgCountZ = dev_props.limits.maxComputeWorkGroupCount[2];

    deco::printf(yel, "vulkan::_init gpu<%d:%s> is_discrete<%d>\n", dev_props.deviceID, dev_props.deviceName, int(is_discrete));
    deco::printf(yel, "         apiVersion<%u>\n", dev_props.apiVersion);
    deco::printf(yel, "         maxImageDimension1D<%u>\n", dev_props.limits.maxImageDimension1D);
    deco::printf(yel, "         maxImageDimension2D<%u>\n", dev_props.limits.maxImageDimension2D);
    deco::printf(yel, "         maxImageDimension3D<%u>\n", dev_props.limits.maxImageDimension3D);
    deco::printf(yel, "         maxImageDimensionCube<%u>\n", dev_props.limits.maxImageDimensionCube);
    deco::printf(yel, "         maxImageArrayLayers<%u>\n", dev_props.limits.maxImageArrayLayers);

    deco::printf(yel, "         maxBoundDescriptorSets<%u>\n", dev_props.limits.maxBoundDescriptorSets);
    deco::printf(yel, "         maxPerStageDescriptorSamplers<%u>\n", dev_props.limits.maxPerStageDescriptorSamplers);
    deco::printf(yel, "         maxPerStageDescriptorUniformBuffers<%u>\n", dev_props.limits.maxPerStageDescriptorUniformBuffers);
    deco::printf(yel, "         maxPerStageDescriptorStorageBuffers<%u>\n", dev_props.limits.maxPerStageDescriptorStorageBuffers);
    deco::printf(yel, "         maxPerStageDescriptorSampledImages<%u>\n", dev_props.limits.maxPerStageDescriptorSampledImages);
    deco::printf(yel, "         maxPerStageDescriptorStorageImages<%u>\n", dev_props.limits.maxPerStageDescriptorStorageImages);
    deco::printf(yel, "         maxPerStageDescriptorInputAttachments<%u>\n", dev_props.limits.maxPerStageDescriptorInputAttachments);

    deco::printf(yel, "         maxUniformBufferRange<%u>\n", dev_props.limits.maxUniformBufferRange);
    deco::printf(yel, "         maxFramebufferWidth<%u>\n", dev_props.limits.maxFramebufferWidth);
    deco::printf(yel, "         maxFramebufferLayers<%u>\n", dev_props.limits.maxFramebufferLayers);
    deco::printf(yel, "         maxColorAttachments<%u>\n", dev_props.limits.maxColorAttachments);
    deco::printf(yel, "         maxComputeSharedMemorySize<%u>\n", dev_props.limits.maxComputeSharedMemorySize);
    deco::printf(yel, "         maxComputeWorkGroupSize<%u>\n", dev_props.limits.maxComputeWorkGroupSize);
    deco::printf(yel, "         maxPushConstantsSize<%u>\n", dev_props.limits.maxPushConstantsSize);

    deco::printf(yel, "         maxPushConstantsSize<%u>\n", dev_props.limits.maxPushConstantsSize);

    deco::printf(
        yel,
        "         maxcomputewkgcount<%u,%u,%u>\n",
        device_info->_maxWkgCountX,
        device_info->_maxWkgCountY,
        device_info->_maxWkgCountZ);
    deco::printf(yel, "         feat.fragmentStoresAndAtomics<%u>\n", int(dev_feats.fragmentStoresAndAtomics));
    deco::printf(yel, "         feat.shaderFloat64<%u>\n", int(dev_feats.shaderFloat64));
    deco::printf(yel, "         feat.sparseBinding<%u>\n", int(dev_feats.sparseBinding));
    deco::printf(yel, "         feat.multiDrawIndirect<%u>\n", int(dev_feats.multiDrawIndirect));

    vkGetPhysicalDeviceMemoryProperties(phy, &dev_memprops);
    auto heaps = dev_memprops.memoryHeaps;
    std::vector<VkMemoryHeap> heapsvect(heaps, heaps + dev_memprops.memoryHeapCount);
    for (const auto& heap : heapsvect) {
      if (heap.flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        // deco::printf(yel, "         heap.size<%zu>\n", heap.size);
      }
    }
    uint32_t numqfamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &numqfamilies, nullptr);
    device_info->_queueprops.resize(numqfamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &numqfamilies, device_info->_queueprops.data());

    uint32_t numextensions = 0;
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &numextensions, nullptr);
    device_info->_extensions.resize(numextensions);
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &numextensions, device_info->_extensions.data());
    for (auto ext : device_info->_extensions) {
      //deco::printf(yel, "         extension: <%s>\n", ext.extensionName);
      device_info->_extension_set.insert(ext.extensionName);
    }
  } // for(auto& phy : phydevs){

  for (int i = 0; i < 1; i++) {
    auto loadctx = std::make_shared<VkLoadContext>();
    // loadctx->_global_plato  = GlOsxPlatformObject::_global_plato;
    load_token_t token;
    token.setShared<VkLoadContext>(loadctx);
    _loadTokens.push(token);
  }

  if (_debugEnabled)
    _setupDebugMessenger();

      static auto gctx = CtxGLFW::globalOffscreenContext();

}

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::~VulkanInstance() {
  vkDestroyInstance(_instance, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void touchClasses() {
  VkContext::GetClassStatic();
}

context_ptr_t createLoaderContext() {

  ///////////////////////////////////////////////////////////
  auto loader = std::make_shared<FxShaderLoader>();
  FxShader::RegisterLoaders("shaders/fxv2/", "fxv2");
  auto shadctx = FileEnv::contextForUriProto("orkshader://");
  auto democtx = FileEnv::contextForUriProto("demo://");
  loader->addLocation(shadctx, ".fxv2"); // for glsl targets
  if (democtx) {
    loader->addLocation(democtx, ".fxv2"); // for glsl targets
  }
  ///////////////////////////////////////////////////////////

  asset::registerLoader<FxShaderAsset>(loader);

  _GVI       = std::make_shared<VulkanInstance>();
  auto clazz = dynamic_cast<object::ObjectClass*>(VkContext::GetClassStatic());
  GfxEnv::setContextClass(clazz);
  auto target = std::make_shared<VkContext>();
  target->initializeLoaderContext();
  GfxEnv::initializeWithContext(target);
  return target;
}

///////////////////////////////////////////////////////////////////////////////

VkFormatConverter::VkFormatConverter() {

  auto do_format = [this](EBufferFormat ork_fmt, VkFormat vk_fmt) {
    _fmtmap[ork_fmt] = vk_fmt;
    _inv_fmtmap[vk_fmt] = ork_fmt;
  };

  #if ! defined(__APPLE__)
  do_format(EBufferFormat::RGB8, VK_FORMAT_R8G8B8_UNORM);
  do_format(EBufferFormat::S3TC_DXT1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
  do_format(EBufferFormat::S3TC_DXT3, VK_FORMAT_BC2_UNORM_BLOCK);
  do_format(EBufferFormat::BGR8, VK_FORMAT_B8G8R8_UNORM);
  #endif

  do_format(EBufferFormat::SRGB_BGRA8, VK_FORMAT_B8G8R8A8_SRGB);
  do_format(EBufferFormat::RGBA8, VK_FORMAT_R8G8B8A8_UNORM);
  do_format(EBufferFormat::RGB16, VK_FORMAT_R16G16B16A16_UNORM);
  do_format(EBufferFormat::RGBA16, VK_FORMAT_R16G16B16A16_UNORM);
  do_format(EBufferFormat::BGR5A1, VK_FORMAT_B5G5R5A1_UNORM_PACK16);
  do_format(EBufferFormat::BGRA8, VK_FORMAT_B8G8R8A8_UNORM);
  do_format(EBufferFormat::R32F,VK_FORMAT_R32_SFLOAT);
  do_format(EBufferFormat::Z32F, VK_FORMAT_D32_SFLOAT);
  do_format(EBufferFormat::Z24S8, VK_FORMAT_D24_UNORM_S8_UINT);
  do_format(EBufferFormat::Z32FS8, VK_FORMAT_D32_SFLOAT_S8_UINT);
  do_format(EBufferFormat::RGBA16F, VK_FORMAT_R16G16B16A16_SFLOAT);
  do_format(EBufferFormat::RGBA32F, VK_FORMAT_R32G32B32A32_SFLOAT);
  do_format(EBufferFormat::RGBA32UI, VK_FORMAT_R32G32B32A32_UINT);
  do_format(EBufferFormat::RGBA16UI, VK_FORMAT_R16G16B16A16_UINT);
  
  do_format(EBufferFormat::RGB10A2, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
  do_format(EBufferFormat::RGB32UI, VK_FORMAT_R32G32B32_UINT);
  do_format(EBufferFormat::R8, VK_FORMAT_R8_UNORM);
  do_format(EBufferFormat::RG16F, VK_FORMAT_R16G16_SFLOAT);
  do_format(EBufferFormat::RG32F, VK_FORMAT_R32G32_SFLOAT);
  do_format(EBufferFormat::RGB32F, VK_FORMAT_R32G32B32_SFLOAT);
  do_format(EBufferFormat::R32UI, VK_FORMAT_R32_UINT);
  do_format(EBufferFormat::RGB16, VK_FORMAT_R16G16B16_UNORM);
  
  do_format(EBufferFormat::RGBA_BPTC_UNORM, VK_FORMAT_BC7_UNORM_BLOCK);
  do_format(EBufferFormat::SRGB_ALPHA_BPTC_UNORM, VK_FORMAT_BC7_SRGB_BLOCK);

  _layoutmap["depth"_crcu]   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  _layoutmap["color"_crcu]   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  _layoutmap["swapchain"_crcu] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  // VK_IMAGE_LAYOUT_PREINITIALIZED
  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

  _aspectmap["depth"_crcu]   = VK_IMAGE_ASPECT_DEPTH_BIT;
  _aspectmap["color"_crcu]   = VK_IMAGE_ASPECT_COLOR_BIT;
  _aspectmap["swapchain"_crcu] = VK_IMAGE_ASPECT_COLOR_BIT;
}
VkFormat VkFormatConverter::convertBufferFormat(EBufferFormat fmt_in) {
  auto fmtname = EBufferFormatToName(fmt_in);
  //printf("convertBufferFormat<%s>\n", fmtname.c_str());
  auto it = _instance._fmtmap.find(fmt_in);
  if( it == _instance._fmtmap.end() ){
    printf("format<%s> conversion not present\n", fmtname.c_str());
    OrkAssert(false);
  }
  return it->second;
}
EBufferFormat VkFormatConverter::convertBufferFormat(VkFormat fmt_in) {
  auto it = _instance._inv_fmtmap.find(fmt_in);
  OrkAssert(it != _instance._inv_fmtmap.end());
  return it->second;
}
VkImageLayout VkFormatConverter::layoutForUsage(uint64_t usage) {
  auto it = _instance._layoutmap.find(usage);
  OrkAssert(it != _instance._layoutmap.end());
  return it->second;
}
VkImageAspectFlagBits VkFormatConverter::aspectForUsage(uint64_t usage) {
  if(usage=="depth"_crcu) {
  }
  auto it = _instance._aspectmap.find(usage);
  OrkAssert(it != _instance._aspectmap.end());
  return it->second;
}

const VkFormatConverter VkFormatConverter::VkFormatConverter::_instance;

///////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan

#endif
