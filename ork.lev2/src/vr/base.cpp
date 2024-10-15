#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/kernel/string/deco.inl>
#include <ork/profiling.inl>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////
static device_ptr_t _gdevice = nullptr;
void setDevice(std::shared_ptr<Device> device) {
  _gdevice = device;
}
device_ptr_t device() {
  return _gdevice;
}
////////////////////////////////////////////////////////////////////////////////
ork::LockedResource<VrTrackingNotificationReceiver_set> gnotifset;
////////////////////////////////////////////////////////////////////////////////
void addVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) { notifset.insert(recvr); });
}
////////////////////////////////////////////////////////////////////////////////
void removeVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) {
    auto it = notifset.find(recvr);
    OrkAssert(it != notifset.end());
    notifset.erase(it);
  });
}
////////////////////////////////////////////////////////////////////////////////
Device::Device() {

  auto imgr      = lev2::InputManager::instance();
  _cameraName = "";

  _hmdinputgroup = imgr->inputGroup("hmd");

  _leftcamera   = new CameraMatrices;
  _centercamera = new CameraMatrices;
  _rightcamera  = new CameraMatrices;

  auto handgroup = imgr->inputGroup("hands");
  handgroup->setChannel("left.button1").as<bool>(false);
  handgroup->setChannel("left.button2").as<bool>(false);
  handgroup->setChannel("left.trigger").as<bool>(false);
  handgroup->setChannel("left.thumb").as<bool>(false);

  handgroup->setChannel("right.button1").as<bool>(false);
  handgroup->setChannel("right.button2").as<bool>(false);
  handgroup->setChannel("right.trigger").as<bool>(false);
  handgroup->setChannel("right.thumb").as<bool>(false);
}

////////////////////////////////////////////////////////////////////////////////

Device::~Device() {
  delete _leftcamera;
  delete _centercamera;
  delete _rightcamera;
}

////////////////////////////////////////////////////////////////////////////////

void Device::overrideSize(int w, int h){
  _width          = w;
  _height         = h;
  float aspect    = 50.0;//float(_width*2) / float(_height);

  _posemap["projl"].perspective(_fov, aspect, _near, _far);
  _posemap["projr"].perspective(_fov, aspect, _near, _far);
  _posemap["projc"].perspective(_fov, aspect, _near, _far);

}

////////////////////////////////////////////////////////////////////////////////

controllerstate_ptr_t Device::controller(int id) {
  controllerstate_ptr_t rval;
  auto it = _controllers.find(id);
  if (it != _controllers.end()) {
    rval = it->second;
  } else {
    rval             = std::make_shared<ControllerState>();
    _controllers[id] = rval;
  }
  return rval;
}

void Device::resetCalibration(){
  _calibstate = 0;
  _calibstateFrame = 0;
  _calibposvect.clear();
  _calibnxvect.clear();
  _calibnyvect.clear();
  _calibnzvect.clear();
  _baseMatrix = fmtx4();
}

////////////////////////////////////////////////////////////////////////////////

void Device::_updatePosesCommon() {
  EASY_BLOCK("vr-upc");
  fmtx4 hmd  = _posemap["hmd"];
  fmtx4 eyeL = _posemap["eyel"];
  fmtx4 eyeR = _posemap["eyer"];

  fvec3 hmdpos;
  fquat hmdrot;
  float hmdscl;

  hmd.decompose(hmdpos, hmdrot, hmdscl);

  _rotMatrix = hmdrot.toMatrix();
  _rotMatrix.transpose();

  //_rotMatrix.dump("rotmtx");
  ///////////////////////////////////////////////////////////

  switch (_calibstate) {
    case 0: // init
      resetCalibration();
      _calibstate = 1;
      break;
    case 1: // calibrating
      if (_calibstateFrame >= 30) {
        if(_do_calibration){
          fvec3 avgpos;
          fvec3 nxaccum, nyaccum, nzaccum;
          float avgang = 0.0f;
          for (size_t i = 0; i < _calibposvect.size(); i++) {
            avgpos += _calibposvect[i];
            nxaccum += _calibnxvect[i];
            nyaccum += _calibnyvect[i];
            nzaccum += _calibnzvect[i];
          }
          avgpos *= (1.0f / float(_calibposvect.size()));
          fvec3 ny    = fvec3(0, 1, 0);
          fvec2 nz_xz = nzaccum.xz().normalized();
          fvec3 nz    = fvec3(nz_xz.x, 0.0f, nz_xz.y).normalized();
          fvec3 nx    = ny.crossWith(nz);
          // printf("nx<%g %g %g>\n", nx.x, nx.y, nx.z);
          // printf("ny<%g %g %g>\n", ny.x, ny.y, ny.z);
          // printf("nz<%g %g %g>\n", nz.x, nz.y, nz.z);
          // printf("avgpos<%g %g %g>\n", avgpos.x, avgpos.y, avgpos.z);
          fmtx4 avgrotmtx;
          avgrotmtx.fromNormalVectors(nx, ny, nz);
          fmtx4 avgtramtx;
          avgtramtx.setTranslation(avgpos);
          fmtx4 refmtx = fmtx4::multiply_ltor(avgtramtx,avgrotmtx);
          _baseMatrix.setTranslation(hmd.inverse().translation());
          deco::prints(hmd.dump4x3cn(), true);
          deco::prints(hmd.inverse().dump4x3cn(), true);
          deco::printf(fvec3::Yellow(), "vrstate: calibrated\n");
        }
        // deco::prints(avgrotmtx.dump4x3cn(), true);
        // deco::prints(refmtx.dump4x3cn(), true);
        // deco::prints(_baseMatrix.dump4x3cn(), true);
        _calibstate = 2;
      } else {
        if(_do_calibration){
          _calibposvect.push_back(hmdpos);
          fvec3 nx, ny, nz;
          hmd.toNormalVectors(nx, ny, nz);
          _calibnxvect.push_back(nx);
          _calibnyvect.push_back(ny);
          _calibnzvect.push_back(nz);
        }
        _calibstateFrame++;
      }
      break;
    case 2: // calibrated
      _calibstate++;
      break;
    case 3: // postcalibrated
      break;
    default:
      OrkAssert(false);
      break;
  }

  ///////////////////////////////////////////////////////////

  _hmdMatrix = hmd;

  ///////////////////////////////////////////////////////////

  fmtx4 usermtx = _usermtxgen ? _usermtxgen() : fmtx4();

  _outputViewOffsetMatrix = usermtx;

  fmtx4 relmtx = fmtx4::multiply_ltor(_baseMatrix,_hmdMatrix);

  fmtx4 cmv = fmtx4::multiply_ltor(usermtx,relmtx);

  if (_calibstate == 2) {
    deco::printf(fvec3::White(), "_baseMatrix: %s\n", _baseMatrix.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "_hmdMatrix: %s\n", _hmdMatrix.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "relmtx: %s\n", relmtx.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "usermtx: %s\n", usermtx.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "cmv: %s\n", cmv.dump4x3cn().c_str());
  }

  fmtx4 lmv = fmtx4::multiply_ltor(cmv,eyeL);
  fmtx4 rmv = fmtx4::multiply_ltor(cmv,eyeR);

  msgrouter::content_t c;
  c.set<fmtx4>(cmv);

  msgrouter::channel("eggytest")->post(c);

  _hmdinputgroup->setChannel("leye.matrix").as<fmtx4>(lmv);
  _hmdinputgroup->setChannel("ceye.matrix").as<fmtx4>(cmv);
  _hmdinputgroup->setChannel("reye.matrix").as<fmtx4>(rmv);

  _leftcamera->setCustomView(lmv);
  _leftcamera->setCustomProjection(_posemap["projl"]);
  _rightcamera->setCustomView(rmv);
  _rightcamera->setCustomProjection(_posemap["projr"]);
  _centercamera->setCustomView(cmv);
  _centercamera->setCustomProjection(_posemap["projc"]);
  // printf( "pose_classes<%s>\n", pose_classes.c_str() );
}

////////////////////////////////////////////////////////////////////////////////

fmtx4 VrProjFrustumPar::composeProjection() const {
  fmtx4 rval;
  float idx = 1.0f / (_right - _left);
  float idy = 1.0f / (_bottom - _top);
  float idz = 1.0f / (_far - _near);
  float sx  = _right + _left;
  float sy  = _bottom + _top;

  rval.setElemYX(0, 0, 2 * idx);
  rval.setElemYX(0, 1, 0);
  rval.setElemYX(0, 2, sx * idx);
  rval.setElemYX(0, 3, 0);
  rval.setElemYX(1, 0, 0);
  rval.setElemYX(1, 1, 2 * idy);
  rval.setElemYX(1, 2, sy * idy);
  rval.setElemYX(1, 3, 0);
  rval.setElemYX(2, 0, 0);
  rval.setElemYX(2, 1, 0);
  rval.setElemYX(2, 2, -_far * idz);
  rval.setElemYX(2, 3, -_far * _near * idz);
  rval.setElemYX(3, 0, 0);
  rval.setElemYX(3, 1, 0);
  rval.setElemYX(3, 2, -1.0f);
  rval.setElemYX(3, 3, 0);

  return rval;
}
//////////////////////////////////////////////
VrTrackingControllerNotificationFrame::VrTrackingControllerNotificationFrame() {
  _left  = std::make_shared<ControllerState>();
  _right = std::make_shared<ControllerState>();
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
