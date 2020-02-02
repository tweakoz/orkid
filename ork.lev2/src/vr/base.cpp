#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/kernel/string/deco.inl>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

Device::Device()
    : _width(1024)
    , _height(1024)
    , _active(false)
    , _supportsStereo(false)
    , _hmdinputgroup(*lev2::InputManager::inputGroup("hmd"))
    , _calibstate(0)
    , _calibstateFrame(0)
    , _usermtxgen(nullptr) {

  _leftcamera   = new CameraMatrices;
  _centercamera = new CameraMatrices;
  _rightcamera  = new CameraMatrices;

  auto handgroup = lev2::InputManager::inputGroup("hands");
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

void Device::_updatePosesCommon() {
  fmtx4 hmd  = _posemap["hmd"];
  fmtx4 eyeL = _posemap["eyel"];
  fmtx4 eyeR = _posemap["eyer"];

  fvec3 hmdpos;
  fquat hmdrot;
  float hmdscl;

  hmd.decompose(hmdpos, hmdrot, hmdscl);

  _rotMatrix = hmdrot.ToMatrix();
  _rotMatrix = _headingmatrix * _rotMatrix;
  _rotMatrix.Transpose();

  //_rotMatrix.dump("rotmtx");
  ///////////////////////////////////////////////////////////

  switch (_calibstate) {
    case 0: // init
      _calibstateFrame = 0;
      _calibposvect.clear();
      _calibnxvect.clear();
      _calibnyvect.clear();
      _calibnzvect.clear();
      _calibstate = 1;
      _baseMatrix = fmtx4();
      break;
    case 1: // calibrating
      if (_calibstateFrame >= 30) {
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
        fvec2 nz_xz = nzaccum.GetXZ().Normal();
        fvec3 nz    = fvec3(nz_xz.x, 0.0f, nz_xz.y).Normal();
        fvec3 nx    = ny.Cross(nz);
        printf("nx<%g %g %g>\n", nx.x, nx.y, nx.z);
        printf("ny<%g %g %g>\n", ny.x, ny.y, ny.z);
        printf("nz<%g %g %g>\n", nz.x, nz.y, nz.z);
        printf("avgpos<%g %g %g>\n", avgpos.x, avgpos.y, avgpos.z);
        fmtx4 avgrotmtx;
        avgrotmtx.fromNormalVectors(nx, ny, nz);
        fmtx4 avgtramtx;
        avgtramtx.SetTranslation(avgpos);
        fmtx4 refmtx = avgtramtx * avgrotmtx;
        _baseMatrix.SetTranslation(hmd.inverse().GetTranslation());
        deco::prints(hmd.dump4x3cn(), true);
        deco::prints(hmd.inverse().dump4x3cn(), true);
        deco::printf(fvec3::Yellow(), "vrstate: calibrated\n");
        deco::prints(avgrotmtx.dump4x3cn(), true);
        deco::prints(refmtx.dump4x3cn(), true);
        deco::prints(_baseMatrix.dump4x3cn(), true);
        _calibstate = 2;
      } else {
        _calibposvect.push_back(hmdpos);
        fvec3 nx, ny, nz;
        hmd.toNormalVectors(nx, ny, nz);
        _calibnxvect.push_back(nx);
        _calibnyvect.push_back(ny);
        _calibnzvect.push_back(nz);
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

  _hmdMatrix = hmd; //_userOffsetMatrix * relmtx;

  ///////////////////////////////////////////////////////////

  fmtx4 usermtx = _usermtxgen ? _usermtxgen() : fmtx4();

  // fvec3 vvtrans = VVMTX.GetTranslation();
  // fmtx4 wmtx;
  // wmtx.SetTranslation(vvtrans + fvec3(0, 0.0, 0));
  // wmtx = _headingmatrix * wmtx;
  // VVMTX.inverseOf(wmtx);

  _outputViewOffsetMatrix = usermtx;

  // deco::prints(VVMTX.dump4x3(), true);

  fmtx4 relmtx = (_baseMatrix * _hmdMatrix);

  fmtx4 cmv = usermtx * _headingmatrix * relmtx;

  if (_calibstate == 2) {
    deco::printf(fvec3::White(), "_baseMatrix: %s\n", _baseMatrix.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "_hmdMatrix: %s\n", _hmdMatrix.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "relmtx: %s\n", relmtx.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "usermtx: %s\n", usermtx.dump4x3cn().c_str());
    deco::printf(fvec3::White(), "cmv: %s\n", cmv.dump4x3cn().c_str());
  }

  fmtx4 lmv = cmv * eyeL;
  fmtx4 rmv = cmv * eyeR;

  msgrouter::content_t c;
  c.Set<fmtx4>(cmv);

  msgrouter::channel("eggytest")->post(c);

  _hmdinputgroup.setChannel("leye.matrix").as<fmtx4>(lmv);
  _hmdinputgroup.setChannel("ceye.matrix").as<fmtx4>(cmv);
  _hmdinputgroup.setChannel("reye.matrix").as<fmtx4>(rmv);

  _leftcamera->setCustomView(lmv);
  _leftcamera->setCustomProjection(_posemap["projl"]);
  _rightcamera->setCustomView(rmv);
  _rightcamera->setCustomProjection(_posemap["projr"]);
  _centercamera->setCustomView(cmv);
  _centercamera->setCustomProjection(_posemap["projc"]);
  // printf( "pose_classes<%s>\n", pose_classes.c_str() );
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
