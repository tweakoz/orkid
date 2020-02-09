#include <ork/pch.h>
#include <vulkan/vulkan.hpp>

namespace ork::lev2::vk{

  void init(){
    VkApplicationInfo vkappdata = {};
    vkappdata.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkappdata.pNext = NULL;
    vkappdata.pApplicationName = "Orkid";
    vkappdata.applicationVersion = 1;
    vkappdata.pEngineName = "Orkid";
    vkappdata.engineVersion = 1;
    vkappdata.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo vkinstancedata = {};
    vkinstancedata.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkinstancedata.pNext = NULL;
    vkinstancedata.flags = 0;
    vkinstancedata.pApplicationInfo = &vkappdata;
    vkinstancedata.enabledExtensionCount = 0;
    vkinstancedata.ppEnabledExtensionNames = NULL;
    vkinstancedata.enabledLayerCount = 0;
    vkinstancedata.ppEnabledLayerNames = NULL;

    VkInstance vkinst;
    VkResult res = vkCreateInstance(&vkinstancedata, NULL, &vkinst);
    OrkAssert(res == 0);
    vkDestroyInstance(vkinst, NULL);
  }

}
