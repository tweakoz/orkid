#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include <ork/kernel/environment.h>
#include <boost/filesystem.hpp>
#include <ork/profiling.inl>

#if defined(ENABLE_OPENVR)

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

static ork::LockedResource<VrTrackingNotificationReceiver_set> gnotifset;
void addVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) { notifset.insert(recvr); });
}
void removeVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) {
    auto it = notifset.find(recvr);
    OrkAssert(it != notifset.end());
    notifset.erase(it);
  });
}

fmtx4 steam34tofmtx4(const _ovr::HmdMatrix34_t& matPose) {
  fmtx4 orkmtx = fmtx4::Identity();
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      orkmtx.SetElemXY(j, i, matPose.m[i][j]);
  return orkmtx;
}
fmtx4 steam44tofmtx4(const _ovr::HmdMatrix44_t& matPose) {
  fmtx4 orkmtx;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      orkmtx.SetElemXY(j, i, matPose.m[i][j]);
  return orkmtx;
}

std::string trackedDeviceString(
    _ovr::TrackedDeviceIndex_t unDevice,
    _ovr::TrackedDeviceProperty prop,
    _ovr::TrackedPropertyError* peError = NULL) {
  uint32_t unRequiredBufferLen = _ovr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
  if (unRequiredBufferLen == 0)
    return "";

  char* pchBuffer     = new char[unRequiredBufferLen];
  unRequiredBufferLen = _ovr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
  std::string sResult = pchBuffer;
  delete[] pchBuffer;
  return sResult;
}

////////////////////////////////////////////////////////////////////////////////

fmtx4 VrProjFrustumPar::composeProjection() const {
  fmtx4 rval;
  float idx = 1.0f / (_right - _left);
  float idy = 1.0f / (_bottom - _top);
  float idz = 1.0f / (_far - _near);
  float sx  = _right + _left;
  float sy  = _bottom + _top;

  auto& p = rval.elements;
  p[0][0] = 2 * idx;
  p[0][1] = 0;
  p[0][2] = sx * idx;
  p[0][3] = 0;
  p[1][0] = 0;
  p[1][1] = 2 * idy;
  p[1][2] = sy * idy;
  p[1][3] = 0;
  p[2][0] = 0;
  p[2][1] = 0;
  p[2][2] = -_far * idz;
  p[2][3] = -_far * _near * idz;
  p[3][0] = 0;
  p[3][1] = 0;
  p[3][2] = -1.0f;
  p[3][3] = 0;
  rval.Transpose();
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

OpenVrDevice::OpenVrDevice()
    : _vrmutex("vrmutex") {

  _leftControllerDeviceIndex  = -1;
  _rightControllerDeviceIndex = -1;

  ///////////////////////////////////////////////////////////
  // override with controller.json
  ///////////////////////////////////////////////////////////

  std::string configpath;
  if (genviron.get("OVR_CONTROLLER_JSON", configpath)) {
    // we already have the config path...
  } else {
    genviron.get("OBT_STAGE", configpath);
    configpath = configpath + "/controller.json";
  }
  if (boost::filesystem::exists(configpath)) {
    FILE* fin = fopen(configpath.c_str(), "rt");
    assert(fin != nullptr);
    fseek(fin, 0, SEEK_END);
    int size = ftell(fin);
    printf("filesize<%d>\n", size);
    fseek(fin, 0, SEEK_SET);
    auto jsondata = (char*)malloc(size + 1);
    fread(jsondata, size, 1, fin);
    jsondata[size] = 0;
    fclose(fin);

    rapidjson::Document document;
    document.Parse(jsondata);
    free((void*)jsondata);
    assert(document.IsObject());
    assert(document.HasMember("ControllerIds"));
    const auto& root = document["ControllerIds"];
    //_leftControllerDeviceIndex  = root["left"].GetInt();
    //_rightControllerDeviceIndex = root["right"].GetInt();
    // printf("LeftId<%d>\n", _leftControllerDeviceIndex);
    // printf("RightId<%d>\n", _rightControllerDeviceIndex);
  }

  ///////////////////////////////////////////////////////////

  _vrthread.start([=](anyp arg) { this->_vrthread_loop(); });
}

////////////////////////////////////////////////////////////////////////////////

OpenVrDevice::~OpenVrDevice() {
  if (_hmd)
    _ovr::VR_Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void OpenVrDevice::_vrthread_loop() {

  _ovr::EVRInitError error = _ovr::VRInitError_None;
  _hmd                     = _ovr::VR_Init(&error, _ovr::VRApplication_Scene);
  _active                  = (error == _ovr::VRInitError_None);

  if (_active) {
    _supportsStereo = true;
    _hmd->GetRecommendedRenderTargetSize(&_width, &_height);
    printf("RECOMMENDED WH<%d %d>\n", _width, _height);
    auto str_driver  = trackedDeviceString(_ovr::k_unTrackedDeviceIndex_Hmd, _ovr::Prop_TrackingSystemName_String);
    auto str_display = trackedDeviceString(_ovr::k_unTrackedDeviceIndex_Hmd, _ovr::Prop_SerialNumber_String);
    printf("str_driver<%s>\n", str_driver.c_str());
    printf("str_driver<%s>\n", str_display.c_str());

    _hmd->GetProjectionRaw(_ovr::Eye_Left, &_frustumLeft._left, &_frustumLeft._right, &_frustumLeft._top, &_frustumLeft._bottom);

    _hmd->GetProjectionRaw(
        _ovr::Eye_Right, &_frustumRight._left, &_frustumRight._right, &_frustumRight._top, &_frustumRight._bottom);

    _frustumCenter._left   = (_frustumLeft._left + _frustumRight._left) * 0.5f;
    _frustumCenter._right  = (_frustumLeft._right + _frustumRight._right) * 0.5f;
    _frustumCenter._top    = (_frustumLeft._top + _frustumRight._top) * 0.5f;
    _frustumCenter._bottom = (_frustumLeft._bottom + _frustumRight._bottom) * 0.5f;

    auto proj_mtx_l = _frustumLeft.composeProjection();
    auto proj_mtx_r = _frustumRight.composeProjection();
    auto proj_mtx_c = _frustumCenter.composeProjection();

    auto eyep_mtx_l = steam34tofmtx4(_hmd->GetEyeToHeadTransform(_ovr::Eye_Left));
    auto eyep_mtx_r = steam34tofmtx4(_hmd->GetEyeToHeadTransform(_ovr::Eye_Right));

    _posemap["projl"] = proj_mtx_l;
    _posemap["projr"] = proj_mtx_r;
    _posemap["projc"] = proj_mtx_c;
    _posemap["eyel"].inverseOf(eyep_mtx_l);
    _posemap["eyer"].inverseOf(eyep_mtx_r);

  } else {
    printf("VR NOT INITIALIZED for some reason...\n");
  }

  while (true) {
    _processControllerEvents();
    ::usleep(100);
  }
} // namespace ork::lev2::orkidvr

////////////////////////////////////////////////////////////////////////////////

void OpenVrDevice::_processControllerEvents() {
  EASY_BLOCK("openvr-pce");
  _ovr::VREvent_t event;
  while (_active and _hmd->PollNextEvent(&event, sizeof(event))) {
    auto data  = event.data;
    auto ctrl  = data.controller;
    int button = ctrl.button;

    switch (event.eventType) {
      case _ovr::VREvent_TrackedDeviceDeactivated:
        printf("Device %u detached.\n", event.trackedDeviceIndex);
        break;
      case _ovr::VREvent_TrackedDeviceUpdated:
        break;
      case _ovr::VREvent_ButtonPress: {
        printf("dev<%d> button<%d> pressed\n", int(event.trackedDeviceIndex), button);
        auto c = controller(event.trackedDeviceIndex);
        _controllerindexset.insert(event.trackedDeviceIndex);
        switch (button) {
          case 1:
            c->_button1Down = true;
            break;
          case 2:
            c->_button2Down = true;
            break;
          case 32:
            c->_buttonThumbDown = true;
            break;
          case 33:
            c->_triggerDown = true;
            break;
        }
        break;
      }
      case _ovr::VREvent_ButtonUnpress: {
        printf("button<%d> released\n", button);
        auto c = controller(event.trackedDeviceIndex);
        _controllerindexset.insert(event.trackedDeviceIndex);
        switch (button) {
          case 1:
            c->_button1Down = false;
            break;
          case 2:
            c->_button2Down = false;
            break;
          case 32:
            c->_buttonThumbDown = false;
            break;
          case 33:
            c->_triggerDown = false;
            break;
        }
        break;
      }
      case _ovr::VREvent_ButtonTouch:
        printf("button<%d> touched\n", button);
        _controllerindexset.insert(event.trackedDeviceIndex);
        break;
      case _ovr::VREvent_ButtonUntouch:
        printf("button<%d> untouched\n", button);
        _controllerindexset.insert(event.trackedDeviceIndex);
        break;
      // case _ovr::VREvent_DualAnalog_Move: {
      // auto dualana = data.dualAnalog;
      // printf("dualanalog<%g %g> %d moved\n", dualana.x, dualana.y, int(dualana.which));
      //}
      default:
        // printf("unknown event<%d>\n", int(event.eventType));
        break;
    }
    ////////////////////////////////////////////////////////////////////////////

  } // while (_active and _hmd->PollNextEvent(&event, sizeof(event))) {
  //////////////////////////////////////////////
  if (_active) {
    ////////////////////////////////////////////////////////////////////////////
    fmtx4 rx, ry, rz, ivomatrix;
    rx.SetRotateX(-PI * 0.5);
    ry.SetRotateY(PI * 0.5);
    rz.SetRotateZ(PI * 0.5);
    ivomatrix.inverseOf(_outputViewOffsetMatrix);
    auto rotmtx         = (rx * ry * rz);
    auto tracking2world = [&](const fmtx4& input) -> fmtx4 { //
      return rotmtx * (input * ivomatrix) * _baseMatrix.inverse();
    };
    ////////////////////////////////////////////////////////////////////////////
    // left/right controller assignment heuristic
    ////////////////////////////////////////////////////////////////////////////

    for (auto& controller_item : _controllers) {
      int id                    = controller_item.first;
      auto controller           = controller_item.second;
      controller->_world_matrix = tracking2world(controller->_tracking_matrix);

      bool is_base = controller->_world_matrix == tracking2world(fmtx4());
      if (false == is_base) {
        if (controller->_association_state < 0) {
          fvec4 cpos = controller->_tracking_matrix.GetTranslation();
          fvec4 rpos = cpos.Transform(_hmd_trackingMatrix.inverse());
          float xpos = rpos.x;
          controller->_xwpos += xpos * 0.001;
          controller->_xwpos *= 0.9999;

          if (abs(controller->_xwpos) > 2.5) {
            controller->_association_state = 1;
            if (controller->_xwpos > 2.5) {
              _rightControllerDeviceIndex = id;
            } else if (controller->_xwpos < -2.5) {
              _leftControllerDeviceIndex = id;
            }
          }
          printf("updating controller<%d> xpos<%g>\n", id, controller->_xwpos);
          // printf("mtxstr<%s>\n", mtxstr.c_str());
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////
    // send input to input manager
    ////////////////////////////////////////////////////////////////////////////
    using inpmgr   = lev2::InputManager;
    auto handgroup = inpmgr::instance()->inputGroup("hands");
    if (_leftControllerDeviceIndex >= 0) {
      auto LCONTROLLER = controller(_leftControllerDeviceIndex);
      handgroup->setChannel("left.button1").as<bool>(LCONTROLLER->_button1Down);
      handgroup->setChannel("left.button2").as<bool>(LCONTROLLER->_button2Down);
      handgroup->setChannel("left.trigger").as<bool>(LCONTROLLER->_triggerDown);
      handgroup->setChannel("left.thumb").as<bool>(LCONTROLLER->_buttonThumbDown);
      handgroup->setChannel("left.matrix").as<fmtx4>(LCONTROLLER->_world_matrix);
    }

    if (_rightControllerDeviceIndex >= 0) {
      auto RCONTROLLER = controller(_rightControllerDeviceIndex);
      handgroup->setChannel("right.button1").as<bool>(RCONTROLLER->_button1Down);
      handgroup->setChannel("right.button2").as<bool>(RCONTROLLER->_button2Down);
      handgroup->setChannel("right.trigger").as<bool>(RCONTROLLER->_triggerDown);
      handgroup->setChannel("right.thumb").as<bool>(RCONTROLLER->_buttonThumbDown);
      handgroup->setChannel("right.matrix").as<fmtx4>(RCONTROLLER->_world_matrix);
    }
  }
  //////////////////////////////////////////////
  if (_active) {
    _ovr::VRActionSetHandle_t actset_demo = _ovr::k_ulInvalidActionSetHandle;
    _ovr::VRActiveActionSet_t actionSet   = {0};
    actionSet.ulActionSet                 = actset_demo;
    // _ovr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );

    using inpmgr   = lev2::InputManager;
    auto handgroup = inpmgr::instance()->inputGroup("hands");

    ork::svar256_t notifvar;
    auto ctrlnotiffram = notifvar.Make<VrTrackingControllerNotificationFrame>();

    if (_leftControllerDeviceIndex >= 0) {
      auto LCONTROLLER = controller(_leftControllerDeviceIndex);
      LCONTROLLER->updateGated();
      *(ctrlnotiffram._left) = *LCONTROLLER;
    }

    if (_rightControllerDeviceIndex >= 0) {
      auto RCONTROLLER = controller(_rightControllerDeviceIndex);
      RCONTROLLER->updateGated();
      *(ctrlnotiffram._right) = *RCONTROLLER;
    }

    gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) {
      for (auto recvr : notifset) {
        recvr->_callback(notifvar);
      }
    });
  }
}

//////////////////////////////////////////////
VrTrackingControllerNotificationFrame::VrTrackingControllerNotificationFrame() {
  _left  = std::make_shared<ControllerState>();
  _right = std::make_shared<ControllerState>();
}

////////////////////////////////////////////////////////////////////////////////

void composite(lev2::Context* targ, Texture* twoeyetex) {

  if (device()._active) {

    auto fbi = targ->FBI();

    auto twoeyetexOBJ = twoeyetex->_varmap.typedValueForKey<GLuint>("gltexobj").value();

    _ovr::Texture_t twoEyeTexture = {(void*)(uintptr_t)twoeyetexOBJ, _ovr::TextureType_OpenGL, _ovr::ColorSpace_Gamma};

    _ovr::VRTextureBounds_t leftEyeBounds = {
        0,
        0, // min
        0.5,
        1 // max
    };
    _ovr::VRTextureBounds_t rightEyeBounds = {
        0.5,
        0, // min
        1,
        1 // max
    };

    //////////////////////////////////////////////////

    int w = device()._width;
    int h = device()._height;
    ViewportRect VPRect(0, 0, w * 2, h);
    fbi->pushViewport(VPRect);
    fbi->pushScissor(VPRect);

    //////////////////////////////////////////////////
    // submit to openvr compositor
    //////////////////////////////////////////////////

    GLuint erl = _ovr::VRCompositor()->Submit(_ovr::Eye_Left, &twoEyeTexture, &leftEyeBounds);

    GLuint err = _ovr::VRCompositor()->Submit(_ovr::Eye_Right, &twoEyeTexture, &rightEyeBounds);

    //////////////////////////////////////////////////
    // undo above PushVp/Scissor
    //////////////////////////////////////////////////

    fbi->popViewport();
    fbi->popScissor();
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenVrDevice::_updatePoses() {
  EASY_BLOCK("openvr-upd");

  if (_active) {

    // we call this on rendering thread, I suppose since
    // the vrcompositor needs the absolute latest pose
    // for the sake of reducing tracking latency
    {
      EASY_BLOCK("openvr-upd-w");

      _ovr::VRCompositor()->WaitGetPoses(_trackedPoses, _ovr::k_unMaxTrackedDeviceCount, NULL, 0);
    }
    int validposecount       = 0;
    std::string pose_classes = "";

    for (int dev_index = 0; dev_index < _ovr::k_unMaxTrackedDeviceCount; dev_index++) {
      if (_trackedPoses[dev_index].bPoseIsValid) {

        ///////////////////////////////////////////////////////
        // discover left and right controller device indices
        ///////////////////////////////////////////////////////

        _ovr::TrackedPropertyError tpe;
        int32_t role = _ovr::VRSystem()->GetInt32TrackedDeviceProperty(
            dev_index, _ovr::ETrackedDeviceProperty::Prop_ControllerRoleHint_Int32, &tpe);

        switch (role) {
          case _ovr::ETrackedControllerRole::TrackedControllerRole_LeftHand: {
            _leftControllerDeviceIndex = dev_index;
            break;
          }
          case _ovr::ETrackedControllerRole::TrackedControllerRole_RightHand: {
            _rightControllerDeviceIndex = dev_index;
            break;
          }
        }

        ///////////////////////////////////////////////////////

        validposecount++;
        auto orkmtx = steam34tofmtx4(_trackedPoses[dev_index].mDeviceToAbsoluteTracking);
        fmtx4 inverse;
        fmtx4 transpose = orkmtx;
        transpose.Transpose();
        inverse.inverseOf(orkmtx);
        _poseMatrices[dev_index] = orkmtx;
        // if (vrimpl->_devclass[dev_index]==0){
        switch (_hmd->GetTrackedDeviceClass(dev_index)) {
          case _ovr::TrackedDeviceClass_Controller:
            _devclass[dev_index]                    = 'C';
            controller(dev_index)->_tracking_matrix = orkmtx;
            break;
          case _ovr::TrackedDeviceClass_HMD:
            _devclass[dev_index] = 'H';
            _hmd_trackingMatrix  = orkmtx;
            break;
          case _ovr::TrackedDeviceClass_Invalid:
            _devclass[dev_index] = 'I';
            break;
          case _ovr::TrackedDeviceClass_GenericTracker:
            _devclass[dev_index] = 'G';
            break;
          case _ovr::TrackedDeviceClass_TrackingReference:
            _devclass[dev_index] = 'T';
            break;
          default:
            _devclass[dev_index] = '?';
            break;
        }
        //}
        pose_classes += _devclass[dev_index];
      }
    }

    ////////////////////////////////////////////////////////////////////////////

    if (_trackedPoses[_ovr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
      fmtx4 hmdmatrix;
      hmdmatrix.inverseOf(_poseMatrices[_ovr::k_unTrackedDeviceIndex_Hmd]);
      _posemap["hmd"] = hmdmatrix;
      _hmdinputgroup->setChannel("hmdmatrix").as<fmtx4>(hmdmatrix);

      ork::svar256_t notifvar;
      auto& hmdnotiffram      = notifvar.Make<VrTrackingHmdPoseNotificationFrame>();
      hmdnotiffram._hmdMatrix = _poseMatrices[_ovr::k_unTrackedDeviceIndex_Hmd];
      EASY_BLOCK("openvr-aop");
      gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) {
        for (auto recvr : notifset) {
          recvr->_callback(notifvar);
        }
      });
    }

    _updatePosesCommon();
  }
}

////////////////////////////////////////////////////////////////////////////////
OpenVrDevice& concrete_get() {
  static OpenVrDevice _device;
  return _device;
}
Device& device() {
  return concrete_get();
}
////////////////////////////////////////////////////////////////////////////////

void gpuUpdate(RenderContextFrameData& RCFD) {
  EASY_BLOCK("openvr-gpuUpdate");
  auto& mgr = concrete_get();
  if (mgr._active) {
    bool ovr_compositor_ok = (bool)_ovr::VRCompositor();
    assert(ovr_compositor_ok);
  }
  mgr._updatePoses();
}

////////////////////////////////////////////////////////////////////////////////

void ControllerState::updateGated() {
  _button1GatedDown     = _button1Down and (not _button1DownPrev);
  _button2GatedDown     = _button2Down and (not _button2DownPrev);
  _buttonThumbGatedDown = _buttonThumbDown and (not _buttonThumbDownPrev);
  _triggerGatedDown     = _triggerDown and (not _triggerDownPrev);

  _button1GatedUp     = _button1DownPrev and (not _button1Down);
  _button2GatedUp     = _button2DownPrev and (not _button2Down);
  _buttonThumbGatedUp = _buttonThumbDownPrev and (not _buttonThumbDown);
  _triggerGatedUp     = _triggerDownPrev and (not _triggerDown);

  _button1DownPrev     = _button1Down;
  _button2DownPrev     = _button2Down;
  _buttonThumbDownPrev = _buttonThumbDown;
  _triggerDownPrev     = _triggerDown;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
#endif // #if defined(ENABLE_OPENVR)
