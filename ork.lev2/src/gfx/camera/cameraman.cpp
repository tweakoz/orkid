////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <math.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/math/polar.h>
#include <ork/pch.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CCamera, "CCamera");

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void CCamera::Describe() {
  ork::reflect::RegisterProperty("Focus", &CCamera::CamFocus);
  ork::reflect::RegisterProperty("Center", &CCamera::mvCenter);
  ork::reflect::RegisterProperty("Loc", &CCamera::mfLoc);
  ork::reflect::RegisterProperty("QuatC", &CCamera::QuatC);

  ork::reflect::AnnotatePropertyForEditor<CCamera>("Loc", "editor.range.min", "0.1f");
  ork::reflect::AnnotatePropertyForEditor<CCamera>("Loc", "editor.range.max", "1000.0f");
}

///////////////////////////////////////////////////////////////////////////////

CCamera::CCamera()
    : CamFocus(0.0f, 0.0f, 0.0f), mfLoc(3.0f), mvCenter(0.0f, 0.0, 0.0f), QuatC(0.0f, -1.0f, 0.0f, 0.0f), locscale(1.0f),
      LastMeasuredCameraVelocity(0.0f, 0.0f, 0.0f), MeasuredCameraVelocity(0.0f, 0.0f, 0.0f), mbInMotion(false), mpViewport(0),
      mfWorldSizeAtLocator(1.0f)
//, ManipHandler( *this )
{
  other_info = (std::string) "";
  mCameraData.SetLev2Camera(this);
  printf("SETLEV2CAM<%p>\n", this);
}

std::string CCamera::get_full_name(void) {
  std::string rval = type_name + (std::string) ":" + instance_name + (std::string) ":" + other_info;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CCamera::IsXVertical() const {
  const fvec3& yn = mCameraData.GetYNormal();
  float dotY = yn.Dot(fvec3(1.0f, 0.0f, 0.0f));
  return (float(CFloat::Abs(dotY)) > float(0.707f));
}

bool CCamera::IsYVertical() const {
  const fvec3& yn = mCameraData.GetYNormal();
  float dotY = yn.Dot(fvec3(0.0f, 1.0f, 0.0f));
  return (float(CFloat::Abs(dotY)) > float(0.707f));
}

bool CCamera::IsZVertical() const {
  const fvec3& yn = mCameraData.GetYNormal();
  float dotY = yn.Dot(fvec3(0.0f, 0.0f, 1.0f));
  return (float(CFloat::Abs(dotY)) > float(0.707f));
}

fquat CCamera::VerticalRot(float amt) const {
  fquat qrot;

  if (IsXVertical()) {
    fvec4 aarot(1.0f, 0.0f, 0.0f, amt);
    qrot.FromAxisAngle(aarot);
  } else if (IsYVertical()) {
    const fvec3& yn = mCameraData.GetYNormal();
    float dotY = yn.Dot(fvec3(0.0f, 1.0f, 0.0f));
    float fsign = (dotY > 0.0f) ? 1.0f : (dotY < 0.0f) ? -1.0f : 0.0f;

    fvec4 aarot(0.0f, 1.0f, 0.0f, amt * fsign);
    qrot.FromAxisAngle(aarot);
  } else if (IsZVertical()) {
    fvec4 aarot(0.0f, 0.0f, 1.0f, amt);
    qrot.FromAxisAngle(aarot);
  }
  return qrot;
}

fquat CCamera::HorizontalRot(float amt) const {
  fquat qrot;

  if (IsYVertical()) {
    fvec4 aarot(1.0f, 0.0f, 0.0f, amt);
    qrot.FromAxisAngle(aarot);
  } else if (IsXVertical()) {
    fvec4 aarot(0.0f, 1.0f, 0.0f, amt);
    qrot.FromAxisAngle(aarot);
  } else if (IsZVertical()) {
    fvec4 aarot(0.0f, 0.0f, 1.0f, amt);
    qrot.FromAxisAngle(aarot);
  }
  return qrot;
}

///////////////////////////////////////////////////////////////////////////////
// motion state tracking

bool CCamera::CheckMotion() {
  auto pVP = GetViewport();

  LastMeasuredCameraVelocity = MeasuredCameraVelocity;
  MeasuredCameraVelocity = (CamLoc - PrevCamLoc);

  float CurVelMag = MeasuredCameraVelocity.Mag();
  float LastVelMag = LastMeasuredCameraVelocity.Mag();

  fvec2 VP;
  if (pVP) {
    VP.SetX((float)pVP->GetW());
    VP.SetY((float)pVP->GetH());
  }
  fvec3 Pos = mvCenter;
  fvec3 UpVector;
  fvec3 RightVector;
  mCameraData.GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
  float CameraMotionThresh = RightVector.Mag() / float(1000.0f);

  if (mbInMotion) {
    if (CurVelMag < CameraMotionThresh) // start motion
    {
      // GetViewport()->GetTarget()->GetCtxBase()->PopRefreshPolicy();
      mbInMotion = false;
    }
  } else {
    if ((LastVelMag < CameraMotionThresh) && (CurVelMag > CameraMotionThresh)) // start motion
    {
      mbInMotion = true;
    }
  }

  return mbInMotion;
}

///////////////////////////////////////////////////////////////////////////////

void CCamera::CommonPostSetup(void) {
  ///////////////////////////////
  // billboard support

  float UpX = mCameraData.GetIVMatrix().GetElemXY(0, 0);
  float UpY = mCameraData.GetIVMatrix().GetElemXY(0, 1);
  float UpZ = mCameraData.GetIVMatrix().GetElemXY(0, 2);
  float RightX = mCameraData.GetIVMatrix().GetElemXY(1, 0);
  float RightY = mCameraData.GetIVMatrix().GetElemXY(1, 1);
  float RightZ = mCameraData.GetIVMatrix().GetElemXY(1, 2);

  vec_billboardUp = fvec4(UpX, UpY, UpZ);
  vec_billboardRight = fvec4(RightX, RightY, RightZ);

  ///////////////////////////////
  // generate frustum (useful for many things, like billboarding, clipping, LOD, etc.. )
  // we generate the frustum points, we should also generate plane eqns

  Frustum& frus = mCameraData.GetFrustum();
  frus.Set(mCameraData.GetIVPMatrix());
  mCameraData.SetXNormal(frus.mXNormal);
  mCameraData.SetYNormal(frus.mYNormal);
  mCameraData.SetZNormal(frus.mZNormal);

  ///////////////////////////////

  CamLoc = mvCenter + (mCameraData.GetZNormal() * (-mfLoc));
}

///////////////////////////////////////////////////////////////////////////////

float CCamera::ViewLengthToWorldLength(const fvec4& pos, float ViewLength) { return float(0.0f); }

///////////////////////////////////////////////////////////////////////////////

CManipHandler::CManipHandler() // const CCamera& pcam)
    : Origin(float(0.0f), float(0.0f), float(0.0f))
//	, mParentCamera(pcam)
{}

void CManipHandler::Init(const ork::fvec2& posubp, const fmtx4& RCurIMVPMat, const fquat& RCurQuat) {
  IMVPMat = RCurIMVPMat;
  Quat = RCurQuat;

  ///////////////////////////////////////

  mFrustum.Set(RCurIMVPMat);

  CamXNormal = mFrustum.mXNormal;
  CamYNormal = mFrustum.mYNormal;
  CamZNormal = mFrustum.mZNormal;

  ///////////////////////////////////////

  IntersectXZ(posubp, XZIntersectBase, XZAngleBase);
  IntersectYZ(posubp, YZIntersectBase, YZAngleBase);
  IntersectXY(posubp, XYIntersectBase, XYAngleBase);
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectXZ(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  YNormal = fmtx4::Identity.GetYNormal();
  XZPlane.CalcFromNormalAndOrigin(YNormal, Origin);
  float isect_dist;
  fray3 ray;
  ray.mOrigin = RayNear;
  ray.mDirection = RayZNormal;
  DoesIntersectXZ = XZPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectXZ)
    XZAngle = rect2pol_ang(Intersection.GetX(), Intersection.GetZ());

  Angle = XZAngle;

  return DoesIntersectXZ;
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectYZ(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  XNormal = fmtx4::Identity.GetXNormal();
  YZPlane.CalcFromNormalAndOrigin(XNormal, Origin);

  float isect_dist;
  fray3 ray;
  ray.mOrigin = RayNear;
  ray.mDirection = RayZNormal;
  DoesIntersectYZ = YZPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectYZ)
    YZAngle = rect2pol_ang(Intersection.GetY(), Intersection.GetZ());

  Angle = YZAngle;

  return DoesIntersectYZ;
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectXY(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  ZNormal = fmtx4::Identity.GetZNormal();
  XYPlane.CalcFromNormalAndOrigin(ZNormal, Origin);
  float isect_dist;
  fray3 ray;
  ray.mOrigin = RayNear;
  ray.mDirection = RayZNormal;
  DoesIntersectXY = XYPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectXY)
    XYAngle = rect2pol_ang(Intersection.GetX(), Intersection.GetY());

  Angle = XYAngle;

  return DoesIntersectXY;
}

///////////////////////////////////////////////////////////////////////////////

void CManipHandler::Intersect(const ork::fvec2& posubp) {
  fvec3 Intersection;
  float Angle;
  IntersectXZ(posubp, Intersection, Angle);
  IntersectXY(posubp, Intersection, Angle);
  IntersectYZ(posubp, Intersection, Angle);
  // Intersection.dump( "manipisec isXZ" );
  // Intersection.dump( "manipisec isYZ" );
  // Intersection.dump( "manipisec isXY" );
}

///////////////////////////////////////////////////////////////////////////////

fvec4 TRayN;
fvec4 TRayF;

void CManipHandler::GenerateIntersectionRays(const ork::fvec2& posubp, fvec3& RayZNormal, fvec3& RayNear) {
  fvec3 RayFar;
  ///////////////////////////////////////////
  fvec3 vWinN(posubp.GetX(), posubp.GetY(), 0.0f);
  fvec3 vWinF(posubp.GetX(), posubp.GetY(), 1.0f);
  fmtx4::UnProject(IMVPMat, vWinN, RayNear);
  fmtx4::UnProject(IMVPMat, vWinF, RayFar);
  TRayN = RayNear;
  TRayF = RayFar;
  ///////////////////////////////////////////
  fvec3 RayD = (RayFar - RayNear);
  ///////////////////////////////////////////
  double draydX = (double)RayD.GetX();
  double draydY = (double)RayD.GetY();
  double draydZ = (double)RayD.GetZ();
  double drayD = 1.0f / sqrt((draydX * draydX) + (draydY * draydY) + (draydZ * draydZ));
  draydX *= drayD;
  draydY *= drayD;
  draydZ *= drayD;
  ///////////////////////////////////////////
  RayZNormal.SetXYZ((f32)draydX, (f32)draydY, (f32)draydZ);
  ///////////////////////////////////////////
}

}} // namespace ork::lev2
