////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/editor/manip_controller.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/math/misc_math.h>

namespace ork::lev2::editor {

////////////////////////////////////////////////////////////////////////////////

ManipController::ManipController() {
}

////////////////////////////////////////////////////////////////////////////////

void ManipController::setMode(ManipMode mode) {
  _mode = mode;
  _hoveredAxis = ManipAxis::NONE;
}

void ManipController::setTarget(manipinterface_ptr_t target) {
  _target = target;
  _hoveredAxis = ManipAxis::NONE;
  _activeAxis = ManipAxis::NONE;
  _isDragging = false;
}

void ManipController::updateCamera(const CameraMatrices& matrices, const fvec2& viewport_dim) {
  _camMatrices = matrices;
  _viewportDim = viewport_dim;
}

////////////////////////////////////////////////////////////////////////////////
// Projection helpers
////////////////////////////////////////////////////////////////////////////////

fvec3 ManipController::_getCameraEye() const {
  // Extract eye position from inverse view matrix
  const fmtx4& ivmat = _camMatrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(3, 0), ivmat.elemXY(3, 1), ivmat.elemXY(3, 2));
}

fvec3 ManipController::_getCameraDir() const {
  const fmtx4& ivmat = _camMatrices.GetIVMatrix();
  return -fvec3(ivmat.elemXY(2, 0), ivmat.elemXY(2, 1), ivmat.elemXY(2, 2)).normalized();
}

fvec3 ManipController::_getCameraRight() const {
  const fmtx4& ivmat = _camMatrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(0, 0), ivmat.elemXY(0, 1), ivmat.elemXY(0, 2)).normalized();
}

fvec3 ManipController::_getCameraUp() const {
  const fmtx4& ivmat = _camMatrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(1, 0), ivmat.elemXY(1, 1), ivmat.elemXY(1, 2)).normalized();
}

fvec2 ManipController::_project(const fvec3& worldPos) const {
  fvec4 clip = _camMatrices.GetVPMatrix() * fvec4(worldPos, 1.0f);
  if (clip.w <= 0.0001f) {
    return fvec2(-10000, -10000); // Behind camera
  }
  fvec2 ndc = fvec2(clip.x, clip.y) / clip.w;
  // Vulkan convention: NDC Y=-1 at top, Y=+1 at bottom
  // Screen Y=0 at top, Y=height at bottom
  // So: screenY = (ndc.y * 0.5 + 0.5) * height (no flip needed)
  return fvec2(
      (ndc.x * 0.5f + 0.5f) * _viewportDim.x,
      (ndc.y * 0.5f + 0.5f) * _viewportDim.y
  );
}

float ManipController::_distToSegment(const fvec2& p, const fvec2& a, const fvec2& b) const {
  fvec2 ab = b - a;
  fvec2 ap = p - a;
  float len_sq = ab.dotWith(ab);
  if (len_sq < 0.0001f) {
    return (p - a).length();
  }
  float t = std::clamp(ap.dotWith(ab) / len_sq, 0.0f, 1.0f);
  fvec2 closest = a + ab * t;
  return (p - closest).length();
}

float ManipController::_computeWorldGizmoScale() const {
  if (!_target) return 1.0f;

  fvec3 gizmoPos = _target->getWorldPosition();
  fvec3 camPos = _getCameraEye();
  float dist = (gizmoPos - camPos).length();

  // Match the rendering scale calculation in sgnode_manipgizmo.cpp
  return dist * 0.1f;
}

////////////////////////////////////////////////////////////////////////////////
// Hit Testing (Screen-Space Proximity)
////////////////////////////////////////////////////////////////////////////////

ManipAxis ManipController::_hitTestTranslation(const fvec2& mousePos) {
  if (!_target) return ManipAxis::NONE;

  fvec3 origin = _target->getWorldPosition();
  float scale = _computeWorldGizmoScale();

  fvec2 origin2D = _project(origin);
  fvec2 xEnd = _project(origin + fvec3(scale, 0, 0));
  fvec2 yEnd = _project(origin + fvec3(0, scale, 0));
  fvec2 zEnd = _project(origin + fvec3(0, 0, scale));

  // Test single axes first (priority)
  float distX = _distToSegment(mousePos, origin2D, xEnd);
  float distY = _distToSegment(mousePos, origin2D, yEnd);
  float distZ = _distToSegment(mousePos, origin2D, zEnd);

  if (distX < _hitThreshold) return ManipAxis::X;
  if (distY < _hitThreshold) return ManipAxis::Y;
  if (distZ < _hitThreshold) return ManipAxis::Z;

  // Test plane handles (small squares at half axis length)
  float planeOffset = scale * 0.5f;
  fvec2 xyPlane = _project(origin + fvec3(planeOffset, planeOffset, 0));
  fvec2 xzPlane = _project(origin + fvec3(planeOffset, 0, planeOffset));
  fvec2 yzPlane = _project(origin + fvec3(0, planeOffset, planeOffset));

  float planeThreshold = _hitThreshold * 1.5f;
  if ((mousePos - xyPlane).length() < planeThreshold) return ManipAxis::XY;
  if ((mousePos - xzPlane).length() < planeThreshold) return ManipAxis::XZ;
  if ((mousePos - yzPlane).length() < planeThreshold) return ManipAxis::YZ;

  // Test center (free movement)
  if ((mousePos - origin2D).length() < _hitThreshold) return ManipAxis::FREE;

  return ManipAxis::NONE;
}

ManipAxis ManipController::_hitTestRotation(const fvec2& mousePos) {
  if (!_target) return ManipAxis::NONE;

  fvec3 origin = _target->getWorldPosition();
  float scale = _computeWorldGizmoScale();
  float ringRadius = scale * 0.8f;  // Match _ringRadius in ManipGizmoDrawableData

  // Get target's rotation to test in local space
  fquat targetRot = _target->getWorldRotation();
  fvec3 localX = targetRot.transform(fvec3(1, 0, 0));
  fvec3 localY = targetRot.transform(fvec3(0, 1, 0));
  fvec3 localZ = targetRot.transform(fvec3(0, 0, 1));

  // Test each rotation ring by sampling points
  auto testRing = [&](const fvec3& perp1, const fvec3& perp2) -> float {
    float minDist = FLT_MAX;
    const int segments = 32;
    for (int i = 0; i < segments; i++) {
      float a0 = (float(i) / segments) * 2.0f * PI;
      float a1 = (float(i + 1) / segments) * 2.0f * PI;
      fvec3 p0 = origin + (perp1 * cos(a0) + perp2 * sin(a0)) * ringRadius;
      fvec3 p1 = origin + (perp1 * cos(a1) + perp2 * sin(a1)) * ringRadius;
      fvec2 p0_2d = _project(p0);
      fvec2 p1_2d = _project(p1);
      minDist = std::min(minDist, _distToSegment(mousePos, p0_2d, p1_2d));
    }
    return minDist;
  };

  float distX = testRing(localY, localZ); // YZ ring in local space (rotate around local X)
  float distY = testRing(localX, localZ); // XZ ring in local space (rotate around local Y)
  float distZ = testRing(localX, localY); // XY ring in local space (rotate around local Z)

  float minDist = std::min({distX, distY, distZ});
  if (minDist < _hitThreshold) {
    if (minDist == distX) return ManipAxis::X;
    if (minDist == distY) return ManipAxis::Y;
    if (minDist == distZ) return ManipAxis::Z;
  }

  // Test trackball (center region)
  fvec2 origin2D = _project(origin);
  float screenRadius = (_project(origin + fvec3(scale * 0.7f, 0, 0)) - origin2D).length();
  if ((mousePos - origin2D).length() < screenRadius * 0.5f) {
    return ManipAxis::FREE;
  }

  return ManipAxis::NONE;
}

ManipAxis ManipController::_hitTestScale(const fvec2& mousePos) {
  return _hitTestTranslation(mousePos);
}

ManipAxis ManipController::_hitTestGizmo(const fvec2& mousePos) {
  switch (_mode) {
    case ManipMode::TRANSLATE: return _hitTestTranslation(mousePos);
    case ManipMode::ROTATE:    return _hitTestRotation(mousePos);
    case ManipMode::SCALE:     return _hitTestScale(mousePos);
  }
  return ManipAxis::NONE;
}

////////////////////////////////////////////////////////////////////////////////
// Delta Computation
////////////////////////////////////////////////////////////////////////////////

fvec3 ManipController::_computeTranslationDelta(const fvec2& mouseDelta, ManipAxis axis) {
  if (!_target) return fvec3();

  fvec3 gizmoPos = _target->getWorldPosition();
  float worldScale = _computeWorldGizmoScale();

  // Use ManipHandler for ray-plane intersection
  _handler.Origin = gizmoPos;

  // Convert screen coordinates to NDC (-1 to +1 range)
  // Vulkan convention: Y increases downward on screen
  fvec2 curMouseNDC(
      (_dragPrevMouse.x / _viewportDim.x) * 2.0f - 1.0f,
      (_dragPrevMouse.y / _viewportDim.y) * 2.0f - 1.0f
  );
  fvec2 newMouseNDC(
      ((_dragPrevMouse.x + mouseDelta.x) / _viewportDim.x) * 2.0f - 1.0f,
      ((_dragPrevMouse.y + mouseDelta.y) / _viewportDim.y) * 2.0f - 1.0f
  );

  // Get intersection points
  fvec3 prevIsect, curIsect;
  float prevAngle, curAngle;

  fvec3 camDir = (_getCameraEye() - gizmoPos).normalized();

  switch (axis) {
    case ManipAxis::X: {
      bool useXZ = fabs(camDir.y) > fabs(camDir.z);
      if (useXZ) {
        _handler.IntersectXZ(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectXZ(newMouseNDC, curIsect, curAngle);
      } else {
        _handler.IntersectXY(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectXY(newMouseNDC, curIsect, curAngle);
      }
      return fvec3((curIsect.x - prevIsect.x), 0, 0);
    }
    case ManipAxis::Y: {
      bool useXY = fabs(camDir.z) > fabs(camDir.x);
      if (useXY) {
        _handler.IntersectXY(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectXY(newMouseNDC, curIsect, curAngle);
      } else {
        _handler.IntersectYZ(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectYZ(newMouseNDC, curIsect, curAngle);
      }
      return fvec3(0, (curIsect.y - prevIsect.y), 0);
    }
    case ManipAxis::Z: {
      bool useXZ = fabs(camDir.y) > fabs(camDir.x);
      if (useXZ) {
        _handler.IntersectXZ(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectXZ(newMouseNDC, curIsect, curAngle);
      } else {
        _handler.IntersectYZ(curMouseNDC, prevIsect, prevAngle);
        _handler.IntersectYZ(newMouseNDC, curIsect, curAngle);
      }
      return fvec3(0, 0, (curIsect.z - prevIsect.z));
    }
    case ManipAxis::XY: {
      _handler.IntersectXY(curMouseNDC, prevIsect, prevAngle);
      _handler.IntersectXY(newMouseNDC, curIsect, curAngle);
      return fvec3(curIsect.x - prevIsect.x, curIsect.y - prevIsect.y, 0);
    }
    case ManipAxis::XZ: {
      _handler.IntersectXZ(curMouseNDC, prevIsect, prevAngle);
      _handler.IntersectXZ(newMouseNDC, curIsect, curAngle);
      return fvec3(curIsect.x - prevIsect.x, 0, curIsect.z - prevIsect.z);
    }
    case ManipAxis::YZ: {
      _handler.IntersectYZ(curMouseNDC, prevIsect, prevAngle);
      _handler.IntersectYZ(newMouseNDC, curIsect, curAngle);
      return fvec3(0, curIsect.y - prevIsect.y, curIsect.z - prevIsect.z);
    }
    case ManipAxis::FREE:
    case ManipAxis::VIEW: {
      fvec3 camRight = _getCameraRight();
      fvec3 camUp = _getCameraUp();
      float sensitivity = worldScale * 0.01f;
      return camRight * mouseDelta.x * sensitivity - camUp * mouseDelta.y * sensitivity;
    }
    default:
      return fvec3();
  }
}

bool ManipController::_computeRotationAngle(const fvec2& mousePos, float& outAngle) const {
  if (!_target) return false;

  fvec3 gizmoPos = _target->getWorldPosition();

  // Convert screen coordinates to NDC
  fvec2 mouseNDC(
      (mousePos.x / _viewportDim.x) * 2.0f - 1.0f,
      (mousePos.y / _viewportDim.y) * 2.0f - 1.0f
  );

  // Generate ray from camera through mouse position
  fvec3 rayNear, rayDir;
  fvec3 vWinN(mouseNDC.x, mouseNDC.y, 0.0f);
  fvec3 vWinF(mouseNDC.x, mouseNDC.y, 1.0f);
  fvec3 rayFar;
  fmtx4::unProject(_camMatrices.GetIVPMatrix(), vWinN, rayNear);
  fmtx4::unProject(_camMatrices.GetIVPMatrix(), vWinF, rayFar);
  rayDir = (rayFar - rayNear).normalized();

  // Create plane from stored normal and gizmo origin
  fplane3 rotationPlane;
  rotationPlane.CalcFromNormalAndOrigin(_rotationPlaneNormal, gizmoPos);

  // Intersect ray with plane
  fray3 ray;
  ray.mOrigin = rayNear;
  ray.mDirection = rayDir;
  float isectDist;
  fvec3 isectPoint;
  bool doesIntersect = rotationPlane.Intersect(ray, isectDist, isectPoint);

  if (!doesIntersect) return false;

  // Compute angle in the plane's 2D coordinate system
  fvec3 toIsect = isectPoint - gizmoPos;
  float coord1 = toIsect.dotWith(_rotationPlanePerp1);
  float coord2 = toIsect.dotWith(_rotationPlanePerp2);
  outAngle = atan2(coord2, coord1);

  return true;
}

fquat ManipController::_computeRotationDelta(const fvec2& mousePos, ManipAxis axis) {
  if (!_target) return fquat();

  float currentAngle;
  if (!_computeRotationAngle(mousePos, currentAngle)) {
    return fquat();  // No intersection
  }

  float deltaAngle = currentAngle - _rotationBaseAngle;

  // Wrap angle
  while (deltaAngle > PI) deltaAngle -= 2.0f * PI;
  while (deltaAngle < -PI) deltaAngle += 2.0f * PI;

  // Update base angle for next frame
  _rotationBaseAngle = currentAngle;

  // Rotation is around the LOCAL axis (stored in _rotationPlaneNormal)
  fquat delta;
  delta.fromAxisAngle(fvec4(_rotationPlaneNormal, deltaAngle));
  return delta;
}

float ManipController::_computeScaleDelta(const fvec2& mouseDelta, ManipAxis axis) {
  float sensitivity = 0.01f;
  return 1.0f + mouseDelta.x * sensitivity;
}

////////////////////////////////////////////////////////////////////////////////
// Event Handling
////////////////////////////////////////////////////////////////////////////////

ui::HandlerResult ManipController::handleEvent(ui::event_constptr_t ev) {
  if (!_target) {
    return ui::HandlerResult();
  }

  fvec2 mousePos(ev->miX, ev->miY);
  ui::HandlerResult result;

  switch (ev->_eventcode) {
    case ui::EventCode::MOVE: {
      _hoveredAxis = _hitTestGizmo(mousePos);
      break;
    }

    case ui::EventCode::PUSH: {
      ManipAxis hit = _hitTestGizmo(mousePos);
      if (hit != ManipAxis::NONE) {
        _activeAxis = hit;
        _isDragging = true;
        _dragStartMouse = mousePos;
        _dragPrevMouse = mousePos;
        _dragStartPos = _target->getWorldPosition();
        _dragStartRot = _target->getWorldRotation();

        // Initialize ManipHandler with NDC coordinates
        fvec2 mouseNDC(
            (mousePos.x / _viewportDim.x) * 2.0f - 1.0f,
            (mousePos.y / _viewportDim.y) * 2.0f - 1.0f
        );
        _handler.Init(mouseNDC, _camMatrices.GetIVPMatrix(), fquat());

        // For rotation mode, set up the rotation plane in LOCAL space
        if (_mode == ManipMode::ROTATE) {
          fquat targetRot = _target->getWorldRotation();
          fvec3 localX = targetRot.transform(fvec3(1, 0, 0));
          fvec3 localY = targetRot.transform(fvec3(0, 1, 0));
          fvec3 localZ = targetRot.transform(fvec3(0, 0, 1));

          switch (hit) {
            case ManipAxis::X:
              // Rotate around local X axis, plane is in local YZ
              _rotationPlaneNormal = localX;
              _rotationPlanePerp1 = localY;
              _rotationPlanePerp2 = localZ;
              break;
            case ManipAxis::Y:
              // Rotate around local Y axis, plane is in local XZ
              _rotationPlaneNormal = localY;
              _rotationPlanePerp1 = localZ;
              _rotationPlanePerp2 = localX;
              break;
            case ManipAxis::Z:
              // Rotate around local Z axis, plane is in local XY
              _rotationPlaneNormal = localZ;
              _rotationPlanePerp1 = localX;
              _rotationPlanePerp2 = localY;
              break;
            case ManipAxis::FREE:
            case ManipAxis::VIEW:
            default:
              // View-aligned rotation
              _rotationPlaneNormal = _getCameraDir();
              _rotationPlanePerp1 = _getCameraRight();
              _rotationPlanePerp2 = _getCameraUp();
              break;
          }

          // Compute and store base angle
          _computeRotationAngle(mousePos, _rotationBaseAngle);
        }

        _target->onBeginManipulation(_mode);
        result.setHandled(reinterpret_cast<ui::Widget*>(this));  // non-null to mark handled
      }
      break;
    }

    case ui::EventCode::DRAG: {
      if (_isDragging && _activeAxis != ManipAxis::NONE) {
        fvec2 mouseDelta = mousePos - _dragPrevMouse;

        switch (_mode) {
          case ManipMode::TRANSLATE: {
            fvec3 delta = _computeTranslationDelta(mouseDelta, _activeAxis);
            _target->applyTranslationDelta(delta);
            break;
          }
          case ManipMode::ROTATE: {
            fquat delta = _computeRotationDelta(mousePos, _activeAxis);
            _target->applyRotationDelta(delta);
            break;
          }
          case ManipMode::SCALE: {
            float delta = _computeScaleDelta(mouseDelta, _activeAxis);
            _target->applyScaleDelta(delta);
            break;
          }
        }

        _dragPrevMouse = mousePos;
        result.setHandled(reinterpret_cast<ui::Widget*>(this));  // non-null to mark handled
      }
      break;
    }

    case ui::EventCode::RELEASE: {
      if (_isDragging) {
        _target->onEndManipulation(_mode);
        _isDragging = false;
        _activeAxis = ManipAxis::NONE;
        result.setHandled(reinterpret_cast<ui::Widget*>(this));  // non-null to mark handled
      }
      break;
    }

    default:
      break;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Drawing - uses PrimitivesInterface built-in axis rendering
////////////////////////////////////////////////////////////////////////////////

void ManipController::draw(Context* ctx) {
  // Gizmo rendering is now handled by ManipGizmoDrawableData in the scenegraph
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::editor
