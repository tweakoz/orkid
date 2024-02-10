////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/math/math_types.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/line.h>
#include <ork/util/crc.h>

////////////////////////////////////////////////////////////////////////////////
//	misc
////////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

enum class PointClassification : uint32_t {
  CrcEnum(FRONT),
  CrcEnum(BACK),
  CrcEnum(COPLANAR)
};

template <typename T> class Plane {
  static T Abs(T in);
  static T Epsilon();

public: //

  using vect3_t = Vector3<T>;
  using vect4_t = Vector4<T>;

  //////////
  vect3_t n;
  T d;
  //////////
  Plane();                                                                    /// set explicitly to 0,0,0,0
  Plane(const vect4_t& vec);                                               /// init from normal and distance
  Plane(const vect3_t& vec, T dist);                                       /// init from normal and distance
  Plane(const vect3_t& pta, const vect3_t& ptb, const vect3_t& ptc); /// init from triangle
  Plane(T nx, T ny, T nz, T dist);                                            /// init from normal and distance
  Plane(T* f32p);                                                             /// set explicitly
  Plane(const vect3_t& NormalVec, const vect3_t& PosVec);               /// calc given normal and position of plane origin

  Plane(const kln::plane& klein_plane);                                            /// init from normal and distance

  void CalcFromNormalAndOrigin(
      const vect3_t& NormalVec,
      const vect3_t& PosVec); //! calc given normal and position of plane origin
  ~Plane();
  void Reset(void);
  bool isPointInFront(const vect3_t& pt) const;
  bool isPointBehind(const vect3_t& pt) const;
  bool isPointCoplanar(const vect3_t& pt) const;
  PointClassification classifyPoint(const vect3_t& pt) const;
  void CalcD(const vect3_t& pt);
  bool IsOn(const vect3_t& pt) const;
  void CalcNormal(const vect3_t& pta, const vect3_t& ptb, const vect3_t& ptc);
  vect3_t closestPointOnPlane(const vect3_t& pt) const;
  
  //////////////////////////////////

  bool Intersect(const TLineSegment3<T>& seg, T& dis, vect3_t& res) const;
  bool Intersect(const Ray3<T>& ray, T& dis, vect3_t& res) const;
  bool Intersect(const Ray3<T>& ray, T& dis) const;

  //////////////////////////////////

  vect3_t reflect(const vect3_t& pt) const;

  //////////////////////////////////

  T pointDistance(const vect3_t& pt) const;
  const vect3_t& GetNormal(void) const;
  T GetD(void) const;
  void crossProduct(T ii1, T jj1, T kk1, T ii2, T jj2, T kk2, T& iicp, T& jjcp, T& kkcp) const;
  void CalcPlaneFromTriangle(const vect3_t& p0, const vect3_t& p1, const vect3_t& p2, T ftolerance = EPSILON);

  bool IsCoPlanar(const Plane<T>& OtherPlane) const;

  bool PlaneIntersect(const Plane<T>& oth, vect3_t& outpos, vect3_t& outdir) const;

  template <typename PolyType> bool ClipPoly(const PolyType& PolyToClip, PolyType& OutPoly) const;

  template <typename PolyType> bool ClipPoly(const PolyType& PolyToClip, PolyType& OutPolyFront, PolyType& OutPolyBack) const;

  void SimpleTransform(const Matrix44<T>& transform);


  Plane<T> operator-() const;

  //////////////////////////////////
  void EndianSwap();
  //////////////////////////////////

  uint64_t hash(T normal_quant=T(16384), T dist_quant=T(4096)) const;
};

using fplane       = Plane<float>;
using fplane3       = Plane<float>;
using fplane3_ptr_t = std::shared_ptr<fplane3>;

using dplane       = Plane<double>;
using dplane3       = Plane<double>;
using dplane3_ptr_t = std::shared_ptr<dplane3>;

dplane3 fplane3_to_dplane3(const fplane3& fplane3);
fplane3 dplane3_to_fplane3(const dplane3& dplane3);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
