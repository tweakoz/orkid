////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/line.h>

////////////////////////////////////////////////////////////////////////////////
//	misc
////////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> class Plane {
  static T Abs(T in);
  static T Epsilon();

public: //
  //////////
  Vector3<T> n;
  T d;
  //////////
  Plane();                                                                    /// set explicitly to 0,0,0,0
  Plane(const Vector4<T>& vec);                                               /// init from normal and distance
  Plane(const Vector3<T>& vec, T dist);                                       /// init from normal and distance
  Plane(const Vector3<T>& pta, const Vector3<T>& ptb, const Vector3<T>& ptc); /// init from triangle
  Plane(T nx, T ny, T nz, T dist);                                            /// init from normal and distance
  Plane(T* f32p);                                                             /// set explicitly
  Plane(const Vector3<T>& NormalVec, const Vector3<T>& PosVec);               /// calc given normal and position of plane origin
  void CalcFromNormalAndOrigin(
      const Vector3<T>& NormalVec,
      const Vector3<T>& PosVec); //! calc given normal and position of plane origin
  ~Plane();
  void Reset(void);
  bool IsPointInFront(const Vector3<T>& pt) const;
  bool IsPointBehind(const Vector3<T>& pt) const;
  void CalcD(const Vector3<T>& pt);
  bool IsOn(const Vector3<T>& pt) const;
  void CalcNormal(const Vector3<T>& pta, const Vector3<T>& ptb, const Vector3<T>& ptc);

  //////////////////////////////////

  bool Intersect(const TLineSegment3<T>& seg, T& dis, Vector3<T>& res) const;
  bool Intersect(const Ray3<T>& ray, T& dis, Vector3<T>& res) const;
  bool Intersect(const Ray3<T>& ray, T& dis) const;

  //////////////////////////////////

  T pointDistance(const Vector3<T>& pt) const;
  const Vector3<T>& GetNormal(void) const;
  const T& GetD(void) const;
  void crossProduct(F64 ii1, F64 jj1, F64 kk1, F64 ii2, F64 jj2, F64 kk2, F64& iicp, F64& jjcp, F64& kkcp) const;
  void CalcPlaneFromTriangle(const Vector3<T>& p0, const Vector3<T>& p1, const Vector3<T>& p2, f64 ftolerance = EPSILON);

  bool IsCoPlanar(const Plane<T>& OtherPlane) const;

  bool PlaneIntersect(const Plane<T>& oth, Vector3<T>& outpos, Vector3<T>& outdir) const;

  template <typename PolyType> bool ClipPoly(const PolyType& PolyToClip, PolyType& OutPoly);

  template <typename PolyType> bool ClipPoly(const PolyType& PolyToClip, PolyType& OutPolyFront, PolyType& OutPolyBack);

  void SimpleTransform(const Matrix44<T>& transform);

  Plane<T> operator-() const;

  //////////////////////////////////
  void EndianSwap();
  //////////////////////////////////
};

using fplane3       = Plane<float>;
using fplane3_ptr_t = std::shared_ptr<fplane3>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
