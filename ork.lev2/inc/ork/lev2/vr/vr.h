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

#if defined(ENABLE_LIBSURVIVE)
#include <libsurvive/survive_api.h>
#endif
#if defined(ENABLE_OPENVR)
#include <openvr/openvr.h>
namespace _ovr = ::vr;
#endif

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

struct Device {
  Device();
  virtual ~Device();
  void _updatePosesCommon();

  virtual void gpuUpdate(RenderContextFrameData& RCFD)              = 0;
  virtual void __composite(Context* targ, Texture* twoeyetex) const = 0;

  std::map<std::string, fmtx4> _posemap;
  CameraMatrices* _leftcamera       = nullptr;
  CameraMatrices* _centercamera     = nullptr;
  CameraMatrices* _rightcamera      = nullptr;
  usermatrixgenerator_t _usermtxgen = nullptr;

  void overrideSize(int w, int h);
  void resetCalibration();

  uint32_t _width      = 128;
  uint32_t _height     = 128;
  float _fov           = 45.0f;
  float _near          = .01f;
  float _far           = 10000.0;
  float _IPD           = 0.0f;
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
  std::string _cameraName;

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
