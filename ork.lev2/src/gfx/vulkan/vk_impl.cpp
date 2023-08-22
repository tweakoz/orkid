#include <ork/pch.h>
#include <ork/kernel/string/deco.inl>
#if defined(ENABLE_VULKAN)
#include "vulkan_ctx.h"

namespace ork::lev2::vk {

void init() {

  VkApplicationInfo vkappdata  = {};
  vkappdata.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  vkappdata.pNext              = NULL;
  vkappdata.pApplicationName   = "Orkid";
  vkappdata.applicationVersion = 1;
  vkappdata.pEngineName        = "Orkid";
  vkappdata.engineVersion      = 1;
  vkappdata.apiVersion         = VK_API_VERSION_1_1;

  VkInstanceCreateInfo vkinstancedata    = {};
  vkinstancedata.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  vkinstancedata.pNext                   = NULL;
  vkinstancedata.flags                   = 0;
  vkinstancedata.pApplicationInfo        = &vkappdata;
  vkinstancedata.enabledExtensionCount   = 0;
  vkinstancedata.ppEnabledExtensionNames = NULL;
  vkinstancedata.enabledLayerCount       = 0;
  vkinstancedata.ppEnabledLayerNames     = NULL;

  VkInstance vkinst;
  VkResult res = vkCreateInstance(&vkinstancedata, NULL, &vkinst);
  OrkAssert(res == 0);

  deco::printf(fvec3::Yellow(), "vk::init res<%d>\n", int(res));

  /////////////////////////////////////////////////////////////////////////////
  // check device groups (for later multidevice support)
  /////////////////////////////////////////////////////////////////////////////

  uint32_t numgroups = 0;
  res = vkEnumeratePhysicalDeviceGroups(vkinst,&numgroups,nullptr);
  std::vector<VkPhysicalDeviceGroupProperties> phygroups;
  phygroups.resize(numgroups);
  vkEnumeratePhysicalDeviceGroups(vkinst, &numgroups, phygroups.data());
  deco::printf(fvec3::Yellow(), "vk::init numgroups<%u>\n", numgroups);
  int igroup = 0;
  for (auto& group : phygroups) {

    int devcount = group.physicalDeviceCount;
    deco::printf(fvec3::Yellow(), "vk::init grp<%d> numgpus<%d>\n", igroup, devcount);
    for( int idev=0; idev<devcount; idev++){
     const VkPhysicalDevice& dev = group.physicalDevices[idev];
      VkPhysicalDeviceProperties devprops;
      vkGetPhysicalDeviceProperties(dev, &devprops);
      bool is_discrete = (devprops.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    deco::printf(
        fvec3::Yellow(), "    gouup<%d> gpu<%d:%s> is_discrete<%d>\n", igroup, devprops.deviceID, devprops.deviceName, int(is_discrete));
    }
    igroup++;
  }

  /////////////////////////////////////////////////////////////////////////////

  uint32_t numgpus = 0;
  res = vkEnumeratePhysicalDevices(vkinst, &numgpus, nullptr);
  std::vector<VkPhysicalDevice> phydevs(numgpus);
  vkEnumeratePhysicalDevices(vkinst, &numgpus, phydevs.data());
  deco::printf(fvec3::Yellow(), "vk::init numgpus<%u>\n", numgpus);
  for (auto& phy : phydevs) {
    VkPhysicalDeviceProperties devprops;
    VkPhysicalDeviceFeatures devfeats;
    vkGetPhysicalDeviceProperties(phy, &devprops);
    vkGetPhysicalDeviceFeatures(phy, &devfeats);
    bool is_discrete = (devprops.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    auto wkgcount    = devprops.limits.maxComputeWorkGroupCount;
    deco::printf(
        fvec3::Yellow(), "vk::init gpu<%d:%s> is_discrete<%d>\n", devprops.deviceID, devprops.deviceName, int(is_discrete));
    // deco::printf(fvec3::Yellow(), "         apiver<%u>\n", devprops.apiVersion);
    // deco::printf(fvec3::Yellow(), "         maxdim3d<%u>\n", devprops.limits.maxImageDimension3D);
    // deco::printf(fvec3::Yellow(), "         maxubrange<%u>\n", devprops.limits.maxUniformBufferRange);
    // deco::printf(fvec3::Yellow(), "         maxfbwidth<%u>\n", devprops.limits.maxFramebufferWidth);
    // deco::printf(fvec3::Yellow(), "         maxfblayers<%u>\n", devprops.limits.maxFramebufferLayers);
    // deco::printf(fvec3::Yellow(), "         maxcolorattachments<%u>\n", devprops.limits.maxColorAttachments);
    // deco::printf(fvec3::Yellow(), "         maxcomputeshmsize<%u>\n", devprops.limits.maxComputeSharedMemorySize);
    // deco::printf(fvec3::Yellow(), "         maxcomputewkgsize<%u>\n", devprops.limits.maxComputeWorkGroupSize);
    // deco::printf(fvec3::Yellow(), "         maxcomputewkgcount<%u,%u,%u>\n", wkgcount[0], wkgcount[1], wkgcount[2]);
    // deco::printf(fvec3::Yellow(), "         feat.fragmentStoresAndAtomics<%u>\n", int(devfeats.fragmentStoresAndAtomics));
    // deco::printf(fvec3::Yellow(), "         feat.shaderFloat64<%u>\n", int(devfeats.shaderFloat64));
    // deco::printf(fvec3::Yellow(), "         feat.sparseBinding<%u>\n", int(devfeats.sparseBinding));
    // deco::printf(fvec3::Yellow(), "         feat.multiDrawIndirect<%u>\n", int(devfeats.multiDrawIndirect));

    VkPhysicalDeviceMemoryProperties memoryprops;
    vkGetPhysicalDeviceMemoryProperties(phy, &memoryprops);
    auto heaps = memoryprops.memoryHeaps;
    std::vector<VkMemoryHeap> heapsvect(heaps, heaps + memoryprops.memoryHeapCount);
    for (const auto& heap : heapsvect) {
      if (heap.flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        // deco::printf(fvec3::Yellow(), "         heap.size<%zu>\n", heap.size);
      }
    }
    uint32_t numqfamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &numqfamilies, nullptr);
    std::vector<VkQueueFamilyProperties> phyqfams(numqfamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &numqfamilies, phyqfams.data());
    uint32_t phy_numextensions;
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &phy_numextensions, nullptr);
    std::vector<VkExtensionProperties> phy_extensions(phy_numextensions);
    vkEnumerateDeviceExtensionProperties(phy, nullptr, &phy_numextensions, phy_extensions.data());
    for (auto ext : phy_extensions) {
      // deco::printf(fvec3::Yellow(),"         extension: <%s>\n", ext.extensionName);
    }
  } // for(auto& phy : phydevs){

  vkDestroyInstance(vkinst, NULL);
}

} // namespace ork::lev2::vk

#endif
