////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>

namespace ork::math {

///////////////////////////////////////////////////////////////////////////////

enum struct CurveSegmentType : uint32_t {
  LINEAR = 0,
  BEZIER = 1,
  CATMULL_ROM = 2,
};

///////////////////////////////////////////////////////////////////////////////

struct TransformCurvePoint : public ork::Object {
  DeclareConcreteX(TransformCurvePoint, ork::Object);

public:
  TransformCurvePoint() = default;

  float _time = 0.0f;
  fvec3 _position;
  fquat _rotation;
  float _scale = 1.0f;
  fvec3 _tangent_out;
  fvec3 _tangent_in;
};

using transformcurvepoint_ptr_t = std::shared_ptr<TransformCurvePoint>;

///////////////////////////////////////////////////////////////////////////////

struct TransformCurveSample {
  fvec3 _position;
  fquat _rotation;
  float _scale = 1.0f;
  fvec3 _tangent;
};

///////////////////////////////////////////////////////////////////////////////

struct TransformCurve : public ork::Object {
  DeclareConcreteX(TransformCurve, ork::Object);

public:
  TransformCurve();

  int addPoint(transformcurvepoint_ptr_t pt);
  void removePoint(int index);
  void setPoint(int index, transformcurvepoint_ptr_t pt);
  transformcurvepoint_ptr_t getPoint(int index) const;
  int numPoints() const;

  void setSegmentType(int seg_index, CurveSegmentType type);
  CurveSegmentType getSegmentType(int seg_index) const;

  TransformCurveSample sample(float t) const;
  fvec3 samplePosition(float t) const;
  fmtx4 sampleMatrix(float t) const;

  std::vector<transformcurvepoint_ptr_t> _points;
  std::vector<CurveSegmentType> _segmentTypes;

private:
  struct SegmentResult {
    int _segIndex = 0;
    float _localT = 0.0f;
  };

  SegmentResult _findSegment(float t) const;
  void _sortByTime();
  void _syncSegmentTypes();

  fvec3 _evalBezier(const fvec3& p0, const fvec3& p1, const fvec3& p2, const fvec3& p3, float t) const;
  fvec3 _evalCatmullRom(const fvec3& p0, const fvec3& p1, const fvec3& p2, const fvec3& p3, float t) const;

  bool preDeserialize(ork::reflect::serdes::IDeserializer& deser) final;
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;
};

///////////////////////////////////////////////////////////////////////////////

using transformcurve_ptr_t = std::shared_ptr<TransformCurve>;

} // namespace ork::math
