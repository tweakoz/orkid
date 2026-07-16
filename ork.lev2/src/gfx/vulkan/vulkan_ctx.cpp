////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#if defined(__linux__)
#include "headers/vk_swapchain_drm.h"
#include <sys/stat.h>
#include <sys/sysmacros.h>
#endif
#include "vulkan_captureasync.h"
#include "vulkan_ubo_dynamic.h"
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/external_gpu_requirements.h>
#include <ork/lev2/gfx/renderphasestats.h> // MT0: peekMs("present-idle")
#include <filesystem> // WS3 pipeline-cache dir creation (mac libc++ includes transitively; libstdc++ doesn't)

#define USE_OIIO
#if defined(USE_OIIO)
#include <OpenImageIO/imageio.h>
#include <filesystem> // pipeline-cache dir creation (libstdc++ needs the explicit include)
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

// X1 external-GPU-requirements seam self-test (env-gated; see vulkan_vkimpl.cpp).
static bool _extGpuSelfTestActive() {
  const char* v = std::getenv("ORKID_EXTGPU_SELFTEST");
  return v and (std::string(v) == "1");
}

////////////////////////////////////////////////////////////////////////////////

// Picks a GPU from the enumerated device list using ORKID_GPU_PREFER:
//   "discrete"   (default): first discrete GPU, else first device
//   "integrated": first integrated GPU, else first device
//   any other value: first device whose name contains the substring
//     (case-sensitive). Useful to single out a specific GPU on multi-GPU
//     systems, e.g. ORKID_GPU_PREFER=Radeon.
static vkdeviceinfo_ptr_t _pickPreferredDevice(const std::vector<vkdeviceinfo_ptr_t>& devs) {
  if (devs.empty()) return nullptr;
  const char* env = std::getenv("ORKID_GPU_PREFER");
  std::string mode = env ? env : "discrete";
  vkdeviceinfo_ptr_t picked = nullptr;
  if (mode == "discrete") {
    for (auto d : devs) if (d->_is_discrete) { picked = d; break; }
  } else if (mode == "integrated") {
    for (auto d : devs) {
      if (d->_devprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) { picked = d; break; }
    }
  } else {
    for (auto d : devs) {
      if (std::string(d->_devprops.deviceName).find(mode) != std::string::npos) { picked = d; break; }
    }
  }
  if (!picked) picked = devs.front();
  logchan_vkctx->log("ORKID_GPU_PREFER=<%s> picked device <%s>", mode.c_str(), picked->_devprops.deviceName);
  return picked;
}

#if defined(__linux__)
// Match a Vulkan physical device to a DRM card node so rendering and KMS
// scanout happen on the same GPU — dma-buf framebuffers cannot cross GPUs
// (on this multi-GPU path the first discrete GPU is not necessarily the one
// driving the selected connector).
static vkdeviceinfo_ptr_t _pickDeviceForDrmRdev(
    const std::vector<vkdeviceinfo_ptr_t>& devs, //
    dev_t rdev) {
  int64_t card_major = major(rdev);
  int64_t card_minor = minor(rdev);
  for (auto d : devs) {
    if (d->_extension_set.count(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME) == 0)
      continue;
    VkPhysicalDeviceDrmPropertiesEXT drm_props = {};
    drm_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2 props2 = {};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &drm_props;
    vkGetPhysicalDeviceProperties2(d->_phydev, &props2);
    bool primary_match = drm_props.hasPrimary && //
                         drm_props.primaryMajor == card_major && //
                         drm_props.primaryMinor == card_minor;
    bool render_match = drm_props.hasRender && //
                        drm_props.renderMajor == card_major && //
                        drm_props.renderMinor == card_minor;
    if (primary_match || render_match) {
      logchan_vkctx->log("matched vulkan device <%s> to DRM card <%ld:%ld>", d->_devprops.deviceName, card_major, card_minor);
      return d;
    }
  }
  logchan_vkctx->log("WARNING: no vulkan device matches DRM card <%ld:%ld>", card_major, card_minor);
  return nullptr;
}

static vkdeviceinfo_ptr_t _pickDeviceForDrmCard(
    const std::vector<vkdeviceinfo_ptr_t>& devs, //
    const std::string& card_path) {
  struct stat st;
  if (::stat(card_path.c_str(), &st) != 0 || !S_ISCHR(st.st_mode))
    return nullptr;
  return _pickDeviceForDrmRdev(devs, st.st_rdev);
}

static vkdeviceinfo_ptr_t _pickDeviceForDrmFd(
    const std::vector<vkdeviceinfo_ptr_t>& devs, //
    int drm_fd) {
  struct stat st;
  if (::fstat(drm_fd, &st) != 0 || !S_ISCHR(st.st_mode))
    return nullptr;
  return _pickDeviceForDrmRdev(devs, st.st_rdev);
}

// Resolve the requested DRM mode string (e.g. "c0") to its owning card and
// pick the matching Vulkan device.
static vkdeviceinfo_ptr_t _pickDeviceForDrmMode(
    const std::vector<vkdeviceinfo_ptr_t>& devs, //
    const std::string& drm_mode) {
  if (drm_mode.empty())
    return nullptr;
  char letter = ::tolower(drm_mode[0]);
  for (auto mon : drm::DRMContext::enumerateAllMonitors()) {
    if (mon->device_letter == letter && !mon->card_path.empty())
      return _pickDeviceForDrmCard(devs, mon->card_path);
  }
  return nullptr;
}
#endif

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
// Max hardware MSAA sample count usable for BOTH color and depth render targets.
// Intersect the two device limits and return the highest power-of-2 supported
// (Apple/MoltenVK typically caps at 4x, sometimes 8x). Used to clamp the requested
// --msaa level so we never ask for an unsupported sample count.
///////////////////////////////////////////////////////////////////////////////
int VkContext::msaaMaxSamples() {
  if (not _vkdeviceinfo)
    return 1;
  VkSampleCountFlags flags = _vkdeviceinfo->_devprops.limits.framebufferColorSampleCounts &
                             _vkdeviceinfo->_devprops.limits.framebufferDepthSampleCounts;
  if (flags & VK_SAMPLE_COUNT_16_BIT) return 16;
  if (flags & VK_SAMPLE_COUNT_8_BIT)  return 8;
  if (flags & VK_SAMPLE_COUNT_4_BIT)  return 4;
  if (flags & VK_SAMPLE_COUNT_2_BIT)  return 2;
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// Vulkan Context Internal Init
///////////////////////////////////////////////////////////////////////////////

void VkContext::_initVulkanForDevInfo(vkdeviceinfo_ptr_t vk_devinfo) {

  // X1: honor an externally-required physical device (VR/XR binds a specific GPU).
  //  This is the single chokepoint every device-creation path funnels through, so
  //  the loader/main context — the one XR binds — and every reuse of it end up on
  //  the required device. Neutral when no device is required.
  if (auto reqs = externalGpuRequirements(); reqs and reqs->_requiredPhysicalDevice) {
    VkPhysicalDevice required = (VkPhysicalDevice)reqs->_requiredPhysicalDevice;
    vkdeviceinfo_ptr_t required_info = nullptr;
    for (auto d : _GVI->_device_infos)
      if (d->_phydev == required) { required_info = d; break; }
    OrkAssertI(required_info != nullptr, "externally-required physical device not found among enumerated devices");
    bool has_gfx = false;
    for (const auto& qp : required_info->_queueprops)
      if (qp.queueCount > 0 and (qp.queueFlags & VK_QUEUE_GRAPHICS_BIT)) { has_gfx = true; break; }
    OrkAssertI(has_gfx, "externally-required physical device has no graphics queue family");
    if (required_info != vk_devinfo) {
      logchan_vkctx->log(
          "ext-gpu: overriding device <%s> with required device <%s>",
          vk_devinfo->_devprops.deviceName,
          required_info->_devprops.deviceName);
      vk_devinfo       = required_info;
      _GVI->_preferred = required_info;
    }
    if (_extGpuSelfTestActive()) {
      printf("ORKID_EXTGPU_SELFTEST: honored required physical device <%s>\n", vk_devinfo->_devprops.deviceName);
      fflush(stdout);
    }
  }

  logchan_vkctx->log("VkContext: using device <%s>", vk_devinfo->_devprops.deviceName);

  _vkphysicaldevice    = vk_devinfo->_phydev;
  _vkdeviceinfo        = vk_devinfo;

  ////////////////////////////
  // get queue families
  ////////////////////////////

  u32      gfx_qfid = NO_QUEUE;
  _vkqfid_compute   = NO_QUEUE;
  _vkqfid_transfer  = NO_QUEUE;

  _num_queue_types = vk_devinfo->_queueprops.size();
  std::vector<float> queuePriorities(_num_queue_types, 1.0f);

  for (uint32_t i = 0; i < _num_queue_types; i++) {
    const auto& QPROP = vk_devinfo->_queueprops[i];
    if (QPROP.queueCount == 0)
      continue;
    
    VkDeviceQueueCreateInfo DQCI;
    initializeVkStruct(DQCI, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);

    DQCI.queueFamilyIndex = i;
    DQCI.queueCount       = 1;
    DQCI.pQueuePriorities = queuePriorities.data();

    bool add = false;
    if (QPROP.queueFlags & VK_QUEUE_GRAPHICS_BIT && gfx_qfid == NO_QUEUE) {
      gfx_qfid = i;
      add       = true;
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

  OrkAssert(gfx_qfid != NO_QUEUE);
  OrkAssert(_vkqfid_compute != NO_QUEUE);
  OrkAssert(_vkqfid_transfer != NO_QUEUE);

  ////////////////////////////
  // create device
  ////////////////////////////

  // Only request swapchain extension for windowed (non-offscreen) rendering
  // In headless/offscreen mode, swapchain is not needed and may not be available
  // In DRM mode the instance has no VK_KHR_surface, and presentation goes through
  // dma-buf/KMS (vulkan_swapchain_drm.cpp), so VK_KHR_swapchain must not be requested
  bool is_offscreen = (_ginitdata && _ginitdata->_offscreen);
  bool use_drm      = (_ginitdata && _ginitdata->_use_drm);
  if (!is_offscreen && !use_drm) {
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

  // Linux cross-process sharing extensions
#if defined(__linux__)
  _device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  _device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  _device_extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  _device_extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);

  // DRM-specific extensions
  if(_ginitdata && _ginitdata->_use_drm) {
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

  // dynamic cull mode (so mtl.doubleSided takes effect without rebuilding pipelines)
  _device_extensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

  // X1 device-ext selftest leg (headless-observable): request one AVAILABLE device
  //  extension not already in the base set, injected into the backend-internal reqs
  //  slot EXACTLY like a real producer's would be, so the device-ext merge + the
  //  final-enabled-list are asserted end-to-end. This leg was previously untested —
  //  the XR device-ext merge (the OpenXR host_image_copy crash) could silently
  //  regress. Runs headless on any platform under ORKID_EXTGPU_SELFTEST=1.
  std::string _extgpu_selftest_dev_append;
  if (_extGpuSelfTestActive()) {
    for (const auto& avail : vk_devinfo->_extension_set) {
      bool in_base = false;
      for (auto b : _device_extensions)
        if (0 == strcmp(b, avail.c_str())) { in_base = true; break; }
      if (not in_base) { _extgpu_selftest_dev_append = avail; break; }
    }
    if (not _extgpu_selftest_dev_append.empty())
      _externalGpuRequirementsMutable()->_deviceExtensions.push_back(_extgpu_selftest_dev_append);
    printf("ORKID_EXTGPU_SELFTEST: request append device ext <%s>\n", _extgpu_selftest_dev_append.c_str());
    fflush(stdout);
  }

  // X1: merge externally-required device extensions (e.g. XR's device ext list),
  //  deduping by name so the platform/portability device set is never disturbed.
  //  Neutral when nothing is registered.
  if (auto reqs = externalGpuRequirements()) {
    for (const auto& ext : reqs->_deviceExtensions) {
      bool already = false;
      for (auto e : _device_extensions)
        if (0 == strcmp(e, ext.c_str())) { already = true; break; }
      if (already) {
        logchan_vkctx->log("ext-gpu: device ext <%s> already present (dedup)", ext.c_str());
      } else {
        _device_extensions.push_back(ext.c_str());
        logchan_vkctx->log("ext-gpu: merging external device ext <%s>", ext.c_str());
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // FINAL enabled device-extension list — logged UNCONDITIONALLY (diagnosability:
  //  an OpenXR runtime proc-loads its ENTIRE device table against THIS VkDevice on
  //  xrCreateSession; a required ext missing here = a hard trap inside the runtime
  //  on the first NULL proc. This line is the proof of what was actually enabled).
  //  Also builds the enabled-set that gates the ext->feature chaining below.
  ////////////////////////////////////////////////////////////////////////////
  std::set<std::string> _enabled_dev_ext_set;
  {
    std::string joined;
    for (auto e : _device_extensions) {
      _enabled_dev_ext_set.insert(e);
      joined += e;
      joined += " ";
    }
    logchan_vkctx->log("device extensions ENABLED (count=%zu): %s", _device_extensions.size(), joined.c_str());
    printf("[VKDEV] enabled device extensions (count=%zu): %s\n", _device_extensions.size(), joined.c_str());
    fflush(stdout);
  }
  if (_extGpuSelfTestActive()) {
    bool present = (not _extgpu_selftest_dev_append.empty()) and (_enabled_dev_ext_set.count(_extgpu_selftest_dev_append) > 0);
    printf("ORKID_EXTGPU_SELFTEST: device ext append <%s> present=%d\n", _extgpu_selftest_dev_append.c_str(), present ? 1 : 0);
    fflush(stdout);
  }

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
  VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extDynStateFeatures{};

  initializeVkStruct(timelineFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
  initializeVkStruct(dynrenderfeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
  initializeVkStruct(ycbcrFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
  initializeVkStruct(extDynStateFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);

  timelineFeatures.timelineSemaphore = VK_TRUE;
  dynrenderfeat.dynamicRendering = VK_TRUE;
  ycbcrFeatures.samplerYcbcrConversion = VK_TRUE;
  extDynStateFeatures.extendedDynamicState = VK_TRUE;

  DCI.pNext = (void*) & timelineFeatures;
  timelineFeatures.pNext = (void*) & dynrenderfeat;
  dynrenderfeat.pNext = (void*) & ycbcrFeatures;
  ycbcrFeatures.pNext = (void*) & extDynStateFeatures;
  extDynStateFeatures.pNext = (void*) nullptr;

  ////////////////////////////////////////////////////////////////////////////
  // Generic known-ext -> feature-struct chain. Some extensions (e.g. an OpenXR
  //  runtime's VK_EXT_host_image_copy client-import path) require BOTH the ext
  //  enabled AND its feature bit set to legally use the commands the consumer
  //  proc-loads. For each ENABLED device ext that advertises such a feature,
  //  confirm the physical device supports it (vkGetPhysicalDeviceFeatures2) and
  //  chain it VK_TRUE. A supported=false with the ext enabled is a driver/modeset
  //  regression -> loud warn. Extend by adding a case (table-at-the-site design).
  ////////////////////////////////////////////////////////////////////////////
  VkBaseOutStructure* ext_feat_head = nullptr;
  VkBaseOutStructure** ext_feat_tail = &ext_feat_head;
  auto appendExtFeature = [&](void* s) {
    auto* base = reinterpret_cast<VkBaseOutStructure*>(s);
    base->pNext = nullptr;
    *ext_feat_tail = base;
    ext_feat_tail  = &base->pNext;
  };
  auto extEnabled = [&](const char* name) { return _enabled_dev_ext_set.count(name) > 0; };

#ifdef VK_EXT_host_image_copy
  VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeat{};
  initializeVkStruct(hostImageCopyFeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT);
  if (extEnabled(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME)) {
    VkPhysicalDeviceFeatures2 probe{};
    initializeVkStruct(probe, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    probe.pNext = &hostImageCopyFeat;
    vkGetPhysicalDeviceFeatures2(_vkphysicaldevice, &probe);
    if (hostImageCopyFeat.hostImageCopy) {
      appendExtFeature(&hostImageCopyFeat);
      logchan_vkctx->log("ext-gpu: chaining VkPhysicalDeviceHostImageCopyFeaturesEXT{hostImageCopy=TRUE}");
    } else {
      printf("[VKDEV] WARNING: VK_EXT_host_image_copy ENABLED but hostImageCopy feature UNSUPPORTED on <%s> — runtime import will trap.\n",
             _vkdeviceinfo->_devprops.deviceName);
      fflush(stdout);
    }
  }
#else
  // SDK header lag: the ext name may still be enabled (it's a runtime-supplied
  //  string), but without the feature struct we cannot chain it — a newer OpenXR
  //  runtime that imports via host_image_copy WILL trap. Loud fleet diagnostic.
  if (_enabled_dev_ext_set.count("VK_EXT_host_image_copy")) {
    printf("[VKDEV] WARNING: VK_EXT_host_image_copy ENABLED but the staged Vulkan SDK headers lack its feature struct — rebuild with newer headers.\n");
    fflush(stdout);
  }
#endif
#ifdef VK_KHR_present_id
  VkPhysicalDevicePresentIdFeaturesKHR presentIdFeat{};
  initializeVkStruct(presentIdFeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR);
  if (extEnabled(VK_KHR_PRESENT_ID_EXTENSION_NAME)) {
    VkPhysicalDeviceFeatures2 probe{};
    initializeVkStruct(probe, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    probe.pNext = &presentIdFeat;
    vkGetPhysicalDeviceFeatures2(_vkphysicaldevice, &probe);
    if (presentIdFeat.presentId) {
      appendExtFeature(&presentIdFeat);
      logchan_vkctx->log("ext-gpu: chaining VkPhysicalDevicePresentIdFeaturesKHR{presentId=TRUE}");
    } else {
      printf("[VKDEV] WARNING: VK_KHR_present_id ENABLED but presentId feature UNSUPPORTED on <%s>.\n",
             _vkdeviceinfo->_devprops.deviceName);
      fflush(stdout);
    }
  }
#endif
#ifdef VK_KHR_present_wait
  VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeat{};
  initializeVkStruct(presentWaitFeat, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR);
  if (extEnabled(VK_KHR_PRESENT_WAIT_EXTENSION_NAME)) {
    VkPhysicalDeviceFeatures2 probe{};
    initializeVkStruct(probe, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    probe.pNext = &presentWaitFeat;
    vkGetPhysicalDeviceFeatures2(_vkphysicaldevice, &probe);
    if (presentWaitFeat.presentWait) {
      appendExtFeature(&presentWaitFeat);
      logchan_vkctx->log("ext-gpu: chaining VkPhysicalDevicePresentWaitFeaturesKHR{presentWait=TRUE}");
    } else {
      printf("[VKDEV] WARNING: VK_KHR_present_wait ENABLED but presentWait feature UNSUPPORTED on <%s>.\n",
             _vkdeviceinfo->_devprops.deviceName);
      fflush(stdout);
    }
  }
#endif

  // splice the ext-driven feature chain onto the tail of the base feature chain.
  extDynStateFeatures.pNext = (void*) ext_feat_head;

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

  ////////////////////////////
  // Load Device PFNs
  ////////////////////////////

  if (_GVI->_debugEnabled) {
    _fetchDeviceProcAddr(_vkCmdDebugMarkerBeginEXT, "vkCmdDebugMarkerBeginEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerEndEXT, "vkCmdDebugMarkerEndEXT");
    _fetchDeviceProcAddr(_vkCmdDebugMarkerInsertEXT, "vkCmdDebugMarkerInsertEXT");
    _fetchDeviceProcAddr(_vkCmdInsertDebugUtilsLabelEXT, "vkCmdInsertDebugUtilsLabelEXT");
  }

  _fetchDeviceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");

  _initPipelineCache(); // WS3: seed the persisted pipeline cache (this ctx owns the device)

  // Load device function pointers needed for rendering
  // These are needed for both window and offscreen contexts
  _fetchDeviceProcAddr(_vkCmdBeginRenderingKHR, "vkCmdBeginRenderingKHR");
  _fetchDeviceProcAddr(_vkCmdEndRenderingKHR, "vkCmdEndRenderingKHR");
  _fetchDeviceProcAddr(_vkCmdSetCullModeEXT, "vkCmdSetCullModeEXT");
  OrkAssertI(_vkCmdBeginRenderingKHR != nullptr, "_vkCmdBeginRenderingKHR function pointer is null!");
  OrkAssertI(_vkCmdEndRenderingKHR != nullptr, "_vkCmdEndRenderingKHR function pointer is null!");
  OrkAssertI(_vkCmdSetCullModeEXT != nullptr, "_vkCmdSetCullModeEXT function pointer is null!");

  ////////////////////////////
  // Init Queues
  ////////////////////////////

  _gfxqueue        = std::make_shared<VkThreadedQueue>();
  _gfxqueue->_qfid = gfx_qfid;

  u32 max_queue_count = _vkdeviceinfo->_queueprops[0].queueCount;
  OrkAssertIFMT(0 < max_queue_count, "Cannot create graphics queue: gfx_qid(0) >= _vkqcapacity_graphics(%u)", max_queue_count);

  logchan_vkctx->log("claiming graphics queue: fid(%u) qid(0/%u)", gfx_qfid, max_queue_count);
  vkGetDeviceQueue(_vkdevice, gfx_qfid, 0, &_gfxqueue->_vkqueue);

  char qname[64];
  snprintf(qname, sizeof(qname), "vk_queue-fid%u-qid0", gfx_qfid);
  _setObjectDebugName(_gfxqueue->_vkqueue, VK_OBJECT_TYPE_QUEUE, qname);

  ////////////////////////////
  // MT0 (JUL05_GPUMICROTASK T2): caps guard — MoltenVK timestamp emulation is
  // unvalidated, so verify BOTH the device-wide capability and the specific
  // graphics queue family's validity before trusting either, and honor the
  // escape hatch. Unsupported (or forced off) => _mtSliceTimer stays null;
  // GpuFrameTiming falls back to present-idle (see _doEndFrame).
  ////////////////////////////
  bool mt_no_timestamps_env = false;
  if (const char* v = std::getenv("ORKID_MT_NO_TIMESTAMPS"))
    mt_no_timestamps_env = (std::string(v) == "1");
  bool mt_caps_ok = vk_devinfo->_devprops.limits.timestampComputeAndGraphics
      && vk_devinfo->_queueprops[gfx_qfid].timestampValidBits > 0;
  _gpuTimestampsSupported = mt_caps_ok && !mt_no_timestamps_env;
  logchan_vkctx->log(
      "MT0 gpu timestamps: %s (timestampComputeAndGraphics=%d validBits=%u envDisable=%d)",
      _gpuTimestampsSupported ? "SUPPORTED" : "unsupported",
      int(vk_devinfo->_devprops.limits.timestampComputeAndGraphics),
      vk_devinfo->_queueprops[gfx_qfid].timestampValidBits,
      int(mt_no_timestamps_env));
  if (_gpuTimestampsSupported) {
    _mtSliceTimer = std::make_shared<VkGpuSliceTimer>(_vkdevice, _vkdeviceinfo->_devprops.limits.timestampPeriod);
  }
  _mtFrameWallTimer.Start();
}

VkResult VkThreadedQueue::queueSubmit(const VkSubmitInfo* pSubmits, VkFence fence) {
  std::lock_guard<std::recursive_mutex> lock(_submit_mutex);
  return vkQueueSubmit(_vkqueue, 1, pSubmits, fence);
}

VkResult VkThreadedQueue::queuePresent(const VkPresentInfoKHR* pPresentInfo) {
  std::lock_guard<std::recursive_mutex> lock(_submit_mutex);
  return vkQueuePresentKHR(_vkqueue, pPresentInfo);
}

VkResult VkThreadedQueue::queueWaitIdle() {
  std::lock_guard<std::recursive_mutex> lock(_submit_mutex);
  return vkQueueWaitIdle(_vkqueue);
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
    _vkPipelineCache = context0->_vkPipelineCache; // borrowed (owner saves/destroys)
    _vkdeviceinfo = context0->_vkdeviceinfo;
    _vkphysicaldevice = context0->_vkphysicaldevice;
    _gfxqueue = context0->_gfxqueue;
    _vkqfid_transfer = context0->_vkqfid_transfer;
    _vkqfid_compute = context0->_vkqfid_compute;
    _vkSetDebugUtilsObjectName = context0->_vkSetDebugUtilsObjectName;
    _vkCmdDebugMarkerBeginEXT = context0->_vkCmdDebugMarkerBeginEXT;
    _vkCmdDebugMarkerEndEXT = context0->_vkCmdDebugMarkerEndEXT;
    _vkCmdDebugMarkerInsertEXT = context0->_vkCmdDebugMarkerInsertEXT;
    _vkCmdBeginRenderingKHR = context0->_vkCmdBeginRenderingKHR;
    _vkCmdEndRenderingKHR = context0->_vkCmdEndRenderingKHR;
    _vkCmdSetCullModeEXT = context0->_vkCmdSetCullModeEXT;

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
    _vkPipelineCache = context0->_vkPipelineCache; // borrowed (owner saves/destroys)
    _vkdeviceinfo = context0->_vkdeviceinfo;
    _vkphysicaldevice = context0->_vkphysicaldevice;
    _gfxqueue = context0->_gfxqueue;
    _vkqfid_transfer = context0->_vkqfid_transfer;
    _vkqfid_compute = context0->_vkqfid_compute;
    _vkSetDebugUtilsObjectName = context0->_vkSetDebugUtilsObjectName;
    _vkCmdDebugMarkerBeginEXT = context0->_vkCmdDebugMarkerBeginEXT;
    _vkCmdDebugMarkerEndEXT = context0->_vkCmdDebugMarkerEndEXT;
    _vkCmdDebugMarkerInsertEXT = context0->_vkCmdDebugMarkerInsertEXT;
    _vkCmdBeginRenderingKHR = context0->_vkCmdBeginRenderingKHR;
    _vkCmdEndRenderingKHR = context0->_vkCmdEndRenderingKHR;
    _vkCmdSetCullModeEXT = context0->_vkCmdSetCullModeEXT;
    _device_extensions = context0->_device_extensions;
    _num_queue_types = context0->_num_queue_types;
    _DQCIs = context0->_DQCIs;
    _initVulkanCommon();
  }
  else{
    if (nullptr == _GVI->_preferred) {
      _GVI->_preferred = _pickPreferredDevice(_GVI->_device_infos);
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
  CPCI_GFX.queueFamilyIndex = _gfxqueue->_qfid;
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
    imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
  // The real teardown moved into _doShutdown() so it runs while owning
  // shared_ptrs are still live (called from Context::shutdown() in the
  // lev2/ezapp teardown paths). If shutdown() wasn't called (e.g.
  // static-destruction path with no explicit teardown), run it here as
  // a fallback so we don't leak the surface.
  shutdown();
}


///////////////////////////////////////////////////////////////////////////////
// WS3: persisted VkPipelineCache. Disk format = raw vkGetPipelineCacheData blob;
// the leading VkPipelineCacheHeaderVersionOne is validated against the physical
// device before use (stale driver/device -> start empty). One file per
// pipelineCacheUUID under <staging>/vkpipelinecache/.
///////////////////////////////////////////////////////////////////////////////

static std::string _pipelineCachePath(VkPhysicalDevice physdev) {
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(physdev, &props);
  std::string uuid_hex;
  for (int i = 0; i < VK_UUID_SIZE; i++)
    uuid_hex += FormatString("%02x", int(props.pipelineCacheUUID[i]));
  auto dir = file::Path::stage_dir() / "vkpipelinecache";
  std::error_code ec;
  std::filesystem::create_directories(dir.c_str(), ec);
  return (dir / FormatString("%s.bin", uuid_hex.c_str())).toStdString();
}

void VkContext::_initPipelineCache() {
  std::vector<uint8_t> initial;
  auto path = _pipelineCachePath(_vkphysicaldevice);
  if (FILE* fin = fopen(path.c_str(), "rb")) {
    fseek(fin, 0, SEEK_END);
    long len = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (len > 32) { // must at least hold the v1 header
      initial.resize(size_t(len));
      if (fread(initial.data(), 1, size_t(len), fin) != size_t(len))
        initial.clear();
    }
    fclose(fin);
  }
  if (not initial.empty()) { // validate header vs THIS device (spec: app's job)
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(_vkphysicaldevice, &props);
    uint32_t hdr_ver = 0, hdr_vendor = 0, hdr_device = 0;
    memcpy(&hdr_ver, initial.data() + 4, 4);
    memcpy(&hdr_vendor, initial.data() + 8, 4);
    memcpy(&hdr_device, initial.data() + 12, 4);
    bool valid = (hdr_ver == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)  //
                 and (hdr_vendor == props.vendorID)                 //
                 and (hdr_device == props.deviceID)                 //
                 and (0 == memcmp(initial.data() + 16, props.pipelineCacheUUID, VK_UUID_SIZE));
    if (not valid) {
      logchan_vkctx->log("pipelinecache<%s>: stale header (driver/device changed) — starting empty", path.c_str());
      initial.clear();
    }
  }
  VkPipelineCacheCreateInfo PCCI;
  initializeVkStruct(PCCI, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
  PCCI.initialDataSize = initial.size();
  PCCI.pInitialData    = initial.empty() ? nullptr : initial.data();
  VkResult OK          = vkCreatePipelineCache(_vkdevice, &PCCI, nullptr, &_vkPipelineCache);
  if (OK != VK_SUCCESS) { // never fatal — pipelines just build uncached
    logchan_vkctx->log("pipelinecache: vkCreatePipelineCache failed<%d> — continuing uncached", int(OK));
    _vkPipelineCache = VK_NULL_HANDLE;
    return;
  }
  _ownsPipelineCache = true;
  logchan_vkctx->log("pipelinecache<%s>: seeded with %zu bytes", path.c_str(), initial.size());
}

void VkContext::_savePipelineCache() {
  if (not _ownsPipelineCache or _vkPipelineCache == VK_NULL_HANDLE or _vkdevice == nullptr)
    return;
  size_t len = 0;
  if (vkGetPipelineCacheData(_vkdevice, _vkPipelineCache, &len, nullptr) == VK_SUCCESS and len > 0) {
    std::vector<uint8_t> data(len);
    if (vkGetPipelineCacheData(_vkdevice, _vkPipelineCache, &len, data.data()) == VK_SUCCESS) {
      auto path = _pipelineCachePath(_vkphysicaldevice);
      if (FILE* fout = fopen(path.c_str(), "wb")) {
        fwrite(data.data(), 1, len, fout);
        fclose(fout);
        logchan_vkctx->log("pipelinecache<%s>: saved %zu bytes", path.c_str(), len);
      }
    }
  }
  vkDestroyPipelineCache(_vkdevice, _vkPipelineCache, nullptr);
  _vkPipelineCache   = VK_NULL_HANDLE;
  _ownsPipelineCache = false;
}

void VkContext::_doShutdown() {
  _savePipelineCache(); // WS3: persist + destroy (owner only; device still valid here)
  if (_vkpresentationsurface != VK_NULL_HANDLE && _GVI) {
    printf("VkContext::_doShutdown: destroying VkSurface %p\n", (void*)_vkpresentationsurface);
    vkDestroySurfaceKHR(_GVI->_instance, _vkpresentationsurface, nullptr);
    _vkpresentationsurface = VK_NULL_HANDLE;
  } else {
    printf("VkContext::_doShutdown: no surface to destroy (surface=%p GVI=%p)\n",
           (void*)_vkpresentationsurface, (void*)_GVI.get());
  }
  _vkdevice = nullptr;
  _isShutdown = true; // gates destroyVertexBuffer/destroyIndexBuffer into a no-op past here
}

////////////////////////////////////////////////////////////////////////////////
// Vertex/index buffer teardown (moved out of ~VulkanVertexBuffer / ~VulkanIndexBuffer).
// Before shutdown: queue the VkBuffer for deferred cleanup on the primary command buffer if one
// is active, else on the context's pending-cleanup list. After _doShutdown(): no-op — the device
// and command buffers are already gone, so there is nothing to queue (and touching them aborts).
////////////////////////////////////////////////////////////////////////////////

void VkContext::destroyVertexBuffer(vkbuffer_ptr_t vkbuffer) {
  if (_isShutdown)
    return;
  auto cb = primary_cb();
  if (cb) {
    cb->_vkbuffers_pending_cleanup.push_back(vkbuffer);
  } else {
    // No active primary CB - queue on context for later cleanup
    std::lock_guard<std::mutex> lock(_vkbuffers_pending_cleanup_mutex);
    _vkbuffers_pending_cleanup.push_back(vkbuffer);
  }
}

void VkContext::destroyIndexBuffer(vkbuffer_ptr_t vkbuffer) {
  if (_isShutdown)
    return;
  auto cb = primary_cb();
  if (cb) {
    cb->_vkbuffers_pending_cleanup.push_back(vkbuffer);
  } else {
    std::lock_guard<std::mutex> lock(_vkbuffers_pending_cleanup_mutex);
    _vkbuffers_pending_cleanup.push_back(vkbuffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
// raw-handle teardown funnels (no-op after _doShutdown — device + handles already gone).
////////////////////////////////////////////////////////////////////////////////

void VkContext::destroyBuffer(VkBuffer buffer) {
  if (_isShutdown || _vkdevice == nullptr)
    return;
  if (buffer != VK_NULL_HANDLE)
    vkDestroyBuffer(_vkdevice, buffer, nullptr);
}

void VkContext::destroyImageMemory(VkDeviceMemory mem) {
  if (_isShutdown || _vkdevice == nullptr)
    return;
  if (mem != VK_NULL_HANDLE)
    vkFreeMemory(_vkdevice, mem, nullptr);
}

void VkContext::destroyImageObject(VkImageView view, VkImage image, VkDeviceMemory mem) {
  if (_isShutdown || _vkdevice == nullptr)
    return;
  if (view != VK_NULL_HANDLE)
    vkDestroyImageView(_vkdevice, view, nullptr);
  if (image != VK_NULL_HANDLE)
    vkDestroyImage(_vkdevice, image, nullptr);
  if (mem != VK_NULL_HANDLE)
    vkFreeMemory(_vkdevice, mem, nullptr);
}

void VkContext::destroyComputePipelineState(
    VkPipeline pipeline, VkPipelineLayout layout,
    VkDescriptorSetLayout dsl, const std::vector<VkDescriptorPool>& pools) {
  if (_isShutdown || _vkdevice == nullptr)
    return;
  if (pipeline != VK_NULL_HANDLE)
    vkDestroyPipeline(_vkdevice, pipeline, nullptr);
  if (layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(_vkdevice, layout, nullptr);
  if (dsl != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(_vkdevice, dsl, nullptr);
  // destroying each pool frees the descriptor sets allocated from it (in _setRing)
  for (auto pool : pools)
    vkDestroyDescriptorPool(_vkdevice, pool, nullptr);
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
  // If a primary CB is still recording (init-time code that cycles whole
  // frames or double-begins — e.g. hypermesh materialize inside ezapp's
  // gpu-init begin/end pair), FLUSH it synchronously instead of abandoning
  // it: init work already recorded there (blank texture-array clears and
  // layout transitions, font uploads) must actually execute.
  ////////////////////////
  if (_pricb_recording and _defaultCommandBuffer) {
    auto CB       = primary_cb();
    CB->_recorded = true;
    vkEndCommandBuffer(CB->_vkcmdbuf);
    _pricb_recording = false;

    VkSubmitInfo SI;
    initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
    SI.commandBufferCount = 1;
    SI.pCommandBuffers    = &CB->_vkcmdbuf;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(_vkdevice, &fenceInfo, nullptr, &fence);
    _gfxqueue->queueSubmit(&SI, fence);
    vkWaitForFences(_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(_vkdevice, fence, nullptr);

    _pri_cmdbuf_pool.deallocate(_defaultCommandBuffer);
    _defaultCommandBuffer     = nullptr;
    _defaultCommandBufferImpl = nullptr;
    _cmdbufcurpri_gfx         = nullptr;
  }
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
  _pricb_recording = true;

}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doEndPrimaryCommandBuffer() {
  // init-time code may have cycled whole frames (begin/endFrame) inside an
  // outer begin/end pair — endFrame already ended and submitted the pri CB,
  // so ending again here would hit a non-RECORDING command buffer.
  if (not _pricb_recording) {
    return;
  }
  auto CB = primary_cb();
  if(0)printf( "END priCB<%p> impl<%p> vkhandle<%p>\n", (void*)_defaultCommandBuffer.get(), (void*)CB.get(), (void*)CB->_vkcmdbuf );
  CB->_recorded = true;
  vkEndCommandBuffer(CB->_vkcmdbuf);
  _pricb_recording = false;
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doSubmitPrimaryCommandBuffer(){
  OrkProfilerSampleScope(CHANNEL_MAIN, "vk:doSubmitPrimaryCommandBuffer");

  // Signal ONLY the semaphores whose one-shot CBs were recorded into THIS frame's
  // primary CB (coupled in _doPreBeginFrame). Sweeping the whole _pendingOneShotSemas
  // set here signaled a frame EARLY for any CB enqueued mid-frame (loading-phase ops
  // enqueue AFTER _doPreBeginFrame's drain, so their commands execute NEXT frame):
  // isSignalled() went true before the GPU copy ran -> premature staging-buffer
  // recycle + premature radiance-map publish (the silent linux load death).
  // _pendingOneShotSemas remains the poll/_onComplete registry (_doBeginFrame).
  for (auto& semaphore : _thisFrameOneShotSemas) {
    _oneShotSignalSemaphores.push_back(semaphore->_vksema);
    _oneShotSignalValues.push_back(1);
  }
  _thisFrameOneShotSemas.clear();

  // Associate captures with the current frame fence before submitting to the GPU
  auto frame_fence = _fbi->_output->currentFrameFence();
  for (auto& capture : _pending_captures) {
    if (auto async_impl = capture->_impl.getShared<VkCaptureAsyncImpl>())
      async_impl->_fence = frame_fence;
  }

  // VkFramebufferOutput deals with output specific submission and waiting.
  _fbi->_output->submit(this);

  // Consumed by submit — clear for next frame.
  _oneShotSignalSemaphores.clear();
  _oneShotSignalValues.clear();

  _processPendingCaptures();
}

///////////////////////////////////////////////////////////////////////////////
// MT2 (JUL05_GPUMICROTASK §2.6 / T7 option "a"): run `record` (an rtgroup
// render + captureAsFormat) as a self-contained GPU job on a DEDICATED primary
// command buffer — submit it, WAIT on its own fence, then read back the capture
// it recorded. The outer frame's primary CB (already begun in _doPreBeginFrame
// with the MT0 timer + profiler BEGIN written into it) is only POINTER-swapped
// aside and restored; it is never ended here, so its timer pair stays intact.
//
// Why a separate submit and not "record into the frame CB": the microtask
// scheduler measures a slice's cost as CPU wall-clock and decrements the frame
// budget by it (§2.3). Recording-without-wait makes that cost ~0 → the budget
// never depletes → every roughness level lands in ONE frame = one giant GPU
// frame (the very hitch the scheduler exists to prevent). The fence wait below
// makes the slice's wall-clock reflect the level's real GPU cost, so the loop
// throttles to ~budget/level-cost slices per frame. The wait counts against the
// budget (T8); the readback runs on a fully-drained single-submit CB with no
// other compute in flight (MoltenVK reboot rule, §1.6 rule 1).
///////////////////////////////////////////////////////////////////////////////

void VkContext::_doExecuteInlineGpuJob(const void_lambda_t& record) {
  OrkAssert(_pricb_recording); // must be mid-frame (the beginFrame drain point)

  // Save the outer frame's CB pointers (untouched below).
  auto saved_default = _defaultCommandBuffer;
  auto saved_impl    = _defaultCommandBufferImpl;
  auto saved_cur     = _cmdbufcurpri_gfx;

  // Allocate + begin a dedicated job CB from the same 16-deep pool. Only the
  // frame CB is otherwise in flight, so pool pressure is +1 (freed below).
  auto job_cb   = _pri_cmdbuf_pool.allocate();
  auto job_impl = job_cb->_impl.getShared<VkPrimaryCommandBufferImpl>();
  _defaultCommandBuffer     = job_cb;
  _defaultCommandBufferImpl = job_impl;
  _cmdbufcurpri_gfx         = job_impl;

  // Return any pooled secondary CBs this recycled primary still held (same
  // reclaim the normal begin path does) before vkBeginCommandBuffer resets it.
  for (auto& cb : job_impl->_secondary_cmdbuffers_pending_cleanup) {
    auto impl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();
    if (impl->_onCleanupCallback)
      impl->_onCleanupCallback();
  }
  job_impl->_secondary_cmdbuffers_pending_cleanup.clear();

  VkCommandBufferBeginInfo bi = {};
  initializeVkStruct(bi, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(job_impl->_vkcmdbuf, &bi);

  // rtgroup render + captureAsFormat record into job_impl (== primary_cb()).
  record();

  // captureAsFormat's trailing resumeRenderPass re-opens a dynamic-rendering
  // pass on the job CB. End it and clear the context render-pass flags BEFORE
  // ending+submitting the job CB — otherwise the restored primary CB inherits a
  // stale _renderPassActive and the next PushRtGroup emits vkCmdEndRenderingKHR
  // on a CB with no live pass (driver crash, seen on the loader context's next
  // beginFrame). Mirror of _pushRtGroup's STEP-1 teardown.
  if (_renderPassActive) {
    _vkCmdEndRenderingKHR(job_impl->_vkcmdbuf);
    _renderPassActive    = false;
    _activeRenderPassRTG = nullptr;
  }

  vkEndCommandBuffer(job_impl->_vkcmdbuf);

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.commandBufferCount = 1;
  SI.pCommandBuffers    = &job_impl->_vkcmdbuf;

  VkFenceCreateInfo fenceInfo = {};
  initializeVkStruct(fenceInfo, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(_vkdevice, &fenceInfo, nullptr, &fence);
  _gfxqueue->queueSubmit(&SI, fence);
  vkWaitForFences(_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(_vkdevice, fence, nullptr);

  // GPU has finished (fence waited) → the capture's staging copy is host-visible;
  // read it back now. At the drain point the outer frame has recorded nothing
  // yet, so _pending_captures holds only this job's capture.
  _processPendingCaptures();

  // Restore the outer frame's still-recording primary CB.
  _pri_cmdbuf_pool.deallocate(job_cb);
  _defaultCommandBuffer     = saved_default;
  _defaultCommandBufferImpl = saved_impl;
  _cmdbufcurpri_gfx         = saved_cur;
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
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,  // SRC=upload, DST=readback staging
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

size_t VkContext::deviceLocalHeapBytes() const {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(_vkphysicaldevice, &props);
  size_t best = 0;
  for (uint32_t h = 0; h < props.memoryHeapCount; h++)
    if (props.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
      best = std::max(best, size_t(props.memoryHeaps[h].size));
  return best;
}

void VkContext::ensureSyncReadbackStagingSize(size_t needed) {
  if (needed > _syncTransfer.readback_size) {
    logchan_vkctx->log("Growing sync READBACK staging buffer: %zu -> %zu bytes",
                       _syncTransfer.readback_size, needed);
    // HOST_CACHED is the whole point: the CPU READS this memory (copyToHost's
    // memcpy), and reads from the default write-combined staging run ~150MB/s.
    _syncTransfer.readback_buffer = std::make_shared<VulkanBuffer>(
      this,
      needed,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      "syncTransferReadback",
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
          VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    _syncTransfer.readback_size = needed;
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

  _gfxqueue->queueSubmit(&submitInfo, fence);

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

  // MT0 (JUL05_GPUMICROTASK §2.4): always-on GPU frame timer — independent of
  // ORK_PROFILER_ENABLE (T3). No-op when timestamps are unsupported (_mtSliceTimer==nullptr).
  if (_mtSliceTimer) {
    _mtSliceTimer->setCmdBuf(primary_cb()->_vkcmdbuf);
    _mtSliceTimer->beginFrame();
  }

  /////////////////////////////////////////
  _pendingOneShotCommands.atomicOp([&](vkseccmdbufarray_t& unlocked) {
    //size_t num_one_shot = unlocked.size();
    //printf("VkContext<%p> executing %zu one-shot secondary command buffers\n", (void*)this, num_one_shot);
    for (auto one_shot : unlocked) {
      enqueueSecondaryCommandBuffer(one_shot);
      // couple this CB's completion semaphore to THIS frame's submit — it must
      // signal only when the frame CONTAINING the commands completes.
      auto impl = one_shot->_impl.getShared<VkSecondaryCommandBufferImpl>();
      if (impl->_completionSemaphore)
        _thisFrameOneShotSemas.push_back(impl->_completionSemaphore);
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
  // TODO is this really necessary?
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

  // If gpu-init code cycled whole frames (e.g. hypermesh materialize), the
  // last endFrame already ended+submitted the pri CB and returned it to the
  // pool (_defaultCommandBuffer == nullptr) — nothing left to submit here.
  if (nullptr == _defaultCommandBuffer or nullptr == _cmdbufcurpri_gfx) {
    _defaultCommandBuffer     = nullptr;
    _defaultCommandBufferImpl = nullptr;
    _cmdbufcurpri_gfx         = nullptr;
    return;
  }

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
  _gfxqueue->queueSubmit(&SI, fence);
  vkWaitForFences(_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(_vkdevice, fence, nullptr);

  // Clear the command buffer pointers
  // The pool will reuse this command buffer on the next beginFrame
  _defaultCommandBuffer = nullptr;
  _defaultCommandBufferImpl = nullptr;
  _cmdbufcurpri_gfx = nullptr;

  //printf("VkContext::_onGpuPostInit: gpuPreInit transitions complete\n");

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

  // MT0: close the whole-frame GPU span (still inside the not-yet-submitted CB).
  if (_mtSliceTimer)
    _mtSliceTimer->endFrame();

  // End and submit primary command buffer. Currently this also waits. (T4 note:
  // MT0's timestamp readback below does NOT rely on that — it is a lag-2,
  // never-waiting availability read; RADV/DRM proved the submit does not fully
  // resolve the frame there and a waiting read wedges the GPU. Only the
  // present-idle peek assumes this frame's swapchain wait already happened.)
  _doEndPrimaryCommandBuffer();
  _doSubmitPrimaryCommandBuffer();

  // read back GPU timestamps now that the GPU has finished executing
  OrkProfilerFrameEnd(CHANNEL_GPU);

  // MT0 (JUL05_GPUMICROTASK §2.2) — SHADOW MODE: measured + logged only, enforced
  // by nothing yet (MT1 adds the scheduler). gpu_ms=-1 when unsupported (T2 caps
  // guard); present-idle is peeked fresh (T13: 0 where no swapchain exists, e.g.
  // offscreen — degrades to src=idle with idle_ms=0 rather than block/guess).
  {
    float gpu_ms  = _mtSliceTimer ? _mtSliceTimer->readbackFrameMs() : -1.0f;
    float idle_ms = float(RenderPhaseStats::instance().peekMs("present-idle"));
    float cpu_ms  = float(_mtFrameWallTimer.SecsSinceStart() * 1000.0);
    _mtFrameWallTimer.Start();
    // MT1 (JUL05_GPUMICROTASK §2.5/§2.6): feed the scheduler's THIS-frame drain
    // activity into the trace BEFORE noteFrame emits the rate-limited [gpumt]
    // line (the scheduler ran back in beginFrame, so its telemetry is current).
    const auto& sched = _microtaskScheduler.telemetry();
    _mtFrameTiming.noteSchedulerTelemetry(sched._slicesLastFrame, sched._spentUsLastFrame, sched._pendingWorkDepth);
    _mtFrameTiming.noteFrame(cpu_ms, idle_ms, gpu_ms);
    // MT1 (§2.2): push THIS frame's measured budget to the scheduler — it uses
    // the LAST frame's budget for the NEXT frame's drain (frame-lagged EMAs make
    // that correct). UNBOUNDED contexts (offscreen/loader) ignore it.
    _microtaskScheduler.setFrameBudgetUs(_mtFrameTiming.lastBudgetUs());
  }

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
  // rendering must happen on the GPU that owns the opened DRM card — the
  // scanout dma-buf cannot be imported across GPUs
  auto vk_devinfo = _pickDeviceForDrmFd(_GVI->_device_infos, ctxdrm->_drmctx->drm_fd);
  if (nullptr == vk_devinfo) {
    vk_devinfo = _GVI->_preferred ? _GVI->_preferred : (_GVI->_device_infos.empty() ? nullptr : _GVI->_device_infos[0]);
  } else if (_GVI->_preferred && (_GVI->_preferred != vk_devinfo)) {
    logchan_vkctx->log(
        "WARNING: DRM card GPU <%s> differs from share-group GPU <%s> — resources are not shareable across devices",
        vk_devinfo->_devprops.deviceName,
        _GVI->_preferred->_devprops.deviceName);
  }
  OrkAssert(vk_devinfo != nullptr);
  logchan_vkctx->log("DRM mode: using device <%s>", vk_devinfo->_devprops.deviceName);

  // Share the existing (loader) context's VkDevice when it lives on the same
  // physical GPU — the loader thread uploads textures on its device, and those
  // handles are only valid in contexts sharing that VkDevice. Creating a
  // separate device here leaves every loader-uploaded texture invalid in the
  // DRM context (skybox/radiance maps silently render as placeholder content).
  vkcontext_rawptr_t share_ctx = nullptr;
  if (_GVI->_contexts.size() >= 1) {
    auto context0 = *_GVI->_contexts.begin();
    if (context0->_vkphysicaldevice == vk_devinfo->_phydev) {
      share_ctx = context0;
    } else {
      logchan_vkctx->log(
          "WARNING: DRM mode: existing context device <%s> != DRM card device <%s> — creating separate VkDevice; "
          "loader-uploaded resources will NOT be shareable",
          context0->_vkdeviceinfo ? context0->_vkdeviceinfo->_devprops.deviceName : "?",
          vk_devinfo->_devprops.deviceName);
    }
  }
  if (share_ctx) {
    logchan_vkctx->log("DRM mode: sharing existing VkDevice with context<%p>", (void*)share_ctx);
    _vkdevice                  = share_ctx->_vkdevice;
    _vkdeviceinfo              = share_ctx->_vkdeviceinfo;
    _vkphysicaldevice          = share_ctx->_vkphysicaldevice;
    _gfxqueue                  = share_ctx->_gfxqueue;
    _vkqfid_transfer           = share_ctx->_vkqfid_transfer;
    _vkqfid_compute            = share_ctx->_vkqfid_compute;
    _vkSetDebugUtilsObjectName = share_ctx->_vkSetDebugUtilsObjectName;
    _vkCmdDebugMarkerBeginEXT  = share_ctx->_vkCmdDebugMarkerBeginEXT;
    _vkCmdDebugMarkerEndEXT    = share_ctx->_vkCmdDebugMarkerEndEXT;
    _vkCmdDebugMarkerInsertEXT = share_ctx->_vkCmdDebugMarkerInsertEXT;
    _vkCmdBeginRenderingKHR    = share_ctx->_vkCmdBeginRenderingKHR;
    _vkCmdEndRenderingKHR      = share_ctx->_vkCmdEndRenderingKHR;
    _vkCmdSetCullModeEXT       = share_ctx->_vkCmdSetCullModeEXT;
    _device_extensions         = share_ctx->_device_extensions;
    _num_queue_types           = share_ctx->_num_queue_types;
    _DQCIs                     = share_ctx->_DQCIs;
    _initVulkanCommon();
  } else {
    _initVulkanForDevInfo(vk_devinfo);
    _initVulkanCommon();
  }
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
  // IMMEDIATE is not available on Wayland (compositor controls vsync); FIFO is always guaranteed
  OrkAssert(_vkpresentation_caps->supportsPresentationMode(VK_PRESENT_MODE_FIFO_KHR));
  ///////////////////////
#if defined(__APPLE__)
  bool use_metal_sc = _ginitdata && _ginitdata->_fullscreen && _ginitdata->_displaylink;
  if (use_metal_sc) {
    logchan_vkctx->log("Apple fullscreen — using VkSwapchainMetal (CVDisplayLink Metal-direct path)");
    _fbi->_output = std::make_shared<VkSwapchainMetal>(this);
  } else {
    logchan_vkctx->log("Apple windowed — using VkSwapChain (standard Vulkan path)");
    _fbi->_output = std::make_shared<VkSwapChain>(this);
  }
#else
  _fbi->_output = std::make_shared<VkSwapChain>(this);
#endif
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

void VkContext::initializeDisplayClientContext(vkdisplayclient_ptr_t client) {
  meTargetType = TargetType::OFFSCREEN;

  miW = client->_shared->frame_width;
  miH = client->_shared->frame_height;

  // Share the device/queue from the first existing context (same as offscreen path).
  // pBuf is unused inside _initVulkanForOffscreen when _GVI->_contexts is non-empty.
  auto plato = std::make_shared<VkPlatformObject>();
  plato->_ctxbase   = global_plato()->_ctxbase;
  plato->_needsInit = false;
  plato->_bindop    = [](){};
  mCtxBase = 0;
  _impl.setShared<VkPlatformObject>(plato);

  _initVulkanForOffscreen(nullptr);

  _fbi->_output = std::make_shared<VkDisplayClientOutput>(this, miW, miH, client);
  logchan_vkctx->log("Display Client Context initialized");
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
    // ORKID_GPU_PREFER takes precedence over findPresentableDevice so a
    // user can target the iGPU on hybrid systems where the dGPU is
    // enumerated first.
    if (std::getenv("ORKID_GPU_PREFER")) {
      _GVI->_preferred = _pickPreferredDevice(_GVI->_device_infos);
    }
#if defined(__linux__)
    else if (use_drm) {
      // must render on the GPU that owns the selected DRM card, otherwise
      // scanout dma-buf import fails (multi-GPU)
      _GVI->_preferred = _pickDeviceForDrmMode(_GVI->_device_infos, _ginitdata->_drm_mode);
    }
#endif
    else if (!use_drm && !glfw_null_plat) {
      auto vk_devinfo = _GVI->findPresentableDevice();
      if (vk_devinfo) {
        _GVI->_preferred = vk_devinfo;
        logchan_vkctx->log("Loader context: Selected presentation-capable device for share group");
      }
    }

    if (nullptr == _GVI->_preferred) {
      _GVI->_preferred = _pickPreferredDevice(_GVI->_device_infos);
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
  vkGetPhysicalDeviceSurfaceSupportKHR(_vkphysicaldevice, _gfxqueue->_qfid, surface, &presentSupport);
  if (!presentSupport) {
    logchan_vkctx->log("ERROR: Graphics queue family %u does not support presentation to this surface!", _gfxqueue->_qfid);
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
    if (auto sc = std::dynamic_pointer_cast<VkSwapChain>(_fbi->_output)) {
      sc->_pendingReinit = true;
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
  hasher.accumulateItem(mode._minMipLevel);
  hasher.accumulateItem(mode._maxMipLevel);
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
  
  // LOD settings — honor the sampling mode's mip range so callers can
  // clamp to a single mip level (set min == max) for inspection / debug.
  sci->mipLodBias = 0.0f;
  sci->minLod     = float(sampling_mode._minMipLevel);
  sci->maxLod     = (sampling_mode._maxMipLevel >= 16)
                  ? VK_LOD_CLAMP_NONE
                  : float(sampling_mode._maxMipLevel);
  
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

////////////////////////////////////////////////////////////////////////////////
// MT0 GPU Slice Timer Implementation (JUL05_GPUMICROTASK §2.4)
//   Always-on — no ORK_PROFILER_ENABLE dependency (T3). Only instantiated when
//   the T2 caps guard (VkContext::_initVulkanForDevInfo) found real timestamp
//   support, so every method here can assume a valid, working query pool.
////////////////////////////////////////////////////////////////////////////////

VkGpuSliceTimer::VkGpuSliceTimer(VkDevice device, float timestamp_period_ns)
    : _device(device) {
  _tickToMs = double(timestamp_period_ns) * 1e-6; // ns/tick -> ms/tick
  VkQueryPoolCreateInfo info = {
    .sType     = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = kNumQueries,
  };
  OrkVkAssert(vkCreateQueryPool(_device, &info, ORK_VK_ALLOC, &_query_pool));
}

VkGpuSliceTimer::~VkGpuSliceTimer() {
  if (_query_pool != VK_NULL_HANDLE)
    vkDestroyQueryPool(_device, _query_pool, ORK_VK_ALLOC);
}

void VkGpuSliceTimer::beginFrame() {
  OrkAssertI(_cmdbuf != VK_NULL_HANDLE, "VkGpuSliceTimer::beginFrame: setCmdBuf() must be called first");
  // THE COUNTER ADVANCES HERE AND ONLY HERE. The first linux gate run proved
  // why: advancing it in readbackFrameMs() desyncs the ring permanently on any
  // path where begin/end and readback aren't strictly 1:1 (windowed startup:
  // acquire failures / resizes / non-visual frames) — reads then poll slots
  // that are never written and availability stays 0 forever. Keyed solely off
  // begin, readback is always lag-2 from the NEWEST write, cadence-proof.
  _frameCounter++;
  // Reset ONLY this frame's ring slot (pair): it was last written kRingDepth
  // frames ago and read (or abandoned) at lag-2 — never still in flight.
  uint32_t slot = uint32_t(_frameCounter % kRingDepth);
  vkCmdResetQueryPool(_cmdbuf, _query_pool, slot * 2, 2);
  vkCmdWriteTimestamp(_cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _query_pool, slot * 2);
}

void VkGpuSliceTimer::endFrame() {
  if (_cmdbuf == VK_NULL_HANDLE) [[unlikely]]
    return;
  uint32_t slot = uint32_t(_frameCounter % kRingDepth);
  vkCmdWriteTimestamp(_cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _query_pool, slot * 2 + 1);
}

float VkGpuSliceTimer::readbackFrameMs() {
  // Lag-2, NEVER-WAITING read (ring rationale at the declaration: WAIT_BIT
  // here wedged RADV/amdgpu in kernel dma_fence_wait). WITH_AVAILABILITY at
  // stride 16 lays out [value, avail] per query. Does NOT advance the counter
  // (begin owns it) — safe to call at any cadence relative to beginFrame.
  if (_frameCounter < 3)
    return -1.0f;
  uint32_t slot   = uint32_t((_frameCounter - 2) % kRingDepth);
  uint64_t res[4] = {0, 0, 0, 0};
  VkResult ok     = vkGetQueryPoolResults(
      _device, _query_pool, slot * 2, 2,
      sizeof(res), res, 2 * sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  // ORKID_MT_TRACE=2: raw-read diagnostics (rate-limited) for on-target triage
  // of exactly the "healthy caps, zero samples" class.
  static const bool s_rawtrace = []() {
    const char* v = std::getenv("ORKID_MT_TRACE");
    return v && v[0] == '2';
  }();
  if (s_rawtrace) [[unlikely]] {
    static int s_count = 0;
    if ((s_count++ & 63) == 0)
      printf("[gpumt-raw] frame=%llu slot=%u vkres=%d ts=(%llu,%llu) avail=(%llu,%llu)\n",
             (unsigned long long)_frameCounter, slot, int(ok),
             (unsigned long long)res[0], (unsigned long long)res[2],
             (unsigned long long)res[1], (unsigned long long)res[3]);
  }
  if (ok != VK_SUCCESS || res[1] == 0 || res[3] == 0) [[unlikely]]
    return -1.0f; // not ready — the model falls back to present-idle this frame
  return float(double(res[2] - res[0]) * _tickToMs);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
//////////////////////////////////////////////////////////////////////////////
