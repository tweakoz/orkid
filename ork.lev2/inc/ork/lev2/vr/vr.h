////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/input/inputdevice.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/lev2_types.h>
#include <ork/orktypes.h>

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
  bool  _poseConjugate    = true;   // conjugate orient before composing the world matrix

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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
