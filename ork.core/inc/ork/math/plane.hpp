////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
typename Plane<T>::vect3_t Plane<T>::reflect(const vect3_t& pt) const {
  // Calculate a point on the plane using the plane's normal and distance.
  // Any point on the plane can be represented as n * d, 
  // where n is the normalized normal and d is the distance from the origin.
  vect3_t pointOnPlane = n * d;
  
  // Calculate the vector from the point on the plane to the input point.
  vect3_t toPointVec = pt - pointOnPlane;
  
  // Project this vector onto the plane's normal.
  T projectionLength = toPointVec.dotWith(n);
  
  // Calculate the reflection vector: Reflect the point across the plane.
  vect3_t reflection = pt - (n * projectionLength*T(2));
  
  return reflection;
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
Plane<T>::Plane(const vect3_t& vec, T nd)
    : n(vec)
    , d(nd) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T>::Plane(const vect3_t& pta, const vect3_t& ptb, const vect3_t& ptc) {
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
  n = vect3_t(Tp[0], Tp[1], Tp[2]);
  d = Tp[3];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Plane<T>::Plane(const vect3_t& NormalVec, const vect3_t& PosVec) //! calc given normal and position of plane origin
{
  CalcFromNormalAndOrigin(NormalVec, PosVec);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T>::~Plane() {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> 
typename Plane<T>::vect3_t Plane<T>::closestPointOnPlane(const vect3_t& pt) const{
  T fdist = pointDistance(pt);
  return pt - (n * fdist);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Plane<T>::CalcFromNormalAndOrigin(
    const vect3_t& NormalVec,
    const vect3_t& PosVec) //! calc given normal and position of plane origin
{
  n = NormalVec;
  d = T(0);
  d = pointDistance(PosVec) * T(-1.0f);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::Reset(void) {
  n = vect3_t(T(0), T(0), T(0));
  d = T(0);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::isPointInFront(const vect3_t& point) const {
  T distance = pointDistance(point);
  return (distance >= -Epsilon());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::isPointBehind(const vect3_t& point) const {
  return not isPointInFront(point);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::isPointCoplanar(const vect3_t& point) const{
  T distance = pointDistance(point);
  return (distance < (Epsilon()) && distance > (-Epsilon()));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
PointClassification Plane<T>::classifyPoint(const vect3_t& point) const {
  T distance = pointDistance(point);
  if (distance > Epsilon())
    return PointClassification::FRONT;
  else if (distance < -Epsilon())
    return PointClassification::BACK;
  else
    return PointClassification::COPLANAR;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::CalcD(const vect3_t& point) {
  d = -point.dotWith(n);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Plane<T>::pointDistance(const vect3_t& pt) const {
  return n.dotWith(pt) + d;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Plane<T>::GetNormal(void) const {
  return n;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Plane<T>::GetD(void) const {
  return d;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Plane<T>::crossProduct(T ii1, T jj1, T kk1, T ii2, T jj2, T kk2, T& iicp, T& jjcp, T& kkcp) const {
  iicp = (jj1 * kk2) - (jj2 * kk1);
  jjcp = (ii2 * kk1) - (ii1 * kk2);
  kkcp = (ii1 * jj2) - (ii2 * jj1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> static bool AreValuesCloseEnoughtToBeEqual(double a, double b, double ftolerance) {
  return (fabs(a - b) >= ftolerance) ? false : true;
}

///////////////////////////////////////////////////////////////////////////////
// ftolerance = smallest distance to consider a point colinear

template <typename T>
void Plane<T>::CalcPlaneFromTriangle(const vect3_t& p0, const vect3_t& p1, const vect3_t& p2, T ftolerance) {
  T ii1, jj1, kk1;
  T ii2, jj2, kk2;
  T iicp, jjcp, kkcp;
  T len0, len1, len2;

  vect4_t p1mp0 = p1 - p0;
  vect4_t p2mp1 = p2 - p1;
  vect4_t p0mp2 = p0 - p2;

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

  n = vect3_t((T)iicp, (T)jjcp, (T)kkcp).normalized();

  // get plane distance from origin
  d = T(0);
  d = pointDistance(p0) * T(-1.0f);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::IsOn(const vect3_t& pt) const {
  T d = pointDistance(pt);
  return (Abs(d) < Epsilon());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Plane<T>::CalcNormal(const vect3_t& pta, const vect3_t& ptb, const vect3_t& ptc) {
  vect3_t bminusa = (ptb - pta);
  vect3_t cminusa = (ptc - pta);

  n = bminusa.crossWith(cminusa).normalized();
  d = -ptc.dotWith(n);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::Intersect(const TLineSegment3<T>& lseg, T& dis, vect3_t& result) const {
  vect3_t dif = (lseg.mEnd - lseg.mStart);
  T length       = dif.magnitude();

  Ray3<T> ray;
  ray.mDirection = dif * (T(1.0) / length); // cheaper normalize since we need the length anyway
  ray.mOrigin    = lseg.mStart;

  bool bOK = Intersect(ray, dis, result);

  bOK &= ((dis >= -0.0001) && (dis < (length+Epsilon())));

  if(not bOK){
    printf( "bOK: %d dis: %f length: %f\n", bOK, dis, length );
  }
  return bOK;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Plane<T>::Intersect(const Ray3<T>& ray, T& dis, vect3_t& res) const {
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
  if (Abs(denom) < Epsilon()){
    //printf( "abs(denom): %f\n", Abs(denom) );
    return false;
  }

  T pointdist = pointDistance(ray.mOrigin);
  T u         = -pointdist / (denom);

  dis = u;

  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> template <typename PolyType> bool Plane<T>::ClipPoly(const PolyType& PolyToClip, PolyType& OutPoly) const {
  const int inuminverts = (int)PolyToClip.GetNumVertices();
  if (inuminverts) {
    const typename PolyType::VertexType& StartVtx = PolyToClip.GetVertex(0);
    bool IsVtxAIn                                 = isPointInFront(StartVtx.Pos());
    // get the side of each vert to the plane
    for (int iva = 0; iva < inuminverts; iva++) {
      int ivb                                 = ((iva == inuminverts - 1) ? 0 : iva + 1);
      const typename PolyType::VertexType& vA = PolyToClip.GetVertex(iva);
      const typename PolyType::VertexType& vB = PolyToClip.GetVertex(ivb);
      if (IsVtxAIn) {
        OutPoly.AddVertex(vA);
      }
      bool IsVtxBIn = isPointInFront(vB.Pos());
      if (IsVtxBIn != IsVtxAIn) {
        vect3_t vPos;
        T isectdist;
        TLineSegment3<T> lseg(vA.Pos(), vB.Pos());
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
                        PolyType& out_back_poly) const { //

  bool debug = false;

  const int inuminverts                         = input_poly.GetNumVertices();
  OrkAssert(input_poly.GetNumVertices()>=3);

  if( debug ) printf( "clip poly num verts<%d>\n", inuminverts );

  // loop around the input polygon's edges

  for (int iva = 0; iva < inuminverts; iva++) {
    if( debug ) printf( "  iva<%d> of inuminverts<%d>\n", iva, inuminverts );
    int ivb                                 = ((iva == inuminverts - 1) ? 0 : iva + 1);

    const auto& vA = input_poly.GetVertex(iva); 
    const auto& vB = input_poly.GetVertex(ivb);

    // get the side of each vert to the plane
    bool is_vertex_a_front = isPointInFront(vA.Pos());
    bool is_vertex_b_front = isPointInFront(vB.Pos());

    if( debug ) printf( "  is_vertex_a_front<%d> is_vertex_b_front<%d>\n", int(is_vertex_a_front), int(is_vertex_b_front) );

    if (is_vertex_a_front) {
      out_front_poly.AddVertex(vA);
      if( debug ) printf("  add a to front cnt<%zu>\n", out_front_poly.GetNumVertices());
    } else {
      out_back_poly.AddVertex(vA);
      if( debug ) printf("  add a to back cnt<%zu>\n", out_back_poly.GetNumVertices());
    }

    if (is_vertex_b_front != is_vertex_a_front) { // did we cross plane ?
      if( debug ) printf("  plane crossed iva<%d> ivb<%d>\n", iva, ivb );
      vect3_t vPos;
      T isectdist;
      TLineSegment3<T> lseg(vA.Pos(), vB.Pos());
      bool isect1 = Intersect(lseg, isectdist, vPos);
      if( debug ) printf("  isect1<%d>\n", int(isect1) );
      if (isect1) {
        T fDist   = (vA.Pos() - vB.Pos()).magnitude();
        T fDist2  = (vA.Pos() - vPos).magnitude();
        T fScalar = (Abs(fDist) < Epsilon()) ? T(0.0) : fDist2 / fDist;
        typename PolyType::VertexType LerpedVertex;
        LerpedVertex.lerp(vA, vB, fScalar);
        out_front_poly.AddVertex(LerpedVertex);
        out_back_poly.AddVertex(LerpedVertex);
        if( debug ) printf("  add l to front cnt<%d>\n", out_front_poly.GetNumVertices());
        if( debug ) printf("  add l to front cnt<%d>\n", out_back_poly.GetNumVertices());
      }
      else{
        TLineSegment3<T> lseg2(vB.Pos(), vA.Pos());
        bool isect2 = Intersect(lseg2, isectdist, vPos);
        if( debug ) printf("  isect2<%d>\n", int(isect2) );
        if (isect2) {
          T fDist   = (vB.Pos() - vA.Pos()).magnitude();
          T fDist2  = (vB.Pos() - vPos).magnitude();
          T fScalar = (Abs(fDist) < Epsilon()) ? T(0.0) : fDist2 / fDist;
          typename PolyType::VertexType LerpedVertex;
          LerpedVertex.lerp(vB, vA, fScalar);
          out_front_poly.AddVertex(LerpedVertex);
          out_back_poly.AddVertex(LerpedVertex);
          if( debug ) printf("  add l2 to front cnt<%d>\n", out_front_poly.GetNumVertices());
          if( debug ) printf("  add l2 to front cnt<%d>\n", out_back_poly.GetNumVertices());
        }
        else{
          if( debug ) printf( "NO INTERSECT vA<%g %g %g> vB<%g %g %g>\n", vA.Pos().x, vA.Pos().y, vA.Pos().z, vB.Pos().x, vB.Pos().y, vB.Pos().z );
          if( debug ) printf( "NO INTERSECT pdA<%g>\n", pointDistance(vA.Pos()) );
          if( debug ) printf( "NO INTERSECT pdB<%g>\n", pointDistance(vB.Pos()) );
          if( debug ) printf( "NO INTERSECT plane_n<%g %g %g> d<%g>\n", n.x, n.y, n.z, d );
        }

      }
    }
  }

  int numfront = out_front_poly.GetNumVertices();
  int numback = out_back_poly.GetNumVertices();

  if( debug ) printf( "numfront<%d> numback<%d>\n", numfront, numback );

  bool front_is_invalid = (numfront>0) and (numfront<3);
  bool back_is_invalid = (numback>0) and (numback<3);

  if( front_is_invalid or back_is_invalid ){
    return false;
  }

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
  vect3_t point(n * -d);
  point = point.transform(transform).xyz();
  n     = n.transform3x3(transform).normalized();
  d = -n.dotWith(point);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Plane<T> Plane<T>::operator-() const {
  return Plane<T>(-n, n * -d);
}

template <typename T> 
uint64_t Plane<T>::hash(T normal_quant, T dist_quant) const{

  //////////////////////////////////////////////////////////
  // quantize normals
  //  2^28 possible encodings more or less equally distributed (octahedral encoding)
  //  -> each encoding covers 4.682e-8 steradians (12.57 steradians / 2^28)
  // TODO: make an argument ?
  //////////////////////////////////////////////////////////

  auto nenc = n.normalOctahedronEncoded();
  OrkAssert(normal_quant>T(0));
  OrkAssert(normal_quant<=T(16384));
  uint64_t ux = uint64_t(nenc.x*normal_quant);        // 14 bits
  uint64_t uy = uint64_t(nenc.y*normal_quant);        // 14 bits  (total of 2^28 possible normals ~= )

  //////////////////////////////////////////////////////////
  // quantize plane distance
  //   (64km [-32k..+32k] range with .25 millimeter precision)
  // TODO: make an argument ?
  //////////////////////////////////////////////////////////

  OrkAssert(dist_quant>T(0));
  OrkAssert(dist_quant<=T(4096));
  uint64_t ud = uint64_t( (d+T(32767))*dist_quant ); //  16+12 bits 
  uint64_t hash = ud | (ux<<32) | (uy<<48);

  return hash;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
