////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/editor/manip.h>
#include <ork/math/transform_curve.h>
#include <ork/reflect/properties/registerX.inl>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::editor {
////////////////////////////////////////////////////////////////////////////////

void ManipulatorInterface::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////////////////////

void DecompTransformManipulator::describeX(class_t* clazz) {
}

DecompTransformManipulator::DecompTransformManipulator()
    : _target(nullptr)
    , _initialScale(1.0f) {
}

DecompTransformManipulator::DecompTransformManipulator(decompxf_ptr_t target)
    : _target(target)
    , _initialScale(1.0f) {
}

void DecompTransformManipulator::setTarget(decompxf_ptr_t target) {
  _target = target;
}

fmtx4 DecompTransformManipulator::getWorldMatrix() const {
  if (_target) {
    return _target->composed();
  }
  return fmtx4::Identity();
}

fvec3 DecompTransformManipulator::getWorldPosition() const {
  if (_target) {
    return _target->_translation;
  }
  return fvec3();
}

fquat DecompTransformManipulator::getWorldRotation() const {
  if (_target) {
    return _target->_rotation;
  }
  return fquat();
}

void DecompTransformManipulator::applyTranslationDelta(const fvec3& delta) {
  if (_target) {
    _target->_translation += delta;
  }
}

void DecompTransformManipulator::applyRotationDelta(const fquat& delta) {
  if (_target) {
    // Apply delta in LOCAL space: rotation * delta
    // Delta is around pure local axis (0,1,0 etc), applied in object's local frame
    _target->_rotation = _target->_rotation * delta;
    _target->_rotation.normalizeInPlace();
  }
}

void DecompTransformManipulator::applyScaleDelta(float uniformDelta) {
  if (_target) {
    _target->_uniformScale *= uniformDelta;
  }
}

void DecompTransformManipulator::setWorldRotation(const fquat& rot) {
  if (_target) {
    _target->_rotation = rot;
  }
}

void DecompTransformManipulator::onBeginManipulation(ManipMode mode) {
  if (_target) {
    _initialTranslation = _target->_translation;
    _initialRotation = _target->_rotation;
    _initialScale = _target->_uniformScale;
  }
}

void DecompTransformManipulator::onEndManipulation(ManipMode mode) {
  // Could emit undo command here using _initial* snapshots
}

bool DecompTransformManipulator::supportsNonUniformScaling() const {
  if (_target) {
    return _target->_useNonUniformScale;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CurvePointManipulator
////////////////////////////////////////////////////////////////////////////////

void CurvePointManipulator::describeX(class_t* clazz) {
}

CurvePointManipulator::CurvePointManipulator()
    : _curve(nullptr)
    , _pointIndex(-1) {
}

CurvePointManipulator::CurvePointManipulator(math::transformcurve_ptr_t curve, int pointIndex)
    : _curve(curve)
    , _pointIndex(pointIndex) {
}

void CurvePointManipulator::setTarget(math::transformcurve_ptr_t curve, int pointIndex) {
  _curve = curve;
  _pointIndex = pointIndex;
}

fmtx4 CurvePointManipulator::getWorldMatrix() const {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    auto pt = _curve->getPoint(_pointIndex);
    fquat rot = _curve->eulerToQuat(pt->_eulerRotation);
    float s = pt->_scale.x;  // uniform scale from X component
    fmtx4 mtx;
    mtx.compose(pt->_position, rot, s);
    return mtx;
  }
  return fmtx4::Identity();
}

fvec3 CurvePointManipulator::getWorldPosition() const {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    return _curve->getPoint(_pointIndex)->_position;
  }
  return fvec3();
}

fquat CurvePointManipulator::getWorldRotation() const {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    return _curve->eulerToQuat(_curve->getPoint(_pointIndex)->_eulerRotation);
  }
  return fquat();
}

void CurvePointManipulator::applyTranslationDelta(const fvec3& delta) {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    _curve->getPoint(_pointIndex)->_position += delta;
    if (_curve->_looping) {
      _curve->enforceLoopConstraints();
    }
    if (_onPointMoved) {
      _onPointMoved();
    }
  }
}

void CurvePointManipulator::applyRotationDelta(const fquat& delta) {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    auto pt = _curve->getPoint(_pointIndex);
    fquat current = _curve->eulerToQuat(pt->_eulerRotation);
    fquat result = current * delta;
    result.normalizeInPlace();
    pt->_eulerRotation = _curve->quatToEuler(result);
    if (_curve->_looping) {
      _curve->enforceLoopConstraints();
    }
    if (_onPointMoved) {
      _onPointMoved();
    }
  }
}

void CurvePointManipulator::applyScaleDelta(float uniformDelta) {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    auto pt = _curve->getPoint(_pointIndex);
    pt->_scale *= uniformDelta;
    if (_curve->_looping) {
      _curve->enforceLoopConstraints();
    }
    if (_onPointMoved) {
      _onPointMoved();
    }
  }
}

void CurvePointManipulator::setWorldRotation(const fquat& rot) {
  if (_curve && _pointIndex >= 0 && _pointIndex < _curve->numPoints()) {
    _curve->getPoint(_pointIndex)->_eulerRotation = _curve->quatToEuler(rot);
    if (_onPointMoved) {
      _onPointMoved();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Legacy JointManipulatorInterface
////////////////////////////////////////////////////////////////////////////////

bool JointManipulatorInterface::supportsTranslation() const {
  return true;
}
bool JointManipulatorInterface::supportsRotation() const {
  return true;
}
bool JointManipulatorInterface::supportsUniformScaling() const {
  return true;
}

void JointManipulatorInterface::_onBeginTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}

void JointManipulatorInterface::_onBeginRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}

void JointManipulatorInterface::_onBeginScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::editor
////////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::editor::ManipulatorInterface, "ManipulatorInterface");
ImplementReflectionX(ork::lev2::editor::DecompTransformManipulator, "DecompTransformManipulator");
ImplementReflectionX(ork::lev2::editor::CurvePointManipulator, "CurvePointManipulator");
