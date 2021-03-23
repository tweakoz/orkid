#pragma once

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/input/inputdevice.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/mutex.h>

#if !defined(ORK_OSX)
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

////////////////////////////////////////////////////////////////////////////////

typedef std::function<fmtx4()> usermatrixgenerator_t;

struct Device {
  Device();
  virtual ~Device();
  void _updatePosesCommon();

  std::map<std::string, fmtx4> _posemap;
  CameraMatrices* _leftcamera   = nullptr;
  CameraMatrices* _centercamera = nullptr;
  CameraMatrices* _rightcamera  = nullptr;
  std::map<int, controllerstate_ptr_t> _controllers;
  bool _active;
  bool _supportsStereo;
  fmtx4 _hmd_trackingMatrix;
  fmtx4 _hmdMatrix;
  fmtx4 _rotMatrix;
  fmtx4 _baseMatrix;

  usermatrixgenerator_t _usermtxgen;

  VrProjFrustumPar _frustumLeft;
  VrProjFrustumPar _frustumCenter;
  VrProjFrustumPar _frustumRight;

  fmtx4 _outputViewOffsetMatrix;
  lev2::inputgroup_ptr_t _hmdinputgroup;
  uint32_t _width, _height;
  svar512_t _private;
  int _calibstate;
  int _calibstateFrame;
  std::vector<fvec3> _calibposvect;
  std::vector<fvec3> _calibnxvect;
  std::vector<fvec3> _calibnyvect;
  std::vector<fvec3> _calibnzvect;

protected:
  controllerstate_ptr_t controller(int id);

private:
  Device(const Device& rhs) = delete;
};

////////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_OPENVR)
////////////////////////////////////////////////////////////////////////////////

struct OpenVrDevice final : public Device {

  OpenVrDevice();
  ~OpenVrDevice();

  void _processControllerEvents();
  void _updatePoses();
  void _vrthread_loop();

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
void composite(Context* targ, Texture* twoeyetex);
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
