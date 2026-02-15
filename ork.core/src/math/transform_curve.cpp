////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/transform_curve.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/DirectObjectVector.inl>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <algorithm>

ImplementReflectionX(ork::math::TransformCurvePoint, "TransformCurvePoint");
ImplementReflectionX(ork::math::TransformCurve, "TransformCurve");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace math {

BeginEnumRegistration(CurveSegmentType);
RegisterEnum(CurveSegmentType, LINEAR);
RegisterEnum(CurveSegmentType, BEZIER);
RegisterEnum(CurveSegmentType, CATMULL_ROM);
EndEnumRegistration();

} // namespace math

ImplementEnumSerializer(math::CurveSegmentType);

namespace math {

void TransformCurvePoint::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("time", &TransformCurvePoint::_time);
  clazz->directProperty("position", &TransformCurvePoint::_position);
  clazz->directProperty("rotation", &TransformCurvePoint::_rotation);
  clazz->directProperty("scale", &TransformCurvePoint::_scale);
  clazz->directProperty("tangent_out", &TransformCurvePoint::_tangent_out);
  clazz->directProperty("tangent_in", &TransformCurvePoint::_tangent_in);
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::describeX(object::ObjectClass* clazz) {
  InvokeEnumRegistration(CurveSegmentType);

  clazz->directObjectVectorProperty("points", &TransformCurve::_points);
  clazz->directVectorProperty("segmentTypes", &TransformCurve::_segmentTypes);
}

///////////////////////////////////////////////////////////////////////////////

TransformCurve::TransformCurve() {
}

///////////////////////////////////////////////////////////////////////////////

int TransformCurve::addPoint(transformcurvepoint_ptr_t pt) {
  _points.push_back(pt);
  _sortByTime();
  _syncSegmentTypes();
  // find the inserted index
  for (int i = 0; i < (int)_points.size(); i++) {
    if (_points[i]->_time == pt->_time && _points[i]->_position == pt->_position) {
      return i;
    }
  }
  return (int)_points.size() - 1;
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::removePoint(int index) {
  OrkAssert(index >= 0 && index < (int)_points.size());
  _points.erase(_points.begin() + index);
  _syncSegmentTypes();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::setPoint(int index, transformcurvepoint_ptr_t pt) {
  OrkAssert(index >= 0 && index < (int)_points.size());
  _points[index] = pt;
  _sortByTime();
  _syncSegmentTypes();
}

///////////////////////////////////////////////////////////////////////////////

transformcurvepoint_ptr_t TransformCurve::getPoint(int index) const {
  OrkAssert(index >= 0 && index < (int)_points.size());
  return _points[index];
}

///////////////////////////////////////////////////////////////////////////////

int TransformCurve::numPoints() const {
  return (int)_points.size();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::setSegmentType(int seg_index, CurveSegmentType type) {
  OrkAssert(seg_index >= 0 && seg_index < (int)_segmentTypes.size());
  _segmentTypes[seg_index] = type;
}

///////////////////////////////////////////////////////////////////////////////

CurveSegmentType TransformCurve::getSegmentType(int seg_index) const {
  OrkAssert(seg_index >= 0 && seg_index < (int)_segmentTypes.size());
  return _segmentTypes[seg_index];
}

///////////////////////////////////////////////////////////////////////////////

TransformCurve::SegmentResult TransformCurve::_findSegment(float t) const {
  SegmentResult result;
  int n = (int)_points.size();
  if (n < 2) {
    return result;
  }

  // clamp t
  if (t <= _points[0]->_time) {
    result._segIndex = 0;
    result._localT = 0.0f;
    return result;
  }
  if (t >= _points[n - 1]->_time) {
    result._segIndex = n - 2;
    result._localT = 1.0f;
    return result;
  }

  // binary search
  int lo = 0, hi = n - 2;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (_points[mid + 1]->_time < t) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  result._segIndex = lo;
  float t0 = _points[lo]->_time;
  float t1 = _points[lo + 1]->_time;
  float dt = t1 - t0;
  result._localT = (dt > 1e-9f) ? (t - t0) / dt : 0.0f;
  return result;
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::_sortByTime() {
  std::sort(_points.begin(), _points.end(), [](const transformcurvepoint_ptr_t& a, const transformcurvepoint_ptr_t& b) {
    return a->_time < b->_time;
  });
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::_syncSegmentTypes() {
  int nsegs = std::max(0, (int)_points.size() - 1);
  if ((int)_segmentTypes.size() < nsegs) {
    _segmentTypes.resize(nsegs, CurveSegmentType::LINEAR);
  } else if ((int)_segmentTypes.size() > nsegs) {
    _segmentTypes.resize(nsegs);
  }
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::_evalBezier(const fvec3& p0, const fvec3& p1, const fvec3& p2, const fvec3& p3, float t) const {
  float u = 1.0f - t;
  float tt = t * t;
  float uu = u * u;
  float uuu = uu * u;
  float ttt = tt * t;
  return p0 * uuu + p1 * (3.0f * uu * t) + p2 * (3.0f * u * tt) + p3 * ttt;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::_evalCatmullRom(const fvec3& p0, const fvec3& p1, const fvec3& p2, const fvec3& p3, float t) const {
  float tt = t * t;
  float ttt = tt * t;
  return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * tt +
          (p1 * 3.0f - p0 - p2 * 3.0f + p3) * ttt) *
         0.5f;
}

///////////////////////////////////////////////////////////////////////////////

TransformCurveSample TransformCurve::sample(float t) const {
  TransformCurveSample result;
  int n = (int)_points.size();

  if (n == 0) {
    return result;
  }
  if (n == 1) {
    result._position = _points[0]->_position;
    result._rotation = _points[0]->_rotation;
    result._scale = _points[0]->_scale;
    return result;
  }

  auto seg = _findSegment(t);
  int i = seg._segIndex;
  float lt = seg._localT;

  const auto& A = *_points[i];
  const auto& B = *_points[i + 1];

  CurveSegmentType segType = (i < (int)_segmentTypes.size()) ? _segmentTypes[i] : CurveSegmentType::LINEAR;

  // position interpolation varies by segment type
  switch (segType) {
    case CurveSegmentType::LINEAR: {
      fvec3 pos;
      pos.lerp(A._position, B._position, lt);
      result._position = pos;
      // tangent is direction
      result._tangent = (B._position - A._position);
      float len = result._tangent.length();
      if (len > 1e-9f) {
        result._tangent = result._tangent * (1.0f / len);
      }
      break;
    }
    case CurveSegmentType::BEZIER: {
      fvec3 cp0 = A._position;
      fvec3 cp1 = A._position + A._tangent_out;
      fvec3 cp2 = B._position + B._tangent_in;
      fvec3 cp3 = B._position;
      result._position = _evalBezier(cp0, cp1, cp2, cp3, lt);
      // tangent = derivative of cubic bezier
      float u = 1.0f - lt;
      result._tangent = (cp1 - cp0) * (3.0f * u * u) + (cp2 - cp1) * (6.0f * u * lt) + (cp3 - cp2) * (3.0f * lt * lt);
      float len = result._tangent.length();
      if (len > 1e-9f) {
        result._tangent = result._tangent * (1.0f / len);
      }
      break;
    }
    case CurveSegmentType::CATMULL_ROM: {
      int im1 = std::max(0, i - 1);
      int ip2 = std::min(n - 1, i + 2);
      result._position = _evalCatmullRom(_points[im1]->_position, A._position, B._position, _points[ip2]->_position, lt);
      // tangent from Catmull-Rom derivative
      float eps = 0.001f;
      fvec3 p_prev = _evalCatmullRom(
          _points[im1]->_position, A._position, B._position, _points[ip2]->_position, std::max(0.0f, lt - eps));
      fvec3 p_next = _evalCatmullRom(
          _points[im1]->_position, A._position, B._position, _points[ip2]->_position, std::min(1.0f, lt + eps));
      result._tangent = (p_next - p_prev);
      float len = result._tangent.length();
      if (len > 1e-9f) {
        result._tangent = result._tangent * (1.0f / len);
      }
      break;
    }
  }

  // rotation — always slerp
  result._rotation = fquat::slerp(A._rotation, B._rotation, lt);

  // scale — always lerp
  result._scale = A._scale + (B._scale - A._scale) * lt;

  return result;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::samplePosition(float t) const {
  return sample(t)._position;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 TransformCurve::sampleMatrix(float t) const {
  auto s = sample(t);
  fmtx4 mtx;
  mtx.compose(s._position, s._rotation, s._scale);
  return mtx;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformCurve::preDeserialize(ork::reflect::serdes::IDeserializer& deser) {
  _points.clear();
  _segmentTypes.clear();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformCurve::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  _sortByTime();
  _syncSegmentTypes();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace math
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
