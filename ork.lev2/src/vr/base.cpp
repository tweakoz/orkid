#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

Device::Device()
    : _width(1024)
    , _height(1024)
    , _active(false)
    , _supportsStereo(false)
    , _hmdinputgroup(*lev2::InputManager::inputGroup("hmd")) {

  _leftcamera = new CameraMatrices;
  _centercamera = new CameraMatrices;
  _rightcamera = new CameraMatrices;

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

void Device::_updatePosesCommon(fmtx4 observermatrix){
    fmtx4 hmd  = _posemap["hmd"];
    fmtx4 eyeL = _posemap["eyel"];
    fmtx4 eyeR = _posemap["eyer"];

    fvec3 hmdpos;
    fquat hmdrot;
    float hmdscl;

    hmd.DecomposeMatrix(hmdpos, hmdrot, hmdscl);

    _rotMatrix = hmdrot.ToMatrix();
    _rotMatrix      = _headingmatrix * _rotMatrix;
    _rotMatrix.Transpose();

    //_rotMatrix.dump("rotmtx");
    ///////////////////////////////////////////////////////////

    _hmdMatrix = hmd;
    _hmdMatrix.dump("hmdmtx");

    fmtx4 VVMTX = observermatrix;

    fvec3 vvtrans = VVMTX.GetTranslation();

    fmtx4 wmtx;
    wmtx.SetTranslation(vvtrans + fvec3(0, 0.5, 0));
    wmtx = _headingmatrix * wmtx;

    VVMTX.inverseOf(wmtx);

    _outputViewOffsetMatrix = VVMTX;

    fmtx4 cmv = VVMTX * hmd;
    fmtx4 lmv = VVMTX * hmd * eyeL;
    fmtx4 rmv = VVMTX * hmd * eyeR;

    msgrouter::content_t c;
    c.Set<fmtx4>(cmv);

    msgrouter::channel("eggytest")->post(c);

    _hmdinputgroup.setChannel("leye.matrix").as<fmtx4>(lmv);
    _hmdinputgroup.setChannel("ceye.matrix").as<fmtx4>(cmv);
    _hmdinputgroup.setChannel("reye.matrix").as<fmtx4>(rmv);

    _leftcamera->_camdat.SetView(lmv);
    _leftcamera->setCustomProjection(_posemap["projl"]);
    _rightcamera->_camdat.SetView(rmv);
    _rightcamera->setCustomProjection(_posemap["projr"]);
    _centercamera->_camdat.SetView(cmv);
    _centercamera->setCustomProjection(_posemap["projc"]);
    // printf( "pose_classes<%s>\n", pose_classes.c_str() );
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
