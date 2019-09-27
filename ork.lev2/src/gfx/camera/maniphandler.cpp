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

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

ManipHandler::ManipHandler() // const Camera& pcam)
    : Origin(float(0.0f), float(0.0f), float(0.0f))
//	, mParentCamera(pcam)
{}

void ManipHandler::Init(const ork::fvec2& posubp, const fmtx4& RCurIMVPMat, const fquat& RCurQuat) {
  IMVPMat = RCurIMVPMat;
  Quat    = RCurQuat;

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

bool ManipHandler::IntersectXZ(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  YNormal = fmtx4::Identity.GetYNormal();
  XZPlane.CalcFromNormalAndOrigin(YNormal, Origin);
  float isect_dist;
  fray3 ray;
  ray.mOrigin     = RayNear;
  ray.mDirection  = RayZNormal;
  DoesIntersectXZ = XZPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectXZ)
    XZAngle = rect2pol_ang(Intersection.GetX(), Intersection.GetZ());

  Angle = XZAngle;

  return DoesIntersectXZ;
}

///////////////////////////////////////////////////////////////////////////////

bool ManipHandler::IntersectYZ(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  XNormal = fmtx4::Identity.GetXNormal();
  YZPlane.CalcFromNormalAndOrigin(XNormal, Origin);

  float isect_dist;
  fray3 ray;
  ray.mOrigin     = RayNear;
  ray.mDirection  = RayZNormal;
  DoesIntersectYZ = YZPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectYZ)
    YZAngle = rect2pol_ang(Intersection.GetY(), Intersection.GetZ());

  Angle = YZAngle;

  return DoesIntersectYZ;
}

///////////////////////////////////////////////////////////////////////////////

bool ManipHandler::IntersectXY(const ork::fvec2& posubp, fvec3& Intersection, float& Angle) {
  fvec3 RayZNormal;
  GenerateIntersectionRays(posubp, RayZNormal, RayNear);
  ZNormal = fmtx4::Identity.GetZNormal();
  XYPlane.CalcFromNormalAndOrigin(ZNormal, Origin);
  float isect_dist;
  fray3 ray;
  ray.mOrigin     = RayNear;
  ray.mDirection  = RayZNormal;
  DoesIntersectXY = XYPlane.Intersect(ray, isect_dist, Intersection);

  if (DoesIntersectXY)
    XYAngle = rect2pol_ang(Intersection.GetX(), Intersection.GetY());

  Angle = XYAngle;

  return DoesIntersectXY;
}

///////////////////////////////////////////////////////////////////////////////

void ManipHandler::Intersect(const ork::fvec2& posubp) {
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

void ManipHandler::GenerateIntersectionRays(const ork::fvec2& posubp, fvec3& RayZNormal, fvec3& RayNear) {
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
  double drayD  = 1.0f / sqrt((draydX * draydX) + (draydY * draydY) + (draydZ * draydZ));
  draydX *= drayD;
  draydY *= drayD;
  draydZ *= drayD;
  ///////////////////////////////////////////
  RayZNormal.SetXYZ((f32)draydX, (f32)draydY, (f32)draydZ);
  ///////////////////////////////////////////
}

} // namespace ork::lev2
