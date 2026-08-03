////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/external_gpu_requirements.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/input/inputdevice.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/lev2_types.h>
#include <ork/orktypes.h>
#include <array>

#if defined(ENABLE_LIBSURVIVE)
#include <libsurvive/survive_api.h>
#endif
#if defined(ENABLE_OPENVR)
#include <openvr/openvr.h>
namespace _ovr = ::vr;
#endif

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
////////////////////////////////////////////////////////////////////////////////
// DistortionRect / distortion_lambda_t : the per-eye output-present hook.
//  The VR output node invokes the distortion lambda once per eye with the eye's
//  resolved color texture and the destination viewport rect on the HMD panel.
////////////////////////////////////////////////////////////////////////////////

struct DistortionRect {
  Texture* _inp_tex;
  SRect _out_vprect;
  char _eye = 0; //'L' or 'R'
};

using distortion_lambda_t = std::function<void(rcfd_ptr_t RCFD, DistortionRect drect)>;

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

struct ControllerState {
  fmtx4 _abs_matrix;
  fmtx4 _tracking_matrix;
  fmtx4 _world_matrix;
  bool _button1Down         = false;
  bool _button2Down         = false;
  bool _buttonThumbDown     = false;
  bool _triggerDown         = false;
  bool _button1DownPrev     = false;
  bool _button2DownPrev     = false;
  bool _buttonThumbDownPrev = false;
  bool _triggerDownPrev     = false;

  void updateGated();

  bool _button1GatedDown     = false;
  bool _button2GatedDown     = false;
  bool _buttonThumbGatedDown = false;
  bool _triggerGatedDown     = false;
  bool _button1GatedUp       = false;
  bool _button2GatedUp       = false;
  bool _buttonThumbGatedUp   = false;
  bool _triggerGatedUp       = false;

  float _xwpos = 0.0f;

  int _association_state = -1;
};

using controllerstate_ptr_t = std::shared_ptr<ControllerState>;

////////////////////////////////////////////////////////////////////////////////
// Articulated hand tracking (generic OpenXR XR_EXT_hand_tracking, default 26-joint
//  set). OPTIONAL: a device populates these only when the runtime advertises the
//  extension AND the system reports hand-tracking support; every other path leaves
//  them unsupported/inactive. The joint ordering matches the extension's default
//  set (index == joint ordinal), so kHandJointCount markers map 1:1 to the runtime
//  joint array. Nothing here carries an SDK type — this is the engine-facing mirror.
////////////////////////////////////////////////////////////////////////////////

static constexpr int kHandJointCount = 26; // default hand-joint set

// Ordinal names for the default 26-joint set (index into HandTrackingState::_joints).
//  Engine-generic; the values follow the standard default-set ordering so a caller can
//  index by semantic name without touching any SDK header.
enum class HandJoint : int {
  Palm = 0,
  Wrist,
  ThumbMetacarpal,
  ThumbProximal,
  ThumbDistal,
  ThumbTip,
  IndexMetacarpal,
  IndexProximal,
  IndexIntermediate,
  IndexDistal,
  IndexTip,
  MiddleMetacarpal,
  MiddleProximal,
  MiddleIntermediate,
  MiddleDistal,
  MiddleTip,
  RingMetacarpal,
  RingProximal,
  RingIntermediate,
  RingDistal,
  RingTip,
  LittleMetacarpal,
  LittleProximal,
  LittleIntermediate,
  LittleDistal,
  LittleTip,
};

// One articulated joint. _matrix is the joint->reference(world) transform in the SAME
//  coordinate convention as the head/controller poses (built through the shared
//  xrPoseToFmtx4 conjugation path, in the device's XR reference space). Validity is
//  honored honestly and per-flag: a consumer must gate on _positionValid /
//  _orientationValid before trusting the corresponding part of _matrix.
struct HandJointPose {
  fmtx4 _matrix;                  // joint->reference (world), engine convention
  float _radius            = 0.0f; // joint capsule radius (meters)
  bool _positionValid      = false;
  bool _orientationValid   = false;
};

// Per-hand articulated state. _supported reflects whether this build + runtime + system
//  can produce hand data at all; _active reflects whether THIS hand is currently tracked
//  (an inactive hand carries no valid joints — never stale data).
struct HandTrackingState {
  bool _supported = false;
  bool _active    = false;
  std::array<HandJointPose, kHandJointCount> _joints;
};

using handjointpose_ptr_t     = std::shared_ptr<HandJointPose>;
using handtrackingstate_ptr_t = std::shared_ptr<HandTrackingState>;

////////////////////////////////////////////////////////////////////////////////

struct VrProjFrustumPar {

  fmtx4 composeProjection() const;

  float _left   = -1.0f;
  float _right  = 1.0f;
  float _top    = -1.0f;
  float _bottom = 1.0f;
  float _near   = .1f;
  float _far    = 50000.0f;
};

////////////////////////////////////////////////////////////////////////////////

struct VrTrackingCameraNotificationFrame {
  CameraMatrices _leftcamera;
  CameraMatrices _centercamera;
  CameraMatrices _rightcamera;
};
struct VrTrackingHmdPoseNotificationFrame {
  fmtx4 _hmdMatrix;
};
struct VrTrackingControllerNotificationFrame {
  VrTrackingControllerNotificationFrame();
  controllerstate_ptr_t _left;
  controllerstate_ptr_t _right;
};

struct VrTrackingNotificationReceiver {
  typedef std::function<void(const svar256_t&)> callback_t;
  callback_t _callback;
};
typedef std::shared_ptr<VrTrackingNotificationReceiver> VrTrackingNotificationReceiver_ptr_t;
typedef std::set<VrTrackingNotificationReceiver_ptr_t> VrTrackingNotificationReceiver_set;

void addVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr);
void removeVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr);
extern ork::LockedResource<VrTrackingNotificationReceiver_set> gnotifset;

////////////////////////////////////////////////////////////////////////////////

typedef std::function<fmtx4()> usermatrixgenerator_t;

////////////////////////////////////////////////////////////////////////////////
// StandardVrPresentation : device-agnostic per-eye HMD presentation profile.
//
//  Configuration data set from the host (e.g. python); the per-eye present pass
//  is executed in C++. genLambda() produces the distortion_lambda_t that the VR
//  output node runs once per eye. Nothing here is device-specific — the
//  distortion shader and the calibration values are supplied by the host.
//
//  Two stages:
//    _eyeViewTransform[2] : RENDER-time per-eye view adjust (IPD offset + cant /
//                           toe-in). Composed into the eye cameras by the device.
//    _eyeTransform[2]     : PRESENT-time per-eye image transform (panel rotation
//                           / flip / placement). Bound to the shader as MatMVP.
//
//  Lens distortion (radial + chromatic) is evaluated in the shader about
//  _lensCenter[eye] using _distortionR/G/B. When the three channels are equal
//  the achromatic technique (one sample) is selected; otherwise the chromatic
//  technique (three samples). Selection is per-eye on the CPU — never a
//  per-pixel branch.
//
//  Shader contract (resolved from _material by name):
//    techniques : _techniqueAchromatic, _techniqueChromatic
//    params     : "MatMVP"(mtx4) "ColorMap"(tex) "LensCenter"(vec2)
//                 "DistortionR"/"DistortionG"/"DistortionB"(vec4)
////////////////////////////////////////////////////////////////////////////////

struct StandardVrPresentation {

  distortion_lambda_t genLambda() const;
  void _resolve() const;

  freestyle_mtl_ptr_t _material;
  std::string _techniqueAchromatic = "achromatic";
  std::string _techniqueChromatic  = "chromatic";

  fmtx4 _eyeViewTransform[2] = {fmtx4(), fmtx4()};
  fmtx4 _eyeTransform[2]     = {fmtx4(), fmtx4()};
  fvec4 _distortionR         = fvec4(0.0f, 0.0f, 0.0f, 0.0f);
  fvec4 _distortionG         = fvec4(0.0f, 0.0f, 0.0f, 0.0f);
  fvec4 _distortionB         = fvec4(0.0f, 0.0f, 0.0f, 0.0f);
  fvec2 _lensCenter[2]       = {fvec2(0.5f, 0.5f), fvec2(0.5f, 0.5f)};
  bool _enable               = true;

  // lazily-resolved shader handles (cache); reset when _material changes
  mutable bool _resolved                        = false;
  mutable fxtechnique_constptr_t _tekAchromatic = nullptr;
  mutable fxtechnique_constptr_t _tekChromatic  = nullptr;
  mutable fxparam_constptr_t _parMVP            = nullptr;
  mutable fxparam_constptr_t _parColorMap       = nullptr;
  mutable fxparam_constptr_t _parLensCenter     = nullptr;
  mutable fxparam_constptr_t _parDistortionR    = nullptr;
  mutable fxparam_constptr_t _parDistortionG    = nullptr;
  mutable fxparam_constptr_t _parDistortionB    = nullptr;
};

using standardvrpresentation_ptr_t = std::shared_ptr<StandardVrPresentation>;

////////////////////////////////////////////////////////////////////////////////

struct Device {
  Device();
  virtual ~Device();
  void _updatePosesCommon();

  virtual void gpuUpdate(RenderContextFrameData& RCFD)              = 0;
  virtual void __composite(Context* targ, Texture* twoeyetex) const = 0;

  // Per-eye handoff for the runtime-owned dual-mono presentation path. Hands the two
  //  FINAL per-eye COLOR textures to the device, which blits each into its half of the
  //  ONE wide runtime swapchain image and submits the frame (mirrors __composite, which
  //  takes one pre-packed wide two-eye texture). Genuine no-op on devices that do NOT
  //  own HMD presentation (NoVR / desktop preview / OpenVR wide-path); the real work
  //  lives in the XR-runtime device.
  //  depthTexL/depthTexR (optional) are the matching per-eye DEPTH textures. When a
  //  device supports the runtime's depth-layer contract (XR_KHR_composition_layer_depth)
  //  and both are supplied, it converts them to the runtime's reverse-Z D16 depth
  //  swapchain and chains a per-view depth layer so the runtime does positional
  //  (depth-based) reprojection. Null (the default) → color-only reprojection, exactly
  //  the prior behavior; devices that do not own HMD presentation ignore them.
  virtual void __compositeStereo(
      Context* targ,
      Texture* texL,
      Texture* texR,
      Texture* depthTexL = nullptr,
      Texture* depthTexR = nullptr) const = 0;

  // Two-phase graphics-init seam (X1). A driver that must shape Vulkan creation
  //  (e.g. an OpenXR device) publishes its instance/device extensions, API-version
  //  window and required physical device into reqs BEFORE the Vulkan instance is
  //  created; after the main context's device+queue exist it receives them via
  //  postGraphicsInit to bind its session. Both default to no-ops (NoVR path).
  virtual void preGraphicsInit(ExternalGpuRequirements& reqs) {}
  virtual void postGraphicsInit(const GraphicsBindingInfo& binding) {}

  // True when this device owns presentation to the HMD directly — it imports its own
  //  swapchain from the XR runtime and presents to the headset itself. Such a device
  //  makes the host app WINDOWLESS: there is no on-screen surface to create, and
  //  attempting to build a window-present swapchain would collide with the presentation
  //  the runtime already owns. The app-init path routes the main context down the
  //  offscreen (no-window, no-surface) branch when this is true AND the device is active.
  //  Default false — NoVR and the desktop-preview path present through the normal window.
  virtual bool ownsHmdPresentation() const { return false; }

  // Post-instance / pre-device seam (X2). Some producers (an OpenXR device) can
  //  only resolve the REQUIRED physical device once the Vulkan instance exists
  //  (its handle does not exist before instance creation). Called with the just-
  //  created VkInstance (opaque uint64) after VulkanInstance construction and
  //  BEFORE logical-device creation, so the producer can write the required
  //  physical device into the backend-internal requirements slot in time for the
  //  device-creation chokepoint to honor it. Default is a no-op (NoVR path).
  virtual void resolvePhysicalDevice(uint64_t vkInstance) {}

  // Scanout predictor shared with the gfx context — call predictNextTargetSystemTick() for pose prediction.
  ork::time_predictor_ptr_t _scan_out_predictor;

  std::map<std::string, fmtx4> _posemap;

  // Posemap is getting updated from the sensor thread and read from the rendering thread.
  // We need a synchronization primitive for this. TODO should be SPSCQueue
  mutable std::mutex _posemap_mutex; 

  cameramatrices_ptr_t _leftcamera       = nullptr;
  cameramatrices_ptr_t _centercamera     = nullptr;
  cameramatrices_ptr_t _rightcamera      = nullptr;
  usermatrixgenerator_t _usermtxgen = nullptr;

  // host-configured per-eye HMD presentation (distortion / rotation / cant).
  //  null => the VR output node falls back to its fixed blit.
  standardvrpresentation_ptr_t _presentation;

  void overrideSize(int w, int h);
  void resetCalibration();

  // Articulated hand tracking (generic OpenXR XR_EXT_hand_tracking). _handTracking[0]=left,
  //  [1]=right; always non-null (constructed unsupported/inactive) so a consumer can read
  //  them on any device without a null check. A device that supports the extension rewrites
  //  them each frame under _hand_mutex; every other path leaves them unsupported/inactive.
  //  handTrackingSnapshot returns a stable value copy under the lock (side 0=left, 1=right).
  handtrackingstate_ptr_t handTrackingSnapshot(int side) const;
  handtrackingstate_ptr_t _handTracking[2];
  bool _handTrackingSupported = false; // extension present + system supports it + trackers live
  mutable std::mutex _hand_mutex;      // guards _handTracking read (render/update) vs write (frame)

  // forward prediction: the host sets the tracked head pose + kinematics; the
  //  device extrapolates to scan-out (+ _predictionBias) in C++ at gpuUpdate and
  //  writes _posemap["hmd"]. Setting the "hmd" pose directly disables this.
  //  setTrackedPose stamps the receive time (Timer::getSystemTick, ns) internally;
  //  _predictHmdPose differences the predicted scan-out tick against it for the lead.
  //  linacc/angacc (optional, default 0) enable 2nd-order extrapolation; the host
  //  forwards the SDK-provided linear/angular acceleration (when the tracking source
  //  publishes it). Zero acc => 1st-order (constant velocity) — backward compatible.
  void setTrackedPose(const fvec3& pos, const fquat& orient, const fvec3& linvel, const fvec3& angvel,
                      const fvec3& linacc = fvec3(), const fvec3& angacc = fvec3());
  void _predictHmdPose();

  uint32_t _width      = 128;
  uint32_t _height     = 128;
  float _fov           = 90.0f;
  float _near          = .1f;
  float _far           = 1000.0;
  float _IPD           = 0.065f;
  int _calibstate      = 0;
  int _calibstateFrame = 0;

  bool _active                      = false;
  bool _supportsStereo              = false;
  // Per-scene toggle for publishing a depth layer to a runtime that supports depth-based
  //  reprojection. Data-driven (the VrDepthPublish scene param sets it via the ECS VR-preset
  //  block); a device that owns HMD presentation consults it before chaining depth. Default ON.
  bool _publishDepth                = true;
  float _stereoTileRotationDegreesL = 0.0f;
  float _stereoTileRotationDegreesR = 0.0f;

  std::map<int, controllerstate_ptr_t> _controllers;
  fmtx4 _hmd_trackingMatrix;
  fmtx4 _hmdMatrix;
  fmtx4 _rotMatrix;
  fmtx4 _baseMatrix;
  fmtx4 _outputViewOffsetMatrix;
  fvec2 _centerH;
  fvec2 _centerV;

  // forward-prediction state (set via setTrackedPose; consumed in _predictHmdPose).
  //  pos/orient/velocities are in the host's already-frame-corrected world space.
  bool  _trackedPoseValid = false;
  fvec3 _trackedPos;
  fquat _trackedQuat;
  fvec3 _trackedLinVel;
  fvec3 _trackedAngVel;
  fvec3 _trackedLinAcc;             // SDK linear accel (2nd-order term); 0 => 1st-order
  fvec3 _trackedAngAcc;             // SDK angular accel (2nd-order term); 0 => 1st-order
  uint64_t _trackedCaptureTick = 0; // Timer::getSystemTick (ns) when setTrackedPose was called
  float _predictionBias   = 0.0f;   // additional lead (s) on top of the scan-out prediction

  VrProjFrustumPar _frustumLeft;
  VrProjFrustumPar _frustumCenter;
  VrProjFrustumPar _frustumRight;

  lev2::inputgroup_ptr_t _hmdinputgroup;
  std::vector<fvec3> _calibposvect;
  std::vector<fvec3> _calibnxvect;
  std::vector<fvec3> _calibnyvect;
  std::vector<fvec3> _calibnzvect;

  bool _do_calibration = false;

  svar512_t _private;
  std::string _camera_name;

protected:
  controllerstate_ptr_t controller(int id);

private:
  Device(const Device& rhs) = delete;
};

using device_ptr_t = std::shared_ptr<Device>;

#if defined(ENABLE_LIBSURVIVE)

namespace libsurvive {

}

#endif

#if defined(ENABLE_OPENVR)
namespace openvr {

struct OpenVrDevice final : public Device {

  OpenVrDevice();
  ~OpenVrDevice() final;

  void _processControllerEvents();
  void _updatePoses();
  void _vrthread_loop();
  void gpuUpdate(RenderContextFrameData& RCFD) final;

  // void __gpuUpdate(RenderContextFrameData& RCFD);
  void __composite(Context* targ, Texture* twoeyetex) const final;
  void __compositeStereo(
      Context* targ, Texture* texL, Texture* texR, Texture* depthTexL, Texture* depthTexR) const final;

  _ovr::IVRSystem* _hmd;
  _ovr::TrackedDevicePose_t _trackedPoses[_ovr::k_unMaxTrackedDeviceCount];
  fmtx4 _poseMatrices[_ovr::k_unMaxTrackedDeviceCount];
  std::string _devclass[_ovr::k_unMaxTrackedDeviceCount];
  std::set<_ovr::TrackedDeviceIndex_t> _controllerindexset;
  int _rightControllerDeviceIndex = -1;
  int _leftControllerDeviceIndex  = -1;
  ork::Thread _vrthread;
  ork::mutex _vrmutex;
};
std::shared_ptr<OpenVrDevice> openvr_device();
} // namespace openvr
#endif

////////////////////////////////////////////////////////////////////////////////
namespace novr {
struct NoVrDevice final : public Device {
  NoVrDevice();
  ~NoVrDevice() final;
  void _processControllerEvents();
  void _updatePoses(RenderContextFrameData& RCFD);
  void gpuUpdate(RenderContextFrameData& RCFD) final;

  // void __gpuUpdate(RenderContextFrameData& RCFD);
  void __composite(Context* targ, Texture* twoeyetex) const final;
  void __compositeStereo(
      Context* targ, Texture* texL, Texture* texR, Texture* depthTexL, Texture* depthTexR) const final;

  msgrouter::subscriber_t _qtmousesubsc;
  msgrouter::subscriber_t _qtkbdownsubs;
  msgrouter::subscriber_t _qtkbupsubs;
  fvec2 _qtmousepos;
};
std::shared_ptr<NoVrDevice> novr_device();
} // namespace novr
////////////////////////////////////////////////////////////////////////////////

void setDevice(device_ptr_t device);

device_ptr_t device();

// Compose the CENTER head VIEW matrix from a rig/walker VIEW matrix (world->root)
//  and the tracked HMD pose, exactly as Device::_updatePosesCommon builds the
//  center-eye view the VR output nodes render: cmv = usermtx*base*hmd. Consumers
//  that need the head (not an eye) — the audio listener — go through here so the
//  composition has ONE definition.
//  Returns rig_view_matrix UNCHANGED when no device is publishing a head pose
//  (no device / inactive / no "hmd" entry): the non-VR path is bit-for-bit the
//  caller's own matrix, never an identity compose.
fmtx4 composeHeadViewMatrix(const fmtx4& rig_view_matrix);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
