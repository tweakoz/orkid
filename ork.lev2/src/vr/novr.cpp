#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/gfx/renderer/compositor.h>
////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////
NoVrDevice::NoVrDevice() 
  : Device() {
  auto handgroup = lev2::InputManager::instance()->inputGroup("hands");
  _qtmousesubsc  = msgrouter::channel("qtmousepos")->subscribe([this](msgrouter::content_t c) { _qtmousepos = c.get<fvec2>(); });

  _active       = true;
  _qtkbdownsubs = msgrouter::channel("qtkeyboard.down")->subscribe([this, handgroup](msgrouter::content_t c) {
    int key = c.get<int>();
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
    int key = c.get<int>();
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

  _supportsStereo = true;
  _width = 2560;
  _height = 1440;
  float aspect = float(_width) / float(_height);

  _posemap["projl"].Perspective(_fov, aspect, .01, 10000);
  _posemap["projr"].Perspective(_fov, aspect, .01, 10000);
  _posemap["projc"].Perspective(_fov, aspect, .01, 10000);


  fmtx4 eyel, eyer;
  eyel.compose(fvec3(+_IPD*0.5,0,0),fquat(),1.0);
  eyer.compose(fvec3(-_IPD*0.5,0,0),fquat(),1.0);

  _posemap["eyel"] = eyel;
  _posemap["eyer"] = eyer;
}
NoVrDevice::~NoVrDevice() {
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::_updatePoses(RenderContextFrameData& RCFD) {
  //auto mpos = _qtmousepos;
  //float r   = mpos.Mag();
  //float z   = 1.0f - r;
  //auto v3   = fvec3(-mpos.x, -mpos.y, z).Normal();
  //fmtx4 w;
  //w.LookAt(fvec3(0, 0, 0), v3, fvec3(0, 1, 0));
  //_posemap["hmd"] = w;
  // printf("v3<%g %g %g>\n", v3.x, v3.y, v3.z);
  auto& CPD    = RCFD.topCPD();
  auto rt      = CPD._irendertarget;
  float aspect = float(_width) / float(_height);

  _posemap["projl"].Perspective(_fov, aspect, .01, 10000);
  _posemap["projr"].Perspective(_fov, aspect, .01, 10000);
  _posemap["projc"].Perspective(_fov, aspect, .01, 10000);

  fmtx4 eyel, eyer;
  eyel.compose(fvec3(+_IPD*0.5,0,0),fquat(),1.0);
  eyer.compose(fvec3(-_IPD*0.5,0,0),fquat(),1.0);

  _posemap["eyel"] = eyel;
  _posemap["eyer"] = eyer;

  auto& LMATRIX = _posemap["projl"];
  auto& CMATRIX = _posemap["projc"];
  auto& RMATRIX = _posemap["projr"];

  fmtx4 lp, cp, rp, rotzL, rotzR;

  lp.Perspective(_fov, aspect, _near, _far);
  cp.Perspective(_fov, aspect, _near, _far);
  rp.Perspective(_fov, aspect, _near, _far);

  rotzL.SetRotateZ(_stereoTileRotationDegreesL*DTOR);
  rotzR.SetRotateZ(_stereoTileRotationDegreesR*DTOR);

  LMATRIX = lp*rotzL;
  CMATRIX = cp;
  RMATRIX = rp*rotzR;

  _updatePosesCommon();
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::_processControllerEvents() {
  auto handgroup = lev2::InputManager::instance()->inputGroup("hands");
  bool curthumbL = handgroup->tryAs<bool>("left.thumb").value();
  bool curthumbR = handgroup->tryAs<bool>("right.thumb").value();
  ///////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<NoVrDevice> novr_device() {
  static std::shared_ptr<NoVrDevice> _device = std::make_shared<NoVrDevice>();
  return _device;
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::gpuUpdate(RenderContextFrameData& RCFD) {
  _processControllerEvents();
  _updatePoses(RCFD);
}
////////////////////////////////////////////////////////////////////////////////
#if ! defined(ENABLE_OPENVR)
void composite(Context* targ, Texture* twoeyetex) {
  if( not device()._active)
    return;

    auto fbi = targ->FBI();

    auto twoeyetexOBJ = twoeyetex->_varmap.typedValueForKey<GLuint>("gltexobj").value();
    //////////////////////////////////////////////////

    int w = device()._width;
    int h = device()._height;
    ViewportRect VPRect(0, 0, w * 2, h);
    fbi->pushViewport(VPRect);
    fbi->pushScissor(VPRect);

    //////////////////////////////////////////////////
    // submit to openvr compositor
    //////////////////////////////////////////////////

    //GLuint erl = _ovr::VRCompositor()->Submit(_ovr::Eye_Left, &twoEyeTexture, &leftEyeBounds);
    //GLuint err = _ovr::VRCompositor()->Submit(_ovr::Eye_Right, &twoEyeTexture, &rightEyeBounds);

    //////////////////////////////////////////////////
    // undo above PushVp/Scissor
    //////////////////////////////////////////////////

    fbi->popViewport();
    fbi->popScissor();
}
#endif
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
