////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <math.h>
#include <ork/pch.h>
#include <ork/math/polar.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/viewport.h>

ImplementReflectionX(ork::lev2::UiCamera, "UiCamera");

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void UiCamera::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("Center", &UiCamera::mvCenter);
  clazz->floatProperty("Loc", float_range{0.1f, 1000.0f}, &UiCamera::mfLoc);
  clazz->directProperty("QuatC", &UiCamera::QuatC);
}

///////////////////////////////////////////////////////////////////////////////

UiCamera::UiCamera()
    : mfLoc(3.0f)
    , mvCenter(0.0f, 0.0, 0.0f)
    , QuatC(0.0f, -1.0f, 0.0f, 0.0f)
    , locscale(1.0f)
    , LastMeasuredCameraVelocity(0.0f, 0.0f, 0.0f)
    , MeasuredCameraVelocity(0.0f, 0.0f, 0.0f)
    , mbInMotion(false)
    , mfWorldSizeAtLocator(1.0f) {
  other_info = (std::string) "";
  _camcamdata.setUiCamera(this);
  printf("SETLEV2CAM<%p>\n", this);
}

///////////////////////////////////////////////////////////////////////////////

std::string UiCamera::get_full_name(void) {
  std::string rval = type_name + (std::string) ":" + instance_name + (std::string) ":" + other_info;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool UiCamera::IsXVertical() const {
  const fvec3& yn = _camcamdata.yNormal();
  float dotY      = yn.Dot(fvec3(1.0f, 0.0f, 0.0f));
  return (float(fabs(dotY)) > float(0.707f));
}

///////////////////////////////////////////////////////////////////////////////

bool UiCamera::IsYVertical() const {
  const fvec3& yn = _camcamdata.yNormal();
  float dotY      = yn.Dot(fvec3(0.0f, 1.0f, 0.0f));
  return (float(fabs(dotY)) > float(0.707f));
}

///////////////////////////////////////////////////////////////////////////////

bool UiCamera::IsZVertical() const {
  const fvec3& yn = _camcamdata.yNormal();
  float dotY      = yn.Dot(fvec3(0.0f, 0.0f, 1.0f));
  return (float(fabs(dotY)) > float(0.707f));
}

///////////////////////////////////////////////////////////////////////////////

fquat UiCamera::VerticalRot(float amt) const {
  fquat qrot;

  if (IsXVertical()) {
    fvec4 aarot(1.0f, 0.0f, 0.0f, amt);
    qrot.fromAxisAngle(aarot);
  } else if (IsYVertical()) {
    const fvec3& yn = _camcamdata.yNormal();
    float dotY      = yn.Dot(fvec3(0.0f, 1.0f, 0.0f));
    float fsign     = (dotY > 0.0f) ? 1.0f : (dotY < 0.0f) ? -1.0f : 0.0f;

    fvec4 aarot(0.0f, 1.0f, 0.0f, amt * fsign);
    qrot.fromAxisAngle(aarot);
  } else if (IsZVertical()) {
    fvec4 aarot(0.0f, 0.0f, 1.0f, amt);
    qrot.fromAxisAngle(aarot);
  }
  return qrot;
}

///////////////////////////////////////////////////////////////////////////////

fquat UiCamera::HorizontalRot(float amt) const {
  fquat qrot;

  if (IsYVertical()) {
    fvec4 aarot(1.0f, 0.0f, 0.0f, amt);
    qrot.fromAxisAngle(aarot);
  } else if (IsXVertical()) {
    fvec4 aarot(0.0f, 1.0f, 0.0f, amt);
    qrot.fromAxisAngle(aarot);
  } else if (IsZVertical()) {
    fvec4 aarot(0.0f, 0.0f, 1.0f, amt);
    qrot.fromAxisAngle(aarot);
  }
  return qrot;
}

///////////////////////////////////////////////////////////////////////////////

void UiCamera::CommonPostSetup(void) {

  float aspect = _vpdim.x / _vpdim.y;
  _curMatrices = _camcamdata.computeMatrices(aspect);

  fmtx4 ivmtx = _curMatrices.GetIVMatrix();

  ///////////////////////////////
  // billboard support

  float UpX    = ivmtx.GetElemXY(0, 0);
  float UpY    = ivmtx.GetElemXY(0, 1);
  float UpZ    = ivmtx.GetElemXY(0, 2);
  float RightX = ivmtx.GetElemXY(1, 0);
  float RightY = ivmtx.GetElemXY(1, 1);
  float RightZ = ivmtx.GetElemXY(1, 2);

  vec_billboardUp    = fvec4(UpX, UpY, UpZ);
  vec_billboardRight = fvec4(RightX, RightY, RightZ);

  auto v3up = vec_billboardUp.xyz();
  auto v3rt = vec_billboardRight.xyz();
  auto v3in = v3up.Cross(v3rt);

  // printf( "CPS: aspect<%g>\n", aspect );
  // printf( "CPS: v3up<%g %g %g>\n", v3up.x, v3up.y, v3up.z );
  // printf( "CPS: v3rt<%g %g %g>\n", v3rt.x, v3rt.y, v3rt.z );
  // printf( "CPS: v3in<%g %g %g>\n", v3in.x, v3in.y, v3in.z );

  ///////////////////////////////
  // generate frustum (useful for many things, like billboarding, clipping, LOD, etc.. )
  // we generate the frustum points, we should also generate plane eqns

  _camcamdata.setXNormal(v3up);
  _camcamdata.setYNormal(v3rt);
  _camcamdata.setZNormal(v3in);

  ///////////////////////////////

  CamLoc = mvCenter + (_camcamdata.zNormal() * (-mfLoc));
}

///////////////////////////////////////////////////////////////////////////////

float UiCamera::ViewLengthToWorldLength(const fvec4& pos, float ViewLength) {
  return float(0.0f);
}

}} // namespace ork::lev2
