#pragma once

#include <ork/lev2/input/inputdevice.h>
#include <ork/math/cmatrix4.h>
#include <ork/gfx/camera.h>
#include <ork/kernel/msgrouter.inl>

#if ! defined(__APPLE__)
#include <openvr/openvr.h>
#define ENABLE_VR
#endif

namespace ork::lev2 {
  class Texture;
}

namespace ork::lev2::openvr {

  struct ControllerState {
    fmtx4 _matrix;
    bool _button1down = false;
    bool _button2down = false;
    bool _buttonThumbdown = false;
    bool _triggerDown = false;
  };

  struct Manager {

    Manager();
    ~Manager();


    #if defined(ENABLE_VR)
      vr::IVRSystem* _hmd;
      vr::TrackedDevicePose_t _trackedPoses[vr::k_unMaxTrackedDeviceCount];
      fmtx4 _poseMatrices[vr::k_unMaxTrackedDeviceCount];
      std::string _devclass[vr::k_unMaxTrackedDeviceCount];
      std::set<vr::TrackedDeviceIndex_t> _controllerindexset;
      int _rightControllerDeviceIndex = -1;
      int _leftControllerDeviceIndex = -1;
    #endif

    std::map<std::string, fmtx4> _posemap;
    CameraData _leftcamera;
    CameraData _rightcamera;
    std::map<int, ControllerState> _controllers;
    bool _active;
    fmtx4 _offsetmatrix;
    fmtx4 _headingmatrix;
    fmtx4 _hmdMatrix;
    bool _prevthumbL = false;
    bool _prevthumbR = false;
    msgrouter::subscriber_t _qtmousesubsc;
    msgrouter::subscriber_t _qtkbdownsubs;
    msgrouter::subscriber_t _qtkbupsubs;
    fvec2 _qtmousepos;
    lev2::InputGroup& _hmdinputgroup;
    uint32_t _width, _height;

  private:

      void _processControllerEvents(lev2::GfxTarget* targ);
      void _updatePoses(lev2::GfxTarget* targ);

  };

  Manager& get();
  void gpuUpdate(GfxTarget* targ);
  void composite(GfxTarget* targ,Texture* ltex, Texture* rtex);

} // namespace ork::lev2::openvr {
