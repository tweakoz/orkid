////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/kernel/environment.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/external_gpu_requirements.h>
#include <ork/lev2/vr/vr.h>

#if defined(ENABLE_VULKAN)
#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include "headers/vulkan_ctx.h"

// Static initializer to configure MoltenVK before library initialization
#if defined(__APPLE__)
namespace {
struct MoltenVKConfigurator {
  MoltenVKConfigurator() {
    // Disable argument buffers - they require type metadata that SPIRV-Cross
    // cannot always determine for storage buffers in graphics pipelines
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "0", 0);
  }
} g_moltenvk_configurator;
}
#endif

namespace ork::lev2::vulkan {
static logchannel_ptr_t logchan_vkimpl = logger()->configureChannel("VKIMPL", fvec3(1,1,0),false);
static logchannel_ptr_t logchan_vkierr = logger()->configureChannel("VKINSTERR", fvec3(1,0,0),true);

vkinstance_ptr_t _GVI = nullptr;
static bool _enable_validate = false;
static bool _enable_renderdoc = false;
static bool _enable_debug = (_enable_validate or _enable_renderdoc);

// X1 external-GPU-requirements seam self-test (env-gated, off by default). When
//  ORKID_EXTGPU_SELFTEST=1 the backend registers a synthetic requirement set that
//  exercises the extension-merge and required-physical-device paths, emitting
//  ORKID_EXTGPU_SELFTEST: verdict lines to stdout for a headless test to grep.
static bool _extGpuSelfTestEnabled() {
  const char* v = std::getenv("ORKID_EXTGPU_SELFTEST");
  return v and (std::string(v) == "1");
}
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
  logchan_vkimpl->log("has_layer<%s> : %s", layerName.c_str(), has_layer ? "true": "false");
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
  // Honor _preferred (set by ORKID_GPU_PREFER during loader init) if it
  // can present to this surface. On hybrid systems the dGPU often
  // enumerates first, so iterating _device_infos in order would silently
  // override the user's iGPU preference.
  auto can_present = [&](vkdeviceinfo_ptr_t devinfo) -> bool {
    for (uint32_t qf_index = 0; qf_index < devinfo->_queueprops.size(); qf_index++) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(devinfo->_phydev, qf_index, surface, &presentSupport);
      if (presentSupport) return true;
    }
    return false;
  };
  if (_preferred && can_present(_preferred)) {
    return _preferred;
  }
  for( auto devinfo : _device_infos ){
    // Check all queue families, not just queue family 0
    // Many GPUs (especially on Linux) don't support presentation on queue family 0
    if (can_present(devinfo)) {
      return devinfo;
    }
  }
  return nullptr;
}

vkdeviceinfo_ptr_t VulkanInstance::findPresentableDevice() {
  for (auto devinfo : _device_infos) {
    for (uint32_t qf_index = 0; qf_index < devinfo->_queueprops.size(); qf_index++) {
      if (glfwGetPhysicalDevicePresentationSupport(_instance, devinfo->_phydev, qf_index))
        return devinfo;
    }
  }
  return nullptr;
}

// ORKID_VULKAN_VALIDATE=2 : CONTINUE mode - report validation errors and keep
//  running, so one pass surfaces every defect instead of dying at the first.
//  Any other truthy value keeps the default trap-on-first-error behavior.
static bool _validationContinueMode() {
  static const bool mode = []() -> bool {
    const char* v = std::getenv("ORKID_VULKAN_VALIDATE");
    return v and (std::string(v) == "2");
  }();
  return mode;
}

// ORKID_VULKAN_TRAP_SKIP : comma-separated VUID substrings that VALIDATE=1 must
//  NOT die on — they take the continue-mode path instead. Without this, one
//  known-and-triaged defect that fires early permanently masks every later one,
//  so the trap can only ever convict whatever happens to be first. Unset/empty
//  leaves trap-on-first-error exactly as it was.
static bool _trapSkipped(const char* message) {
  static const std::vector<std::string> needles = []() {
    std::vector<std::string> rval;
    const char* v = std::getenv("ORKID_VULKAN_TRAP_SKIP");
    for (size_t pos = 0; v and pos <= strlen(v);) {
      const char* comma = strchr(v + pos, ',');
      size_t end        = comma ? size_t(comma - v) : strlen(v);
      if (end > pos)
        rval.push_back(std::string(v + pos, end - pos));
      pos = end + 1;
    }
    return rval;
  }();
  if (needles.empty() or not message)
    return false;
  std::string msg(message);
  for (const auto& needle : needles)
    if (msg.find(needle) != std::string::npos)
      return true;
  return false;
}

static std::atomic<int> _validationErrorCount(0);

// process-global, not per-context: the debug messenger belongs to the instance.
int vkValidationErrorCount() {
  return _validationErrorCount.load();
}
bool vkValidationArmed() {
  return _GVI ? _GVI->_debugEnabled : false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      if (not _validationContinueMode() and not _trapSkipped(pCallbackData->pMessage)) {
        logchan_vkierr->log("VULKAN ERROR: %s", pCallbackData->pMessage);
        fflush(stdout);
        // die through the engine assert path rather than __builtin_trap: a bare
        //  SIGILL leaves no record of WHICH engine call the layer was validating,
        //  and the trap fires on the offending thread inside the layer callback,
        //  so the C++ backtrace still spans the vk* call and its engine caller.
        // assembled by concatenation, not FormatString: validation messages run
        //  well past FormatString's 512-byte buffer and the spec quote at the tail
        //  is the part that says what to DO about the VUID.
        std::string reason = "Assert At: [File " __FILE__ "] [Reason: VULKAN VALIDATION ERROR <";
        reason += pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "<unnamed>";
        reason += ">] [Info: ";
        reason += pCallbackData->pMessage ? pCallbackData->pMessage : "";
        reason += "]";
        OrkAssertFunction(reason.c_str());
      }
      // one defect on a per-frame path floods the console and buries the rest
      //  of the run, so print each distinct VUID only a few times.
      int total         = _validationErrorCount.fetch_add(1) + 1;
      std::string msgid = pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "<unnamed>";
      int seen          = 0;
      {
        static std::mutex id_mutex;
        static std::map<std::string, int> id_counts;
        std::lock_guard<std::mutex> lock(id_mutex);
        seen = ++id_counts[msgid];
      }
      if (seen <= 3) {
        logchan_vkierr->log("VULKAN ERROR (continue, #%d): %s", total, pCallbackData->pMessage);
      }
      else if (seen == 4) {
        logchan_vkierr->log("VULKAN ERROR: suppressing further <%s> reports", msgid.c_str());
      }
      fflush(stdout);
      return VK_FALSE;
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      logchan_vkierr->log("VULKAN WARNING: %s", pCallbackData->pMessage);
    }
    else {
      //logchan_vkierr->log("VULKAN INFO: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::VulkanInstance() {
  logchan_vkimpl->log("Constructing Vulkan Instance");

#if defined(__APPLE__)
  // Metal argument buffers are REQUIRED on macOS: without AB, Metal caps
  // samplers at 16 per fragment stage, and generated forward fragments
  // (terrain FWD_SSBO_CUSTOM + impostor atlas + IBL + light cookies + sun
  // cascade shadows) exceed that: MSL compile error ("'sampler' attribute
  // parameter is out of bounds"), surfacing as pipeline create
  // VK_ERROR_INITIALIZATION_FAILED. The former default-off here (SPIRV-Cross
  // SSBO type-metadata concern) no longer reproduces with the staging
  // MoltenVK. MoltenVK snapshots its config BEFORE any engine code runs
  // (observed: even this constructor is too late), so the AUTHORITATIVE
  // setting is shell env — exported by obt.project/scripts/init_env.py.
  // This setenv only covers spawned children; warn loudly if the shell
  // didn't provide it, instead of limping toward a cryptic -3 later.
  if (nullptr == getenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS")) {
    printf(
        "[VKIMPL] WARNING: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS missing from shell env — "
        "MoltenVK already snapshotted config with argument buffers OFF; fragments using >16 "
        "samplers will fail pipeline creation (VkResult -3). Re-enter the OBT shell "
        "(obt.project/scripts/init_env.py exports it).\n");
  }
  setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1", 0); // 0 = don't overwrite if set
  logchan_vkimpl->log("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS: %s", getenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS"));
#endif

  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  logchan_vkimpl->log("Working dir: %s", cwd);
  logchan_vkimpl->log("VK_ICD_FILENAMES: %s", getenv("VK_ICD_FILENAMES"));
  logchan_vkimpl->log("VK_LAYER_PATH: %s", getenv("VK_LAYER_PATH"));
  logchan_vkimpl->log("DYLD_LIBRARY_PATH: %s", getenv("DYLD_LIBRARY_PATH"));
  logchan_vkimpl->log("MVK_CONFIG_LOG_LEVEL: %s", getenv("MVK_CONFIG_LOG_LEVEL"));

  // Check if we're using DRM mode (direct rendering without GLFW)
  bool use_drm = (lev2::_ginitdata && lev2::_ginitdata->_use_drm);

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = nullptr;

  if (!use_drm) {
    // GLFW mode: initialize GLFW and get required extensions
    static auto gctx = CtxGLFW::globalOffscreenContext();
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  }

  std::vector<const char*> validation_layers;

  std::string ORKID_VULKAN_VALIDATE;
  if (genviron.get("ORKID_VULKAN_VALIDATE", ORKID_VULKAN_VALIDATE) && !ORKID_VULKAN_VALIDATE.empty()) {
    if (ORKID_VULKAN_VALIDATE == "1") {
      logchan_vkimpl->log("VulkanInstance::VulkanInstance() ENABLE VALIDATION");
      _enable_validate = true;
      _enable_debug = true;
    }
    if (ORKID_VULKAN_VALIDATE == "2") {
      logchan_vkimpl->log("VulkanInstance::VulkanInstance() ENABLE VALIDATION (CONTINUE MODE)");
      _enable_validate = true;
      _enable_debug = true;
    }
    if (ORKID_VULKAN_VALIDATE == "0") {
      logchan_vkimpl->log("VulkanInstance::VulkanInstance() DISABLE VALIDATION");
      _enable_validate = false;
      _enable_debug = false;
    }
  }


  if( _enable_validate ){
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
  }
  if( _enable_renderdoc ){
    validation_layers.push_back("VK_LAYER_RENDERDOC_Capture");
  }
  auto layer_props = _layerProperties();
  for(size_t i=0; i<layer_props.size(); i++){
   logchan_vkimpl->log("layer<%zu:%s>", i, layer_props[i].layerName);
  }

  // Check if validation layer is available when debug is enabled
  if(_enable_debug && _enable_validate){
    _debugEnabled = _hasLayer(layer_props, "VK_LAYER_KHRONOS_validation");
    if(_debugEnabled){
      logchan_vkimpl->log("VK_LAYER_KHRONOS_validation found and enabled");
    } else {
      logchan_vkimpl->log("WARNING: VK_LAYER_KHRONOS_validation requested but not available");
    }
  } else {
    _debugEnabled = false;
  }

  initializeVkStruct(_appdata,VK_STRUCTURE_TYPE_APPLICATION_INFO);
  _appdata.pApplicationName   = "Orkid";
  _appdata.applicationVersion = 1;
  _appdata.pEngineName        = "Orkid";
  _appdata.engineVersion      = 1;
  // apiVersion: ask for the highest the LOADER actually supports, capped at the
  //  highest this engine is written against. Capability-driven, NOT hardcoded — the
  //  ceiling differs per platform+driver (MoltenVK is 1.4 here; a given Linux driver
  //  may be 1.3), and requesting more than the loader has fails vkCreateInstance.
  //
  //  Why it matters beyond feature access: vkGetDeviceProcAddr withholds EVERY core
  //  entrypoint newer than the requested version, including promoted aliases of
  //  extensions that ARE enabled. An in-process guest that proc-loads by core name —
  //  an OpenXR runtime loaded into this process — then traps on a NULL pfn that looks
  //  inexplicable, because the extension is right there in the enabled list. A guest
  //  runtime that loads vkCmdPushDescriptorSet (VK_KHR_push_descriptor promoted in 1.4)
  //  under a 1.3 instance gets NULL while vkCmdPushDescriptorSetKHR resolves fine.
  //  vkEnumerateInstanceVersion is 1.1+; its absence means a 1.0 loader.
  uint32_t loader_api_version = VK_API_VERSION_1_0;
  if (auto pfn_enum_version = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion")) {
    if (VK_SUCCESS != pfn_enum_version(&loader_api_version))
      loader_api_version = VK_API_VERSION_1_0;
  }
  // Composed numerically, NOT the named VK_API_VERSION_1_4 macro: that macro only
  //  exists in 1.4-era headers, and seats build against whatever vulkan_core.h their
  //  staging carries (a Linux seat with 1.3 headers must still COMPILE — the loader
  //  min() below keeps it from ever REQUESTING beyond what the runtime supports).
  constexpr uint32_t ORKID_MAX_VK_API_VERSION = VK_MAKE_API_VERSION(0, 1, 4, 0);
  _appdata.apiVersion = (loader_api_version < ORKID_MAX_VK_API_VERSION) //
                            ? loader_api_version
                            : ORKID_MAX_VK_API_VERSION;
  logchan_vkimpl->log(
      "apiVersion: loader supports <%u.%u.%u>, requesting <%u.%u.%u>",
      VK_API_VERSION_MAJOR(loader_api_version),
      VK_API_VERSION_MINOR(loader_api_version),
      VK_API_VERSION_PATCH(loader_api_version),
      VK_API_VERSION_MAJOR(_appdata.apiVersion),
      VK_API_VERSION_MINOR(_appdata.apiVersion),
      VK_API_VERSION_PATCH(_appdata.apiVersion));

  // X1: honor an externally-required Vulkan API-version window (e.g. XR's
  //  xrGetVulkanGraphicsRequirements min/max). Neutral when both bounds are 0.
  if (auto reqs = externalGpuRequirements()) {
    if (reqs->_minApiVersion and _appdata.apiVersion < reqs->_minApiVersion) {
      logchan_vkimpl->log("ext-gpu: raising apiVersion to required min <%u>", reqs->_minApiVersion);
      _appdata.apiVersion = reqs->_minApiVersion;
    }
    if (reqs->_maxApiVersion and _appdata.apiVersion > reqs->_maxApiVersion) {
      logchan_vkimpl->log("ext-gpu: lowering apiVersion to required max <%u>", reqs->_maxApiVersion);
      _appdata.apiVersion = reqs->_maxApiVersion;
    }
  }

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

  if (!use_drm) {
    // GLFW mode: add GLFW required extensions
    for( size_t i=0; i<glfwExtensionCount; i++ ){
      _instance_extensions.push_back(glfwExtensions[i]);
    }

    _instance_extensions.push_back("VK_KHR_surface");
    _instance_extensions.push_back("VK_EXT_swapchain_colorspace");

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
  }
  // DRM mode: no surface extensions needed (direct display)

  ////////////////////////////////////////
  // X1: external GPU requirements — instance extensions
  ////////////////////////////////////////

  // Self-test hook (env-gated, off by default): synthesize a requirement set that
  //  exercises the merge — one available extension NOT in the base list (proves
  //  APPEND) plus one already in the base list (proves DEDUP). Registered into the
  //  backend-internal slot so the merge below consumes it exactly like a real
  //  producer's would.
  std::string _extgpu_selftest_append; // the ext we asked to append (for verification)
  if (_extGpuSelfTestEnabled()) {
    uint32_t st_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &st_count, nullptr);
    std::vector<VkExtensionProperties> st_avail(st_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &st_count, st_avail.data());
    auto* st_reqs = _externalGpuRequirementsMutable();
    for (auto& e : st_avail) {
      bool in_base = false;
      for (auto b : _instance_extensions)
        if (0 == strcmp(b, e.extensionName)) { in_base = true; break; }
      if (not in_base) {
        _extgpu_selftest_append = e.extensionName;
        st_reqs->_instanceExtensions.push_back(_extgpu_selftest_append);
        break;
      }
    }
    if (not _instance_extensions.empty())
      st_reqs->_instanceExtensions.push_back(_instance_extensions.front()); // dedup probe
    printf("ORKID_EXTGPU_SELFTEST: request append instance ext <%s>\n", _extgpu_selftest_append.c_str());
    printf("ORKID_EXTGPU_SELFTEST: request dedup instance ext <%s>\n",
           _instance_extensions.empty() ? "" : _instance_extensions.front());
    fflush(stdout);
  }

  // Merge externally-required instance extensions into the base list, deduping by
  //  name so the MoltenVK portability / GLFW surface set is never disturbed.
  if (auto reqs = externalGpuRequirements()) {
    for (const auto& ext : reqs->_instanceExtensions) {
      bool already = false;
      for (auto e : _instance_extensions)
        if (0 == strcmp(e, ext.c_str())) { already = true; break; }
      if (already) {
        logchan_vkimpl->log("ext-gpu: instance ext <%s> already present (dedup)", ext.c_str());
      } else {
        _instance_extensions.push_back(ext.c_str());
        logchan_vkimpl->log("ext-gpu: merging external instance ext <%s>", ext.c_str());
      }
    }
  }

  // Enumerate available instance extensions and validate they are vailable
  uint32_t available_extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, nullptr);
  std::vector<VkExtensionProperties> availailable_extensions(available_extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, availailable_extensions.data());

  for (auto req_it = _instance_extensions.begin(); req_it != _instance_extensions.end(); ) {
    bool found = false;

    for(auto& avail_it : availailable_extensions){
      if(strcmp(avail_it.extensionName, *req_it) == 0){
        found = true;
        break;
      } 
    }

    if (found) {
      logchan_vkimpl->log("Requested Extension Available: %s", *req_it);
      ++req_it;
    } else {
      logchan_vkimpl->log("Requested Extension Unavailable: %s", *req_it);
       req_it = _instance_extensions.erase(req_it);
    }
  }

  _instancedata.enabledExtensionCount   = _instance_extensions.size();
  _instancedata.ppEnabledExtensionNames = _instance_extensions.data();

  if (_extGpuSelfTestEnabled()) {
    int append_hits = 0, dedup_hits = 0;
    for (auto e : _instance_extensions) {
      if (not _extgpu_selftest_append.empty() and _extgpu_selftest_append == e) append_hits++;
      if (not _instance_extensions.empty() and 0 == strcmp(e, _instance_extensions.front())) dedup_hits++;
    }
    printf("ORKID_EXTGPU_SELFTEST: instance ext append <%s> present=%d\n",
           _extgpu_selftest_append.c_str(), append_hits >= 1 ? 1 : 0);
    printf("ORKID_EXTGPU_SELFTEST: instance ext dedup <%s> count=%d\n",
           _instance_extensions.empty() ? "" : _instance_extensions.front(), dedup_hits);
    fflush(stdout);
  }

  OrkVkAssert(vkCreateInstance(&_instancedata, nullptr, &_instance));

  // X1: the instance exists — external requirements are consumed; forbid any
  //  further external registration (backend-internal late resolution still allowed).
  _lockExternalGpuRequirements();

  // Fetch immediately via instance so we can start putting names on vk objects right away.
  _fetchInstanceProcAddr(_vkSetDebugUtilsObjectName, "vkSetDebugUtilsObjectNameEXT");

  if(_debugEnabled) {
    _fetchInstanceProcAddr(_vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT");

    //deco::printf(yel, "vulkan::_init instance<%p> res<%d>\n", (void*) & _instance, int(res));
    VkDebugUtilsMessengerEXT debugMessenger;
      
      VkDebugUtilsMessengerCreateInfoEXT dbg_createInfo = {};
      dbg_createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      dbg_createInfo.messageSeverity = 
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      dbg_createInfo.messageType = 
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      dbg_createInfo.pfnUserCallback = debugCallback;
      dbg_createInfo.pUserData = nullptr; // Optional user data
      
      if (_vkCreateDebugUtilsMessengerEXT(_instance, &dbg_createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
          OrkAssert(false);
      }
  }

  /////////////////////////////////////////////////////////////////////////////
  // check device groups (for later multidevice support)
  /////////////////////////////////////////////////////////////////////////////

  OrkVkAssert(vkEnumeratePhysicalDeviceGroups(_instance, &_numgroups, nullptr));
  _phygroups.resize(_numgroups);
  // Initialize sType for each group properties structure
  for (auto& group : _phygroups) {
    group.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    group.pNext = nullptr;
  }
  vkEnumeratePhysicalDeviceGroups(_instance, &_numgroups, _phygroups.data());
  logchan_vkimpl->log("vulkan::_init numgroups<%u>", _numgroups);
  int igroup = 0;
  for (auto& group : _phygroups) {
    vkdevgrp_ptr_t dev_group_out = std::make_shared<VulkanDeviceGroup>();
    _devgroups.push_back(dev_group_out);

    dev_group_out->_deviceCount = group.physicalDeviceCount;
    logchan_vkimpl->log("vulkan::_init grp<%d> numgpus<%zu>", igroup, dev_group_out->_deviceCount);
    for (int idev = 0; idev < dev_group_out->_deviceCount; idev++) {
      auto device_info = std::make_shared<VulkanDeviceInfo>();
      dev_group_out->_device_infos.push_back(device_info);
      _device_infos.push_back(device_info);
      device_info->_phydev = group.physicalDevices[idev];
      vkGetPhysicalDeviceProperties(device_info->_phydev, &device_info->_devprops);
      device_info->_is_discrete = (device_info->_devprops.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
      logchan_vkimpl->log(
          "    group<%d> gpu<%d:%s> is_discrete<%d>",
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
      
      VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
      indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
      indexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
      indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
      indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
      indexingFeatures.runtimeDescriptorArray = VK_TRUE;
      vk13Features.pNext = &indexingFeatures;
      
      vkGetPhysicalDeviceFeatures2(device_info->_phydev, &device_info->_devfeatures2);
      device_info->_devfeatures = device_info->_devfeatures2.features;
      
      //device_info->_supportsDynamicRendering = dynRenderFeatures.dynamicRendering;
      device_info->_supportsVulkan13 = true;
      

    }
    igroup++;
  }

  /////////////////////////////////////////////////////////////////////////////

  OrkVkAssert(vkEnumeratePhysicalDevices(_instance, &_numgpus, nullptr));

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

    logchan_vkimpl->log("vulkan::_init gpu<%d:%s> is_discrete<%d>", dev_props.deviceID, dev_props.deviceName, int(is_discrete));
    logchan_vkimpl->log("         apiVersion<%u>", dev_props.apiVersion);
    logchan_vkimpl->log("         maxImageDimension1D<%u>", dev_props.limits.maxImageDimension1D);
    logchan_vkimpl->log("         maxImageDimension2D<%u>", dev_props.limits.maxImageDimension2D);
    logchan_vkimpl->log("         maxImageDimension3D<%u>", dev_props.limits.maxImageDimension3D);
    logchan_vkimpl->log("         maxImageDimensionCube<%u>", dev_props.limits.maxImageDimensionCube);
    logchan_vkimpl->log("         maxImageArrayLayers<%u>", dev_props.limits.maxImageArrayLayers);

    logchan_vkimpl->log("         maxBoundDescriptorSets<%u>", dev_props.limits.maxBoundDescriptorSets);
    logchan_vkimpl->log("         maxPerStageDescriptorSamplers<%u>", dev_props.limits.maxPerStageDescriptorSamplers);
    logchan_vkimpl->log("         maxPerStageDescriptorUniformBuffers<%u>", dev_props.limits.maxPerStageDescriptorUniformBuffers);
    logchan_vkimpl->log("         maxPerStageDescriptorStorageBuffers<%u>", dev_props.limits.maxPerStageDescriptorStorageBuffers);
    logchan_vkimpl->log("         maxPerStageDescriptorSampledImages<%u>", dev_props.limits.maxPerStageDescriptorSampledImages);
    logchan_vkimpl->log("         maxPerStageDescriptorStorageImages<%u>", dev_props.limits.maxPerStageDescriptorStorageImages);
    logchan_vkimpl->log("         maxPerStageDescriptorInputAttachments<%u>", dev_props.limits.maxPerStageDescriptorInputAttachments);

    logchan_vkimpl->log("         maxUniformBufferRange<%u>", dev_props.limits.maxUniformBufferRange);
    logchan_vkimpl->log("         maxFramebufferWidth<%u>", dev_props.limits.maxFramebufferWidth);
    logchan_vkimpl->log("         maxFramebufferLayers<%u>", dev_props.limits.maxFramebufferLayers);
    logchan_vkimpl->log("         maxColorAttachments<%u>", dev_props.limits.maxColorAttachments);
    logchan_vkimpl->log("         maxComputeSharedMemorySize<%u>", dev_props.limits.maxComputeSharedMemorySize);
    logchan_vkimpl->log("         maxComputeWorkGroupSize<%u>", dev_props.limits.maxComputeWorkGroupSize);
    logchan_vkimpl->log("         maxPushConstantsSize<%u>", dev_props.limits.maxPushConstantsSize);

    logchan_vkimpl->log(
        "         maxcomputewkgcount<%u,%u,%u>",
        device_info->_maxWkgCountX,
        device_info->_maxWkgCountY,
        device_info->_maxWkgCountZ);
    logchan_vkimpl->log("         feat.fragmentStoresAndAtomics<%u>", int(dev_feats.fragmentStoresAndAtomics));
    logchan_vkimpl->log("         feat.shaderFloat64<%u>", int(dev_feats.shaderFloat64));
    logchan_vkimpl->log("         feat.sparseBinding<%u>", int(dev_feats.sparseBinding));
    logchan_vkimpl->log("         feat.multiDrawIndirect<%u>", int(dev_feats.multiDrawIndirect));

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

  // X1: physical devices now exist (their handles are only knowable after
  //  instance creation). Under self-test, require the device the default picker
  //  would choose (first discrete, else first) so the required-device path is
  //  exercised without changing which GPU renders. Populates the backend-internal
  //  slot directly (the locked external setter would reject a post-instance write).
  if (_extGpuSelfTestEnabled() and not _device_infos.empty()) {
    vkdeviceinfo_ptr_t def = nullptr;
    for (auto d : _device_infos)
      if (d->_is_discrete) { def = d; break; }
    if (not def)
      def = _device_infos.front();
    _externalGpuRequirementsMutable()->_requiredPhysicalDevice = uint64_t(def->_phydev);
    printf("ORKID_EXTGPU_SELFTEST: require physical device <%s>\n", def->_devprops.deviceName);
    fflush(stdout);
  }

  for (int i = 0; i < 1; i++) {
    auto loadctx = std::make_shared<VkLoadContext>();
    // loadctx->_global_plato  = GlOsxPlatformObject::_global_plato;
    load_token_t token;
    token.setShared<VkLoadContext>(loadctx);
    _loadTokens.push(token);
  }

  if (_debugEnabled)
    _setupDebugMessenger();

  // Note: globalOffscreenContext() is now called conditionally at the start of constructor
  // based on use_drm flag (line 141)

}

///////////////////////////////////////////////////////////////////////////////////////////////

VulkanInstance::~VulkanInstance() {
  //vkDestroyInstance(_instance, nullptr);
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

  ////////////////////////////////////////
  // X1: pre-graphics-init seam. An active external graphics client (e.g. a VR/XR
  //  driver) publishes its Vulkan requirements BEFORE the instance is created. The
  //  default (NoVR) path yields an empty set, so nothing is registered — neutral.
  ////////////////////////////////////////

  auto vrdev = orkidvr::device();
  if (vrdev and vrdev->_active) {
    ExternalGpuRequirements reqs;
    vrdev->preGraphicsInit(reqs);
    bool any = (not reqs._instanceExtensions.empty())   //
               or (not reqs._deviceExtensions.empty())  //
               or reqs._requiredPhysicalDevice          //
               or reqs._minApiVersion                   //
               or reqs._maxApiVersion;
    if (any)
      setExternalGpuRequirements(reqs);
  }

  _GVI       = std::make_shared<VulkanInstance>();

  ////////////////////////////////////////
  // X2: post-instance / pre-device seam. Now that the VkInstance exists, let the
  //  producer resolve the physical device it REQUIRES (XR: xrGetVulkanGraphicsDeviceKHR
  //  needs the instance). It writes the required physical device into the backend-
  //  internal requirements slot, which the device-creation chokepoint honors below.
  ////////////////////////////////////////

  if (vrdev and vrdev->_active and externalGpuRequirements()) {
    vrdev->resolvePhysicalDevice(uint64_t(_GVI->_instance));
  }

  auto clazz = dynamic_cast<object::ObjectClass*>(VkContext::GetClassStatic());
  GfxEnv::setContextClass(clazz);
  auto target = std::make_shared<VkContext>();
  target->initializeLoaderContext();
  GfxEnv::initializeWithContext(target);

  ////////////////////////////////////////
  // X1: post-graphics-init seam. Hand the MAIN context's device+queue to the
  //  producer that registered requirements (XR binds this exact device+queue).
  //  Only fires when a producer registered — the NoVR path is untouched.
  ////////////////////////////////////////

  if (vrdev and vrdev->_active and externalGpuRequirements()) {
    GraphicsBindingInfo binding;
    binding._vkInstance       = uint64_t(_GVI->_instance);
    binding._vkPhysicalDevice = uint64_t(target->_vkphysicaldevice);
    binding._vkDevice         = uint64_t(target->_vkdevice);
    binding._queueFamilyIndex = target->_gfxqueue ? target->_gfxqueue->_qfid : 0;
    binding._queueIndex       = 0;
    vrdev->postGraphicsInit(binding);
  }

  return target;
}

///////////////////////////////////////////////////////////////////////////////

VkFormatConverter::VkFormatConverter() {

  auto do_format = [this](EBufferFormat ork_fmt, VkFormat vk_fmt) {
    _fmtmap[ork_fmt] = vk_fmt;
    _inv_fmtmap[vk_fmt] = ork_fmt;
  };

  // S3TC compression formats are widely supported on desktop GPUs
  // MoltenVK supports BC formats via software decompression
  do_format(EBufferFormat::S3TC_DXT1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
  do_format(EBufferFormat::S3TC_DXT3, VK_FORMAT_BC2_UNORM_BLOCK);
  do_format(EBufferFormat::S3TC_DXT5, VK_FORMAT_BC3_UNORM_BLOCK);

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
  // single-channel 16-bit: normalized-on-read so sampler2D returns raw/65535 in [0,1]
  // (heightfields baked as png16/R16UI). NOT R16_UINT, which would need usampler2D and
  // disallow bilinear filtering.
  do_format(EBufferFormat::R16UI, VK_FORMAT_R16_UNORM);
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
    fprintf(stderr, "format<%s> conversion not present", fmtname.c_str());
    fflush(stderr);
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

#endif // defined(ENABLE_VULKAN)
