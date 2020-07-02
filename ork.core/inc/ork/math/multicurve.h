////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/spline.h>

#include <ork/config/config.h>
#include <ork/reflect/properties/registerX.inl>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

enum class MultiCurveSegmentType {
  LINEAR = 0,
  BOX,
  LOG,
  EXP,
};
///////////////////////////////////////////////////////////////////////////////
DeclareEnumSerializer(MultiCurveSegmentType);
///////////////////////////////////////////////////////////////////////////////

template <typename T> class ObjProxy : public ork::Object {
public:
  T* _parent;

  ObjProxy(T* val)
      : _parent(val) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MultiCurve1D : public ork::Object {
  RttiDeclareConcrete(MultiCurve1D, ork::Object);

public:
  orkvector<MultiCurveSegmentType> mSegmentTypes;
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
  MultiCurveSegmentType GetSegmentType(int is) const {
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
  void SetSegmentType(int iseg, MultiCurveSegmentType etype);
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
  bool postDeserialize(reflect::serdes::IDeserializer&) final;           // virtual
  bool preDeserialize(ork::reflect::serdes::IDeserializer& deser) final; // virtual
};

///////////////////////////////////////////////////////////////////////////////

}; // namespace ork

//////////////////////////////////////////////////////////////////////////////
