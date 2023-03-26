////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/plane.h>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct AABox {

  fvec3 mMin;
  fvec3 mMax;

  fplane3 mPlaneNX[2];
  fplane3 mPlaneNY[2];
  fplane3 mPlaneNZ[2];

  void ComputePlanes();

  void SupportMapping(const fvec3 &v, fvec3 &result) const;

  fvec3 Corner(int n) const;

  bool Intersect(const fray3 &ray, fvec3 &isect_in, fvec3 &isect_out) const;
  bool contains(const fvec3 &test_point) const;
  bool contains(const float test_point_X, const float test_point_Z) const;
  void Constrain(float &test_point_X, float &test_point_Z) const;
  void Constrain(fvec3 &test_point) const;

  AABox();
  AABox(const fvec3 &vmin, const fvec3 &vmax);
  AABox(const AABox &oth);
  void operator=(const AABox &oth);

  inline const fvec3 &Min() const { return mMin; }
  inline const fvec3 &Max() const { return mMax; }
  inline fvec3 GetSize() const { return Max() - Min(); }

  fvec3 center() const;
  fvec3 size() const;

  void BeginGrow();
  void Grow(const fvec3 &vin);
  void EndGrow();

  void SetMinMax(const fvec3 &vmin, const fvec3 &vmax);

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
