////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/input/inputdevice.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/Compositor.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/input.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

#if !defined(__APPLE__)
#include <openvr/openvr.h>
#define ENABLE_VR
#endif

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::VrCompositingNode, "VrCompositingNode");

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::Describe() {}

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

std::string trackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop,
                                vr::TrackedPropertyError* peError = NULL) {
  uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
  if (unRequiredBufferLen == 0)
    return "";

  char* pchBuffer = new char[unRequiredBufferLen];
  unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
  std::string sResult = pchBuffer;
  delete[] pchBuffer;
  return sResult;
}
#endif
///////////////////////////////////////////////////////////////////////////

struct ControllerState {
  fmtx4 _matrix;
  bool _button1down = false;
  bool _button2down = false;
  bool _buttonThumbdown = false;
  bool _triggerDown = false;
};

///////////////////////////////////////////////////////////////////////////

constexpr int NUMSAMPLES = 4;

struct VrFrameTechnique final : public FrameTechniqueBase {
  VrFrameTechnique(int w, int h) : FrameTechniqueBase(w, h), _rtg_left(nullptr), _rtg_right(nullptr) {}

  void DoInit(GfxTarget* pTARG) final {
    if (nullptr == _rtg_left) {
      _rtg_left = new RtGroup(pTARG, miW, miH, NUMSAMPLES);
      _rtg_right = new RtGroup(pTARG, miW, miH, NUMSAMPLES);

      auto lbuf = new RtBuffer(_rtg_left, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);
      auto rbuf = new RtBuffer(_rtg_right, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);

      _rtg_left->SetMrt(0, lbuf);
      _rtg_right->SetMrt(0, rbuf);

      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }
  void renderBothEyes(FrameRenderer& renderer, CompositorSystemDrawData& drawdata, CameraData* lcam, CameraData* rcam,
                      const std::map<int, ControllerState>& controllers) {
    RenderContextFrameData& FrameData = renderer.GetFrameData();
    GfxTarget* pTARG = FrameData.GetTarget();

    SRect tgt_rect(0, 0, miW, miH);

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek = this;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName = nullptr; // default == "All"

    //////////////////////////////////////////////////////
    // render all controller poses
    //////////////////////////////////////////////////////

    auto renderposes = [&](CameraData* camdat) {
      fmtx4 rx;
      fmtx4 ry;
      fmtx4 rz;
      rx.SetRotateX(-PI * 0.5);
      ry.SetRotateY(PI * 0.5);
      rz.SetRotateZ(PI * 0.5);

      for (auto item : controllers) {

        auto c = item.second;
        fmtx4 ivomatrix;
        ivomatrix.inverseOf(_viewOffsetMatrix);

        fmtx4 scalemtx;
        scalemtx.SetScale(c._button1down ? 0.05 : 0.025);

        fmtx4 controller_worldspace = (c._matrix * ivomatrix);

        fmtx4 mmtx = (scalemtx * rx * ry * rz * controller_worldspace);

        pTARG->MTXI()->PushMMatrix(mmtx);
        pTARG->MTXI()->PushVMatrix(camdat->GetVMatrix());
        pTARG->MTXI()->PushPMatrix(camdat->GetPMatrix());
        pTARG->PushModColor(fvec4::White());
        {
          if (c._button2down)
            ork::lev2::GfxPrimitives::GetRef().RenderBox(pTARG);
          else
            ork::lev2::GfxPrimitives::GetRef().RenderAxis(pTARG);
        }
        pTARG->PopModColor();
        pTARG->MTXI()->PopPMatrix();
        pTARG->MTXI()->PopVMatrix();
        pTARG->MTXI()->PopMMatrix();
      }
    };

    //////////////////////////////////////////////////////

    pTARG->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    // draw left ////////////////////////////////////////

    lcam->BindGfxTarget(pTARG);
    FrameData.SetCameraData(lcam);
    _CPD._impl.Set<const CameraData*>(lcam);
    _CPD._clearColor = fvec4(0, 0, .1, 1);

    RtGroupRenderTarget rtL(_rtg_left);
    drawdata.mCompositingGroupStack.push(_CPD);
    {
      pTARG->SetRenderContextFrameData(&FrameData);
      FrameData.SetDstRect(tgt_rect);
      FrameData.PushRenderTarget(&rtL);
      pTARG->FBI()->PushRtGroup(_rtg_left);
      pTARG->BeginFrame();
      FrameData.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      renderposes(lcam);
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      FrameData.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
      drawdata.mCompositingGroupStack.pop();
    }

    // draw right ///////////////////////////////////////

    rcam->BindGfxTarget(pTARG);
    FrameData.SetCameraData(rcam);
    _CPD._impl.Set<const CameraData*>(rcam);
    _CPD._clearColor = fvec4(0, 0, .1, 1);

    drawdata.mCompositingGroupStack.push(_CPD);
    {
      RtGroupRenderTarget rtR(_rtg_right);
      pTARG->SetRenderContextFrameData(&FrameData);
      FrameData.SetDstRect(tgt_rect);
      FrameData.PushRenderTarget(&rtR);
      pTARG->FBI()->PushRtGroup(_rtg_right);
      pTARG->BeginFrame();
      FrameData.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      renderposes(rcam);
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      FrameData.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
    }
  }

  RtGroup* _rtg_left;
  RtGroup* _rtg_right;
  BuiltinFrameEffectMaterial _effect;
  ent::CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
};

///////////////////////////////////////////////////////////////////////////////
struct VRSYSTEMIMPL {
  ///////////////////////////////////////
  VRSYSTEMIMPL()
      : _frametek(nullptr), _camname(AddPooledString("Camera")), _layers(AddPooledString("All")), _active(false), _width(1024),
        _height(1024), _hmdinputgroup(*lev2::InputManager::inputGroup("hmd")) {

    auto handgroup = lev2::InputManager::inputGroup("hands");
    handgroup->setChannel("left.button1").as<bool>(false);
    handgroup->setChannel("left.button2").as<bool>(false);
    handgroup->setChannel("left.trigger").as<bool>(false);
    handgroup->setChannel("left.thumb").as<bool>(false);

    handgroup->setChannel("right.button1").as<bool>(false);
    handgroup->setChannel("right.button2").as<bool>(false);
    handgroup->setChannel("right.trigger").as<bool>(false);
    handgroup->setChannel("right.thumb").as<bool>(false);

#if defined(ENABLE_VR)
    vr::EVRInitError error = vr::VRInitError_None;
    _hmd = vr::VR_Init(&error, vr::VRApplication_Scene);
    _active = (error == vr::VRInitError_None);

    if (_active) {
      _hmd->GetRecommendedRenderTargetSize(&_width, &_height);
      printf("RECOMMENDED WH<%d %d>\n", _width, _height);
      auto str_driver = trackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
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
    _qtkbupsubs = msgrouter::channel("qtkeyboard.up")->subscribe([this, handgroup](msgrouter::content_t c) {
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
  ///////////////////////////////////////
  ~VRSYSTEMIMPL() {

#if defined(ENABLE_VR)
    if (_hmd)
      vr::VR_Shutdown();
#endif

    if (_frametek)
      delete _frametek;
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);

    _frametek = new VrFrameTechnique(_width, _height);
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _myrender(SceneInst* psi, FrameRenderer& renderer, CompositorSystemDrawData& drawdata, fmtx4 rootmatrix) {

    auto playerspawn = psi->FindEntity(AddPooledString("playerspawn"));
    auto playermtx = playerspawn->GetEffectiveMatrix();

    fmtx4 hmd = _posemap["hmd"];
    fmtx4 eyeL = _posemap["eyel"];
    fmtx4 eyeR = _posemap["eyer"];

    float xlaterate = 12.0 / 80.0;

    fvec3 hmdpos;
    fquat hmdrot;
    float hmdscl;

    hmd.DecomposeMatrix(hmdpos, hmdrot, hmdscl);

    auto rotmtx = hmdrot.ToMatrix();
    rotmtx = _headingmatrix * rotmtx;
    rotmtx.Transpose();
    // rotmtx.dump("rotmtx");

    using inpmgr = lev2::InputManager;
    auto& handgroup = *inpmgr::inputGroup("hands");

    fmtx4 xlate;
///////////////////////////////////////////////////////////
// up down
///////////////////////////////////////////////////////////
#if defined(ENABLE_VR)
    if (_rightControllerDeviceIndex >= 0 and _leftControllerDeviceIndex >= 0) {

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
    } // if( _rightControllerDeviceIndex>=0 and _leftControllerDeviceIndex>=0 ){
    bool curthumbL = LCONTROLLER._buttonThumbdown;
    bool curthumbR = RCONTROLLER._buttonThumbdown;
#else

    bool curthumbL = handgroup.tryAs<bool>("left.thumb").value();
    bool curthumbR = handgroup.tryAs<bool>("right.thumb").value();

#endif

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

    ///////////////////////////////////////////////////////////

    _hmdMatrix = hmd;

    fmtx4 VVMTX = playermtx;

    fvec3 vvtrans = VVMTX.GetTranslation();

    fmtx4 wmtx;
    wmtx.SetTranslation(vvtrans + fvec3(0, 2, 0));
    wmtx = _headingmatrix * wmtx;

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

    _leftcamera.SetView(lmv);
    _leftcamera.setCustomProjection(_posemap["projl"]);
    _rightcamera.SetView(rmv);
    _rightcamera.setCustomProjection(_posemap["projr"]);
    _frametek->renderBothEyes(renderer, drawdata, &_leftcamera, &_rightcamera, _controllers);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
  VrFrameTechnique* _frametek;
  uint32_t _width, _height;
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
  InputSystem* _inputsys = nullptr;
  lev2::InputGroup& _hmdinputgroup;
  msgrouter::subscriber_t _qtmousesubsc;
  msgrouter::subscriber_t _qtkbdownsubs;
  msgrouter::subscriber_t _qtkbupsubs;
  fvec2 _qtmousepos;
#if defined(ENABLE_VR)
  vr::IVRSystem* _hmd;
  vr::TrackedDevicePose_t _trackedPoses[vr::k_unMaxTrackedDeviceCount];
  fmtx4 _poseMatrices[vr::k_unMaxTrackedDeviceCount];
  std::string _devclass[vr::k_unMaxTrackedDeviceCount];
  std::set<vr::TrackedDeviceIndex_t> _controllerindexset;
  int _rightControllerDeviceIndex = -1;
  int _leftControllerDeviceIndex = -1;
#endif
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRSYSTEMIMPL>(); }
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();

#if defined(ENABLE_VR)
  if (vrimpl->_active) {
    bool ovr_compositor_ok = (bool)vr::VRCompositor();
    assert(ovr_compositor_ok);
  }
#endif

  if (nullptr == vrimpl->_frametek) {
    vrimpl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CompositorSystemDrawData& drawdata, CompositingSystem* compsys) // virtual
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
  static PoolString vrcamname = AddPooledString("vrcam");

  //////////////////////////////////////////////
  // find vr camera
  //////////////////////////////////////////////

  auto psi = compsys->sceneinst();
  auto vrcam = psi->GetCameraData(vrcamname);

  fmtx4 rootmatrix;

  if (vrcam != nullptr) {
    auto eye = vrcam->GetEye();
    auto tgt = vrcam->GetTarget();
    auto up = vrcam->GetUp();
    rootmatrix.LookAt(eye, tgt, up);
  }

//////////////////////////////////////////////
// process OpenVR events
//////////////////////////////////////////////
#if defined(ENABLE_VR)

  vr::VREvent_t event;
  while (vrimpl->_active and vrimpl->_hmd->PollNextEvent(&event, sizeof(event))) {
    auto data = event.data;
    auto ctrl = data.controller;
    int button = ctrl.button;

    switch (event.eventType) {
      case vr::VREvent_TrackedDeviceDeactivated:
        printf("Device %u detached.\n", event.trackedDeviceIndex);
        break;
      case vr::VREvent_TrackedDeviceUpdated:
        break;
      case vr::VREvent_ButtonPress: {
        printf("dev<%d> button<%d> pressed\n", int(event.trackedDeviceIndex), button);
        auto& c = vrimpl->_controllers[event.trackedDeviceIndex];
        vrimpl->_controllerindexset.insert(event.trackedDeviceIndex);
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
        auto& c = vrimpl->_controllers[event.trackedDeviceIndex];
        vrimpl->_controllerindexset.insert(event.trackedDeviceIndex);
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
        vrimpl->_controllerindexset.insert(event.trackedDeviceIndex);
        break;
      case vr::VREvent_ButtonUntouch:
        printf("button<%d> untouched\n", button);
        vrimpl->_controllerindexset.insert(event.trackedDeviceIndex);
        break;
      case vr::VREvent_DualAnalog_Move: {
        auto dualana = data.dualAnalog;
        printf("dualanalog<%d> untouched\n", dualana.x, dualana.y, int(dualana.which));
      }
      default:
        printf("unknown event<%d>\n", int(event.eventType));
        break;
    }
  }

  //////////////////////////////////////////////
  if (vrimpl->_active) {
    vr::VRActionSetHandle_t actset_demo = vr::k_ulInvalidActionSetHandle;
    vr::VRActiveActionSet_t actionSet = {0};
    actionSet.ulActionSet = actset_demo;
    // vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );
  }
  //////////////////////////////////////////////

#endif

  // const ent::CompositingGroup* pCG = _group;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();

  if (vrimpl->_frametek) {

    /////////////////////////////////////////////////////////////////////////////
    // render eyes
    /////////////////////////////////////////////////////////////////////////////

    anyp PassData;
    PassData.Set<const char*>("All");
    the_renderer.GetFrameData().SetUserProperty("pass", PassData);
    vrimpl->_myrender(psi, the_renderer, drawdata, rootmatrix);

    /////////////////////////////////////////////////////////////////////////////
    // VR compositor
    /////////////////////////////////////////////////////////////////////////////

    auto bufferL = vrimpl->_frametek->_rtg_left->GetMrt(0);
    assert(bufferL != nullptr);
    auto bufferR = vrimpl->_frametek->_rtg_right->GetMrt(0);
    assert(bufferR != nullptr);

    auto ptexL = bufferL->GetTexture();
    auto ptexR = bufferR->GetTexture();
    if (ptexL && ptexR) {

      auto texobjL = ptexL->getProperty<GLuint>("gltexobj");
      auto texobjR = ptexR->getProperty<GLuint>("gltexobj");

#if defined(ENABLE_VR)

      if (vrimpl->_active) {

        vr::Texture_t leftEyeTexture = {(void*)(uintptr_t)texobjL, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
        vr::Texture_t rightEyeTexture = {(void*)(uintptr_t)texobjR, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};

        //////////////////////////////////////////////////
        // odd that you need to set the viewport
        //  before submitting to the vr compositor,
        //  since the texture contains the size of itself...
        //////////////////////////////////////////////////

        SRect VPRect(0, 0, vrimpl->_width, vrimpl->_height);
        auto targ = framedata.GetTarget();
        targ->FBI()->PushViewport(VPRect);
        targ->FBI()->PushScissor(VPRect);

        //////////////////////////////////////////////////
        // submit to openvr compositor
        //////////////////////////////////////////////////

        GLuint erl = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
        GLuint err = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);

        //////////////////////////////////////////////////
        // undo above PushVp/Scissor
        //////////////////////////////////////////////////

        targ->FBI()->PopViewport();
        targ->FBI()->PopScissor();
      }

      //////////////////////////////////////////////////

#endif
    }
    /////////////////////////////////////////////////////////////////////////////
  }
#if defined(ENABLE_VR)

  auto hmd = vrimpl->_hmd;

  if (vrimpl->_active and hmd->IsInputAvailable()) {

    // we call this on rendering thread, I suppose since
    // the vrcompositor needs the absolute latest pose
    // for the sake of reducing tracking latency

    vr::VRCompositor()->WaitGetPoses(vrimpl->_trackedPoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    int validposecount = 0;
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
        auto orkmtx = steam34tofmtx4(vrimpl->_trackedPoses[dev_index].mDeviceToAbsoluteTracking);
        fmtx4 inverse;
        fmtx4 transpose = orkmtx;
        transpose.Transpose();
        inverse.inverseOf(orkmtx);
        vrimpl->_poseMatrices[dev_index] = orkmtx;
        // if (vrimpl->_devclass[dev_index]==0){
        switch (hmd->GetTrackedDeviceClass(dev_index)) {
          case vr::TrackedDeviceClass_Controller:
            vrimpl->_devclass[dev_index] = 'C';
            vrimpl->_controllers[dev_index]._matrix = orkmtx;
            break;
          case vr::TrackedDeviceClass_HMD:
            vrimpl->_devclass[dev_index] = 'H';
            break;
          case vr::TrackedDeviceClass_Invalid:
            vrimpl->_devclass[dev_index] = 'I';
            break;
          case vr::TrackedDeviceClass_GenericTracker:
            vrimpl->_devclass[dev_index] = 'G';
            break;
          case vr::TrackedDeviceClass_TrackingReference:
            vrimpl->_devclass[dev_index] = 'T';
            break;
          default:
            vrimpl->_devclass[dev_index] = '?';
            break;
        }
        //}
        pose_classes += vrimpl->_devclass[dev_index];
      }
    }
    if (vrimpl->_trackedPoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
      fmtx4 hmdmatrix;
      hmdmatrix.inverseOf(vrimpl->_poseMatrices[vr::k_unTrackedDeviceIndex_Hmd]);
      vrimpl->_posemap["hmd"] = hmdmatrix;
      vrimpl->_hmdinputgroup.setChannel("hmdmatrix").as<fmtx4>(hmdmatrix);
    }

    // printf( "pose_classes<%s>\n", pose_classes.c_str() );
  }
#else // ENABLE_VR

  auto mpos = vrimpl->_qtmousepos;
  float r = mpos.Mag();
  float z = 1.0f - r;
  auto v3 = fvec3(-mpos.x, -mpos.y, z).Normal();
  fmtx4 w;
  w.LookAt(fvec3(0, 0, 0), v3, fvec3(0, 1, 0));
  vrimpl->_posemap["hmd"] = w;
  printf("v3<%g %g %g>\n", v3.x, v3.y, v3.z);

#endif
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtGroup* VrCompositingNode::GetOutput() const {
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
  if (vrimpl->_frametek)
    return vrimpl->_frametek->_rtg_left;
  else
    return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
