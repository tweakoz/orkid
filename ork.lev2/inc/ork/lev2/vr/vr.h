#pragma once

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/input/inputdevice.h>
#include <ork/math/cmatrix4.h>

#if !defined(__APPLE__)
#define ENABLE_OPENVR
#include <openvr/openvr.h>
namespace _ovr = ::vr;
#endif

////////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class Texture;
}

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

struct ControllerState {
  fmtx4 _matrix;
  bool _button1down     = false;
  bool _button2down     = false;
  bool _buttonThumbdown = false;
  bool _triggerDown     = false;
};

////////////////////////////////////////////////////////////////////////////////

struct VrProjFrustumPar {

  fmtx4 composeProjection() const;

  float _left = -1.0f;
  float _right = 1.0f;
  float _top = -1.0f;
  float _bottom = 1.0f;
  float _near = .1f;
  float _far = 50000.0f;
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
  ControllerState _left;
  ControllerState _right;
};

struct VrTrackingNotificationReceiver {
  typedef std::function<void(const svar256_t &)> callback_t;
  callback_t _callback;
};
typedef std::shared_ptr<VrTrackingNotificationReceiver> VrTrackingNotificationReceiver_ptr_t;
typedef std::set<VrTrackingNotificationReceiver_ptr_t> VrTrackingNotificationReceiver_set;

void addVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr);

////////////////////////////////////////////////////////////////////////////////

struct Device {
  Device();
  virtual ~Device();
  std::map<std::string, fmtx4> _posemap;
  CameraMatrices* _leftcamera = nullptr;
  CameraMatrices* _centercamera = nullptr;
  CameraMatrices* _rightcamera = nullptr;
  std::map<int, ControllerState> _controllers;
  bool _active;
  bool _supportsStereo;
  fmtx4 _offsetmatrix;
  fmtx4 _headingmatrix;
  fmtx4 _hmdMatrix;
  fmtx4 _rotMatrix;

  VrProjFrustumPar _frustumLeft;
  VrProjFrustumPar _frustumCenter;
  VrProjFrustumPar _frustumRight;

  fmtx4 _outputViewOffsetMatrix;
  bool _prevthumbL = false;
  bool _prevthumbR = false;
  lev2::InputGroup& _hmdinputgroup;
  uint32_t _width, _height;
  void _updatePosesCommon(fmtx4 observermatrix);
  svar512_t _private;
};

////////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_OPENVR)
////////////////////////////////////////////////////////////////////////////////

struct OpenVrDevice : public Device {

  OpenVrDevice();
  ~OpenVrDevice() final;

  _ovr::IVRSystem* _hmd;
  _ovr::TrackedDevicePose_t _trackedPoses[_ovr::k_unMaxTrackedDeviceCount];
  fmtx4 _poseMatrices[_ovr::k_unMaxTrackedDeviceCount];
  std::string _devclass[_ovr::k_unMaxTrackedDeviceCount];
  std::set<_ovr::TrackedDeviceIndex_t> _controllerindexset;
  int _rightControllerDeviceIndex = -1;
  int _leftControllerDeviceIndex  = -1;

  void _processControllerEvents();
  void _updatePoses(fmtx4 observermatrix);
};
#else
////////////////////////////////////////////////////////////////////////////////
struct NoVrDevice : public Device {
  NoVrDevice();
  ~NoVrDevice() final;
  void _processControllerEvents();
  void _updatePoses(RenderContextFrameData& RCFD);
  msgrouter::subscriber_t _qtmousesubsc;
  msgrouter::subscriber_t _qtkbdownsubs;
  msgrouter::subscriber_t _qtkbupsubs;
  fvec2 _qtmousepos;
};
#endif
////////////////////////////////////////////////////////////////////////////////
Device& device();
////////////////////////////////////////////////////////////////////////////////
void gpuUpdate(RenderContextFrameData& RCFD);
void composite(GfxTarget* targ, Texture* twoeyetex);
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
