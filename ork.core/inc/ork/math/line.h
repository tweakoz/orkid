////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/any.h>
#include <ork/math/cvector2.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> class InfiniteLine2D {
  typedef Vector2<T> vec2_type;

protected:
  vec2_type mNormal; // A, B
  T mfC;             // C
                     // Ax+By+c==0
public:
  InfiniteLine2D()
      : mNormal(T(0.0f), T(1.0f))
      , mfC(T(0.0f)) {
  }

  inline void CalcFromNormalAndOrigin(const vec2_type& Normal, const vec2_type& PosVec) {
    // mFC = C
    mNormal = Normal;
    mfC     = T(0.0f);
    mfC     = pointDistance(PosVec) * T(-1.0f);
  }
  inline void CalcFromTwoPoints(const vec2_type& Pnt0, const vec2_type& Pnt1) {
    T dX = (Pnt1.x - Pnt0.x);
    if (dX == T(0.0f)) {
      if (Pnt1.y < Pnt0.y)
        mNormal.set(1.0f, 0.0f);
      else if (Pnt1.y > Pnt0.y)
        mNormal.set(-1.0f, 0.0f);
      else {
        OrkAssert(false); // pnt1 must != pnt0
      }
    } else { // b = y-mx
      // a = m*b
      // c = -ax-by
      T dY = (Pnt1.y - Pnt0.y);
      T m  = dY / dX;
      T b  = (Pnt0.y - m * Pnt0.x);
      T a  = m * b;
      mNormal.set(a, b);
      mNormal.Normalize();
      T c = -a * Pnt0.x - b * Pnt0.y;
      mfC = c;
    }
  }
  inline void CalcFromTwoPoints(float x0, float y0, float x1, float y1) {
    T dX = (x1 - x0);
    if (dX == T(0.0f)) {
      if (y1 < y0)
        mNormal.set(1.0f, 0.0f);
      else if (y1 > y0)
        mNormal.set(-1.0f, 0.0f);
      else {
        OrkAssert(false); // pnt1 must != pnt0
      }
    } else { // b = y-mx
      // a = m*b
      // c = -ax-by

      // m = -A/B
      // 1/m = B/-A
      // B = -A/m
      // b = -C/B
      // 1/b = B/-C
      // B = -C/b

      // B = -A/m
      // B = -C/b
      // -A/m = -C/b
      // m/-A = b/-C
      // m = -Ab/-C
      // m = Ab/C
      // Ab/C = -A/B
      // Ab = -AC/B
      // b = -C/B
      // bB = -C
      // C = -bB
      // C = bA/m
      // Ax-Ay/m+bA/m = 0
      // Ax=Ay/m-bA/m
      // Axm=Ay-Ab
      // Axm+Ab-Ay=0
      // Axm+Ab-Ay+1=1

      T dY = (y1 - y0);
      T m  = dY / dX;
      T b  = (y0 - m * x0);
      T a  = m * b;
      mNormal.set(a, b);
      mNormal.Normalize();
      T c  = -a * x0 - b * y0;
      T fZ = a * x0 + b * y0 + c;
      mfC  = -c;
    }
  }
  float pointDistance(const vec2_type& point) const {
    return mNormal.dotWith(point) + mfC;
  }
  float pointDistance(float fx, float fy) const {
    return (mNormal.x * fx) + (mNormal.y * fy) + mfC;
  }
  bool IsPointInFront(const vec2_type& point) const {
    T distance = pointDistance(point);
    return (distance >= T(0.0f));
  }
  bool IsPointBehind(const vec2_type& point) const {
    return (!IsPointInFront(point));
  }
  bool IsPointInFront(float fx, float fy) const {
    T distance = pointDistance(fx, fy);
    return (distance >= T(0.0f));
  }
  bool IsPointBehind(float fx, float fy) const {
    return (!IsPointInFront(fx, fy));
  }
};

template <typename T> struct TLineSegment3 {
  typedef Vector3<T> vec3_type;

  vec3_type mStart;
  vec3_type mEnd;

  TLineSegment3(const vec3_type& s, const vec3_type& e)
      : mStart(s)
      , mEnd(e) {
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class TLineSegment2 {
  typedef Vector2<T> vec2_type;

protected:
  vec2_type mStart;
  vec2_type mEnd;

  TLineSegment2(const vec2_type& s, const vec2_type& e)
      : mStart(s)
      , mEnd(e) {
  }
  TLineSegment2(){};
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class TLineSegment2Helper {
  typedef Vector2<T> vec2_type;

  vec2_type mStart;
  vec2_type mEnd;
  vec2_type mOrigin;
  T mMag;

public:
  float pointDistanceSquared(const vec2_type& pt) const;
  float pointDistancePercent(const vec2_type& pt) const;
  T GetMag() const {
    return (mMag);
  }
  TLineSegment2Helper(const vec2_type& s, const vec2_type& e);
  TLineSegment2Helper();
  void SetStartEnd(const vec2_type& s, const vec2_type& e);
  T GetStartX() const {
    return (mStart.x);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Ray3 {

  using vec3_type = Vector3<T>;

  Ray3()
      : mID(-1) {
  }
  Ray3(const vec3_type& o, const vec3_type& d)
      : mOrigin(o)
      , mDirection(d)
      , mInverseDirection(1.0f / d.x, 1.0f / d.y, 1.0f / d.z)
      , mID(-1) {
    mdot_dd = mDirection.dotWith(mDirection);
    mdot_do = mDirection.dotWith(mOrigin);
    mdot_oo = mOrigin.dotWith(mOrigin);
    mbSignX = (mInverseDirection.x >= 0.0f);
    mbSignY = (mInverseDirection.y >= 0.0f);
    mbSignZ = (mInverseDirection.z >= 0.0f);
  }
  void SetID(int id) {
    mID = id;
  }
  int GetID() const {
    return mID;
  }

  void lerp(const Ray3& a, const Ray3& b, float fi) {
    vec3_type o, d;
    o.lerp(a.mOrigin, b.mOrigin, fi);
    d.lerp(a.mDirection, b.mDirection, fi);
    d.normalizeInPlace();
    *this = Ray3(o, d);
  }
  void BiLerp(const Ray3& x0y0, const Ray3& x1y0, const Ray3& x0y1, const Ray3& x1y1, float fx, float fy) {
    Ray3 t;
    t.lerp(x0y0, x1y0, fx);
    Ray3 b;
    b.lerp(x0y1, x1y1, fx);
    lerp(t, b, fy);
  }

  bool intersectSegment(
      const TLineSegment3<T>& segment,      //
      vec3_type& intersect_out,          //
      float coplanar_threshold   = 0.7f, //
      float length_error_threshold = 1e-3f //
  ) const { //

    auto da = mDirection; // Unnormalized direction of the ray
    auto db = segment.mEnd - segment.mStart;
    auto dc = segment.mStart - mOrigin;

    if (fabs(dc.dotWith(da.crossWith(db))) >= coplanar_threshold) // Lines are not coplanar
      return false;

    T s = dc.crossWith(db).dotWith(da.crossWith(db)) / da.crossWith(db).magnitudeSquared();

    if (s >= T(0.0) && s <= T(1.0)) { // intersection ?

      intersect_out = mOrigin + s * da;

      // See if this lies on the segment
      if( ((intersect_out - segment.mStart).magnitudeSquared() //
        + (intersect_out - segment.mEnd).magnitudeSquared()) //
        <= //
          (segment.mEnd-segment.mStart).magnitudeSquared() //
          + length_error_threshold) {
          return true;
      }
    }

    return false;
  }

  vec3_type mOrigin;
  vec3_type mDirection;
  vec3_type mInverseDirection;
  bool mbSignX;
  bool mbSignY;
  bool mbSignZ;
  T mdot_dd;
  T mdot_do;
  T mdot_oo;
  int mID;
};

using fray3            = Ray3<float>;
using dray3            = Ray3<double>;
using fray3_ptr_t      = std::shared_ptr<fray3>;
using fray3_constptr_t = std::shared_ptr<const fray3>;
using dray3_ptr_t      = std::shared_ptr<dray3>;
using dray3_constptr_t = std::shared_ptr<const dray3>;

//using LineSegment2 = TLineSegment2<float>;
//using LineSegment3 = TLineSegment3<float>;
using LineSegment2Helper = TLineSegment2Helper<float>;

using flineseg2 = TLineSegment2<float>;
using flineseg3 = TLineSegment3<float>;
using dlineseg2 = TLineSegment2<double>;
using dlineseg3 = TLineSegment3<double>;

///////////////////////////////////////////////////////////////////////////////
// temporary till all code done being refactored

struct Ray3HitTest {
  int miSphTests;
  int miSphTestsPassed;
  int miTriTests;
  int miTriTestsPassed;
  Ray3HitTest()
      : miSphTests(0)
      , miSphTestsPassed(0)
      , miTriTests(0)
      , miTriTestsPassed(0) {
  }
  void OnHit(const any32& userdata, const fray3& r) {
    DoOnHit(userdata, r);
  }
  virtual void DoOnHit(const any32& userdata, const fray3& r) {
  }
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
