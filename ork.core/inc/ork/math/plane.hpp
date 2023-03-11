////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <ork/math/plane.h>
#include <ork/math/math_types.inl>
#include <ork/util/endian.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {

template <typename T> bool Plane<T>::IsCoPlanar(const Plane<T>& OtherPlane) const {
  T fdot  = Abs(n.dotWith(OtherPlane.n) - T(1.0f));
  T fDelD = Abs(d - OtherPlane.d);
  return ((fdot < T(0.01f)) && (fDelD < T(0.01f)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane()
    : n()
    , d(T(0)) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(const Vector4<T>& vec)
    : n(vec.xyz())
    , d(vec.w) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(const Vector3<T>& vec, T nd)
    : n(vec)
    , d(nd) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T>::Plane(const Vector3<T>& pta, const Vector3<T>& ptb, const Vector3<T>& ptc) {
  CalcPlaneFromTriangle(pta, ptb, ptc);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(T nx, T ny, T nz, T dist)
    : n(nx, ny, nz)
    , d(dist) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(const kln::plane& klein_plane)
  : n(klein_plane.x(),klein_plane.y(),klein_plane.z())
  , d(klein_plane.d()){
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(T* Tp) //! set explicitly
{
  n = Vector3<T>(Tp[0], Tp[1], Tp[2]);
  d = Tp[3];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(const Vector3<T>& NormalVec, const Vector3<T>& PosVec) //! calc given normal and position of plane origin
{
  CalcFromNormalAndOrigin(NormalVec, PosVec);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T>::~Plane() {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Plane<T>::CalcFromNormalAndOrigin(
    const Vector3<T>& NormalVec,
    const Vector3<T>& PosVec) //! calc given normal and position of plane origin
{
  n = NormalVec;
  d = T(0.0f);
  d = pointDistance(PosVec) * T(-1.0f);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::Reset(void) {
  n = Vector3<T>(T(0.0f), T(0.0f), T(0.0f));
  d = T(0.0f);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::IsPointInFront(const Vector3<T>& point) const {
  T distance = pointDistance(point);
  return (distance >= T(0.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::IsPointBehind(const Vector3<T>& point) const {
  return (!IsPointInFront(point));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::CalcD(const Vector3<T>& pt) {
  d = -pt.dotWith(n);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Plane<T>::pointDistance(const Vector3<T>& pt) const {
  return n.dotWith(pt) + d;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Plane<T>::GetNormal(void) const {
  return n;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const T& Plane<T>::GetD(void) const {
  return d;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Plane<T>::crossProduct(F64 ii1, F64 jj1, F64 kk1, F64 ii2, F64 jj2, F64 kk2, F64& iicp, F64& jjcp, F64& kkcp) const {
  iicp = (jj1 * kk2) - (jj2 * kk1);
  jjcp = (ii2 * kk1) - (ii1 * kk2);
  kkcp = (ii1 * jj2) - (ii2 * jj1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> static bool AreValuesCloseEnoughtToBeEqual(f64 a, f64 b, f64 ftolerance) {
  return (fabs(a - b) >= ftolerance) ? false : true;
}

///////////////////////////////////////////////////////////////////////////////
// ftolerance = smallest distance to consider a point colinear

template <typename T>
void Plane<T>::CalcPlaneFromTriangle(const Vector3<T>& p0, const Vector3<T>& p1, const Vector3<T>& p2, F64 ftolerance) {
  F64 ii1, jj1, kk1;
  F64 ii2, jj2, kk2;
  F64 iicp, jjcp, kkcp;
  F64 len0, len1, len2;

  Vector4<T> p1mp0 = p1 - p0;
  Vector4<T> p2mp1 = p2 - p1;
  Vector4<T> p0mp2 = p0 - p2;

  len0 = p1mp0.magnitude();
  len1 = p2mp1.magnitude();
  len2 = p0mp2.magnitude();

  if ((len0 >= len1) && (len0 >= len2)) {
    ii1 = (p0.x - p2.x);
    ii2 = (p1.x - p2.x);
    jj1 = (p0.y - p2.y);
    jj2 = (p1.y - p2.y);
    kk1 = (p0.z - p2.z);
    kk2 = (p1.z - p2.z);
  } else if ((len1 >= len0) && (len1 >= len2)) {
    ii1 = (p1.x - p0.x);
    ii2 = (p2.x - p0.x);
    jj1 = (p1.y - p0.y);
    jj2 = (p2.y - p0.y);
    kk1 = (p1.z - p0.z);
    kk2 = (p2.z - p0.z);
  } else {
    ii1 = (p2.x - p1.x);
    ii2 = (p0.x - p1.x);
    jj1 = (p2.y - p1.y);
    jj2 = (p0.y - p1.y);
    kk1 = (p2.z - p1.z);
    kk2 = (p0.z - p1.z);
  }

  crossProduct(ii2, jj2, kk2, ii1, jj1, kk1, iicp, jjcp, kkcp);

  //   assert(!(IS_EQ(iicp,0.0) && IS_EQ(jjcp,0.0) && IS_EQ(kkcp,0.0)));
  if (AreValuesCloseEnoughtToBeEqual<T>(iicp, 0.0, ftolerance) && AreValuesCloseEnoughtToBeEqual<T>(jjcp, 0.0, ftolerance) &&
      AreValuesCloseEnoughtToBeEqual<T>(kkcp, 0.0, ftolerance)) {
    // orkprintf( "Whoops, 3 colinear points in a quad.\n");
    return;
  }
  // if(IS_EQ_TIGHT(plane->aa,0)&&IS_EQ_TIGHT(plane->bb,0)&&IS_EQ_TIGHT(plane->cc,0))
  //{	//messageh( DEBUG_PANE, "Plane error\n");
  //}

  // get plane normal

  n = Vector3<T>((T)iicp, (T)jjcp, (T)kkcp).normalized();

  // get plane distance from origin
  d = T(0.0f);
  d = pointDistance(p0) * T(-1.0f);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::IsOn(const Vector3<T>& pt) const {
  T d = pointDistance(pt);
  return (Abs(d) < Epsilon()) ? true : false;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::CalcNormal(const Vector3<T>& pta, const Vector3<T>& ptb, const Vector3<T>& ptc) {
  Vector3<T> bminusa = (ptb - pta);
  Vector3<T> cminusa = (ptc - pta);

  n = bminusa.crossWith(cminusa).normalized();
  d = -ptc.dotWith(n);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::Intersect(const TLineSegment3<T>& lseg, T& dis, Vector3<T>& result) const {
  Vector3<T> dif = (lseg.mEnd - lseg.mStart);
  T length       = dif.magnitude();

  Ray3<T> ray;
  ray.mDirection = dif * (T(1.0f) / length); // cheaper normalize since we need the length anyway
  ray.mOrigin    = lseg.mStart;

  bool bOK = Intersect(ray, dis, result);

  bOK &= ((dis >= 0.0f) && (dis < length));

  return bOK;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::Intersect(const Ray3<T>& ray, T& dis, Vector3<T>& res) const {
  bool bOK = Intersect(ray, dis);

  if (bOK) {
    res = ray.mOrigin + (ray.mDirection * dis);
  }
  return bOK;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::Intersect(const Ray3<T>& ray, T& dis) const {
  T denom = n.dotWith(ray.mDirection);
  // Line is parallel to the plane or plane normal faces away from ray
  if (Abs(denom) < Epsilon())
    return false;

  T pointdist = pointDistance(ray.mOrigin);
  T u         = -pointdist / (denom);

  dis = u;

  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> template <typename PolyType> bool Plane<T>::ClipPoly(const PolyType& PolyToClip, PolyType& OutPoly) {
  const int inuminverts = (int)PolyToClip.GetNumVertices();
  if (inuminverts) {
    const typename PolyType::VertexType& StartVtx = PolyToClip.GetVertex(0);
    bool IsVtxAIn                                 = IsPointInFront(StartVtx.Pos());
    // get the side of each vert to the plane
    for (int iva = 0; iva < inuminverts; iva++) {
      int ivb                                 = ((iva == inuminverts - 1) ? 0 : iva + 1);
      const typename PolyType::VertexType& vA = PolyToClip.GetVertex(iva);
      const typename PolyType::VertexType& vB = PolyToClip.GetVertex(ivb);
      if (IsVtxAIn) {
        OutPoly.AddVertex(vA);
      }
      bool IsVtxBIn = IsPointInFront(vB.Pos());
      if (IsVtxBIn != IsVtxAIn) {
        fvec3 vPos;
        T isectdist;
        LineSegment3 lseg(vA.Pos(), vB.Pos());
        if (Intersect(lseg, isectdist, vPos)) {
          T fDist   = (vA.Pos() - vB.Pos()).magnitude();
          T fDist2  = (vA.Pos() - vPos).magnitude();
          T fScalar = (Abs(fDist) < Epsilon()) ? 0.0f : fDist2 / fDist;
          typename PolyType::VertexType LerpedVertex;
          LerpedVertex.lerp(vA, vB, fScalar);
          OutPoly.AddVertex(LerpedVertex);
        }
      }
      IsVtxAIn = IsVtxBIn;
    }
  }
  return (OutPoly.GetNumVertices() >= 3);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> //
template <typename PolyType> //
bool Plane<T>::ClipPoly(const PolyType& input_poly, //
                        PolyType& out_front_poly, //
                        PolyType& out_back_poly) { //

  const int inuminverts                         = input_poly.GetNumVertices();
  OrkAssert(input_poly.GetNumVertices()>=3);

  //printf( "clip poly num verts<%d>\n", inuminverts );

  for (int iva = 0; iva < inuminverts; iva++) {
    //printf( "  iva<%d> of inuminverts<%d>\n", iva, inuminverts );
    int ivb                                 = ((iva == inuminverts - 1) ? 0 : iva + 1);
    const auto& vA = input_poly.GetVertex(iva);
    const auto& vB = input_poly.GetVertex(ivb);

    // get the side of each vert to the plane
    bool is_vertex_a_front = IsPointInFront(vA.Pos());
    bool is_vertex_b_front = IsPointInFront(vB.Pos());

    //printf( "  is_vertex_a_front<%d> is_vertex_b_front<%d>\n", int(is_vertex_a_front), int(is_vertex_b_front) );

    if (is_vertex_a_front) {
      out_front_poly.AddVertex(vA);
      //printf("  add a to front cnt<%d>\n", out_front_poly.GetNumVertices());
    } else {
      out_back_poly.AddVertex(vA);
      //printf("  add a to back cnt<%d>\n", out_back_poly.GetNumVertices());
    }

    if (is_vertex_b_front != is_vertex_a_front) {
      Vector3<T> vPos;
      T isectdist;
      TLineSegment3<T> lseg(vA.Pos(), vB.Pos());
      if (Intersect(lseg, isectdist, vPos)) {
        T fDist   = (vA.Pos() - vB.Pos()).magnitude();
        T fDist2  = (vA.Pos() - vPos).magnitude();
        T fScalar = (Abs(fDist) < Epsilon()) ? T(0.0) : fDist2 / fDist;
        typename PolyType::VertexType LerpedVertex;
        LerpedVertex.lerp(vA, vB, fScalar);
        out_front_poly.AddVertex(LerpedVertex);
        out_back_poly.AddVertex(LerpedVertex);
        //printf("  add l to front cnt<%d>\n", out_front_poly.GetNumVertices());
        //printf("  add l to front cnt<%d>\n", out_back_poly.GetNumVertices());
      }
      else{
        TLineSegment3<T> lseg2(vB.Pos(), vA.Pos());
        if (Intersect(lseg2, isectdist, vPos)) {
          T fDist   = (vB.Pos() - vA.Pos()).magnitude();
          T fDist2  = (vB.Pos() - vPos).magnitude();
          T fScalar = (Abs(fDist) < Epsilon()) ? T(0.0) : fDist2 / fDist;
          typename PolyType::VertexType LerpedVertex;
          LerpedVertex.lerp(vB, vA, fScalar);
          out_front_poly.AddVertex(LerpedVertex);
          out_back_poly.AddVertex(LerpedVertex);
          //printf("  add l2 to front cnt<%d>\n", out_front_poly.GetNumVertices());
          //printf("  add l2 to front cnt<%d>\n", out_back_poly.GetNumVertices());
        }

      }
    }
  }

  int numfront = out_front_poly.GetNumVertices();
  int numback = out_back_poly.GetNumVertices();

  OrkAssert(numfront==0 or numfront>=3);
  OrkAssert(numback==0 or numback>=3);
  OrkAssert((numfront+numback)>=inuminverts);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::EndianSwap() {
  swapbytes_dynamic(n[0]);
  swapbytes_dynamic(n[1]);
  swapbytes_dynamic(n[2]);
  swapbytes_dynamic(d);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::SimpleTransform(const Matrix44<T>& transform) {
  Vector3<T> point(n * -d);
  point = point.transform(transform).xyz();
  n     = n.transform3x3(transform).normalized();
  d = -n.dotWith(point);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T> Plane<T>::operator-() const {
  return Plane<T>(-n, n * -d);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
