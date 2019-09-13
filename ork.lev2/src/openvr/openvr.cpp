#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/openvr/openvr.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::openvr {
////////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_VR)

fmtx4 steam34tofmtx4(const vr::HmdMatrix34_t& matPose) {
  fmtx4 orkmtx = fmtx4::Identity;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      orkmtx.SetElemXY(j, i, matPose.m[i][j]);
  return orkmtx;
}
fmtx4 steam44tofmtx4(const vr::HmdMatrix44_t& matPose) {
  fmtx4 orkmtx;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      orkmtx.SetElemXY(j, i, matPose.m[i][j]);
  return orkmtx;
}

std::string
trackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError = NULL) {
  uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
  if (unRequiredBufferLen == 0)
    return "";

  char* pchBuffer     = new char[unRequiredBufferLen];
  unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
  std::string sResult = pchBuffer;
  delete[] pchBuffer;
  return sResult;
}
#endif

////////////////////////////////////////////////////////////////////////////////

Manager& get() {
  static Manager _instance;
  return _instance;
}

////////////////////////////////////////////////////////////////////////////////

Manager::Manager()
    : _width(1024)
    , _height(1024)
    , _active(false)
    , _hmdinputgroup(*lev2::InputManager::inputGroup("hmd")) {
#if defined(ENABLE_VR)
  vr::EVRInitError error = vr::VRInitError_None;
  _hmd                   = vr::VR_Init(&error, vr::VRApplication_Scene);
  _active                = (error == vr::VRInitError_None);

  if (_active) {
    _hmd->GetRecommendedRenderTargetSize(&_width, &_height);
    printf("RECOMMENDED WH<%d %d>\n", _width, _height);
    auto str_driver  = trackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
    auto str_display = trackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
    printf("str_driver<%s>\n", str_driver.c_str());
    printf("str_driver<%s>\n", str_display.c_str());
    auto proj_mtx_l = steam44tofmtx4(_hmd->GetProjectionMatrix(vr::Eye_Left, .1, 50000.0f));
    auto proj_mtx_r = steam44tofmtx4(_hmd->GetProjectionMatrix(vr::Eye_Right, .1, 50000.0f));
    auto eyep_mtx_l = steam34tofmtx4(_hmd->GetEyeToHeadTransform(vr::Eye_Left));
    auto eyep_mtx_r = steam34tofmtx4(_hmd->GetEyeToHeadTransform(vr::Eye_Right));

    _posemap["projl"] = proj_mtx_l;
    _posemap["projr"] = proj_mtx_r;
    _posemap["eyel"].inverseOf(eyep_mtx_l);
    _posemap["eyer"].inverseOf(eyep_mtx_r);

  } else {
    printf("VR NOT INITIALIZED for some reason...\n");
  }

  auto handgroup = lev2::InputManager::inputGroup("hands");
  handgroup->setChannel("left.button1").as<bool>(false);
  handgroup->setChannel("left.button2").as<bool>(false);
  handgroup->setChannel("left.trigger").as<bool>(false);
  handgroup->setChannel("left.thumb").as<bool>(false);

  handgroup->setChannel("right.button1").as<bool>(false);
  handgroup->setChannel("right.button2").as<bool>(false);
  handgroup->setChannel("right.trigger").as<bool>(false);
  handgroup->setChannel("right.thumb").as<bool>(false);

#else
  _qtmousesubsc = msgrouter::channel("qtmousepos")->subscribe([this](msgrouter::content_t c) { _qtmousepos = c.Get<fvec2>(); });

  _qtkbdownsubs = msgrouter::channel("qtkeyboard.down")->subscribe([this, handgroup](msgrouter::content_t c) {
    int key = c.Get<int>();
    switch (key) {
      case 'w':
        handgroup->setChannel("left.trigger").as<bool>(true);
        break;
      case 'a':
        handgroup->setChannel("left.thumb").as<bool>(true);
        break;
      case 's':
        break;
      case 'd':
        handgroup->setChannel("right.thumb").as<bool>(true);
        break;
    }
  });
  _qtkbupsubs   = msgrouter::channel("qtkeyboard.up")->subscribe([this, handgroup](msgrouter::content_t c) {
    int key = c.Get<int>();
    switch (key) {
      case 'w':
        handgroup->setChannel("left.trigger").as<bool>(false);
        break;
      case 'a':
        handgroup->setChannel("left.thumb").as<bool>(false);
        break;
      case 's':
        break;
      case 'd':
        handgroup->setChannel("right.thumb").as<bool>(false);
        break;
    }
  });

  _posemap["projl"].Perspective(45, 16.0 / 9.0, .1, 100000);
  _posemap["projr"].Perspective(45, 16.0 / 9.0, .1, 100000);
  _posemap["eyel"] = fmtx4::Identity;
  _posemap["eyer"] = fmtx4::Identity;
#endif

  _leftcamera.SetWidth(_width);
  _leftcamera.SetHeight(_height);
  _rightcamera.SetWidth(_width);
  _rightcamera.SetHeight(_height);
}

////////////////////////////////////////////////////////////////////////////////

Manager::~Manager() {
#if defined(ENABLE_VR)
  if (_hmd)
    vr::VR_Shutdown();
#endif
}

////////////////////////////////////////////////////////////////////////////////

void Manager::_processControllerEvents(lev2::GfxTarget* targ) {
  vr::VREvent_t event;
  while (_active and _hmd->PollNextEvent(&event, sizeof(event))) {
    auto data  = event.data;
    auto ctrl  = data.controller;
    int button = ctrl.button;

    switch (event.eventType) {
      case vr::VREvent_TrackedDeviceDeactivated:
        printf("Device %u detached.\n", event.trackedDeviceIndex);
        break;
      case vr::VREvent_TrackedDeviceUpdated:
        break;
      case vr::VREvent_ButtonPress: {
        printf("dev<%d> button<%d> pressed\n", int(event.trackedDeviceIndex), button);
        auto& c = _controllers[event.trackedDeviceIndex];
        _controllerindexset.insert(event.trackedDeviceIndex);
        switch (button) {
          case 1:
            c._button1down = true;
            break;
          case 2:
            c._button2down = true;
            break;
          case 32:
            c._buttonThumbdown = true;
            break;
          case 33:
            c._triggerDown = true;
            break;
        }
        break;
      }
      case vr::VREvent_ButtonUnpress: {
        printf("button<%d> released\n", button);
        auto& c = _controllers[event.trackedDeviceIndex];
        _controllerindexset.insert(event.trackedDeviceIndex);
        switch (button) {
          case 1:
            c._button1down = false;
            break;
          case 2:
            c._button2down = false;
            break;
          case 32:
            c._buttonThumbdown = false;
            break;
          case 33:
            c._triggerDown = false;
            break;
        }
        break;
      }
      case vr::VREvent_ButtonTouch:
        printf("button<%d> touched\n", button);
        _controllerindexset.insert(event.trackedDeviceIndex);
        break;
      case vr::VREvent_ButtonUntouch:
        printf("button<%d> untouched\n", button);
        _controllerindexset.insert(event.trackedDeviceIndex);
        break;
      case vr::VREvent_DualAnalog_Move: {
        auto dualana = data.dualAnalog;
        printf("dualanalog<%d> untouched\n", dualana.x, dualana.y, int(dualana.which));
      }
      default:
        printf("unknown event<%d>\n", int(event.eventType));
        break;
    }
#if defined(ENABLE_VR)
    if (_rightControllerDeviceIndex >= 0 and _leftControllerDeviceIndex >= 0) {

      using inpmgr    = lev2::InputManager;
      auto& handgroup = *inpmgr::inputGroup("hands");

      auto& LCONTROLLER = _controllers[_leftControllerDeviceIndex];
      auto& RCONTROLLER = _controllers[_rightControllerDeviceIndex];

      handgroup.setChannel("left.button1").as<bool>(LCONTROLLER._button1down);
      handgroup.setChannel("left.button2").as<bool>(LCONTROLLER._button2down);
      handgroup.setChannel("left.trigger").as<bool>(LCONTROLLER._triggerDown);
      handgroup.setChannel("left.thumb").as<bool>(LCONTROLLER._buttonThumbdown);

      handgroup.setChannel("right.button1").as<bool>(RCONTROLLER._button1down);
      handgroup.setChannel("right.button2").as<bool>(RCONTROLLER._button2down);
      handgroup.setChannel("right.trigger").as<bool>(RCONTROLLER._triggerDown);
      handgroup.setChannel("right.thumb").as<bool>(RCONTROLLER._buttonThumbdown);

      //////////////////////////////////////
      // hand positions
      //////////////////////////////////////

      fmtx4 rx, ry, rz, ivomatrix;
      rx.SetRotateX(-PI * 0.5);
      ry.SetRotateY(PI * 0.5);
      rz.SetRotateZ(PI * 0.5);
      ivomatrix.inverseOf(_frametek->_viewOffsetMatrix);
      fmtx4 lworld = (LCONTROLLER._matrix * ivomatrix);
      fmtx4 rworld = (RCONTROLLER._matrix * ivomatrix);
      handgroup.setChannel("left.matrix").as<fmtx4>(rx * ry * rz * lworld);
      handgroup.setChannel("right.matrix").as<fmtx4>(rx * ry * rz * rworld);

      //////////////////////////////////////

      fmtx4 xlate;
      float xlaterate = 12.0 / 80.0;

      if (LCONTROLLER._button1down) {
        xlate.SetTranslation(0, xlaterate, 0);
        auto trans = (xlate * rotmtx).GetTranslation();
        printf("trans<%g %g %g>\n", trans.x, trans.y, trans.z);
        xlate.SetTranslation(trans);
        _offsetmatrix = _offsetmatrix * xlate;
      }
      if (LCONTROLLER._button2down) {
        xlate.SetTranslation(0, -xlaterate, 0);
        auto trans = (xlate * rotmtx).GetTranslation();
        printf("trans<%g %g %g>\n", trans.x, trans.y, trans.z);
        xlate.SetTranslation(trans);
        _offsetmatrix = _offsetmatrix * xlate;
      }
      ///////////////////////////////////////////////////////////
      // fwd back
      ///////////////////////////////////////////////////////////
      if (RCONTROLLER._button1down) {
        xlate.SetTranslation(0, 0, xlaterate);
        auto trans = (xlate * rotmtx).GetTranslation();
        xlate.SetTranslation(trans);
        _offsetmatrix = _offsetmatrix * xlate;
      }
      if (RCONTROLLER._button2down) {
        xlate.SetTranslation(0, 0, -xlaterate);
        auto trans = (xlate * rotmtx).GetTranslation();
        xlate.SetTranslation(trans);
        _offsetmatrix = _offsetmatrix * xlate;
      }
      bool curthumbL = LCONTROLLER._buttonThumbdown;
      bool curthumbR = RCONTROLLER._buttonThumbdown;
      ///////////////////////////////////////////////////////////
      // turn left,right ( we rotate in discrete steps here, because it causes eye strain otherwise)
      ///////////////////////////////////////////////////////////

      if (curthumbL and false == _prevthumbL) {

        fquat q;
        q.FromAxisAngle(fvec4(0, 1, 0, PI / 12.0));
        _headingmatrix = _headingmatrix * q.ToMatrix();
      } else if (curthumbR and false == _prevthumbR) {
        fquat q;
        q.FromAxisAngle(fvec4(0, 1, 0, -PI / 12.0));
        _headingmatrix = _headingmatrix * q.ToMatrix();
      }
      _prevthumbL = curthumbL;
      _prevthumbR = curthumbR;
    } // if( _rightControllerDeviceIndex>=0 and _leftControllerDeviceIndex>=0 ){
#else

    bool curthumbL = handgroup.tryAs<bool>("left.thumb").value();
    bool curthumbR = handgroup.tryAs<bool>("right.thumb").value();
    ///////////////////////////////////////////////////////////
    // turn left,right ( we rotate in discrete steps here, because it causes eye strain otherwise)
    ///////////////////////////////////////////////////////////

    if (curthumbL and false == _prevthumbL) {

      fquat q;
      q.FromAxisAngle(fvec4(0, 1, 0, PI / 12.0));
      _headingmatrix = _headingmatrix * q.ToMatrix();
    } else if (curthumbR and false == _prevthumbR) {
      fquat q;
      q.FromAxisAngle(fvec4(0, 1, 0, -PI / 12.0));
      _headingmatrix = _headingmatrix * q.ToMatrix();
    }
    _prevthumbL = curthumbL;
    _prevthumbR = curthumbR;

#endif
  }

  //////////////////////////////////////////////
  if (_active) {
    vr::VRActionSetHandle_t actset_demo = vr::k_ulInvalidActionSetHandle;
    vr::VRActiveActionSet_t actionSet   = {0};
    actionSet.ulActionSet               = actset_demo;
    // vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );
  }
  //////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////

void composite(lev2::GfxTarget* targ, Texture* ltex, Texture* rtex) {

  auto& mgr = get();

  if (mgr._active) {

    auto fbi = targ->FBI();

    auto texobjL = ltex->getProperty<GLuint>("gltexobj");
    auto texobjR = rtex->getProperty<GLuint>("gltexobj");

    vr::Texture_t leftEyeTexture  = {(void*)(uintptr_t)texobjL, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::Texture_t rightEyeTexture = {(void*)(uintptr_t)texobjR, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};

    //////////////////////////////////////////////////
    // odd that you need to set the viewport
    //  before submitting to the vr compositor,
    //  since the texture contains the size of itself...
    //////////////////////////////////////////////////

    SRect VPRect(0, 0, mgr._width, mgr._height);
    fbi->PushViewport(VPRect);
    fbi->PushScissor(VPRect);

    //////////////////////////////////////////////////
    // submit to openvr compositor
    //////////////////////////////////////////////////

    GLuint erl = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
    GLuint err = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);

    //////////////////////////////////////////////////
    // undo above PushVp/Scissor
    //////////////////////////////////////////////////

    fbi->PopViewport();
    fbi->PopScissor();
  }
}

////////////////////////////////////////////////////////////////////////////////

void Manager::_updatePoses(lev2::GfxTarget* targ) {

#if defined(ENABLE_VR)
  if (_active and _hmd->IsInputAvailable()) {

    // we call this on rendering thread, I suppose since
    // the vrcompositor needs the absolute latest pose
    // for the sake of reducing tracking latency

    vr::VRCompositor()->WaitGetPoses(vrimpl->_trackedPoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    int validposecount       = 0;
    std::string pose_classes = "";

    for (int dev_index = 0; dev_index < vr::k_unMaxTrackedDeviceCount; dev_index++) {
      if (vrimpl->_trackedPoses[dev_index].bPoseIsValid) {

        ///////////////////////////////////////////////////////
        // discover left and right controller device indices
        ///////////////////////////////////////////////////////

        vr::TrackedPropertyError tpe;
        int32_t role = vr::VRSystem()->GetInt32TrackedDeviceProperty(
            dev_index, vr::ETrackedDeviceProperty::Prop_ControllerRoleHint_Int32, &tpe);

        switch (role) {
          case vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
            vrimpl->_leftControllerDeviceIndex = dev_index;
            break;
          case vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
            vrimpl->_rightControllerDeviceIndex = dev_index;
            break;
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
          case vr::TrackedDeviceClass_Controller:
            _devclass[dev_index]            = 'C';
            _controllers[dev_index]._matrix = orkmtx;
            break;
          case vr::TrackedDeviceClass_HMD:
            _devclass[dev_index] = 'H';
            break;
          case vr::TrackedDeviceClass_Invalid:
            _devclass[dev_index] = 'I';
            break;
          case vr::TrackedDeviceClass_GenericTracker:
            _devclass[dev_index] = 'G';
            break;
          case vr::TrackedDeviceClass_TrackingReference:
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

    if (_trackedPoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
      fmtx4 hmdmatrix;
      hmdmatrix.inverseOf(_poseMatrices[vr::k_unTrackedDeviceIndex_Hmd]);
      _posemap["hmd"] = hmdmatrix;
      _hmdinputgroup.setChannel("hmdmatrix").as<fmtx4>(hmdmatrix);
    }
    auto& posemap = openvr::get()._posemap;

    fmtx4 hmd  = posemap["hmd"];
    fmtx4 eyeL = posemap["eyel"];
    fmtx4 eyeR = posemap["eyer"];

    fvec3 hmdpos;
    fquat hmdrot;
    float hmdscl;

    hmd.DecomposeMatrix(hmdpos, hmdrot, hmdscl);

    auto rotmtx = hmdrot.ToMatrix();
    rotmtx      = openvr::get()._headingmatrix * rotmtx;
    rotmtx.Transpose();

    ///////////////////////////////////////////////////////////

    openvr::get()._hmdMatrix = hmd;

    fmtx4 VVMTX = playermtx;

    fvec3 vvtrans = VVMTX.GetTranslation();

    fmtx4 wmtx;
    wmtx.SetTranslation(vvtrans + fvec3(0, 0.5, 0));
    wmtx = openvr::get()._headingmatrix * wmtx;

    VVMTX.inverseOf(wmtx);

    _frametek->_viewOffsetMatrix = VVMTX;

    fmtx4 cmv = VVMTX * hmd;
    fmtx4 lmv = VVMTX * hmd * eyeL;
    fmtx4 rmv = VVMTX * hmd * eyeR;

    msgrouter::content_t c;
    c.Set<fmtx4>(cmv);

    msgrouter::channel("eggytest")->post(c);

    _hmdinputgroup.setChannel("leye.matrix").as<fmtx4>(lmv);
    _hmdinputgroup.setChannel("reye.matrix").as<fmtx4>(rmv);

    _rightcamera.SetView(lmv);
    _rightcamera.setCustomProjection(posemap["projl"]);
    RCAM.SetView(rmv);
    RCAM.setCustomProjection(posemap["projr"]);
    // printf( "pose_classes<%s>\n", pose_classes.c_str() );
  }
#else // ENABLE_VR

  auto mpos = _qtmousepos;
  float r   = mpos.Mag();
  float z   = 1.0f - r;
  auto v3   = fvec3(-mpos.x, -mpos.y, z).Normal();
  fmtx4 w;
  w.LookAt(fvec3(0, 0, 0), v3, fvec3(0, 1, 0));
  _posemap["hmd"] = w;
  // printf("v3<%g %g %g>\n", v3.x, v3.y, v3.z);

#endif
}

////////////////////////////////////////////////////////////////////////////////

void gpuUpdate(lev2::GfxTarget* targ) {
  auto& mgr = get();
#if defined(ENABLE_VR)
  if (mgr._active) {
    bool ovr_compositor_ok = (bool)vr::VRCompositor();
    assert(ovr_compositor_ok);
  }
#endif
  mgr._processControllerEvents(targ);
  mgr._updatePoses(targ);
}

} // namespace ork::lev2::openvr
