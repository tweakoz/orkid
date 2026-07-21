////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/vr/vr.h>

////////////////////////////////////////////////////////////////////////////////
// OpenXrDevice : native, IP-clean OpenXR 1.x driver (the third orkidvr::Device
//  beside NoVR and the legacy OpenVR path). A pure loader-discovered client — the
//  runtime is found at runtime via XR_RUNTIME_JSON / active_runtime.json and is
//  never named here. Vulkan graphics binding via XR_KHR_vulkan_enable v1.
//
//  This header carries NO OpenXR/Vulkan types: all XR/Vulkan state lives behind a
//  PIMPL (OpenXrImpl, defined in openxr_device.cpp) so consumers (lev2_init) stay
//  free of the SDK headers, mirroring vr.h's opaque-handle discipline.
////////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_OPENXR)

namespace ork::lev2::orkidvr::openxr_ {

////////////////////////////////////////////////////////////////////////////////

struct OpenXrImpl; // PIMPL: all XrInstance/XrSession/Vulkan state.
using openxrimpl_ptr_t = std::shared_ptr<OpenXrImpl>;

////////////////////////////////////////////////////////////////////////////////

struct OpenXrDevice final : public Device {

  OpenXrDevice();
  ~OpenXrDevice() final;

  // Device virtuals.
  void gpuUpdate(RenderContextFrameData& RCFD) final;
  void __composite(Context* targ, Texture* twoeyetex) const final;
  void __compositeStereo(
      Context* targ, Texture* texL, Texture* texR, Texture* depthTexL, Texture* depthTexR) const final;

  // Two-phase graphics-init seam (X1) + post-instance resolve (X2).
  void preGraphicsInit(ExternalGpuRequirements& reqs) final;
  void resolvePhysicalDevice(uint64_t vkInstance) final;
  void postGraphicsInit(const GraphicsBindingInfo& binding) final;

  // Imports the runtime's swapchain and presents to the headset itself → the host
  //  app comes up windowless. Honored only while the device is _active (a runtime was
  //  found); a failed preGraphicsInit clears _active and the app stays windowed.
  bool ownsHmdPresentation() const final { return true; }

  openxrimpl_ptr_t _impl;

private:
  void _createSwapchain();
  // Optional depth swapchain (XR_KHR_composition_layer_depth): ONE wide D16_UNORM
  //  swapchain mirroring the color one. Self-gates on the extension being enabled AND
  //  the runtime offering D16_UNORM — otherwise a no-op (color-only reprojection).
  void _createDepthSwapchain();
  void _createActions();
  // Optional articulated hand tracking (XR_EXT_hand_tracking, default 26-joint set).
  //  _createHandTrackers: after session start, create one XrHandTrackerEXT per hand IFF
  //  the extension was enabled AND the system supports it; emits ONE availability line and
  //  otherwise leaves the feature cleanly unavailable. _locateHands: per-frame, locate the
  //  26 joints against the base reference space at the frame's predicted display time and
  //  publish honest per-joint validity + the hand active flag into the engine hand mirror.
  void _createHandTrackers();
  void _locateHands();
  void _pollEvents();
  void _syncActions();
  // Shared frame-submit orchestration for __composite / __compositeStereo: acquire +
  //  wait the swapchain image, run the per-path blit closure (imgIndex → GPU copy),
  //  release, build the ONE two-view projection layer (half-rect imageRects) and
  //  xrEndFrame. tag labels the first-xrEndFrame one-shot.
  //  depthBlitFn (optional): when set AND the depth layer is supported, the depth
  //  swapchain is acquired/waited alongside color, depthBlitFn(depthImgIndex) writes
  //  the reverse-Z D16 image (returning success), and a per-view XrCompositionLayer-
  //  DepthInfoKHR is chained — engaging the runtime's positional reprojection. Empty
  //  (the default) → color-only, byte-identical to the pre-depth path.
  void _submitProjectionFrame(
      Context* targ,
      const char* tag,
      bool haveContent,
      const std::function<void(uint32_t)>& blitfn,
      const std::function<bool(uint32_t)>& depthBlitFn = {}) const;
  // Throttled composite-cost log (first frame / >0.1ms change / every 120th).
  void _logCompositeMs(double comp_ms) const;

  // Throttled frame-pacing / pose-anomaly instrumentation (steady_clock timing). Per-
  //  frame in gpuUpdate: _instrumentPacing captures the xrWaitFrame block duration and
  //  the predictedDisplayTime delta (in display periods); _instrumentHeadPose derives
  //  head angular velocity from consecutive orientations. Both emit rate-limited anomaly
  //  one-liners and a ~5s summary. Engine-generic; silent when the device is inactive.
  //  Args are plain scalars (no XR types) so the header stays SDK-free.
  void _instrumentPacing(double waitMs, int64_t displayTimeNs) const;
  void _instrumentHeadPose(float qx, float qy, float qz, float qw) const;
};

using openxrdevice_ptr_t = std::shared_ptr<OpenXrDevice>;

////////////////////////////////////////////////////////////////////////////////
// Process-singleton accessor (analogous to novr_device / openvr_device). Does NOT
//  set itself as the active device — GfxInit does that when ORKID_VR_DRIVER=openxr.
openxrdevice_ptr_t openxr_device();

////////////////////////////////////////////////////////////////////////////////
// Env-gated pose/FOV math self-test (no XR runtime, no graphics needed). Prints
//  "ORKID_OPENXR_SELFTEST:" verdict lines to stdout; returns true iff all pass.
//  Invoked from GfxInit when ORKID_OPENXR_SELFTEST=1 (X1's env-gated pattern).
bool runSelfTests();

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr::openxr_

#endif // ENABLE_OPENXR
