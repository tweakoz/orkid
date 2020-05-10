////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _MATH_MULTICURVE_H
#define _MATH_MULTICURVE_H

#include <ork/math/spline.h>

#include <ork/config/config.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

enum EMCSEGTYPE {
  EMCST_LINEAR = 0,
  EMCST_BOX,
  EMCST_LOG,
  EMCST_EXP,
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class ObjProxy : public ork::Object {
public:
  T* mParent;

  ObjProxy(T* val)
      : mParent(val) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MultiCurve1D : public ork::Object {
  RttiDeclareConcrete(MultiCurve1D, ork::Object);

public:
  orkvector<EMCSEGTYPE> mSegmentTypes;
  orklut<float, float> mVertices;
  ObjProxy<MultiCurve1D> mProxy;

  float mMin, mMax;

  ork::Object* ProxyAccessor() {
    return &mProxy;
  }

  int GetNumSegments() const;
  float Sample(float u) const;
  const std::pair<float, float>& GetVertex(int iv) const {
    return *(mVertices.begin() + iv);
  }
  EMCSEGTYPE GetSegmentType(int is) const {
    return mSegmentTypes[is];
  }
  size_t GetNumVertices() const {
    return mVertices.size();
  }
  orklut<float, float>& GetVertices() {
    return mVertices;
  }

  /////////////////////////////////////

  void SplitSegment(int iseg);
  void MergeSegment(int ifirstseg);
  void SetSegmentType(int iseg, EMCSEGTYPE etype);
  void SetPoint(int ipoint, float fu, float fv);

  void SetMin(float fmin) {
    mMin = fmin;
  }
  void SetMax(float fmax) {
    mMax = fmax;
  }
  float GetMin() const {
    return mMin;
  }
  float GetMax() const {
    return mMax;
  }

  /////////////////////////////////////

  bool IsOk() const;

  /////////////////////////////////////

  MultiCurve1D();
  void Init(int inumsegs);

private:
  bool PostDeserialize(reflect::IDeserializer&) final;           // virtual
  bool PreDeserialize(ork::reflect::IDeserializer& deser) final; // virtual
};

///////////////////////////////////////////////////////////////////////////////

}; // namespace ork

//////////////////////////////////////////////////////////////////////////////

#endif
