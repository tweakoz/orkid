////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// OpenXrDevice (OPENXR X2): native, IP-clean OpenXR client. Vulkan graphics
// binding via XR_KHR_vulkan_enable v1. Loader-discovered runtime — never named.
//
// Verification note: mac has NO XR runtime, so the live session FSM, swapchain,
// frame loop and __composite are UNVERIFIED-BY-DESIGN here (a real runtime — the
// nameable Monado CI, or the owner's private box — is the gate). What IS verified
// on mac: the loader-fallback (preGraphicsInit fails cleanly → one log line →
// engine proceeds as NoVR-equivalent) and the pure pose/FOV math (runSelfTests).
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>

#if defined(ENABLE_OPENXR)

#include <ork/lev2/vr/openxr.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/vr_composite.h>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <limits>

#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr::openxr_ {
////////////////////////////////////////////////////////////////////////////////

// Diagnostic channel (disabled by default — success-path chatter stays quiet). The
// ONE loader-fallback failure line is a printf so it is always observable.
static logchannel_ptr_t logchan_xr = logger()->configureChannel("OPENXR", fvec3(0.2, 0.9, 0.9), false);

////////////////////////////////////////////////////////////////////////////////
// Pose math (unit-tested). XR is right-handed, +Y up, -Z forward. compose(p,q,1)
// builds a matrix that actively rotates a point BY the pose quaternion q (the
// tracked object's local->reference (world) transform); a view matrix is its
// inverse.
////////////////////////////////////////////////////////////////////////////////

fmtx4 xrPoseToFmtx4(const XrPosef& pose) {
  fquat q(pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w);
  fvec3 p(pose.position.x, pose.position.y, pose.position.z);
  fmtx4 m;
  m.compose(p, q, 1.0f);
  return m;
}

////////////////////////////////////////////////////////////////////////////////
// Asymmetric-tangent projection. XrFovf carries the four half-angles (radians,
// signed); the engine's frustum-from-signed-tangents helper (VrProjFrustumPar)
// consumes TANGENTS. Do NOT hand-roll the projection — fill the tangents and let
// VrProjFrustumPar::composeProjection build the matrix (same path as OpenVR's
// GetProjectionRaw).
////////////////////////////////////////////////////////////////////////////////

VrProjFrustumPar fovToVrFrustum(const XrFovf& fov, float near_, float far_) {
  VrProjFrustumPar f;
  f._left   = std::tan(fov.angleLeft);
  f._right  = std::tan(fov.angleRight);
  // Convention contract: composeProjection expects OpenVR-raw polarity (top NEGATIVE,
  //  bottom POSITIVE) — its Y-scale is 2/(_bottom-_top), calibrated so (_bottom-_top)>0.
  //  XrFovf reports angleUp>0 / angleDown<0, so feed angleDown->_top (negative) and
  //  angleUp->_bottom (positive). This keeps the Y-scale POSITIVE, matching native
  //  fmtx4::perspective / NoVR; the prior straight mapping produced a NEGATIVE Y-scale
  //  (Y-flipped projection -> inverted winding -> inside-out backface culling in XR).
  f._top    = std::tan(fov.angleDown);
  f._bottom = std::tan(fov.angleUp);
  f._near   = near_;
  f._far    = far_;
  return f;
}

////////////////////////////////////////////////////////////////////////////////
// Degenerate-FOV self-defense. A runtime that reports an all-zero (or non-finite)
// per-view FOV before the first frame submission feeds tan(0)=0 tangents into the
// projection: composeProjection then divides by a zero-width/zero-height span and
// every eye matrix comes out NaN (the observed vp_l/vp_r/mvp_l/mvp_r poisoning).
// Detect that (any non-finite half-angle, or a horizontal/vertical span below
// epsilon) and substitute a symmetric +/-45deg fallback so a finite projection
// always reaches the posemap. Returns true when the fallback fired (so the caller
// can warn once with the raw values).
////////////////////////////////////////////////////////////////////////////////

bool sanitizeVrFov(XrFovf& fov) {
  const float kSpanEps = 1e-4f;
  bool finite = std::isfinite(fov.angleLeft) and std::isfinite(fov.angleRight) and
                std::isfinite(fov.angleUp) and std::isfinite(fov.angleDown);
  bool degenerate = (not finite) or                                //
                    (std::fabs(fov.angleRight - fov.angleLeft) < kSpanEps) or
                    (std::fabs(fov.angleUp - fov.angleDown) < kSpanEps);
  if (degenerate) {
    const float k45 = float(M_PI) * 0.25f;
    fov.angleLeft   = -k45;
    fov.angleRight  = k45;
    fov.angleUp     = k45;
    fov.angleDown   = -k45;
  }
  return degenerate;
}

////////////////////////////////////////////////////////////////////////////////
// Degenerate-pose self-defense. An all-zero (or non-finite) orientation quaternion
// has no valid rotation; xrPoseToFmtx4 would build a singular local->world matrix
// whose inverse (the view) is NaN. Substitute an identity orientation so the view
// matrix stays finite. Returns true when the fallback fired.
////////////////////////////////////////////////////////////////////////////////

bool sanitizeVrPose(XrPosef& pose) {
  const XrQuaternionf& q = pose.orientation;
  bool finite = std::isfinite(q.x) and std::isfinite(q.y) and //
                std::isfinite(q.z) and std::isfinite(q.w);
  float len2          = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  const float kQuatEps = 1e-8f;
  bool degenerate     = (not finite) or (len2 < kQuatEps);
  if (degenerate)
    pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};
  return degenerate;
}

////////////////////////////////////////////////////////////////////////////////
// Depth convention conversion (unit-tested). orkid renders STANDARD-Z window depth:
// the forward PBR pipeline tests LEQUALS, the depth buffer clears to 1.0, and the eye
// projection (VrProjFrustumPar::composeProjection) maps the near plane to ndc.z=0 and
// the far plane to ndc.z=1. The runtime's depth-layer contract wants REVERSE-Z in
// [0,1]: 1.0 at the near plane, 0.0 at the far plane, taken AS-IS. So window depth
// converts as revZ = 1 - stdZ.
//
// COVERAGE: the runtime treats a packed depth of exactly 0 as EMPTY (those pixels
// composite THROUGH to home content). orkid's cleared background sits at stdZ=1 → revZ
// 0 → transparent, which we do NOT want. Clamp the output to a FLOOR so every rendered
// pixel — sky/background/cleared included — stays opaque with correct relative depth.
//
// FLOOR VALUE: originally one D16 quantum (1/65535). RAISED to 1/255 (~0.4% of far) because
// the runtime's TESSELATION-mode vertex-stage coverage/anchor math clips background/far tiles
// that survive the fragment threshold but sit too near the far sentinel (observed: distant
// terrain composited BLACK). 0.4% parallax error on the sky is negligible; the larger floor
// clears any plausible vertex-stage quantization. Override via ORKID_XR_DEPTH_FLOOR for tuning.
// (This math is mirrored by the depth-copy fragment pass — keep the two in lockstep; the GPU
// pass takes the floor as a PUSH CONSTANT so it is parametric, never baked.)
static float kRevZFloor = [] {
  if (const char* e = getenv("ORKID_XR_DEPTH_FLOOR"); e and e[0]) {
    float v = float(atof(e));
    if (v > 0.0f and v < 1.0f)
      return v;
  }
  return 1.0f / 255.0f;
}();

static inline float stdZtoRevZ(float stdz) {
  float r = 1.0f - stdz;
  if (r < kRevZFloor)
    r = kRevZFloor;
  if (r > 1.0f)
    r = 1.0f;
  return r;
}

// Depth-layer near/far self-defense. The runtime consumes nearZ/farZ as the positive
// view-space plane distances; for our reverse-Z values a depth of 1.0 sits at nearZ and
// 0.0 at farZ, so we submit nearZ=device _near, farZ=device _far (the real, FINITE far —
// never +inf). Reject a degenerate pair (non-finite, non-positive, or equal planes) so
// a bad configuration drops to color-only rather than feeding the runtime garbage.
static bool validDepthNearFar(float nearZ, float farZ) {
  return std::isfinite(nearZ) and std::isfinite(farZ) and //
         nearZ > 0.0f and farZ > 0.0f and std::fabs(farZ - nearZ) > 1e-6f;
}

////////////////////////////////////////////////////////////////////////////////

static uint32_t xrVersionToVk(XrVersion v) {
  return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(v), XR_VERSION_MINOR(v), 0);
}

// split a space-separated extension list (the form the KHR ext-string entry points
// return) into individual names. Delimiters are SPACE and NUL: the two-call idiom's
// buffer is sized from the runtime-reported capacity which INCLUDES the terminating
// NUL (and some runtimes NUL-pad), so treating '\0' as a delimiter makes the parse
// robust to trailing/embedded NULs without a separate trim step. Empty runs never
// produce empty tokens (the j>i guard).
static std::vector<std::string> splitExts(const std::string& s) {
  auto isdelim = [](char c) { return c == ' ' or c == '\0'; };
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() and isdelim(s[i]))
      i++;
    size_t j = i;
    while (j < s.size() and not isdelim(s[j]))
      j++;
    if (j > i)
      out.push_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// Frame-pacing / pose-anomaly instrumentation — fixed-size ring stats, no heap in
// the frame path. Percentiles/medians copy the ring to a stack buffer and nth_element
// (only every frame for the small angvel ring; the wait ring only every 5s summary).
////////////////////////////////////////////////////////////////////////////////

static constexpr int kPacePeriodRing = 120;  // displayTime-delta ring → median = period
static constexpr int kPaceWaitRing   = 1024; // xrWaitFrame durations for the 5s summary
static constexpr int kPaceAngvelRing = 128;  // ~1.4s of head angvel at 90Hz → rolling median

static int64_t medianI64(const int64_t* buf, int n) {
  if (n <= 0)
    return 0;
  int64_t tmp[kPacePeriodRing];
  for (int i = 0; i < n; i++)
    tmp[i] = buf[i];
  int k = n / 2;
  std::nth_element(tmp, tmp + k, tmp + n);
  return tmp[k];
}

static float medianF(const float* buf, int n) {
  if (n <= 0)
    return 0.0f;
  float tmp[kPaceAngvelRing];
  for (int i = 0; i < n; i++)
    tmp[i] = buf[i];
  int k = n / 2;
  std::nth_element(tmp, tmp + k, tmp + n);
  return tmp[k];
}

static void waitStats(const float* buf, int n, float& p50, float& p95, float& mx) {
  if (n <= 0) {
    p50 = p95 = mx = 0.0f;
    return;
  }
  float tmp[kPaceWaitRing];
  for (int i = 0; i < n; i++)
    tmp[i] = buf[i];
  mx = tmp[0];
  for (int i = 1; i < n; i++)
    if (tmp[i] > mx)
      mx = tmp[i];
  int k50 = int(0.50 * (n - 1) + 0.5);
  std::nth_element(tmp, tmp + k50, tmp + n);
  p50 = tmp[k50];
  int k95 = int(0.95 * (n - 1) + 0.5);
  std::nth_element(tmp, tmp + k95, tmp + n);
  p95 = tmp[k95];
}

// Angle (radians) between two unit quaternions (double-cover safe via |dot|).
static double quatAngleBetween(const float a[4], const float b[4]) {
  double dot = double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2] + double(a[3]) * b[3];
  dot = std::fabs(dot);
  if (dot > 1.0)
    dot = 1.0;
  return 2.0 * std::acos(dot);
}

struct PacingStats {
  bool _primed = false;
  using clock_t = std::chrono::steady_clock;

  int64_t _prevDisplayTime = 0; // ns (XrTime)
  int64_t _lastRawDeltaNs  = 0; // this frame's displayTime delta (reused by pose angvel dt)
  bool _lastContinuous     = true; // this frame's displayTime advanced exactly 1 period
  uint64_t _frameIdx       = 0;

  int64_t _deltaRing[kPacePeriodRing] = {};
  int _deltaCount = 0, _deltaHead = 0;

  float _waitRing[kPaceWaitRing] = {};
  int _waitCount = 0, _waitHead = 0;

  float _angvelRing[kPaceAngvelRing] = {};
  int _angvelCount = 0, _angvelHead = 0;
  float _prevHeadQuat[4] = {0, 0, 0, 1};
  bool _prevHeadValid    = false;

  uint32_t _winFrames     = 0;
  uint32_t _winPeriodSkips = 0;
  uint32_t _winPoseSpikes  = 0;
  clock_t::time_point _winStart{};
  clock_t::time_point _lastAnomaly{};
};

////////////////////////////////////////////////////////////////////////////////
// PIMPL — all XR/Vulkan handle state (keeps openxr.h SDK-type-free).
////////////////////////////////////////////////////////////////////////////////

enum HandSide { HAND_LEFT = 0, HAND_RIGHT = 1 };

struct OpenXrImpl {
  XrInstance _instance = XR_NULL_HANDLE;
  XrSystemId _systemId = XR_NULL_SYSTEM_ID;
  XrSession _session   = XR_NULL_HANDLE;
  XrSpace _refSpace    = XR_NULL_HANDLE; // STAGE (LOCAL fallback)
  XrSpace _viewSpace   = XR_NULL_HANDLE; // head
  XrSwapchain _swapchain = XR_NULL_HANDLE;
  std::vector<XrSwapchainImageVulkanKHR> _swapImages;
  int64_t _swapFormat = 0;

  // Optional depth layer (XR_KHR_composition_layer_depth). A SECOND double-wide
  //  swapchain of D16_UNORM the app writes reverse-Z window depth into; chaining it per
  //  projection view engages the runtime's positional (depth-based) reprojection —
  //  fixing translation-parallax ghosting during head pans. Enabled only when the
  //  runtime advertises the extension AND offers D16_UNORM AND creation succeeds; a
  //  frame that fails to publish depth simply omits the chain (color-only that frame).
  bool _depthExtEnabled         = false;         // extension enabled at xrCreateInstance
  bool _depthChainSupported     = false;         // ext + D16 swapchain both live
  XrSwapchain _depthSwapchain   = XR_NULL_HANDLE; // wide D16 depth swapchain
  std::vector<XrSwapchainImageVulkanKHR> _depthSwapImages;
  int64_t _depthFormat          = 0;
  std::vector<XrCompositionLayerDepthInfoKHR> _projDepthInfos; // lifetime must span xrEndFrame

  uint32_t _eyeW = 0, _eyeH = 0; // recommended per-eye rect
  uint32_t _swapW = 0, _swapH = 0; // created swapchain extent (must == 2*eyeW x eyeH)
  bool _needsGammaEncode = false;

  XrViewConfigurationType _viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrSessionState _sessionState            = XR_SESSION_STATE_UNKNOWN;
  bool _sessionRunning                    = false;
  bool _needReinit                        = false; // set on INSTANCE_LOSS_PENDING

  // per-frame state carried gpuUpdate -> __composite
  XrFrameState _frameState = {};
  std::vector<XrView> _views;
  std::vector<XrCompositionLayerProjectionView> _projViews;
  bool _frameBegun   = false;
  bool _shouldRender = false;

  // X3 composite measurement (throttled / on-change logging).
  double _lastCompositeMs   = -1.0;
  uint64_t _compositeFrames = 0;

  // Frame-pacing / pose-anomaly instrumentation (throttled). Mutated from the const
  // gpuUpdate instrument helpers through the shared PIMPL (mirrors _logCompositeMs).
  PacingStats _pace;

  // graphics binding (postGraphicsInit)
  VkInstance _vkInstance       = VK_NULL_HANDLE;
  VkPhysicalDevice _vkPhysDev  = VK_NULL_HANDLE;
  VkDevice _vkDevice           = VK_NULL_HANDLE;
  uint32_t _qfi = 0, _qidx = 0;

  // input
  XrActionSet _actionSet   = XR_NULL_HANDLE;
  XrAction _gripPoseAction = XR_NULL_HANDLE;
  XrAction _triggerAction  = XR_NULL_HANDLE;
  XrAction _squeezeAction  = XR_NULL_HANDLE;
  XrAction _menuAction     = XR_NULL_HANDLE;
  XrAction _aButtonAction  = XR_NULL_HANDLE;
  XrPath _handSubPath[2]   = {XR_NULL_PATH, XR_NULL_PATH};
  XrSpace _gripSpace[2]    = {XR_NULL_HANDLE, XR_NULL_HANDLE};
  bool _actionsAttached    = false;
  // Input degrades SOFTLY: a runtime that returns XR_ERROR_FUNCTION_UNSUPPORTED for
  // an optional input function (or null-procs it) must never fail the SESSION — we
  // disable input, log exactly once (throttle), and keep rendering.
  bool _inputActive    = true;
  bool _inputWarnLogged = false;

  // KHR vulkan-enable entry points (resolved via xrGetInstanceProcAddr).
  PFN_xrGetVulkanGraphicsRequirementsKHR pfnGfxReqs = nullptr;
  PFN_xrGetVulkanInstanceExtensionsKHR pfnInstExts  = nullptr;
  PFN_xrGetVulkanDeviceExtensionsKHR pfnDevExts     = nullptr;
  PFN_xrGetVulkanGraphicsDeviceKHR pfnGfxDevice     = nullptr;

  // XR_EXT_hand_tracking (OPTIONAL). All gated behind _handTrackingEnabled (extension
  //  requested at instance creation) AND _handTrackingSystem (system reports support).
  //  Absent → the whole feature stays cleanly unavailable (engine mirror unsupported).
  bool _handTrackingEnabled = false;                       // extension enabled at xrCreateInstance
  bool _handTrackingSystem  = false;                       // supportsHandTracking + procs resolved
  XrHandTrackerEXT _handTracker[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE}; // 0=left 1=right
  XrHandJointLocationEXT _handJointBuf[2][XR_HAND_JOINT_COUNT_EXT] = {}; // reusable locate buffer
  PFN_xrCreateHandTrackerEXT pfnCreateHandTracker   = nullptr;
  PFN_xrDestroyHandTrackerEXT pfnDestroyHandTracker = nullptr;
  PFN_xrLocateHandJointsEXT pfnLocateHandJoints     = nullptr;

  bool resolveKhrEntryPoints();
  bool resolveHandTrackingEntryPoints();
  XrPath strToPath(const char* s) const;
  bool suggestBindings(const char* profile, const std::vector<XrActionSuggestedBinding>& binds) const;
};

XrPath OpenXrImpl::strToPath(const char* s) const {
  XrPath p = XR_NULL_PATH;
  xrStringToPath(_instance, s, &p);
  return p;
}

bool OpenXrImpl::resolveKhrEntryPoints() {
  auto get = [&](const char* name, PFN_xrVoidFunction* fn) -> bool {
    return XR_SUCCEEDED(xrGetInstanceProcAddr(_instance, name, fn));
  };
  bool ok = true;
  ok = ok and get("xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGfxReqs);
  ok = ok and get("xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&pfnInstExts);
  ok = ok and get("xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&pfnDevExts);
  ok = ok and get("xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&pfnGfxDevice);
  return ok and pfnGfxReqs and pfnInstExts and pfnDevExts and pfnGfxDevice;
}

bool OpenXrImpl::resolveHandTrackingEntryPoints() {
  // XR_EXT_hand_tracking entry points are NOT loader-exported; resolve them via
  //  xrGetInstanceProcAddr (same discipline as the KHR vulkan-enable procs). A runtime
  //  that advertises the extension but null-procs a function → treat as unsupported.
  auto get = [&](const char* name, PFN_xrVoidFunction* fn) -> bool {
    return XR_SUCCEEDED(xrGetInstanceProcAddr(_instance, name, fn));
  };
  bool ok = true;
  ok = ok and get("xrCreateHandTrackerEXT", (PFN_xrVoidFunction*)&pfnCreateHandTracker);
  ok = ok and get("xrDestroyHandTrackerEXT", (PFN_xrVoidFunction*)&pfnDestroyHandTracker);
  ok = ok and get("xrLocateHandJointsEXT", (PFN_xrVoidFunction*)&pfnLocateHandJoints);
  return ok and pfnCreateHandTracker and pfnDestroyHandTracker and pfnLocateHandJoints;
}

bool OpenXrImpl::suggestBindings(const char* profile, const std::vector<XrActionSuggestedBinding>& binds) const {
  XrPath prof = strToPath(profile);
  XrInteractionProfileSuggestedBinding sug{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  sug.interactionProfile     = prof;
  sug.countSuggestedBindings = uint32_t(binds.size());
  sug.suggestedBindings      = binds.data();
  return XR_SUCCEEDED(xrSuggestInteractionProfileBindings(_instance, &sug));
}

////////////////////////////////////////////////////////////////////////////////

OpenXrDevice::OpenXrDevice() {
  _impl = std::make_shared<OpenXrImpl>();
  // Optimistic: the X1 seam only calls preGraphicsInit on an _active device.
  // preGraphicsInit flips this to false (cleanly, one log line) if no runtime.
  _active         = true;
  _supportsStereo = true;
  _camera_name    = "vrcam";
  // XR poses are already predicted by the runtime — never run the host-side
  // extrapolator, and OpenXrDevice OWNS _posemap while active.
  _trackedPoseValid = false;
}

OpenXrDevice::~OpenXrDevice() {
  auto I = _impl;
  if (!I)
    return;
  // XR_EXT_hand_tracking: the trackers are session children — destroy them BEFORE the
  //  session (below), on this (the owning) thread, so no locate races a torn-down session.
  for (int h = 0; h < 2; h++)
    if (I->_handTracker[h] and I->pfnDestroyHandTracker) {
      I->pfnDestroyHandTracker(I->_handTracker[h]);
      I->_handTracker[h] = XR_NULL_HANDLE;
    }
  for (int h = 0; h < 2; h++)
    if (I->_gripSpace[h])
      xrDestroySpace(I->_gripSpace[h]);
  if (I->_actionSet)
    xrDestroyActionSet(I->_actionSet);
  if (I->_depthSwapchain)
    xrDestroySwapchain(I->_depthSwapchain);
  if (I->_swapchain)
    xrDestroySwapchain(I->_swapchain);
  if (I->_viewSpace)
    xrDestroySpace(I->_viewSpace);
  if (I->_refSpace)
    xrDestroySpace(I->_refSpace);
  if (I->_session)
    xrDestroySession(I->_session);
  if (I->_instance)
    xrDestroyInstance(I->_instance);
}

////////////////////////////////////////////////////////////////////////////////
// preGraphicsInit — BEFORE the Vulkan instance. On ANY failure: exactly ONE log
// line, mark self inactive, leave reqs EMPTY (clean NoVR-equivalent fallback).
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::preGraphicsInit(ExternalGpuRequirements& reqs) {
  auto I = _impl;

  auto bail = [&](const char* why, XrResult r) {
    // The one, quiet failure line — no runtime is the common, non-error case.
    printf("[OPENXR] no runtime / pre-graphics init failed (%s, XrResult=%d) — falling back to NoVR.\n", why, int(r));
    fflush(stdout);
    _active = false;
    // leave reqs untouched (empty) — nothing registers, backend stays neutral.
  };

  // 1) xrEnumerateInstanceExtensionProperties — probe the loader/runtime is there.
  uint32_t extCount = 0;
  XrResult r = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
  if (XR_FAILED(r)) {
    bail("enumerate-instance-extensions", r);
    return;
  }
  std::vector<XrExtensionProperties> exts(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
  if (extCount)
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, exts.data());
  bool hasVk    = false;
  bool hasDepth = false;
  bool hasHands = false;
  for (auto& e : exts) {
    if (0 == std::strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME))
      hasVk = true;
    if (0 == std::strcmp(e.extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
      hasDepth = true;
    if (0 == std::strcmp(e.extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME))
      hasHands = true;
  }
  if (not hasVk) {
    bail("runtime lacks XR_KHR_vulkan_enable", XR_ERROR_EXTENSION_NOT_PRESENT);
    return;
  }

  // 2) xrCreateInstance — request XR_API_VERSION_1_0 + XR_KHR_vulkan_enable, and
  //    OPTIONALLY XR_KHR_composition_layer_depth (chained depth layer → positional
  //    reprojection). Absent → depth stays off and the session runs color-only.
  std::vector<const char*> enabledExts;
  enabledExts.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
  if (hasDepth)
    enabledExts.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
  // OPTIONAL articulated hand tracking — request it only if advertised; its absence never
  //  affects instance creation (the feature just stays unavailable, reported once below).
  if (hasHands)
    enabledExts.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
  std::strncpy(ici.applicationInfo.applicationName, "orkid", XR_MAX_APPLICATION_NAME_SIZE - 1);
  std::strncpy(ici.applicationInfo.engineName, "orkid", XR_MAX_ENGINE_NAME_SIZE - 1);
  ici.applicationInfo.applicationVersion = 1;
  ici.applicationInfo.engineVersion      = 1;
  ici.applicationInfo.apiVersion         = XR_API_VERSION_1_0;
  ici.enabledExtensionCount              = uint32_t(enabledExts.size());
  ici.enabledExtensionNames              = enabledExts.data();
  r                                      = xrCreateInstance(&ici, &I->_instance);
  if (XR_FAILED(r)) {
    bail("xrCreateInstance", r);
    return;
  }
  I->_depthExtEnabled     = hasDepth;
  I->_handTrackingEnabled = hasHands;

  if (not I->resolveKhrEntryPoints()) {
    bail("resolve KHR vulkan-enable entry points", XR_ERROR_FUNCTION_UNSUPPORTED);
    return;
  }

  // 3) xrGetSystem(HMD).
  XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
  sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  r              = xrGetSystem(I->_instance, &sgi, &I->_systemId);
  if (XR_FAILED(r)) {
    bail("xrGetSystem (no HMD)", r);
    return;
  }

  // 3b) OPTIONAL hand-tracking system probe. Only meaningful when the extension was
  //  enabled; chain XrSystemHandTrackingPropertiesEXT onto xrGetSystemProperties and honor
  //  supportsHandTracking. Also resolve the (loader-unexported) EXT entry points here. Any
  //  miss → the feature is unavailable (no error); the actual trackers are created after the
  //  session in _createHandTrackers, which emits the single availability line.
  if (I->_handTrackingEnabled) {
    XrSystemHandTrackingPropertiesEXT htp{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
    XrSystemProperties sysprops{XR_TYPE_SYSTEM_PROPERTIES};
    sysprops.next = &htp;
    bool sys_ok   = XR_SUCCEEDED(xrGetSystemProperties(I->_instance, I->_systemId, &sysprops)) and
                  (htp.supportsHandTracking == XR_TRUE);
    I->_handTrackingSystem = sys_ok and I->resolveHandTrackingEntryPoints();
  }

  // 4) xrGetVulkanGraphicsRequirementsKHR — MUST be called before graphics-device
  //    queries per XR_KHR_vulkan_enable; yields the supported Vulkan API window.
  XrGraphicsRequirementsVulkanKHR gfxreq{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
  r = I->pfnGfxReqs(I->_instance, I->_systemId, &gfxreq);
  if (XR_FAILED(r)) {
    bail("xrGetVulkanGraphicsRequirementsKHR", r);
    return;
  }
  reqs._minApiVersion = xrVersionToVk(gfxreq.minApiVersionSupported);
  reqs._maxApiVersion = xrVersionToVk(gfxreq.maxApiVersionSupported);

  // 5) xrGetVulkan{Instance,Device}ExtensionsKHR — space-separated lists, split.
  //    Two-call idiom: first call (capacity 0) → required size; second fills the
  //    buffer. Bound the buffer to the runtime-reported written count; splitExts is
  //    NUL/space robust. The parsed-count diagnostic MIRRORS the backend's [VKDEV]
  //    enabled-list line so producer and consumer counts can never silently disagree
  //    again (this incident: runtime advertised fewer exts than its proc-loader used).
  auto queryExts = [&](PFN_xrGetVulkanInstanceExtensionsKHR fn, std::vector<std::string>& dst, const char* label) -> bool {
    uint32_t cap = 0;
    if (XR_FAILED(fn(I->_instance, I->_systemId, 0, &cap, nullptr)))
      return false;
    std::string buf(cap, '\0');
    uint32_t written = 0;
    if (cap and XR_FAILED(fn(I->_instance, I->_systemId, cap, &written, buf.data())))
      return false;
    if (written and written <= buf.size())
      buf.resize(written);
    dst = splitExts(buf);
    std::string joined;
    for (const auto& e : dst) {
      joined += e;
      joined += " ";
    }
    printf("[OPENXR] runtime %s extensions parsed (count=%zu, cap=%u): %s\n", label, dst.size(), cap, joined.c_str());
    fflush(stdout);
    return true;
  };
  if (not queryExts(I->pfnInstExts, reqs._instanceExtensions, "instance")) {
    bail("xrGetVulkanInstanceExtensionsKHR", XR_ERROR_RUNTIME_FAILURE);
    return;
  }
  // pfnDevExts has the same signature shape as pfnInstExts.
  if (not queryExts((PFN_xrGetVulkanInstanceExtensionsKHR)I->pfnDevExts, reqs._deviceExtensions, "device")) {
    bail("xrGetVulkanDeviceExtensionsKHR", XR_ERROR_RUNTIME_FAILURE);
    return;
  }

  // 6) xrEnumerateViewConfigurationViews — recommended per-eye rect.
  uint32_t viewCount = 0;
  r = xrEnumerateViewConfigurationViews(I->_instance, I->_systemId, I->_viewConfigType, 0, &viewCount, nullptr);
  if (XR_FAILED(r) or viewCount < 2) {
    bail("xrEnumerateViewConfigurationViews", r);
    return;
  }
  std::vector<XrViewConfigurationView> vcv(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  xrEnumerateViewConfigurationViews(I->_instance, I->_systemId, I->_viewConfigType, viewCount, &viewCount, vcv.data());
  I->_eyeW = vcv[0].recommendedImageRectWidth;
  I->_eyeH = vcv[0].recommendedImageRectHeight;
  // engine-facing per-eye size (VrOutputNode reads _width*2 for the wide RTG).
  _width  = I->_eyeW;
  _height = I->_eyeH;

  logchan_xr->log("preGraphicsInit OK: eye<%u x %u> instExts<%zu> devExts<%zu> vkApi<0x%x..0x%x>",
                  I->_eyeW, I->_eyeH, reqs._instanceExtensions.size(), reqs._deviceExtensions.size(),
                  reqs._minApiVersion, reqs._maxApiVersion);
}

////////////////////////////////////////////////////////////////////////////////
// resolvePhysicalDevice — post-instance / pre-device. xrGetVulkanGraphicsDeviceKHR
// needs the VkInstance; write the REQUIRED physical device into the backend slot so
// the device-creation chokepoint selects the GPU the runtime demands.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::resolvePhysicalDevice(uint64_t vkInstance) {
  auto I = _impl;
  if (not _active or not I->_instance or not I->pfnGfxDevice)
    return;
  VkPhysicalDevice required = VK_NULL_HANDLE;
  XrResult r                = I->pfnGfxDevice(I->_instance, I->_systemId, (VkInstance)vkInstance, &required);
  if (XR_FAILED(r) or required == VK_NULL_HANDLE) {
    logchan_xr->log("resolvePhysicalDevice: xrGetVulkanGraphicsDeviceKHR failed (%d) — leaving default GPU", int(r));
    return;
  }
  if (auto* slot = _externalGpuRequirementsMutable())
    slot->_requiredPhysicalDevice = uint64_t(required);
  logchan_xr->log("resolvePhysicalDevice: required VkPhysicalDevice<%p>", (void*)required);
}

////////////////////////////////////////////////////////////////////////////////
// postGraphicsInit — AFTER device creation. Build the Vulkan graphics binding,
// create the session, reference space (STAGE, LOCAL fallback), the wide swapchain,
// and the input action set (attached EXACTLY ONCE).
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::postGraphicsInit(const GraphicsBindingInfo& binding) {
  auto I = _impl;
  if (not _active or not I->_instance)
    return;

  I->_vkInstance = (VkInstance)binding._vkInstance;
  I->_vkPhysDev  = (VkPhysicalDevice)binding._vkPhysicalDevice;
  I->_vkDevice   = (VkDevice)binding._vkDevice;
  I->_qfi        = binding._queueFamilyIndex;
  I->_qidx       = binding._queueIndex;

  // Verify the bound physical device is the one XR requires (single-GPU targets
  // match trivially; a mismatch means the backend picked a GPU the runtime can't
  // present from — loud, not silent).
  if (I->pfnGfxDevice) {
    VkPhysicalDevice want = VK_NULL_HANDLE;
    if (XR_SUCCEEDED(I->pfnGfxDevice(I->_instance, I->_systemId, I->_vkInstance, &want)) and want != I->_vkPhysDev) {
      logchan_xr->log("WARNING: bound VkPhysicalDevice<%p> != XR-required<%p>", (void*)I->_vkPhysDev, (void*)want);
    }
  }

  // xrCreateSession with the Vulkan graphics binding.
  XrGraphicsBindingVulkanKHR gb{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
  gb.instance         = I->_vkInstance;
  gb.physicalDevice   = I->_vkPhysDev;
  gb.device           = I->_vkDevice;
  gb.queueFamilyIndex = I->_qfi;
  gb.queueIndex       = I->_qidx;

  XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
  sci.next     = &gb;
  sci.systemId = I->_systemId;
  XrResult r   = xrCreateSession(I->_instance, &sci, &I->_session);
  if (XR_FAILED(r)) {
    logchan_xr->log("xrCreateSession failed (%d) — VR disabled.", int(r));
    _active = false;
    return;
  }

  // Reference space — STAGE preferred, LOCAL fallback.
  XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rsci.poseInReferenceSpace.orientation.w = 1.0f;
  rsci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_STAGE;
  if (XR_FAILED(xrCreateReferenceSpace(I->_session, &rsci, &I->_refSpace))) {
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    if (XR_FAILED(xrCreateReferenceSpace(I->_session, &rsci, &I->_refSpace)))
      logchan_xr->log("WARNING: no STAGE or LOCAL reference space");
  }
  // View (head) space for the center pose.
  XrReferenceSpaceCreateInfo vsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  vsci.poseInReferenceSpace.orientation.w = 1.0f;
  vsci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_VIEW;
  xrCreateReferenceSpace(I->_session, &vsci, &I->_viewSpace);

  _createSwapchain();
  _createDepthSwapchain();
  _createActions();
  _createHandTrackers();
}

////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_createSwapchain() {
  auto I = _impl;

  // Format selection: prefer SRGB (hardware OETF on store — no double-gamma); if
  // only UNORM is offered, select it and flag _needsGammaEncode so the composite
  // blit applies the sRGB OETF exactly once (A8: encode once, in the blit).
  uint32_t fmtCount = 0;
  xrEnumerateSwapchainFormats(I->_session, 0, &fmtCount, nullptr);
  std::vector<int64_t> fmts(fmtCount, 0);
  if (fmtCount)
    xrEnumerateSwapchainFormats(I->_session, fmtCount, &fmtCount, fmts.data());

  auto has = [&](int64_t f) { for (auto x : fmts) if (x == f) return true; return false; };

  I->_needsGammaEncode = false;
  if (has(VK_FORMAT_R8G8B8A8_SRGB))
    I->_swapFormat = VK_FORMAT_R8G8B8A8_SRGB;
  else if (has(VK_FORMAT_B8G8R8A8_SRGB))
    I->_swapFormat = VK_FORMAT_B8G8R8A8_SRGB;
  else if (has(VK_FORMAT_R8G8B8A8_UNORM)) {
    I->_swapFormat       = VK_FORMAT_R8G8B8A8_UNORM;
    I->_needsGammaEncode = true;
  } else if (has(VK_FORMAT_B8G8R8A8_UNORM)) {
    I->_swapFormat       = VK_FORMAT_B8G8R8A8_UNORM;
    I->_needsGammaEncode = true;
  } else if (fmtCount) {
    I->_swapFormat = fmts[0];
  }

  // ONE wide swapchain: width = 2*eyeW, arraySize = 1 (matches VrOutputNode's wide
  // RTG — zero compositor re-arch). Left/right occupy the two horizontal halves.
  XrSwapchainCreateInfo swci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
  swci.format      = I->_swapFormat;
  swci.sampleCount = 1;
  swci.width       = I->_eyeW * 2;
  swci.height      = I->_eyeH;
  swci.faceCount   = 1;
  swci.arraySize   = 1;
  swci.mipCount    = 1;

  if (XR_FAILED(xrCreateSwapchain(I->_session, &swci, &I->_swapchain))) {
    logchan_xr->log("xrCreateSwapchain failed — VR disabled.");
    _active = false;
    return;
  }
  I->_swapW = swci.width;
  I->_swapH = swci.height;

  // Wide-surface bring-up check (loud): the single 2*eyeW arraySize-1 layout is
  // only valid if the created extent actually is 2*eyeW x eyeH.
  bool wide_ok = (I->_swapW == I->_eyeW * 2) and (I->_swapH == I->_eyeH);
  printf("[OPENXR] wide-swapchain check: created<%u x %u> expected<%u x %u> match=%d fmt=%lld gammaEncode=%d\n",
         I->_swapW, I->_swapH, I->_eyeW * 2, I->_eyeH, int(wide_ok), (long long)I->_swapFormat, int(I->_needsGammaEncode));
  fflush(stdout);

  uint32_t imgCount = 0;
  xrEnumerateSwapchainImages(I->_swapchain, 0, &imgCount, nullptr);
  I->_swapImages.assign(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
  xrEnumerateSwapchainImages(
      I->_swapchain, imgCount, &imgCount, (XrSwapchainImageBaseHeader*)I->_swapImages.data());
}

////////////////////////////////////////////////////////////////////////////////
// _createDepthSwapchain — the SECOND wide swapchain, D16_UNORM, mirroring the color
// one (width = 2*eyeW, arraySize 1; left/right occupy the same horizontal halves the
// runtime already assumes for color). Self-gates: the extension must be enabled and the
// runtime MUST offer D16_UNORM. Depth images carry DEPTH_STENCIL_ATTACHMENT usage only,
// so the app writes them via a render pass (the reverse-Z copy pass), not a transfer.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_createDepthSwapchain() {
  auto I = _impl;
  // Escape hatch: ORKID_XR_NO_DEPTH=1 disables depth chaining entirely — the device behaves
  //  exactly as pre-depth (no D16 swapchain -> _depthChainSupported stays false -> no depth
  //  layer chained -> runtime stays in BASIC reprojection = the known-good visuals). One-shot.
  if (const char* nd = getenv("ORKID_XR_NO_DEPTH"); nd and nd[0] == '1') {
    printf("[OPENXR] ORKID_XR_NO_DEPTH=1 — depth layer DISABLED (color-only reprojection, BASIC runtime mode).\n");
    fflush(stdout);
    return;
  }
  if (not I->_depthExtEnabled)
    return; // runtime lacks XR_KHR_composition_layer_depth — color-only reprojection.

  // The runtime's depth-layer format list includes D16_UNORM; require it explicitly (it
  //  packs our LOCAL D16 images to its shared depth format at xrEndFrame).
  uint32_t fmtCount = 0;
  xrEnumerateSwapchainFormats(I->_session, 0, &fmtCount, nullptr);
  std::vector<int64_t> fmts(fmtCount, 0);
  if (fmtCount)
    xrEnumerateSwapchainFormats(I->_session, fmtCount, &fmtCount, fmts.data());
  bool hasD16 = false;
  for (auto x : fmts)
    if (x == VK_FORMAT_D16_UNORM)
      hasD16 = true;
  if (not hasD16) {
    printf("[OPENXR] depth layer disabled: runtime offers no VK_FORMAT_D16_UNORM depth format — color-only reprojection.\n");
    fflush(stdout);
    return;
  }
  I->_depthFormat = VK_FORMAT_D16_UNORM;

  XrSwapchainCreateInfo swci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swci.usageFlags  = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  swci.format      = I->_depthFormat;
  swci.sampleCount = 1;
  swci.width       = I->_eyeW * 2;
  swci.height      = I->_eyeH;
  swci.faceCount   = 1;
  swci.arraySize   = 1;
  swci.mipCount    = 1;
  if (XR_FAILED(xrCreateSwapchain(I->_session, &swci, &I->_depthSwapchain))) {
    printf("[OPENXR] depth layer disabled: xrCreateSwapchain(D16) failed — color-only reprojection.\n");
    fflush(stdout);
    I->_depthSwapchain = XR_NULL_HANDLE;
    return;
  }

  uint32_t imgCount = 0;
  xrEnumerateSwapchainImages(I->_depthSwapchain, 0, &imgCount, nullptr);
  I->_depthSwapImages.assign(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
  xrEnumerateSwapchainImages(
      I->_depthSwapchain, imgCount, &imgCount, (XrSwapchainImageBaseHeader*)I->_depthSwapImages.data());

  I->_depthChainSupported = true;
  printf("[OPENXR] depth swapchain created <%u x %u> fmt=D16_UNORM images=%u — depth reprojection AVAILABLE "
         "(chains once a valid per-eye depth is published each frame).\n",
         I->_eyeW * 2, I->_eyeH, imgCount);
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_createActions() {
  auto I = _impl;

  XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strncpy(asci.actionSetName, "orkid_main", XR_MAX_ACTION_SET_NAME_SIZE - 1);
  std::strncpy(asci.localizedActionSetName, "orkid", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
  asci.priority = 0;
  if (XR_FAILED(xrCreateActionSet(I->_instance, &asci, &I->_actionSet))) {
    // Input unsupported by this runtime — degrade politely, keep the session.
    I->_inputActive = false;
    logchan_xr->log("input: xrCreateActionSet unsupported/failed — VR input disabled (session unaffected).");
    return;
  }

  I->_handSubPath[HAND_LEFT]  = I->strToPath("/user/hand/left");
  I->_handSubPath[HAND_RIGHT] = I->strToPath("/user/hand/right");

  auto mkAction = [&](const char* name, XrActionType type, XrAction& out) {
    XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
    std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(aci.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    aci.actionType          = type;
    aci.countSubactionPaths = 2;
    aci.subactionPaths      = I->_handSubPath;
    xrCreateAction(I->_actionSet, &aci, &out);
  };
  mkAction("grip_pose", XR_ACTION_TYPE_POSE_INPUT, I->_gripPoseAction);
  mkAction("trigger", XR_ACTION_TYPE_BOOLEAN_INPUT, I->_triggerAction);
  mkAction("squeeze", XR_ACTION_TYPE_BOOLEAN_INPUT, I->_squeezeAction);
  mkAction("menu", XR_ACTION_TYPE_BOOLEAN_INPUT, I->_menuAction);
  mkAction("a_button", XR_ACTION_TYPE_BOOLEAN_INPUT, I->_aButtonAction);

  // Suggested bindings. Two profiles: the universal simple controller and the
  // Index controller (a/b + squeeze). Paths per the OpenXR interaction profiles.
  auto bindBoth = [&](std::vector<XrActionSuggestedBinding>& v, XrAction a, const char* lp, const char* rp) {
    v.push_back({a, I->strToPath(lp)});
    v.push_back({a, I->strToPath(rp)});
  };

  {
    std::vector<XrActionSuggestedBinding> binds;
    bindBoth(binds, I->_gripPoseAction, "/user/hand/left/input/grip/pose", "/user/hand/right/input/grip/pose");
    bindBoth(binds, I->_triggerAction, "/user/hand/left/input/select/click", "/user/hand/right/input/select/click");
    bindBoth(binds, I->_menuAction, "/user/hand/left/input/menu/click", "/user/hand/right/input/menu/click");
    I->suggestBindings("/interaction_profiles/khr/simple_controller", binds);
  }
  {
    std::vector<XrActionSuggestedBinding> binds;
    bindBoth(binds, I->_gripPoseAction, "/user/hand/left/input/grip/pose", "/user/hand/right/input/grip/pose");
    bindBoth(binds, I->_triggerAction, "/user/hand/left/input/trigger/click", "/user/hand/right/input/trigger/click");
    bindBoth(binds, I->_squeezeAction, "/user/hand/left/input/squeeze/click", "/user/hand/right/input/squeeze/click");
    bindBoth(binds, I->_menuAction, "/user/hand/left/input/b/click", "/user/hand/right/input/b/click");
    bindBoth(binds, I->_aButtonAction, "/user/hand/left/input/a/click", "/user/hand/right/input/a/click");
    I->suggestBindings("/interaction_profiles/valve/index_controller", binds);
  }

  // grip pose action spaces (per hand).
  for (int h = 0; h < 2; h++) {
    XrActionSpaceCreateInfo spci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    spci.action                        = I->_gripPoseAction;
    spci.subactionPath                 = I->_handSubPath[h];
    spci.poseInActionSpace.orientation.w = 1.0f;
    xrCreateActionSpace(I->_session, &spci, &I->_gripSpace[h]);
  }

  // Attach EXACTLY ONCE per session.
  if (not I->_actionsAttached) {
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets      = &I->_actionSet;
    if (XR_SUCCEEDED(xrAttachSessionActionSets(I->_session, &attach)))
      I->_actionsAttached = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// _createHandTrackers — OPTIONAL articulated hand tracking (XR_EXT_hand_tracking,
// default 26-joint set). Creates one XrHandTrackerEXT per hand ONLY when the extension
// was enabled AND the system reports support AND the procs resolved (all decided in
// preGraphicsInit). Emits EXACTLY ONE availability line covering every degrade reason,
// so the feature is never silently on/off. Never fails the session.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_createHandTrackers() {
  auto I = _impl;

  const char* why = nullptr;
  if (not I->_handTrackingEnabled)
    why = "runtime does not advertise XR_EXT_hand_tracking";
  else if (not I->_handTrackingSystem)
    why = "system reports no hand-tracking support (or EXT procs unresolved)";
  else if (not I->pfnCreateHandTracker)
    why = "xrCreateHandTrackerEXT unresolved";

  bool created = false;
  if (not why) {
    const XrHandEXT sides[2] = {XR_HAND_LEFT_EXT, XR_HAND_RIGHT_EXT};
    created                  = true;
    for (int h = 0; h < 2; h++) {
      XrHandTrackerCreateInfoEXT ci{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
      ci.hand         = sides[h];
      ci.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT; // the 26-joint default set
      if (XR_FAILED(I->pfnCreateHandTracker(I->_session, &ci, &I->_handTracker[h]))) {
        I->_handTracker[h] = XR_NULL_HANDLE;
        created            = false;
      }
    }
    if (not created) {
      why = "xrCreateHandTrackerEXT failed";
      // roll back any partial creation so teardown/locate never touch a half state.
      for (int h = 0; h < 2; h++)
        if (I->_handTracker[h]) {
          if (I->pfnDestroyHandTracker)
            I->pfnDestroyHandTracker(I->_handTracker[h]);
          I->_handTracker[h] = XR_NULL_HANDLE;
        }
    }
  }

  if (created) {
    _handTrackingSupported = true;
    std::lock_guard<std::mutex> lock(_hand_mutex);
    for (int h = 0; h < 2; h++)
      if (_handTracking[h])
        _handTracking[h]->_supported = true;
  } else {
    // ensure the mirror stays honestly unsupported (defensive; it defaults so).
    I->_handTrackingSystem = false;
  }

  // The ONE availability line.
  printf("[OPENXR] hand tracking %s%s%s.\n",
         created ? "AVAILABLE (XR_EXT_hand_tracking, 26-joint default set, both hands)" : "unavailable — ",
         created ? "" : why,
         created ? "" : " (degraded cleanly; head/eye tracking unaffected)");
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// _locateHands — per-frame joint locate against the app's base reference space at the
// frame's predicted display time (the SAME space+time as the head/view locate). Honors
// isActive AND per-joint position/orientation validity; an inactive hand publishes NO
// valid joints (never stale data). Poses go through the SHARED xrPoseToFmtx4 convention
// path (the same conjugation the head/controller poses use). Writes the engine mirror
// under _hand_mutex. Silent no-op when hand tracking is unavailable.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_locateHands() {
  auto I = _impl;
  if (not I->_handTrackingSystem or not I->pfnLocateHandJoints)
    return;

  for (int h = 0; h < 2; h++) {
    auto st = _handTracking[h];
    if (I->_handTracker[h] == XR_NULL_HANDLE or not st)
      continue;

    XrHandJointsLocateInfoEXT li{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
    li.baseSpace = I->_refSpace;                          // the app base space (as head)
    li.time      = I->_frameState.predictedDisplayTime;   // the frame predicted display time

    XrHandJointLocationsEXT locs{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
    locs.jointCount     = XR_HAND_JOINT_COUNT_EXT;
    locs.jointLocations = I->_handJointBuf[h];
    XrResult r          = I->pfnLocateHandJoints(I->_handTracker[h], &li, &locs);

    bool active = XR_SUCCEEDED(r) and (locs.isActive == XR_TRUE);

    std::lock_guard<std::mutex> lock(_hand_mutex);
    st->_active = active;
    if (not active) {
      // honest: no valid data for an inactive hand — clear all per-joint validity.
      for (auto& j : st->_joints) {
        j._positionValid    = false;
        j._orientationValid = false;
      }
      continue;
    }
    for (int j = 0; j < XR_HAND_JOINT_COUNT_EXT; j++) {
      const XrHandJointLocationEXT& src = I->_handJointBuf[h][j];
      HandJointPose& dst                = st->_joints[j];
      bool posv = (src.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
      bool oriv = (src.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
      dst._positionValid    = posv;
      dst._orientationValid = oriv;
      dst._radius           = src.radius;
      if (posv or oriv) {
        XrPosef pose = src.pose;
        sanitizeVrPose(pose);            // finite-quat guard (reused head/eye self-defense)
        dst._matrix = xrPoseToFmtx4(pose); // SAME convention path as head/controller poses
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Session FSM event pump.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_pollEvents() {
  auto I = _impl;
  XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
  while (xrPollEvent(I->_instance, &ev) == XR_SUCCESS) {
    switch (ev.type) {
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
        I->_needReinit    = true;
        I->_sessionRunning = false;
        _active            = false;
        logchan_xr->log("INSTANCE_LOSS_PENDING — reinit flagged.");
        break;
      }
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        auto* ssc        = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
        I->_sessionState = ssc->state;
        switch (ssc->state) {
          case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
            bi.primaryViewConfigurationType = I->_viewConfigType;
            if (XR_SUCCEEDED(xrBeginSession(I->_session, &bi)))
              I->_sessionRunning = true;
            break;
          }
          case XR_SESSION_STATE_STOPPING: {
            xrEndSession(I->_session);
            I->_sessionRunning = false;
            break;
          }
          case XR_SESSION_STATE_EXITING:
          case XR_SESSION_STATE_LOSS_PENDING: {
            I->_sessionRunning = false;
            _active            = false;
            break;
          }
          default:
            break;
        }
        break;
      }
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
        // Recreate the reference space (runtime recentered / bounds changed).
        if (I->_refSpace) {
          xrDestroySpace(I->_refSpace);
          I->_refSpace = XR_NULL_HANDLE;
        }
        XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rsci.poseInReferenceSpace.orientation.w = 1.0f;
        rsci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(I->_session, &rsci, &I->_refSpace))) {
          rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
          xrCreateReferenceSpace(I->_session, &rsci, &I->_refSpace);
        }
        break;
      }
      default:
        break;
    }
    ev = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_syncActions() {
  auto I = _impl;
  if (not I->_actionsAttached or not I->_inputActive)
    return;

  // A runtime may support session/frame but NOT the action sync/state functions
  // (XR_ERROR_FUNCTION_UNSUPPORTED). Disable input on first failure with ONE log —
  // never retry-spam, never fail the session.
  auto softDisableInput = [&](const char* which) {
    I->_inputActive = false;
    if (not I->_inputWarnLogged) {
      I->_inputWarnLogged = true;
      logchan_xr->log("input: %s unsupported/failed — VR input disabled (session unaffected).", which);
    }
  };

  XrActiveActionSet aas{I->_actionSet, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets      = &aas;
  if (XR_FAILED(xrSyncActions(I->_session, &sync))) {
    softDisableInput("xrSyncActions");
    return;
  }

  auto readBool = [&](XrAction a, XrPath sub) -> bool {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action        = a;
    gi.subactionPath = sub;
    XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_FAILED(xrGetActionStateBoolean(I->_session, &gi, &st)))
      return false;
    return st.isActive and st.currentState;
  };

  for (int h = 0; h < 2; h++) {
    auto c = controller(h);
    c->_triggerDown      = readBool(I->_triggerAction, I->_handSubPath[h]);
    c->_button1Down      = readBool(I->_aButtonAction, I->_handSubPath[h]);
    c->_button2Down      = readBool(I->_menuAction, I->_handSubPath[h]);
    c->_buttonThumbDown  = readBool(I->_squeezeAction, I->_handSubPath[h]);

    // grip pose -> controller world matrix
    if (I->_gripSpace[h] and I->_refSpace) {
      XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
      if (XR_SUCCEEDED(xrLocateSpace(I->_gripSpace[h], I->_refSpace, I->_frameState.predictedDisplayTime, &loc)) and
          (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
        c->_tracking_matrix = xrPoseToFmtx4(loc.pose);
        c->_world_matrix    = c->_tracking_matrix;
      }
    }
    c->updateGated();
  }

  // publish to the "hands" input group (channel names match the base contract).
  auto handgroup = lev2::InputManager::instance()->inputGroup("hands");
  auto L = controller(HAND_LEFT);
  auto R = controller(HAND_RIGHT);
  handgroup->setChannel("left.button1").as<bool>(L->_button1Down);
  handgroup->setChannel("left.button2").as<bool>(L->_button2Down);
  handgroup->setChannel("left.trigger").as<bool>(L->_triggerDown);
  handgroup->setChannel("left.thumb").as<bool>(L->_buttonThumbDown);
  handgroup->setChannel("left.matrix").as<fmtx4>(L->_world_matrix);
  handgroup->setChannel("right.button1").as<bool>(R->_button1Down);
  handgroup->setChannel("right.button2").as<bool>(R->_button2Down);
  handgroup->setChannel("right.trigger").as<bool>(R->_triggerDown);
  handgroup->setChannel("right.thumb").as<bool>(R->_buttonThumbDown);
  handgroup->setChannel("right.matrix").as<fmtx4>(R->_world_matrix);
}

////////////////////////////////////////////////////////////////////////////////
// gpuUpdate — at the beginAssemble call site (the correct xrWaitFrame moment):
// poll events; WaitFrame -> predictedDisplayTime; BeginFrame; LocateViews -> per-
// eye poses + asymmetric projections -> _posemap; _updatePosesCommon (which is
// bypassed of _predictHmdPose since _trackedPoseValid is forced false).
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::gpuUpdate(RenderContextFrameData& RCFD) {
  auto I = _impl;
  if (not _active or not I->_session)
    return;

  _pollEvents();
  if (not I->_sessionRunning)
    return;

  XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
  I->_frameState = XrFrameState{XR_TYPE_FRAME_STATE};
  // Instrumentation: time the xrWaitFrame block — the runtime's per-frame pacing point.
  auto t_wait0 = std::chrono::steady_clock::now();
  bool wait_ok = XR_SUCCEEDED(xrWaitFrame(I->_session, &fwi, &I->_frameState));
  auto t_wait1 = std::chrono::steady_clock::now();
  if (not wait_ok)
    return;
  double wait_ms = std::chrono::duration<double, std::milli>(t_wait1 - t_wait0).count();
  _instrumentPacing(wait_ms, int64_t(I->_frameState.predictedDisplayTime));
  I->_shouldRender = I->_frameState.shouldRender;

  XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
  xrBeginFrame(I->_session, &fbi);
  I->_frameBegun = true;

  _syncActions();
  _locateHands(); // articulated hand joints (optional; no-op when unavailable)

  // xrLocateViews -> per-eye pose + fov.
  I->_views.assign(2, {XR_TYPE_VIEW});
  XrViewState vstate{XR_TYPE_VIEW_STATE};
  XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
  vli.viewConfigurationType = I->_viewConfigType;
  vli.displayTime           = I->_frameState.predictedDisplayTime;
  vli.space                 = I->_refSpace;
  uint32_t vcount           = 0;
  if (XR_FAILED(xrLocateViews(I->_session, &vli, &vstate, 2, &vcount, I->_views.data())) or vcount < 2)
    return;

  // head (center) pose in reference space.
  fmtx4 headWorld; // head->world
  bool head_located = false;
  XrQuaternionf head_q{0, 0, 0, 1};
  {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (I->_viewSpace and
        XR_SUCCEEDED(xrLocateSpace(I->_viewSpace, I->_refSpace, I->_frameState.predictedDisplayTime, &loc))) {
      headWorld    = xrPoseToFmtx4(loc.pose);
      head_q       = loc.pose.orientation;
      head_located = true;
    }
  }
  fmtx4 worldToHead = headWorld.inverse();

  // Instrumentation: head angular-velocity anomaly detection (pose-pipeline vs pacing).
  if (head_located)
    _instrumentHeadPose(head_q.x, head_q.y, head_q.z, head_q.w);

  // Fill _posemap so _updatePosesCommon derives the eye cameras (see derivation in
  // openxr_device.cpp): cmv = usermtx * base * hmd ; lmv = cmv * eyeOffset.
  {
    std::lock_guard<std::mutex> lock(_posemap_mutex);
    _posemap["hmd"] = worldToHead;
    for (int e = 0; e < 2; e++) {
      XrView& v = I->_views[e];
      // zero-FOV / zero-quat self-defense: never let a NaN projection or view reach
      // the posemap. Sanitize in place so the center-average and __composite layer
      // (which re-read I->_views) also see the finite fallback.
      XrFovf rawfov = v.fov;
      if (sanitizeVrFov(v.fov)) {
        static bool s_warned_fov = false;
        if (not s_warned_fov) {
          s_warned_fov = true;
          printf("[OPENXR] WARN degenerate per-view FOV (angleL=%g R=%g U=%g D=%g) — substituting symmetric +/-45deg fallback frustum.\n",
                 double(rawfov.angleLeft), double(rawfov.angleRight), double(rawfov.angleUp), double(rawfov.angleDown));
          fflush(stdout);
        }
      }
      XrQuaternionf rawq = v.pose.orientation;
      if (sanitizeVrPose(v.pose)) {
        static bool s_warned_pose = false;
        if (not s_warned_pose) {
          s_warned_pose = true;
          printf("[OPENXR] WARN degenerate per-view pose quat (x=%g y=%g z=%g w=%g) — substituting identity orientation.\n",
                 double(rawq.x), double(rawq.y), double(rawq.z), double(rawq.w));
          fflush(stdout);
        }
      }
      fmtx4 eyeWorld  = xrPoseToFmtx4(v.pose);           // eye->world
      fmtx4 worldToEye = eyeWorld.inverse();             // world->eye (the view)
      fmtx4 offset     = fmtx4::multiply_ltor(headWorld, worldToEye); // head->eye
      auto proj        = fovToVrFrustum(v.fov, _near, _far).composeProjection();
      if (e == 0) {
        _posemap["eyel"] = offset;
        _posemap["projl"] = proj;
      } else {
        _posemap["eyer"] = offset;
        _posemap["projr"] = proj;
      }
    }
    // center projection = average of the two fovs.
    XrFovf fc;
    fc.angleLeft  = 0.5f * (I->_views[0].fov.angleLeft + I->_views[1].fov.angleLeft);
    fc.angleRight = 0.5f * (I->_views[0].fov.angleRight + I->_views[1].fov.angleRight);
    fc.angleUp    = 0.5f * (I->_views[0].fov.angleUp + I->_views[1].fov.angleUp);
    fc.angleDown  = 0.5f * (I->_views[0].fov.angleDown + I->_views[1].fov.angleDown);
    _posemap["projc"] = fovToVrFrustum(fc, _near, _far).composeProjection();
  }

  // OpenXrDevice OWNS _posemap while active: discard any host-set tracked pose so
  // the extrapolator is bypassed and the XR poses win.
  _trackedPoseValid = false;
  _updatePosesCommon();

  // one-shot liveness beacon: proves the frame loop reached a full gpuUpdate with a
  // filled posemap (WaitFrame/BeginFrame/SyncActions/LocateViews all succeeded).
  static bool s_frame_loop_live = false;
  if (not s_frame_loop_live) {
    s_frame_loop_live = true;
    printf("[OPENXR] frame loop live — gpuUpdate completed (views located, posemap filled).\n");
    fflush(stdout);
  }
}

////////////////////////////////////////////////////////////////////////////////
// _submitProjectionFrame — shared orchestration for both handoff shapes: acquire +
// wait a swapchain image, run the per-path blit closure (imgIndex → the GPU copy
// into the acquired VkImage), release, and xrEndFrame with ONE projection layer of
// two views (left/right half imageRects). If the session should not render, still
// Begin/EndFrame with layerCount 0 — skipping STALLS the runtime.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_submitProjectionFrame(
    Context* targ,
    const char* tag,
    bool haveContent,
    const std::function<void(uint32_t)>& blitfn,
    const std::function<bool(uint32_t)>& depthBlitFn) const {
  auto I = _impl;

  // The runtime submits to the engine's bound graphics queue inside its own
  // frame-submission calls (swapchain acquire/release and xrEndFrame). A VkQueue
  // requires external synchronization, so hold the bound queue's submit mutex across
  // this ENTIRE region — it serializes the runtime's submits against every engine-side
  // submit (render + loader threads). The mutex is recursive: the composite blit closure
  // re-enters queueSubmit on this thread while the region lock is held.
  std::unique_lock<std::recursive_mutex> queueLock;
  if (auto* qmtx = targ ? targ->externalSubmitMutex() : nullptr)
    queueLock = std::unique_lock<std::recursive_mutex>(*qmtx);

  const bool render = I->_shouldRender and (I->_swapchain != XR_NULL_HANDLE) and haveContent;

  XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  std::vector<XrCompositionLayerProjectionView>& pv = I->_projViews;

  if (render) {
    uint32_t imgIndex = 0;
    XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (XR_SUCCEEDED(xrAcquireSwapchainImage(I->_swapchain, &acq, &imgIndex))) {
      XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      wi.timeout = XR_INFINITE_DURATION;
      xrWaitSwapchainImage(I->_swapchain, &wi);

      // per-path GPU copy into the acquired image (wide two-eye tex, or two per-eye
      //  texes into the halves) — the actual submit+WAIT CB + sRGB OETF live in the
      //  vulkan vr-composite hook.
      blitfn(imgIndex);

      XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
      xrReleaseSwapchainImage(I->_swapchain, &rel);
    }

    // Depth layer (optional): acquire/wait the D16 depth swapchain in the SAME acquire
    //  cycle as color (slot indices correspond), let depthBlitFn write the reverse-Z
    //  image, release. Only when the caller supplied a depth writer AND the layer is
    //  supported; a failing writer omits the chain THIS frame (color-only) — never a
    //  crash. Chain depth EVERY frame once you start, or the runtime demotes the session.
    bool depthWritten = false;
    if (I->_depthChainSupported and depthBlitFn) {
      uint32_t dImgIndex = 0;
      XrSwapchainImageAcquireInfo dacq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      if (XR_SUCCEEDED(xrAcquireSwapchainImage(I->_depthSwapchain, &dacq, &dImgIndex))) {
        XrSwapchainImageWaitInfo dwi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        dwi.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(I->_depthSwapchain, &dwi);
        depthWritten = depthBlitFn(dImgIndex);
        XrSwapchainImageReleaseInfo drel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(I->_depthSwapchain, &drel);
      }
    }

    // One projection layer, two views; left/right occupy the horizontal halves.
    pv.assign(2, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
    // Chain a per-view depth-info onto BOTH views (same double-wide depth swapchain,
    //  mirrored halves) only when a valid depth image was published this frame and the
    //  near/far pair is sane. The runtime consumes nearZ/farZ + subImage.swapchain; our
    //  reverse-Z values put depth 1.0 at nearZ (device _near) and 0.0 at farZ (the real,
    //  FINITE device _far). Held in the PIMPL so the pointers survive to xrEndFrame.
    const bool chainDepth = depthWritten and validDepthNearFar(_near, _far);
    if (chainDepth)
      I->_projDepthInfos.assign(2, {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR});
    for (int e = 0; e < 2; e++) {
      pv[e].pose                     = I->_views[e].pose;
      pv[e].fov                      = I->_views[e].fov;
      pv[e].subImage.swapchain       = I->_swapchain;
      pv[e].subImage.imageArrayIndex = 0;
      pv[e].subImage.imageRect.offset = {int32_t(e * I->_eyeW), 0};
      pv[e].subImage.imageRect.extent = {int32_t(I->_eyeW), int32_t(I->_eyeH)};
      if (chainDepth) {
        XrCompositionLayerDepthInfoKHR& di = I->_projDepthInfos[e];
        di.subImage.swapchain        = I->_depthSwapchain;
        di.subImage.imageArrayIndex  = 0;
        di.subImage.imageRect.offset = {int32_t(e * I->_eyeW), 0};
        di.subImage.imageRect.extent = {int32_t(I->_eyeW), int32_t(I->_eyeH)};
        di.minDepth                  = 0.0f;
        di.maxDepth                  = 1.0f;
        di.nearZ                     = _near; // reverse-Z depth 1.0 sits at the near plane
        di.farZ                      = _far;  // ... and 0.0 at the (finite) far plane
        pv[e].next                   = &I->_projDepthInfos[e];
      }
    }
    if (chainDepth) {
      static bool s_depth_chain_first = false;
      if (not s_depth_chain_first) {
        s_depth_chain_first = true;
        printf("[OPENXR] depth layer CHAINED (both views -> D16 %ux%u, nearZ=%g farZ=%g, reverseZ=1-stdZ clamp>=%g) "
               "— runtime should promote to positional (depth) reprojection.\n",
               I->_eyeW * 2, I->_eyeH, double(_near), double(_far), double(kRevZFloor));
        fflush(stdout);
      }
    }
    layer.space     = I->_refSpace;
    layer.viewCount = 2;
    layer.views     = pv.data();
  }

  const XrCompositionLayerBaseHeader* layers[1] = {(const XrCompositionLayerBaseHeader*)&layer};
  XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
  fei.displayTime          = I->_frameState.predictedDisplayTime;
  fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  fei.layerCount           = render ? 1 : 0;
  fei.layers               = render ? layers : nullptr;
  XrResult endr            = xrEndFrame(I->_session, &fei);
  // one-shot: proves at least one frame was submitted to the runtime, and with what.
  static bool s_endframe_first = false;
  if (not s_endframe_first) {
    s_endframe_first = true;
    printf("[OPENXR] %s first xrEndFrame (render=%d, XrResult=%d).\n", tag, int(render), int(endr));
    fflush(stdout);
  }
  I->_frameBegun = false;
}

////////////////////////////////////////////////////////////////////////////////
// _logCompositeMs — throttled composite-cost log (first frame / >0.1ms change /
// every 120th frame). X3 requires the composite CB be MEASURED.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_logCompositeMs(double comp_ms) const {
  auto I = _impl;
  I->_compositeFrames++;
  bool changed = (I->_lastCompositeMs < 0.0) or (std::fabs(comp_ms - I->_lastCompositeMs) > 0.1);
  if (changed or (I->_compositeFrames % 120) == 0) {
    I->_lastCompositeMs = comp_ms;
    printf("[OPENXR] composite CB %.3f ms (gammaEncode=%d, %ux%u)\n",
           comp_ms, int(I->_needsGammaEncode), I->_eyeW * 2, I->_eyeH);
    fflush(stdout);
  }
}

////////////////////////////////////////////////////////////////////////////////
// _instrumentPacing — per-frame pacing accounting. Times the xrWaitFrame block and the
// predictedDisplayTime delta (in display periods, period = rolling median of deltas).
// Emits a rate-limited (max ~2/sec) anomaly line when the delta != 1 period (a skipped
// or doubled display cycle — the beat-pattern signature of two unsynced pacers), and a
// ~5s summary (fps, waitMs p50/p95/max, periodSkips, poseSpikes). Only reached on the
// live XR frame path, so it is inherently silent when the device is inactive.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_instrumentPacing(double waitMs, int64_t displayTimeNs) const {
  auto& P    = _impl->_pace;
  auto now   = PacingStats::clock_t::now();
  P._frameIdx++;

  if (not P._primed) {
    P._primed          = true;
    P._prevDisplayTime = displayTimeNs;
    P._lastRawDeltaNs  = 0;
    P._lastContinuous  = true;
    P._winStart        = now;
    P._lastAnomaly     = now - std::chrono::seconds(1); // allow an immediate first anomaly
    return;
  }

  int64_t rawDelta   = displayTimeNs - P._prevDisplayTime;
  P._prevDisplayTime = displayTimeNs;
  P._lastRawDeltaNs  = rawDelta;

  // period estimate = median of recent raw deltas
  P._deltaRing[P._deltaHead] = rawDelta;
  P._deltaHead               = (P._deltaHead + 1) % kPacePeriodRing;
  if (P._deltaCount < kPacePeriodRing)
    P._deltaCount++;
  int64_t period = medianI64(P._deltaRing, P._deltaCount);
  if (period <= 0)
    period = (rawDelta > 0) ? rawDelta : 1;

  double periodsF = double(rawDelta) / double(period);
  long periodsR   = std::lround(periodsF);
  P._lastContinuous = (periodsR == 1);

  // wait-duration ring (window percentiles)
  P._waitRing[P._waitHead] = float(waitMs);
  P._waitHead              = (P._waitHead + 1) % kPaceWaitRing;
  if (P._waitCount < kPaceWaitRing)
    P._waitCount++;

  P._winFrames++;

  if (not P._lastContinuous) {
    P._winPeriodSkips++;
    if ((now - P._lastAnomaly) >= std::chrono::milliseconds(500)) {
      P._lastAnomaly = now;
      printf("[OPENXR] pacing anomaly: displayTime delta=%.2f periods (wait=%.2fms, frame=%llu)\n",
             periodsF, waitMs, (unsigned long long)P._frameIdx);
      fflush(stdout);
    }
  }

  double win_s = std::chrono::duration<double>(now - P._winStart).count();
  if (win_s >= 5.0) {
    double fps = (win_s > 0.0) ? double(P._winFrames) / win_s : 0.0;
    float p50, p95, mx;
    waitStats(P._waitRing, P._waitCount, p50, p95, mx);
    printf("[OPENXR] pacing: fps=%.1f waitMs p50/p95/max=%.2f/%.2f/%.2f periodSkips=%u poseSpikes=%u (5s window)\n",
           fps, p50, p95, mx, P._winPeriodSkips, P._winPoseSpikes);
    fflush(stdout);
    P._winStart       = now;
    P._winFrames      = 0;
    P._winPeriodSkips = 0;
    P._winPoseSpikes  = 0;
    P._waitCount      = 0;
    P._waitHead       = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// _instrumentHeadPose — head angular velocity from consecutive orientations (quat delta
// / displayTime delta). A >5x spike over the ~1s rolling median WHILE the displayTime was
// continuous discriminates a pose-pipeline inconsistency (bad frame) from a pacing skip
// (which _instrumentPacing already flags). Shares the pacing rate-limiter (max ~2/sec).
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::_instrumentHeadPose(float qx, float qy, float qz, float qw) const {
  auto& P = _impl->_pace;
  float cur[4] = {qx, qy, qz, qw};

  if (not P._prevHeadValid) {
    P._prevHeadValid  = true;
    P._prevHeadQuat[0] = qx; P._prevHeadQuat[1] = qy;
    P._prevHeadQuat[2] = qz; P._prevHeadQuat[3] = qw;
    return;
  }

  double dt_s = double(P._lastRawDeltaNs) * 1.0e-9;
  double angle = quatAngleBetween(P._prevHeadQuat, cur);
  P._prevHeadQuat[0] = qx; P._prevHeadQuat[1] = qy;
  P._prevHeadQuat[2] = qz; P._prevHeadQuat[3] = qw;
  if (dt_s <= 0.0)
    return;
  double angvel = angle / dt_s;

  // spike test against the rolling median of PRIOR samples (before pushing this one)
  double median = (P._angvelCount >= 8) ? double(medianF(P._angvelRing, P._angvelCount)) : 0.0;
  bool spike = (P._angvelCount >= 8) and (median > 1.0e-3) and (angvel > 5.0 * median) and P._lastContinuous;
  if (spike) {
    P._winPoseSpikes++;
    auto now = PacingStats::clock_t::now();
    if ((now - P._lastAnomaly) >= std::chrono::milliseconds(500)) {
      P._lastAnomaly = now;
      printf("[OPENXR] pose anomaly: angvel spike %.2frad/s vs median %.2f (displayTime continuous)\n",
             angvel, median);
      fflush(stdout);
    }
  }

  P._angvelRing[P._angvelHead] = float(angvel);
  P._angvelHead                = (P._angvelHead + 1) % kPaceAngvelRing;
  if (P._angvelCount < kPaceAngvelRing)
    P._angvelCount++;
}

////////////////////////////////////////////////////////////////////////////////
// __composite — the WIDE two-eye handoff (VrOutputNode / FWDPBRVR path): one
// submit+WAIT CB copies the pre-packed wide texture into the acquired swapchain
// image (SRGB blit, or SRGB-intermediate encode for a UNORM swapchain — the OETF
// exactly once). Validated only on a real runtime.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::__composite(Context* targ, Texture* twoeyetex) const {
  auto I = _impl;
  // EMPIRICAL PARITY RECORD (do NOT re-derive from convention arguments — this was flipped
  //  back and forth by winding/row-order reasoning and the CODE convention lost every time):
  //   (a) the OLD Y-flipped projection + flipV=true was orientation-CORRECT on the HMD for a
  //       full day of live use.
  //   (b) the CORRECTED projection (2026-07 winding fix) + flipV=true showed pitch(X)+roll(Z)
  //       motion INVERTED, yaw correct — the vertical-mirror fingerprint.
  //  Fixing the projection un-flipped the eye render's stored row order, so with the correct
  //  projection the XR eye render arrives TOP-DOWN and the blit must NOT flip. flipV=false is
  //  the live-verified pairing for the corrected projection. Depth follows the same variable.
  const bool flipV = false;
  // one-shot: proves the composite chain actually reached the device (the link the
  // HMD-box run showed was never invoked — no swapchain acquire, no xrEndFrame).
  static bool s_composite_first = false;
  if (not s_composite_first) {
    s_composite_first = true;
    printf("[OPENXR] __composite first entry (active=%d session=%d frameBegun=%d flipV=%d).\n",
           int(_active), int(I->_session != XR_NULL_HANDLE), int(I->_frameBegun), int(flipV));
    fflush(stdout);
  }
  if (not _active or not I->_session or not I->_frameBegun) {
    static bool s_composite_guard = false;
    if (not s_composite_guard) {
      s_composite_guard = true;
      const char* which = (not _active) ? "not _active" : (not I->_session) ? "no _session" : "not _frameBegun";
      printf("[OPENXR] __composite early-return guard fired: %s — no frame reaches the runtime.\n", which);
      fflush(stdout);
    }
    return;
  }

  // Depth reprojection is NOT chained on the wide path: the pre-packed wide two-eye
  //  color texture arrives here with no matching per-eye depth (the wide FWDPBRVR handoff
  //  keeps only color), and reconstructing it would need a re-plumb of that node. The
  //  DualMonoVr per-eye path (__compositeStereo) is THE depth-supported model; note once.
  if (I->_depthChainSupported) {
    static bool s_wide_nodepth = false;
    if (not s_wide_nodepth) {
      s_wide_nodepth = true;
      printf("[OPENXR] wide __composite path ships NO depth layer (color-only reprojection); the DualMonoVr per-eye path is depth-supported.\n");
      fflush(stdout);
    }
  }

  _submitProjectionFrame(targ, "__composite", twoeyetex != nullptr, [&](uint32_t imgIndex) {
    double comp_s = ::ork::lev2::vulkan::vrCompositeBlitToXrImage(
        targ,
        twoeyetex,
        uint64_t(I->_swapImages[imgIndex].image),
        int64_t(I->_swapFormat),
        I->_eyeW * 2,
        I->_eyeH,
        I->_needsGammaEncode,
        0, 0, /*dstDiscard*/ true, flipV);
    _logCompositeMs(comp_s * 1000.0);
  });
}

////////////////////////////////////////////////////////////////////////////////
// __compositeStereo — the PER-EYE handoff (DualMonoVrOutputNode / FWDPBRVRDM path).
// Mirrors __composite but the two final per-eye textures are blitted into their
// halves of the ONE wide swapchain image: left at dst x-offset 0 (discards the fresh
// image), right at dst x-offset eyeW (dstDiscard=false → PRESERVES the left half).
// Each blit applies the same sRGB-OETF-exactly-once encode as the wide path.
////////////////////////////////////////////////////////////////////////////////

void OpenXrDevice::__compositeStereo(
    Context* targ, Texture* texL, Texture* texR, Texture* depthTexL, Texture* depthTexR) const {
  auto I = _impl;
  // EMPIRICAL PARITY RECORD (do NOT re-derive from convention arguments — see __composite):
  //   (a) OLD Y-flipped projection + flipV=true was orientation-CORRECT on the HMD for a full
  //       day of live use.
  //   (b) CORRECTED projection (2026-07 winding fix) + flipV=true re-introduced pitch(X)+roll(Z)
  //       motion inversion, yaw correct — the vertical-mirror fingerprint.
  //  With the corrected projection the eye render arrives TOP-DOWN, so the blit must NOT flip.
  //  flipV=false is the live-verified pairing. The depth conversion pass reads THIS SAME
  //  variable (captured into depthBlitFn below), so color and depth stay row-aligned.
  const bool flipV = false;
  // one-shot: proves the per-eye handoff reached the device (distinct tag from the
  // wide __composite one-shot so the HMD-box log names which path is live).
  static bool s_cs_first = false;
  if (not s_cs_first) {
    s_cs_first = true;
    printf("[OPENXR] __compositeStereo first entry (active=%d session=%d frameBegun=%d flipV=%d).\n",
           int(_active), int(I->_session != XR_NULL_HANDLE), int(I->_frameBegun), int(flipV));
    fflush(stdout);
  }
  if (not _active or not I->_session or not I->_frameBegun) {
    static bool s_cs_guard = false;
    if (not s_cs_guard) {
      s_cs_guard = true;
      const char* which = (not _active) ? "not _active" : (not I->_session) ? "no _session" : "not _frameBegun";
      printf("[OPENXR] __compositeStereo early-return guard fired: %s — no frame reaches the runtime.\n", which);
      fflush(stdout);
    }
    return;
  }

  const bool haveTex = (texL != nullptr) and (texR != nullptr);

  // Depth writer (optional): reverse-Z convert + clamp + V-flip (the SAME flipV as
  //  color, so the depth rows follow color automatically if the winding analysis flips
  //  that constant) each per-eye depth texture into its half of the ONE wide D16 depth
  //  image — left at x=0, right at x=eyeW. Built ONLY when the layer is supported AND
  //  both per-eye depth sources were supplied by the compositor. A conversion failure
  //  disables the layer (drops permanently to color-only) and warns once — fail loud,
  //  never flap.
  // Per-scene toggle (VrDepthPublish -> Device::_publishDepth): a scene can keep the runtime
  //  in color-only/BASIC reprojection without touching the env kill switch. Data-driven, so a
  //  new "depth off" scene is ZERO C++ diff. (The env emergency override ORKID_XR_NO_DEPTH=1 is
  //  handled earlier in _createDepthSwapchain — it never creates the D16 swapchain, so
  //  _depthChainSupported is already false and wins over this param.)
  std::function<bool(uint32_t)> depthBlitFn;
  if (I->_depthChainSupported and not _publishDepth) {
    static bool s_param_off = false;
    if (not s_param_off) {
      s_param_off = true;
      printf("[OPENXR] VrDepthPublish=0 (scene param) — depth layer NOT chained (color-only, BASIC reprojection).\n");
      fflush(stdout);
    }
  }
  if (I->_depthChainSupported and _publishDepth and depthTexL and depthTexR) {
    depthBlitFn = [this, I, targ, depthTexL, depthTexR, flipV](uint32_t dImgIndex) -> bool {
      uint64_t ddst = uint64_t(I->_depthSwapImages[dImgIndex].image);
      double dl = ::ork::lev2::vulkan::vrCompositeDepthToXrImage(
          targ, depthTexL, ddst, int64_t(I->_depthFormat), I->_eyeW, I->_eyeH, _near, _far, 0, 0, flipV);
      double dr = ::ork::lev2::vulkan::vrCompositeDepthToXrImage(
          targ, depthTexR, ddst, int64_t(I->_depthFormat), I->_eyeW, I->_eyeH, _near, _far, I->_eyeW, 0, flipV);
      bool ok = (dl >= 0.0) and (dr >= 0.0);
      if (not ok) {
        static bool s_depth_fail = false;
        if (not s_depth_fail) {
          s_depth_fail = true;
          printf("[OPENXR] depth conversion pass FAILED — disabling depth layer, dropping to color-only reprojection.\n");
          fflush(stdout);
        }
        I->_depthChainSupported = false; // stop chaining (no flapping)
        return false;
      }
      static bool s_depth_ok = false;
      if (not s_depth_ok) {
        s_depth_ok = true;
        printf("[OPENXR] first depth publish: D16 %ux%u nearZ=%g farZ=%g flipV=%d convert=reverseZ(1-stdZ) clamp>=%g.\n",
               I->_eyeW * 2, I->_eyeH, double(_near), double(_far), int(flipV), double(kRevZFloor));
        fflush(stdout);
      }
      return true;
    };
  }

  _submitProjectionFrame(
      targ,
      "__compositeStereo",
      haveTex,
      [&](uint32_t imgIndex) {
        uint64_t dst = uint64_t(I->_swapImages[imgIndex].image);
        // Two per-eye blits into the ONE wide swapchain image, back-to-back within this
        //  single acquire/release. Left DISCARDS the fresh image; right PRESERVES the
        //  just-written left half (dstDiscard=false).
        double l_s = ::ork::lev2::vulkan::vrCompositeBlitToXrImage(
            targ, texL, dst, int64_t(I->_swapFormat), I->_eyeW, I->_eyeH, I->_needsGammaEncode,
            0, 0, /*dstDiscard*/ true, flipV);
        double r_s = ::ork::lev2::vulkan::vrCompositeBlitToXrImage(
            targ, texR, dst, int64_t(I->_swapFormat), I->_eyeW, I->_eyeH, I->_needsGammaEncode,
            I->_eyeW, 0, /*dstDiscard*/ false, flipV);
        _logCompositeMs((l_s + r_s) * 1000.0);
      },
      depthBlitFn);
}

////////////////////////////////////////////////////////////////////////////////
// Env-gated pose/FOV self-test (no runtime, no graphics). Verdict lines are
// grepped by test_x2_openxr.py.
////////////////////////////////////////////////////////////////////////////////

static bool approx(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}
static bool vapprox(const fvec4& v, float x, float y, float z, float eps = 1e-4f) {
  return approx(v.x, x, eps) and approx(v.y, y, eps) and approx(v.z, z, eps);
}

bool runSelfTests() {
  bool ok = true;
  auto verdict = [&](const char* name, bool cond) {
    printf("ORKID_OPENXR_SELFTEST: [%s] %s\n", cond ? "PASS" : "FAIL", name);
    fflush(stdout);
    ok = ok and cond;
  };

  auto mkpose = [](float qx, float qy, float qz, float qw, float px, float py, float pz) {
    XrPosef p{};
    p.orientation = {qx, qy, qz, qw};
    p.position    = {px, py, pz};
    return p;
  };

  // identity pose -> identity transform.
  {
    fmtx4 m = xrPoseToFmtx4(mkpose(0, 0, 0, 1, 0, 0, 0));
    fvec4 t = fvec3(1, 2, 3).transform(m);
    verdict("pose_identity", vapprox(t, 1, 2, 3));
  }
  // pure translation.
  {
    fmtx4 m = xrPoseToFmtx4(mkpose(0, 0, 0, 1, 1, 2, 3));
    fvec3 tr = m.translation();
    fvec4 o  = fvec3(0, 0, 0).transform(m);
    verdict("pose_translation", approx(tr.x, 1) and approx(tr.y, 2) and approx(tr.z, 3) and vapprox(o, 1, 2, 3));
  }
  // yaw +90deg about +Y: rotates a point (1,0,0) -> (0,0,-1); (0,0,1) -> (1,0,0).
  {
    float s = std::sin(float(M_PI) * 0.25f), c = std::cos(float(M_PI) * 0.25f);
    fmtx4 m  = xrPoseToFmtx4(mkpose(0, s, 0, c, 0, 0, 0));
    fvec4 vx = fvec3(1, 0, 0).transform(m);
    fvec4 vz = fvec3(0, 0, 1).transform(m);
    verdict("pose_yaw90", vapprox(vx, 0, 0, -1, 1e-3f) and vapprox(vz, 1, 0, 0, 1e-3f));
  }
  // combined yaw + translation.
  {
    float s = std::sin(float(M_PI) * 0.25f), c = std::cos(float(M_PI) * 0.25f);
    fmtx4 m  = xrPoseToFmtx4(mkpose(0, s, 0, c, 5, 0, 0));
    fvec4 vx = fvec3(1, 0, 0).transform(m);
    verdict("pose_combined", vapprox(vx, 5, 0, -1, 1e-3f));
  }
  // FOV symmetric -> matches a hand-built VrProjFrustumPar of the same tangents,
  // and is symmetric (no x/y center shift).
  {
    XrFovf fov;
    fov.angleLeft = -0.5f; fov.angleRight = 0.5f; fov.angleUp = 0.4f; fov.angleDown = -0.4f;
    fmtx4 a = fovToVrFrustum(fov, 0.1f, 100.0f).composeProjection();
    VrProjFrustumPar ref;
    ref._left = std::tan(-0.5f); ref._right = std::tan(0.5f);
    // OpenVR-raw polarity (top negative / bottom positive) — mirrors fovToVrFrustum's
    //  angleDown->_top, angleUp->_bottom mapping. Self-referential (both sides go
    //  through composeProjection), so it is BLIND to the Y-flip; proj_y_sign_matches_native
    //  below is the cross-convention guard for that.
    ref._top = std::tan(-0.4f);  ref._bottom = std::tan(0.4f);
    ref._near = 0.1f;            ref._far = 100.0f;
    fmtx4 b = ref.composeProjection();
    bool same = true;
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        same = same and approx(a.elemXY(i, j), b.elemXY(i, j), 1e-5f);
    // symmetric: center-shift elements (col 2 of rows 0,1 in the YX layout) are 0.
    bool symmetric = approx(a.elemYX(0, 2), 0.0f, 1e-5f) and approx(a.elemYX(1, 2), 0.0f, 1e-5f);
    verdict("fov_symmetric_matches_engine_helper", same and symmetric);
  }

  // XR projection polarity vs the native/NoVR convention (CROSS-convention, not self-
  //  referential). For an equivalent symmetric FOV the XR projection's Y-scale element
  //  SIGN — and the whole matrix's determinant SIGN — must MATCH fmtx4::perspective (the
  //  convention NoVR fills the posemap with). A Y-flipped projection (the backface-culling
  //  bug) inverts the Y-scale sign, flipping the determinant sign too; both native and XR
  //  are RH here so their determinants share sign only when the polarity is correct. This
  //  is the guard the structurally-blind fov_symmetric fixture above cannot provide.
  {
    XrFovf fov;
    fov.angleLeft = -0.5f; fov.angleRight = 0.5f; fov.angleUp = 0.5f; fov.angleDown = -0.5f;
    fmtx4 xrp = fovToVrFrustum(fov, 0.1f, 100.0f).composeProjection();
    fmtx4 nat;
    // vertical FOV (radians) = angleUp - angleDown; perspective() consumes radians.
    nat.perspective(fov.angleUp - fov.angleDown, 1.0f, 0.1f, 100.0f);
    float y_xr  = xrp.elemYX(1, 1);
    float y_nat = nat.elemYX(1, 1);
    bool y_sign_match   = (y_xr > 0.0f) and (y_nat > 0.0f);
    bool det_sign_match = (xrp.determinant() * nat.determinant()) > 0.0f;
    verdict("proj_y_sign_matches_native", y_sign_match and det_sign_match);
  }

  // splitExts: the ext-string parser the producer feeds the X1 merge. The runtime's
  // real strings can't be reproduced on mac, but the split can — cover multiple/
  // leading/trailing spaces AND trailing NULs (the two-call buffer is NUL-padded).
  {
    auto v = splitExts("VK_EXT_a VK_EXT_b VK_EXT_host_image_copy");
    verdict("splitexts_basic", v.size() == 3 and v[0] == "VK_EXT_a" and v[1] == "VK_EXT_b" and v[2] == "VK_EXT_host_image_copy");
  }
  {
    auto v = splitExts("  VK_A   VK_B  ");
    verdict("splitexts_ws", v.size() == 2 and v[0] == "VK_A" and v[1] == "VK_B");
  }
  {
    // simulate the two-call buffer: trailing NUL(s) past the content must not yield
    // an empty/garbage token nor drop the last real name.
    std::string s("VK_ONE VK_TWO", 13);
    s.push_back('\0');
    s.push_back('\0');
    auto v = splitExts(s);
    verdict("splitexts_trailing_nul", v.size() == 2 and v[0] == "VK_ONE" and v[1] == "VK_TWO");
  }
  {
    auto v = splitExts("");
    verdict("splitexts_empty", v.empty());
  }

  // degenerate-FOV self-defense: an all-zero FOV (the observed pre-first-frame case)
  // yields tan(0)=0 tangents -> composeProjection divides by zero -> NaN projection.
  // sanitizeVrFov must FIRE and the resulting projection must be all-finite — a
  // runtime-free proxy for the vp_l/vp_r NaN-bind failure on the HMD box.
  {
    XrFovf fov;
    fov.angleLeft = 0.0f; fov.angleRight = 0.0f; fov.angleUp = 0.0f; fov.angleDown = 0.0f;
    bool fired  = sanitizeVrFov(fov);
    fmtx4 p     = fovToVrFrustum(fov, 0.1f, 100.0f).composeProjection();
    bool finite = true;
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        finite = finite and std::isfinite(p.elemXY(i, j));
    verdict("fov_degenerate_fallback", fired and finite);
  }

  // __compositeStereo guard (runtime-owned per-eye path): with no XR session the
  //  handoff MUST early-return BEFORE any acquire/xrEndFrame — the only mac-observable
  //  facet of the path (a real session/swapchain is gated on a runtime). Proxy: force
  //  the frame-active flag, call with a null session + null texes, and assert the flag
  //  is untouched (the submit path would clear it via xrEndFrame; the guard returns
  //  first). A broken guard would instead xrEndFrame a null session and clear the flag.
  {
    auto dev  = openxr_device();
    auto impl = dev->_impl;
    bool saved_frameBegun    = impl->_frameBegun;
    XrSession saved_session  = impl->_session;
    impl->_session           = XR_NULL_HANDLE;
    impl->_frameBegun        = true;
    dev->__compositeStereo(nullptr, nullptr, nullptr, nullptr, nullptr);
    bool guard_held          = (impl->_frameBegun == true);
    impl->_frameBegun        = saved_frameBegun;
    impl->_session           = saved_session;
    verdict("compositestereo_guard_no_session", guard_held);
  }

  // depth convention conversion (standard-Z -> reverse-Z + coverage clamp). orkid window
  //  depth is 0 at the near plane and 1 at the far plane; the runtime wants 1 at near,
  //  0 at far, and treats a packed 0 as EMPTY. So near->1, far->clamp floor (NOT 0, else
  //  the background would composite through), mid stays mid, and the output never leaves
  //  [floor,1]. This is the exact math the depth-copy fragment pass must mirror.
  {
    bool near_opaque = approx(stdZtoRevZ(0.0f), 1.0f);
    bool far_clamped = approx(stdZtoRevZ(1.0f), kRevZFloor) and (stdZtoRevZ(1.0f) > 0.0f);
    bool mid_linear  = approx(stdZtoRevZ(0.5f), 0.5f);
    bool in_range    = (stdZtoRevZ(1.0f) >= kRevZFloor) and (stdZtoRevZ(0.0f) <= 1.0f);
    verdict("depth_stdz_to_revz", near_opaque and far_clamped and mid_linear and in_range);
  }

  // depth-layer near/far self-defense. The submitted pair for a typical device (near
  //  0.1, far 1000) must be accepted; a +inf far (the runtime forbids it), a zero far,
  //  and equal planes must all be rejected so a bad config drops to color-only.
  {
    bool good     = validDepthNearFar(0.1f, 1000.0f);
    bool rej_inf  = not validDepthNearFar(0.1f, std::numeric_limits<float>::infinity());
    bool rej_zero = not validDepthNearFar(0.1f, 0.0f);
    bool rej_eq   = not validDepthNearFar(5.0f, 5.0f);
    verdict("depth_nearfar_selfdefense", good and rej_inf and rej_zero and rej_eq);
  }

  // hand-tracking engine surface (no runtime): the OPTIONAL XR_EXT_hand_tracking mirror
  //  must degrade cleanly. On mac there is no runtime, so both hands must snapshot as a
  //  non-null, unsupported, inactive 26-joint state — the runtime-free proxy that the
  //  engine surface + degrade wiring are intact. Out-of-range side must also default-safe.
  {
    auto dev = openxr_device();
    auto L   = dev->handTrackingSnapshot(0);
    auto R   = dev->handTrackingSnapshot(1);
    auto bad = dev->handTrackingSnapshot(7);
    bool shape_ok = L and R and bad and                              //
                    (L->_joints.size() == size_t(kHandJointCount)) and //
                    (kHandJointCount == 26);
    bool degraded = L and R and                                       //
                    (not L->_supported) and (not L->_active) and      //
                    (not R->_supported) and (not R->_active) and      //
                    (not dev->_handTrackingSupported);
    bool joints_clear = true;
    if (L)
      for (const auto& j : L->_joints)
        joints_clear = joints_clear and (not j._positionValid) and (not j._orientationValid);
    verdict("handtracking_unavailable_degrades_clean", shape_ok and degraded and joints_clear);
  }

  printf("ORKID_OPENXR_SELFTEST: %s\n", ok ? "ALL PASS" : "SOME FAILED");
  fflush(stdout);
  return ok;
}

////////////////////////////////////////////////////////////////////////////////

openxrdevice_ptr_t openxr_device() {
  static openxrdevice_ptr_t _device = std::make_shared<OpenXrDevice>();
  return _device;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr::openxr_
////////////////////////////////////////////////////////////////////////////////
#endif // ENABLE_OPENXR
