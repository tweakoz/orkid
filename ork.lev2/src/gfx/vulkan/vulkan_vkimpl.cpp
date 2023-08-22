#include <ork/pch.h>
#include <ork/kernel/string/deco.inl>
#if defined(ENABLE_VULKAN)
#include "vulkan_ctx.h"

namespace ork::lev2::vulkan {

vkinstance_ptr_t _GVI = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::VulkanInstance() {

  auto yel = fvec3::Yellow();

  _appdata.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  _appdata.pNext              = NULL;
  _appdata.pApplicationName   = "Orkid";
  _appdata.applicationVersion = 1;
  _appdata.pEngineName        = "Orkid";
  _appdata.engineVersion      = 1;
  _appdata.apiVersion         = VK_API_VERSION_1_1;

  _instancedata.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  _instancedata.pNext                   = NULL;
  _instancedata.flags                   = 0;
  _instancedata.pApplicationInfo        = &_appdata;
  _instancedata.enabledExtensionCount   = 0;
  _instancedata.ppEnabledExtensionNames = NULL;
  _instancedata.enabledLayerCount       = 0;
  _instancedata.ppEnabledLayerNames     = NULL;

  VkResult res = vkCreateInstance(&_instancedata, NULL, &_instance);
  OrkAssert(res == 0);

  deco::printf(yel, "vulkan::_init res<%d>\n", int(res));

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
      auto out_device = std::make_shared<VulkanDevice>();
      dev_group_out->_devices.push_back(out_device);
      _devices.push_back(out_device);
      out_device->_phydev = group.physicalDevices[idev];
      vkGetPhysicalDeviceProperties(out_device->_phydev, &out_device->_devprops);
      out_device->_is_discrete = (out_device->_devprops.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
      deco::printf(
          yel,
          "    gouup<%d> gpu<%d:%s> is_discrete<%d>\n",
          igroup,
          out_device->_devprops.deviceID,
          out_device->_devprops.deviceName,
          int(out_device->_is_discrete));
    }
    igroup++;
  }

  /////////////////////////////////////////////////////////////////////////////

  res              = vkEnumeratePhysicalDevices(_instance, &_numgpus, nullptr);
  OrkAssert(_devices.size() == _numgpus);
  
  //std::vector<VkPhysicalDevice> phydevs(_numgpus);
  //vkEnumeratePhysicalDevices(_instance, &_numgpus, phydevs.data());

  deco::printf(yel, "vulkan::_init numgpus<%u>\n", _numgpus);
  for (auto out_device : _devices) {

    const auto& phy = out_device->_phydev;
    auto& dev_props = out_device->_devprops;
    auto& dev_feats = out_device->_devfeatures;
    auto& dev_memprops = out_device->_devmemprops;

    vkGetPhysicalDeviceProperties(phy, &dev_props);
    vkGetPhysicalDeviceFeatures(phy, &dev_feats);
    bool is_discrete = (dev_props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    out_device->_maxWkgCountX    = dev_props.limits.maxComputeWorkGroupCount[0];
    out_device->_maxWkgCountY    = dev_props.limits.maxComputeWorkGroupCount[1];
    out_device->_maxWkgCountZ    = dev_props.limits.maxComputeWorkGroupCount[2];

    deco::printf(
        yel, "vulkan::_init gpu<%d:%s> is_discrete<%d>\n", dev_props.deviceID, dev_props.deviceName, int(is_discrete));
    deco::printf(yel, "         apiver<%u>\n", dev_props.apiVersion);
    deco::printf(yel, "         maxdim3d<%u>\n", dev_props.limits.maxImageDimension3D);
    deco::printf(yel, "         maxubrange<%u>\n", dev_props.limits.maxUniformBufferRange);
    deco::printf(yel, "         maxfbwidth<%u>\n", dev_props.limits.maxFramebufferWidth);
    deco::printf(yel, "         maxfblayers<%u>\n", dev_props.limits.maxFramebufferLayers);
    deco::printf(yel, "         maxcolorattachments<%u>\n", dev_props.limits.maxColorAttachments);
    deco::printf(yel, "         maxcomputeshmsize<%u>\n", dev_props.limits.maxComputeSharedMemorySize);
    deco::printf(yel, "         maxcomputewkgsize<%u>\n", dev_props.limits.maxComputeWorkGroupSize);
    deco::printf(yel, "         maxcomputewkgcount<%u,%u,%u>\n", out_device->_maxWkgCountX, out_device->_maxWkgCountY, out_device->_maxWkgCountZ);
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
    out_device->_queueprops.resize(numqfamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &numqfamilies, out_device->_queueprops.data());

    uint32_t numextensions = 0;
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &numextensions, nullptr);
    out_device->_extensions.resize(numextensions);
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &numextensions, out_device->_extensions.data());
    for (auto ext : out_device->_extensions) {
       deco::printf(yel,"         extension: <%s>\n", ext.extensionName);
       out_device->_extension_set.insert(ext.extensionName);
    }
  } // for(auto& phy : phydevs){
}

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::~VulkanInstance() {
  vkDestroyInstance(_instance, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////

context_ptr_t ContextInit() {
  _GVI = std::make_shared<VulkanInstance>();
  auto clazz                   = dynamic_cast<object::ObjectClass*>(VkContext::GetClassStatic());
  GfxEnv::setContextClass(clazz);
  auto target = VkContext::makeShared();
  target->initializeLoaderContext();
  GfxEnv::initializeWithContext(target);
  return target;
}


///////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan

#endif
