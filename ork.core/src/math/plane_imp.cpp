////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/plane.h>
#include <ork/math/plane.hpp>
//#include <ork/math/gjk.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/frustum.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>

namespace ork {

template <typename T> bool Plane<T>::PlaneIntersect(const Plane<T>& oth, Vector3<T>& outpos, Vector3<T>& outdir) const {
  outdir        = GetNormal().crossWith(oth.GetNormal());
  T num         = outdir.magnitudeSquared();
  Vector3<T> c1 = (GetD() * oth.GetNormal()) + (oth.GetD() * GetNormal());
  outpos        = c1.crossWith(outdir) * T(1.0) / num;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <> float Plane<float>::Abs(float in) {
  return (in>=0.0f) ? in : -in;
}
template <> float Plane<float>::Epsilon() {
  return 1.0e-5f;
}

///////////////////////////////////////////////////////////////////////////////

template <> double Plane<double>::Abs(double in) {
  return (in>=0.0) ? in : -in;
}
template <> double Plane<double>::Epsilon() {
  return 1.0e-6;
}

///////////////////////////////////////////////////////////////////////////////

dplane3 fplane3_to_dplane3(const fplane3& f3){
  fvec3 N = f3.n;
  float D = f3.d;
  return dplane3(fvec3_to_dvec3(N), D);
}
fplane3 dplane3_to_fplane3(const dplane3& dplane3){
  dvec3 N = dplane3.n;
  double D = dplane3.d;
  return fplane3(dvec3_to_fvec3(N), D);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

template class ork::Plane<float>;  // explicit template instantiation
template class ork::Plane<double>; // explicit template instantiation

// template class ork::chunkfile::Reader<ork::lev2::CollisionLoadAllocator>;
