////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/misc_math.h>
#include <ork/object/Object.h>

namespace ork::math {

///////////////////////////////////////////////////////////////////////////////

struct SplineV2 {
  static const int Nu_components = 2;

  SplineV2(const fvec2& pos);
  SplineV2() {
  }

  fvec2 mData;

  float GetComponent(int idx) const;
  void SetComponent(int idx, float fv);
};

///////////////////////////////////////////////////////////////////////////////

struct SplineV3 {
  static const int Nu_components = 3;

  SplineV3(const fvec3& pos);
  SplineV3() {
  }

  fvec3 mData;

  float GetComponent(int idx) const;
  void SetComponent(int idx, float fv);
};

///////////////////////////////////////////////////////////////////////////////

struct SplineV4 {
  static const int Nu_components = 4;

  SplineV4(const fvec4& pos);
  SplineV4() {
  }

  fvec4 mData;

  float GetComponent(int idx) const;
  void SetComponent(int idx, float fv);
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class CatmullRomSpline {
public:
  static const int knumcomponents = T::Nu_components;

  orkvector<T> mSeqVertices;
  bool mbClosed;

  //////////////////////////////////////////////////////////

  mutable orkvector<Polynomial> mBases;
  mutable orkvector<Polynomial> mBasesDeriv;

  int BaseIndex(int ipoint, int icomponent) const;
  //{ return (ipoint*knumcomponents)+icomponent; }

  //////////////////////////////////////////////////////////

  void GetCV(int idx, T& out) const;
  void AddCV(const T& vert);
  int NumCVs() const {
    return int(mSeqVertices.size());
  }
  void ClearCVS();

  //////////////////////////////////////////////////////////

  int GetCVIndex(int i) const;
  void SampleAt(float flerp, T& res, T& deriv) const;
  void GenBases(int pt) const;

  //////////////////////////////////////////////////////////

  CatmullRomSpline();
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::math
