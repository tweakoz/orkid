////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/spline.h>
#include <ork/util/crc.h>
#include <ork/config/config.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/reflect/enum_serializer.inl>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
enum struct MultiCurveSegmentType : crc_enum_t {
  CrcEnum(LINEAR),
  CrcEnum(BOX),
  CrcEnum(LOG),
  CrcEnum(EXP),
};
DeclareEnumSerializer(MultiCurveSegmentType);
///////////////////////////////////////////////////////////////////////////////

struct MultiCurve1D : public ork::Object {
  DeclareConcreteX(MultiCurve1D, ork::Object);

public:
  orkvector<MultiCurveSegmentType> mSegmentTypes;
  orklut<float, float> mVertices;

  float mMin, mMax;

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
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;           // virtual
  bool preDeserialize(ork::reflect::serdes::IDeserializer& deser) final; // virtual
};

///////////////////////////////////////////////////////////////////////////////

using multicurve1d_ptr_t      = std::shared_ptr<MultiCurve1D>;
}; // namespace ork

//////////////////////////////////////////////////////////////////////////////
