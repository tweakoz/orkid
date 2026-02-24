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
#include <ork/math/cmatrix4.hpp>
#include <algorithm>
#include <unordered_map>

ImplementReflectionX(ork::math::TransformCurvePoint, "TransformCurvePoint");
ImplementReflectionX(ork::math::TransformCurve, "TransformCurve");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace math {

BeginEnumRegistration(CurveSegmentType);
RegisterEnum(CurveSegmentType, LINEAR);
RegisterEnum(CurveSegmentType, BEZIER);
RegisterEnum(CurveSegmentType, CATMULL_ROM);
RegisterEnum(CurveSegmentType, STEP);
EndEnumRegistration();

BeginEnumRegistration(RotationOrder);
RegisterEnum(RotationOrder, XYZ);
RegisterEnum(RotationOrder, XZY);
RegisterEnum(RotationOrder, YXZ);
RegisterEnum(RotationOrder, YZX);
RegisterEnum(RotationOrder, ZXY);
RegisterEnum(RotationOrder, ZYX);
EndEnumRegistration();

} // namespace math

ImplementEnumSerializer(math::CurveSegmentType);
ImplementEnumSerializer(math::RotationOrder);

namespace math {

void TransformCurvePoint::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("time", &TransformCurvePoint::_time);
  clazz->directProperty("position", &TransformCurvePoint::_position);
  clazz->directProperty("eulerRotation", &TransformCurvePoint::_eulerRotation);
  clazz->directProperty("scale", &TransformCurvePoint::_scale);
  clazz->directProperty("tangent_out", &TransformCurvePoint::_tangent_out);
  clazz->directProperty("tangent_in", &TransformCurvePoint::_tangent_in);
  clazz->directProperty("rot_tangent_out", &TransformCurvePoint::_rot_tangent_out);
  clazz->directProperty("rot_tangent_in", &TransformCurvePoint::_rot_tangent_in);
  clazz->directProperty("scale_tangent_out", &TransformCurvePoint::_scale_tangent_out);
  clazz->directProperty("scale_tangent_in", &TransformCurvePoint::_scale_tangent_in);
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::describeX(object::ObjectClass* clazz) {
  InvokeEnumRegistration(CurveSegmentType);
  InvokeEnumRegistration(RotationOrder);

  clazz->directObjectVectorProperty("points", &TransformCurve::_points);
  clazz->directVectorProperty("segmentTypes", &TransformCurve::_segmentTypes);
  clazz->directVectorProperty("channelSegmentTypes", &TransformCurve::_channelSegmentTypes);
  clazz->directProperty("useNonUniformScale", &TransformCurve::_useNonUniformScale);
  clazz->directProperty("looping", &TransformCurve::_looping);
  clazz->directEnumProperty("rotationOrder", &TransformCurve::_rotationOrder);
}

///////////////////////////////////////////////////////////////////////////////

TransformCurve::TransformCurve() {
}

///////////////////////////////////////////////////////////////////////////////

int TransformCurve::addPoint(transformcurvepoint_ptr_t pt) {
  _points.push_back(pt);
  _sortByTime();
  _syncChannelSegmentTypes();
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
  int cc_count = (int)CurveChannel::CC_COUNT;
  int nsegs = (int)_points.size() - 1;
  if (!_channelSegmentTypes.empty() && nsegs > 0) {
    int segToRemove = std::min(index, nsegs - 1);
    int eraseStart = segToRemove * cc_count;
    int eraseEnd = eraseStart + cc_count;
    if (eraseEnd <= (int)_channelSegmentTypes.size()) {
      _channelSegmentTypes.erase(
          _channelSegmentTypes.begin() + eraseStart,
          _channelSegmentTypes.begin() + eraseEnd);
    }
  }
  _points.erase(_points.begin() + index);
  _syncChannelSegmentTypes();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::setPoint(int index, transformcurvepoint_ptr_t pt) {
  OrkAssert(index >= 0 && index < (int)_points.size());
  _points[index] = pt;
  _sortByTime();
  _syncChannelSegmentTypes();
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
  _syncChannelSegmentTypes();
  int cc_count = (int)CurveChannel::CC_COUNT;
  if (seg_index >= 0 && seg_index * cc_count + cc_count - 1 < (int)_channelSegmentTypes.size()) {
    for (int c = 0; c < cc_count; c++)
      _channelSegmentTypes[seg_index * cc_count + c] = type;
  }
}

///////////////////////////////////////////////////////////////////////////////

CurveSegmentType TransformCurve::getSegmentType(int seg_index) const {
  return getChannelSegmentType(seg_index, CurveChannel::CC_POS_X);
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::setChannelSegmentType(int seg_index, CurveChannel ch, CurveSegmentType type) {
  _syncChannelSegmentTypes();
  int cc_count = (int)CurveChannel::CC_COUNT;
  int idx = seg_index * cc_count + (int)ch;
  if (idx >= 0 && idx < (int)_channelSegmentTypes.size())
    _channelSegmentTypes[idx] = type;
}

///////////////////////////////////////////////////////////////////////////////

CurveSegmentType TransformCurve::getChannelSegmentType(int seg_index, CurveChannel ch) const {
  int cc_count = (int)CurveChannel::CC_COUNT;
  int idx = seg_index * cc_count + (int)ch;
  if (idx < 0 || idx >= (int)_channelSegmentTypes.size())
    return CurveSegmentType::LINEAR;
  return _channelSegmentTypes[idx];
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::_syncChannelSegmentTypes() {
  int nsegs = std::max(0, (int)_points.size() - 1);
  int cc_count = (int)CurveChannel::CC_COUNT;
  int needed = nsegs * cc_count;
  if ((int)_channelSegmentTypes.size() < needed) {
    _channelSegmentTypes.resize(needed, CurveSegmentType::LINEAR);
  } else if ((int)_channelSegmentTypes.size() > needed) {
    _channelSegmentTypes.resize(needed);
  }
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
  int n = (int)_points.size();
  if (n <= 1) return;

  int cc_count = (int)CurveChannel::CC_COUNT;

  // Associate per-channel segment types with the LEFT point of each segment (before sort)
  using ChannelTypes = std::array<CurveSegmentType, 9>;
  std::unordered_map<TransformCurvePoint*, ChannelTypes> pointSegTypes;
  for (int i = 0; i + 1 < n; i++) {
    ChannelTypes ct;
    for (int c = 0; c < cc_count; c++) {
      int idx = i * cc_count + c;
      ct[c] = (idx < (int)_channelSegmentTypes.size()) ? _channelSegmentTypes[idx] : CurveSegmentType::LINEAR;
    }
    pointSegTypes[_points[i].get()] = ct;
  }

  // Sort points by time
  std::sort(_points.begin(), _points.end(), [](const transformcurvepoint_ptr_t& a, const transformcurvepoint_ptr_t& b) {
    return a->_time < b->_time;
  });

  // Rebuild channel segment types preserving association with left point
  int nsegs = n - 1;
  _channelSegmentTypes.resize(nsegs * cc_count);
  for (int i = 0; i < nsegs; i++) {
    auto it = pointSegTypes.find(_points[i].get());
    for (int c = 0; c < cc_count; c++) {
      _channelSegmentTypes[i * cc_count + c] = (it != pointSegTypes.end()) ? it->second[c] : CurveSegmentType::LINEAR;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::_syncSegmentTypes() {
  // Legacy — kept for backward compat, but now defers to channel-based
  _syncChannelSegmentTypes();
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

float TransformCurve::_evalScalarBezier(float p0, float p1, float p2, float p3, float t) const {
  float u = 1.0f - t;
  float uu = u * u;
  float uuu = uu * u;
  float tt = t * t;
  float ttt = tt * t;
  return p0 * uuu + p1 * (3.0f * uu * t) + p2 * (3.0f * u * tt) + p3 * ttt;
}

///////////////////////////////////////////////////////////////////////////////

float TransformCurve::_evalScalarCatmullRom(float vm1, float v0, float v1, float v2, float t) const {
  float tt = t * t;
  float ttt = tt * t;
  return (v0 * 2.0f + (v1 - vm1) * t + (vm1 * 2.0f - v0 * 5.0f + v1 * 4.0f - v2) * tt +
          (v0 * 3.0f - vm1 - v1 * 3.0f + v2) * ttt) *
         0.5f;
}

///////////////////////////////////////////////////////////////////////////////

fquat TransformCurve::eulerToQuat(const fvec3& eulerDeg) const {
  constexpr float D2R = 3.14159265358979323846f / 180.0f;
  float rx = eulerDeg.x * D2R;
  float ry = eulerDeg.y * D2R;
  float rz = eulerDeg.z * D2R;
  fmtx4 mtx;
  switch (_rotationOrder) {
    case RotationOrder::XYZ: mtx.fromEulerXYZ(rx, ry, rz); break;
    case RotationOrder::XZY: { auto m = glm::eulerAngleXZY(rx, rz, ry); mtx = fmtx4(m); break; }
    case RotationOrder::YXZ: { auto m = glm::eulerAngleYXZ(ry, rx, rz); mtx = fmtx4(m); break; }
    case RotationOrder::YZX: { auto m = glm::eulerAngleYZX(ry, rz, rx); mtx = fmtx4(m); break; }
    case RotationOrder::ZXY: { auto m = glm::eulerAngleZXY(rz, rx, ry); mtx = fmtx4(m); break; }
    case RotationOrder::ZYX: { auto m = glm::eulerAngleZYX(rz, ry, rx); mtx = fmtx4(m); break; }
  }
  fquat q;
  q.fromMatrix(mtx);
  return q;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::quatToEuler(const fquat& q) const {
  constexpr float R2D = 180.0f / 3.14159265358979323846f;
  fmtx4 mtx = q.toMatrix();
  float ex, ey, ez;
  switch (_rotationOrder) {
    case RotationOrder::XYZ: glm::extractEulerAngleXYZ((const glm::mat4&)mtx, ex, ey, ez); break;
    case RotationOrder::XZY: glm::extractEulerAngleXZY((const glm::mat4&)mtx, ex, ez, ey); break;
    case RotationOrder::YXZ: glm::extractEulerAngleYXZ((const glm::mat4&)mtx, ey, ex, ez); break;
    case RotationOrder::YZX: glm::extractEulerAngleYZX((const glm::mat4&)mtx, ey, ez, ex); break;
    case RotationOrder::ZXY: glm::extractEulerAngleZXY((const glm::mat4&)mtx, ez, ex, ey); break;
    case RotationOrder::ZYX: glm::extractEulerAngleZYX((const glm::mat4&)mtx, ez, ey, ex); break;
  }
  return fvec3(ex * R2D, ey * R2D, ez * R2D);
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
    result._rotation = eulerToQuat(_points[0]->_eulerRotation);
    result._scale = _points[0]->_scale;
    return result;
  }

  // Wrap time for looping curves
  if (_looping && n >= 2) {
    float tStart = _points[0]->_time;
    float tEnd = _points[n - 1]->_time;
    float duration = tEnd - tStart;
    if (duration > 1e-9f) {
      t = tStart + fmodf(t - tStart, duration);
      if (t < tStart) t += duration;
    }
  }

  auto seg = _findSegment(t);
  int i = seg._segIndex;
  float lt = seg._localT;

  const auto& A = *_points[i];
  const auto& B = *_points[i + 1];

  int im1 = std::max(0, i - 1);
  int ip2 = std::min(n - 1, i + 2);
  const auto& Aprev = *_points[im1];
  const auto& Bnext = *_points[ip2];

  // Helper lambda: interpolate a single scalar per-channel
  auto evalScalar = [&](CurveChannel ch, float vA, float vB, float vPrev, float vNext,
                        float tangentOutA, float tangentInB) -> float {
    auto chType = getChannelSegmentType(i, ch);
    switch (chType) {
      case CurveSegmentType::LINEAR: {
        return vA + (vB - vA) * lt;
      }
      case CurveSegmentType::BEZIER: {
        float cp0 = vA;
        float cp1 = vA + tangentOutA;
        float cp2 = vB + tangentInB;
        float cp3 = vB;
        return _evalScalarBezier(cp0, cp1, cp2, cp3, lt);
      }
      case CurveSegmentType::CATMULL_ROM: {
        return _evalScalarCatmullRom(vPrev, vA, vB, vNext, lt);
      }
      case CurveSegmentType::STEP:
      default:
        return vA;
    }
  };

  // Position: 3 channels
  float posVals[3];
  float posTangentComponents[3]; // for tangent derivative
  for (int c = 0; c < 3; c++) {
    CurveChannel ch = (CurveChannel)((int)CurveChannel::CC_POS_X + c);
    float vA = (&A._position.x)[c];
    float vB = (&B._position.x)[c];
    float vPrev = (&Aprev._position.x)[c];
    float vNext = (&Bnext._position.x)[c];
    float tOutA = (&A._tangent_out.x)[c];
    float tInB = (&B._tangent_in.x)[c];
    posVals[c] = evalScalar(ch, vA, vB, vPrev, vNext, tOutA, tInB);

    // Compute tangent derivative per-component
    auto chType = getChannelSegmentType(i, ch);
    switch (chType) {
      case CurveSegmentType::LINEAR:
        posTangentComponents[c] = vB - vA;
        break;
      case CurveSegmentType::BEZIER: {
        float cp0 = vA, cp1 = vA + tOutA, cp2 = vB + tInB, cp3 = vB;
        float u = 1.0f - lt;
        posTangentComponents[c] = (cp1 - cp0) * (3.0f * u * u) + (cp2 - cp1) * (6.0f * u * lt) + (cp3 - cp2) * (3.0f * lt * lt);
        break;
      }
      case CurveSegmentType::CATMULL_ROM: {
        float eps = 0.001f;
        float p_prev = _evalScalarCatmullRom(vPrev, vA, vB, vNext, std::max(0.0f, lt - eps));
        float p_next = _evalScalarCatmullRom(vPrev, vA, vB, vNext, std::min(1.0f, lt + eps));
        posTangentComponents[c] = (p_next - p_prev) / (2.0f * eps);
        break;
      }
      case CurveSegmentType::STEP:
      default:
        posTangentComponents[c] = 0.0f;
        break;
    }
  }
  result._position = fvec3(posVals[0], posVals[1], posVals[2]);
  result._tangent = fvec3(posTangentComponents[0], posTangentComponents[1], posTangentComponents[2]);
  float tangentLen = result._tangent.length();
  if (tangentLen > 1e-9f) {
    result._tangent = result._tangent * (1.0f / tangentLen);
  }

  // Rotation: 3 euler channels, then convert to quat
  float rotVals[3];
  for (int c = 0; c < 3; c++) {
    CurveChannel ch = (CurveChannel)((int)CurveChannel::CC_ROT_X + c);
    float vA = (&A._eulerRotation.x)[c];
    float vB = (&B._eulerRotation.x)[c];
    float vPrev = (&Aprev._eulerRotation.x)[c];
    float vNext = (&Bnext._eulerRotation.x)[c];
    float tOutA = (&A._rot_tangent_out.x)[c];
    float tInB = (&B._rot_tangent_in.x)[c];
    rotVals[c] = evalScalar(ch, vA, vB, vPrev, vNext, tOutA, tInB);
  }
  result._rotation = eulerToQuat(fvec3(rotVals[0], rotVals[1], rotVals[2]));

  // Scale: 3 channels
  float scaleVals[3];
  for (int c = 0; c < 3; c++) {
    CurveChannel ch = (CurveChannel)((int)CurveChannel::CC_SCALE_X + c);
    float vA = (&A._scale.x)[c];
    float vB = (&B._scale.x)[c];
    float vPrev = (&Aprev._scale.x)[c];
    float vNext = (&Bnext._scale.x)[c];
    float tOutA = (&A._scale_tangent_out.x)[c];
    float tInB = (&B._scale_tangent_in.x)[c];
    scaleVals[c] = evalScalar(ch, vA, vB, vPrev, vNext, tOutA, tInB);
  }
  result._scale = fvec3(scaleVals[0], scaleVals[1], scaleVals[2]);

  return result;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::sampleEuler(float t) const {
  int n = (int)_points.size();
  if (n == 0) return fvec3();
  if (n == 1) return _points[0]->_eulerRotation;

  // Wrap time for looping curves
  if (_looping && n >= 2) {
    float tStart = _points[0]->_time;
    float tEnd = _points[n - 1]->_time;
    float duration = tEnd - tStart;
    if (duration > 1e-9f) {
      t = tStart + fmodf(t - tStart, duration);
      if (t < tStart) t += duration;
    }
  }

  auto seg = _findSegment(t);
  int i = seg._segIndex;
  float lt = seg._localT;

  const auto& A = *_points[i];
  const auto& B = *_points[i + 1];
  int im1 = std::max(0, i - 1);
  int ip2 = std::min(n - 1, i + 2);
  const auto& Aprev = *_points[im1];
  const auto& Bnext = *_points[ip2];

  float rotVals[3];
  for (int c = 0; c < 3; c++) {
    CurveChannel ch = (CurveChannel)((int)CurveChannel::CC_ROT_X + c);
    float vA = (&A._eulerRotation.x)[c];
    float vB = (&B._eulerRotation.x)[c];
    float vPrev = (&Aprev._eulerRotation.x)[c];
    float vNext = (&Bnext._eulerRotation.x)[c];
    float tOutA = (&A._rot_tangent_out.x)[c];
    float tInB = (&B._rot_tangent_in.x)[c];
    auto chType = getChannelSegmentType(i, ch);
    switch (chType) {
      case CurveSegmentType::LINEAR:
        rotVals[c] = vA + (vB - vA) * lt;
        break;
      case CurveSegmentType::BEZIER:
        rotVals[c] = _evalScalarBezier(vA, vA + tOutA, vB + tInB, vB, lt);
        break;
      case CurveSegmentType::CATMULL_ROM:
        rotVals[c] = _evalScalarCatmullRom(vPrev, vA, vB, vNext, lt);
        break;
      case CurveSegmentType::STEP:
      default:
        rotVals[c] = vA;
        break;
    }
  }
  return fvec3(rotVals[0], rotVals[1], rotVals[2]);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 TransformCurve::samplePosition(float t) const {
  return sample(t)._position;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 TransformCurve::sampleMatrix(float t) const {
  auto s = sample(t);
  fmtx4 mtx;
  if (_useNonUniformScale) {
    mtx.compose(s._position, s._rotation, s._scale);
  } else {
    mtx.compose(s._position, s._rotation, s._scale.x);
  }
  return mtx;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformCurve::preDeserialize(ork::reflect::serdes::IDeserializer& deser) {
  _points.clear();
  _segmentTypes.clear();
  _channelSegmentTypes.clear();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformCurve::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  _sortByTime();

  // Backward-compat: if channelSegmentTypes is empty but legacy segmentTypes is not,
  // expand each legacy type to all 9 channels
  if (_channelSegmentTypes.empty() && !_segmentTypes.empty()) {
    int cc_count = (int)CurveChannel::CC_COUNT;
    int nsegs = (int)_segmentTypes.size();
    _channelSegmentTypes.resize(nsegs * cc_count);
    for (int s = 0; s < nsegs; s++) {
      for (int c = 0; c < cc_count; c++) {
        _channelSegmentTypes[s * cc_count + c] = _segmentTypes[s];
      }
    }
  }

  _syncChannelSegmentTypes();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurve::enforceLoopConstraints() {
  if (!_looping) return;
  int n = (int)_points.size();
  if (n < 2) return;

  auto& first = _points[0];
  auto& last = _points[n - 1];

  // Lock last point's values to first point (time stays independent)
  last->_position = first->_position;
  last->_eulerRotation = first->_eulerRotation;
  last->_scale = first->_scale;

  int lastSeg = n - 2;

  // Mirror position tangents at the seam
  if (lastSeg >= 0 && getChannelSegmentType(lastSeg, CurveChannel::CC_POS_X) == CurveSegmentType::BEZIER) {
    last->_tangent_in = first->_tangent_out * -1.0f;
  }
  if (getChannelSegmentType(0, CurveChannel::CC_POS_X) == CurveSegmentType::BEZIER) {
    first->_tangent_in = last->_tangent_out * -1.0f;
  }

  // Mirror rotation tangents at the seam
  if (lastSeg >= 0 && getChannelSegmentType(lastSeg, CurveChannel::CC_ROT_X) == CurveSegmentType::BEZIER) {
    last->_rot_tangent_in = first->_rot_tangent_out * -1.0f;
  }
  if (getChannelSegmentType(0, CurveChannel::CC_ROT_X) == CurveSegmentType::BEZIER) {
    first->_rot_tangent_in = last->_rot_tangent_out * -1.0f;
  }

  // Mirror scale tangents at the seam
  if (lastSeg >= 0 && getChannelSegmentType(lastSeg, CurveChannel::CC_SCALE_X) == CurveSegmentType::BEZIER) {
    last->_scale_tangent_in = first->_scale_tangent_out * -1.0f;
  }
  if (getChannelSegmentType(0, CurveChannel::CC_SCALE_X) == CurveSegmentType::BEZIER) {
    first->_scale_tangent_in = last->_scale_tangent_out * -1.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace math
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
