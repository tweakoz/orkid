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
  STEP = 3,
};

enum struct CurveChannel : int {
  CC_POS_X = 0, CC_POS_Y, CC_POS_Z,
  CC_ROT_X, CC_ROT_Y, CC_ROT_Z,
  CC_SCALE_X, CC_SCALE_Y, CC_SCALE_Z,
  CC_COUNT = 9,
};

enum struct RotationOrder : uint32_t {
  XYZ = 0,
  XZY = 1,
  YXZ = 2,
  YZX = 3,
  ZXY = 4,
  ZYX = 5,
};

///////////////////////////////////////////////////////////////////////////////

struct TransformCurvePoint : public ork::Object {
  DeclareConcreteX(TransformCurvePoint, ork::Object);

public:
  TransformCurvePoint() = default;

  float _time = 0.0f;
  fvec3 _position;
  fvec3 _eulerRotation;  // degrees, interpreted per TransformCurve::_rotationOrder
  fvec3 _scale = fvec3(1, 1, 1);
  fvec3 _tangent_out;
  fvec3 _tangent_in;
  fvec3 _rot_tangent_out;
  fvec3 _rot_tangent_in;
  fvec3 _scale_tangent_out;
  fvec3 _scale_tangent_in;
};

using transformcurvepoint_ptr_t = std::shared_ptr<TransformCurvePoint>;

///////////////////////////////////////////////////////////////////////////////

struct TransformCurveSample {
  fvec3 _position;
  fquat _rotation;
  fvec3 _scale = fvec3(1, 1, 1);
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

  void setChannelSegmentType(int seg_index, CurveChannel ch, CurveSegmentType type);
  CurveSegmentType getChannelSegmentType(int seg_index, CurveChannel ch) const;

  TransformCurveSample sample(float t) const;
  fvec3 sampleEuler(float t) const;  // lerps raw euler degrees (no quat decomposition)
  fvec3 samplePosition(float t) const;
  fmtx4 sampleMatrix(float t) const;

  bool _useNonUniformScale = false;
  bool _looping = false;
  RotationOrder _rotationOrder = RotationOrder::XYZ;

  // When _looping is true, enforce that the last point's values match the first,
  // and tangents at the seam are mirrored for bezier continuity.
  void enforceLoopConstraints();

  fquat eulerToQuat(const fvec3& euler) const;
  fvec3 quatToEuler(const fquat& q) const;

  std::vector<transformcurvepoint_ptr_t> _points;
  std::vector<CurveSegmentType> _segmentTypes;  // legacy, kept for backward-compat deser
  std::vector<CurveSegmentType> _channelSegmentTypes;  // flat: [segIdx * CC_COUNT + channelIdx]

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
  float _evalScalarBezier(float p0, float p1, float p2, float p3, float t) const;
  float _evalScalarCatmullRom(float vm1, float v0, float v1, float v2, float t) const;
  void _syncChannelSegmentTypes();

  bool preDeserialize(ork::reflect::serdes::IDeserializer& deser) final;
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;
};

///////////////////////////////////////////////////////////////////////////////

using transformcurve_ptr_t = std::shared_ptr<TransformCurve>;

} // namespace ork::math
