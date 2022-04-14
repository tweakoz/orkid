////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/spline.hpp>
#include <ork/orkmath.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::math {
///////////////////////////////////////////////////////////////////////////////

SplineV2::SplineV2(const fvec2& data)
    : mData(data) {
}

float SplineV2::GetComponent(int idx) const {
  OrkAssert(idx < Nu_components);
  float rval = 0.0f;
  switch (idx) {
    case 0:
      rval = mData.x;
      break;
    case 1:
      rval = mData.y;
      break;
  }
  return rval;
}
void SplineV2::SetComponent(int idx, float fv) {
  OrkAssert(idx < Nu_components);
  switch (idx) {
    case 0:
      mData.x = (fv);
      break;
    case 1:
      mData.y = (fv);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

SplineV3::SplineV3(const fvec3& data)
    : mData(data) {
}

float SplineV3::GetComponent(int idx) const {
  OrkAssert(idx < Nu_components);
  float rval = 0.0f;
  switch (idx) {
    case 0:
      rval = mData.x;
      break;
    case 1:
      rval = mData.y;
      break;
    case 2:
      rval = mData.z;
      break;
  }
  return rval;
}
void SplineV3::SetComponent(int idx, float fv) {
  OrkAssert(idx < Nu_components);
  switch (idx) {
    case 0:
      mData.x = (fv);
      break;
    case 1:
      mData.y = (fv);
      break;
    case 2:
      mData.z = (fv);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

SplineV4::SplineV4(const fvec4& data)
    : mData(data) {
}

float SplineV4::GetComponent(int idx) const {
  OrkAssert(idx < Nu_components);
  float rval = 0.0f;
  switch (idx) {
    case 0:
      rval = mData.x;
      break;
    case 1:
      rval = mData.y;
      break;
    case 2:
      rval = mData.z;
      break;
    case 3:
      rval = mData.w;
      break;
  }
  return rval;
}
void SplineV4::SetComponent(int idx, float fv) {
  OrkAssert(idx < Nu_components);
  switch (idx) {
    case 0:
      mData.x = (fv);
      break;
    case 1:
      mData.y = (fv);
      break;
    case 2:
      mData.z = (fv);
      break;
    case 3:
      mData.w = (fv);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

template class CatmullRomSpline<SplineV2>;
template class CatmullRomSpline<SplineV3>;
template class CatmullRomSpline<SplineV4>;

} // namespace ork::math
