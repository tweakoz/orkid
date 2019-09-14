#pragma once

#include <ork/gfx/camera.h>
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

struct Device {
  Device();
  virtual ~Device();
  std::map<std::string, fmtx4> _posemap;
  CameraData _leftcamera;
  CameraData _rightcamera;
  std::map<int, ControllerState> _controllers;
  bool _active;
  fmtx4 _offsetmatrix;
  fmtx4 _headingmatrix;
  fmtx4 _hmdMatrix;
  fmtx4 _rotMatrix;
  fmtx4 _outputViewOffsetMatrix;
  bool _prevthumbL = false;
  bool _prevthumbR = false;
  lev2::InputGroup& _hmdinputgroup;
  uint32_t _width, _height;
  void _updatePosesCommon(fmtx4 observermatrix);
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
  void _updatePoses(fmtx4 observermatrix);
  msgrouter::subscriber_t _qtmousesubsc;
  msgrouter::subscriber_t _qtkbdownsubs;
  msgrouter::subscriber_t _qtkbupsubs;
  fvec2 _qtmousepos;
};
#endif
////////////////////////////////////////////////////////////////////////////////
Device& device();
////////////////////////////////////////////////////////////////////////////////
void gpuUpdate(fmtx4 observermatrix);
void composite(GfxTarget* targ, Texture* ltex, Texture* rtex);
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
