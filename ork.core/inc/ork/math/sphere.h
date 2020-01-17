////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/box.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
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
  AABox projectedBounds(const fmtx4& mvp) const;
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

inline AABox Sphere::projectedBounds(const fmtx4& mvp) const {

    fvec3 c0 = mCenter+fvec3(-mRadius,-mRadius,-mRadius);
    fvec3 c1 = mCenter+fvec3(-mRadius,-mRadius,+mRadius);
    fvec3 c2 = mCenter+fvec3(-mRadius,+mRadius,-mRadius);
    fvec3 c3 = mCenter+fvec3(-mRadius,+mRadius,+mRadius);
    fvec3 c4 = mCenter+fvec3(+mRadius,-mRadius,-mRadius);
    fvec3 c5 = mCenter+fvec3(+mRadius,-mRadius,+mRadius);
    fvec3 c6 = mCenter+fvec3(+mRadius,+mRadius,-mRadius);
    fvec3 c7 = mCenter+fvec3(+mRadius,+mRadius,+mRadius);

    fvec4 c0x = fvec4(c0,1).Transform(mvp);
    fvec4 c1x = fvec4(c1,1).Transform(mvp);
    fvec4 c2x = fvec4(c2,1).Transform(mvp);
    fvec4 c3x = fvec4(c3,1).Transform(mvp);
    fvec4 c4x = fvec4(c4,1).Transform(mvp);
    fvec4 c5x = fvec4(c5,1).Transform(mvp);
    fvec4 c6x = fvec4(c6,1).Transform(mvp);
    fvec4 c7x = fvec4(c7,1).Transform(mvp);

    AABox rval;
    rval.BeginGrow();
      rval.Grow(c0x.perspectiveDivided().xyz());
      rval.Grow(c1x.perspectiveDivided().xyz());
      rval.Grow(c2x.perspectiveDivided().xyz());
      rval.Grow(c3x.perspectiveDivided().xyz());
      rval.Grow(c4x.perspectiveDivided().xyz());
      rval.Grow(c5x.perspectiveDivided().xyz());
      rval.Grow(c6x.perspectiveDivided().xyz());
      rval.Grow(c7x.perspectiveDivided().xyz());
    rval.EndGrow();
    return rval;
}

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
