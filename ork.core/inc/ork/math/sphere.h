////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/box.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/line.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Sphere {
  fvec3 mCenter;
  float mRadius;

  Sphere(const fvec3 &pos, float r) : mCenter(pos), mRadius(r) {}
  Sphere(const fvec3 &boxmin, const fvec3 &boxmax);

  void SupportMapping(const fvec3 &v, fvec3 &result) const;

  bool Intersect(const fray3 &ray, fvec3 &isect_in, fvec3 &isect_out,
                 fvec3 &sphnormal) const;
  // AABox2D projectedBounds(const fmtx4& mvp) const;
};

///////////////////////////////////////////////////////////////////////////////

inline void Sphere::SupportMapping(const fvec3 &v, fvec3 &result) const {}

inline bool Sphere::Intersect(const fray3 &ray, fvec3 &isect_in,
                              fvec3 &isect_out, fvec3 &isect_normal) const {
  fvec3 L = mCenter - ray.mOrigin;
  float tca = L.Dot(ray.mDirection);
  if (tca < 0.0f)
    return false;
  float d2 = L.Dot(L) - (tca * tca);
  float r2 = (mRadius * mRadius);
  if (d2 > r2)
    return false;
  float thc = sqrtf(r2 - d2);
  isect_in = ray.mOrigin + ray.mDirection * (tca - thc);
  isect_out = ray.mOrigin + ray.mDirection * (tca + thc);
  isect_normal = (isect_in - mCenter).Normal();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
/*
AABox Sphere::projectedBounds(const fmtx4& mvp) const {

    float d2 = mCenter.Dot(mCenter);
    float a = 1.0f/sqrt(d2 - mRadius * mRadius);
    fvec3 right = (mRadius*a) * vec3(-mCenter.z, 0, mCenter.x);
    fvec3 up = fvec3(0,mRadius,0);

    fvec4 ctr = fvec4(center,1).Transform(mvp);
    fvec4 right  = fvec4(right,0).Transform(mvp);
    fvec4 up     = fvec4(up,0).Transform(mvp);

    AABox2D rval;
    rval.BeginGrow();
      rval.Grow((ctr+up).PerspectiveDivide().xyz());
      rval.Grow((ctr+right).PerspectiveDivide().xyz());
      rval.Grow((ctr-up).PerspectiveDivide().xyz());
      rval.Grow((ctr-right).PerspectiveDivide().xyz());
    rval.EndGrow();

    return rval;
}*/

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Circle {
  fvec2 mCenter;
  float mRadius;

  Circle(const fvec2 &pos, float r) : mCenter(pos), mRadius(r) {}
};

//////////////////////////
// TRIANGLE INTERSECT

//  http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld019.htm
//  http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld021.htm

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
