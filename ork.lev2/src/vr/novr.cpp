#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/gfx/renderer/compositor.h>
////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr::novr {
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
  _width          = 2880;
  _height         = 1440;
  float aspect    = float(_width) / float(_height);

  _posemap["projl"].perspective(_fov, aspect, _near, _far);
  _posemap["projr"].perspective(_fov, aspect, _near, _far);
  _posemap["projc"].perspective(_fov, aspect, _near, _far);

  fmtx4 eyel_t, eyel_r, eyel_s;
  fmtx4 eyer_t, eyer_r, eyer_s;

  eyel_t.setTranslation(+_IPD * 0.5, 0, 0);
  eyer_t.setTranslation(-_IPD * 0.5, 0, 0);

  _posemap["eyel"] = eyel_t*eyel_r*eyel_s;
  _posemap["eyer"] = eyer_t*eyer_r*eyer_s;
}
NoVrDevice::~NoVrDevice() {
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::_updatePoses(RenderContextFrameData& RCFD) {
  // auto mpos = _qtmousepos;
  // float r   = mpos.Mag();
  // float z   = 1.0f - r;
  // auto v3   = fvec3(-mpos.x, -mpos.y, z).Normal();
  // fmtx4 w;
  // w.LookAt(fvec3(0, 0, 0), v3, fvec3(0, 1, 0));
  //_posemap["hmd"] = w;

  ///////////////////////////////////////////////////////////////////
  // eye matrices (part of viewing transformation, not including pose)
  ///////////////////////////////////////////////////////////////////

  fmtx4 eyel_t, eyel_r, eyel_s;
  fmtx4 eyer_t, eyer_r, eyer_s;

  eyel_t.setTranslation(+_IPD * 0.5, 0, 0);
  eyer_t.setTranslation(-_IPD * 0.5, 0, 0);

  _posemap["eyel"] = eyel_t*eyel_r*eyel_s;
  _posemap["eyer"] = eyer_t*eyer_r*eyer_s;

  ///////////////////////////////////////////////////////////////////
  // projection matrices
  ///////////////////////////////////////////////////////////////////

  auto& CPD    = RCFD.topCPD();
  //auto rt      = CPD._irendertarget;


  auto& LMATRIX = _posemap["projl"];
  auto& CMATRIX = _posemap["projc"];
  auto& RMATRIX = _posemap["projr"];

  fmtx4 lp, cp, rp, rotzL, rotzR;

  float aspect = float(_width) / float(_height);
  lp.perspective(_fov, aspect, _near, _far);
  cp.perspective(_fov, aspect, _near, _far);
  rp.perspective(_fov, aspect, _near, _far);

  ////////////////////////////////////////
  // apply display panel rotation, if any..
  ////////////////////////////////////////

  rotzL.setRotateZ(_stereoTileRotationDegreesL*DTOR);
  rotzR.setRotateZ(_stereoTileRotationDegreesR*DTOR);

  LMATRIX = fmtx4::multiply_ltor(lp,rotzL);
  CMATRIX = cp;
  RMATRIX = fmtx4::multiply_ltor(rp,rotzR);

  ////////////////////////////////////////

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
  static bool init = true;
  static std::shared_ptr<NoVrDevice> _device = std::make_shared<NoVrDevice>();
  if(init){
    init = false;
    setDevice(_device);
  }
  return _device;
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::gpuUpdate(RenderContextFrameData& RCFD) {
  _processControllerEvents();
  _updatePoses(RCFD);
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::__composite(Context* targ, Texture* twoeyetex) const {
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr::novr
////////////////////////////////////////////////////////////////////////////////
