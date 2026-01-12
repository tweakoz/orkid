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

  // Get axes based on space mode
  fvec3 axisX(1, 0, 0), axisY(0, 1, 0), axisZ(0, 0, 1);
  if (_space == ManipSpace::LOCAL) {
    fquat targetRot = _target->getWorldRotation();
    axisX = targetRot.transform(fvec3(1, 0, 0));
    axisY = targetRot.transform(fvec3(0, 1, 0));
    axisZ = targetRot.transform(fvec3(0, 0, 1));
  }

  fvec2 origin2D = _project(origin);
  fvec2 xEnd = _project(origin + axisX * scale);
  fvec2 yEnd = _project(origin + axisY * scale);
  fvec2 zEnd = _project(origin + axisZ * scale);

  // Test single axes first (priority)
  float distX = _distToSegment(mousePos, origin2D, xEnd);
  float distY = _distToSegment(mousePos, origin2D, yEnd);
  float distZ = _distToSegment(mousePos, origin2D, zEnd);

  if (distX < _hitThreshold) return ManipAxis::X;
  if (distY < _hitThreshold) return ManipAxis::Y;
  if (distZ < _hitThreshold) return ManipAxis::Z;

  // Test plane handles (small squares at half axis length)
  float planeOffset = scale * 0.5f;
  fvec2 xyPlane = _project(origin + (axisX + axisY) * planeOffset);
  fvec2 xzPlane = _project(origin + (axisX + axisZ) * planeOffset);
  fvec2 yzPlane = _project(origin + (axisY + axisZ) * planeOffset);

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
  float ringRadius = scale * _ringRadiusScale;

  // Get target's rotation to test in local space
  fquat targetRot = _target->getWorldRotation();
  fvec3 localX = targetRot.transform(fvec3(1, 0, 0));
  fvec3 localY = targetRot.transform(fvec3(0, 1, 0));
  fvec3 localZ = targetRot.transform(fvec3(0, 0, 1));

  // Check which rings are selectable (not too edge-on)
  // We skip hit-testing for edge-on rings even though they're rendered dimmed
  fvec3 camDir = _getCameraDir();
  auto isRingSelectable = [&](const fvec3& ringNormal) -> bool {
    float dotProduct = fabs(ringNormal.dotWith(camDir));
    float angleDegrees = 90.0f - (acos(dotProduct) * 180.0f / PI);
    return angleDegrees >= _minRingElevationDegrees;
  };

  bool xSelectable = isRingSelectable(localX);
  bool ySelectable = isRingSelectable(localY);
  bool zSelectable = isRingSelectable(localZ);

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

  float distX = xSelectable ? testRing(localY, localZ) : FLT_MAX; // YZ ring (rotate around X)
  float distY = ySelectable ? testRing(localX, localZ) : FLT_MAX; // XZ ring (rotate around Y)
  float distZ = zSelectable ? testRing(localX, localY) : FLT_MAX; // XY ring (rotate around Z)

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

  // Get local axes if in local space mode
  fquat targetRot = _target->getWorldRotation();
  fvec3 axisX = (_space == ManipSpace::LOCAL) ? targetRot.transform(fvec3(1, 0, 0)) : fvec3(1, 0, 0);
  fvec3 axisY = (_space == ManipSpace::LOCAL) ? targetRot.transform(fvec3(0, 1, 0)) : fvec3(0, 1, 0);
  fvec3 axisZ = (_space == ManipSpace::LOCAL) ? targetRot.transform(fvec3(0, 0, 1)) : fvec3(0, 0, 1);

  // Convert screen coordinates to NDC (-1 to +1 range)
  fvec2 curMouseNDC(
      (_dragPrevMouse.x / _viewportDim.x) * 2.0f - 1.0f,
      (_dragPrevMouse.y / _viewportDim.y) * 2.0f - 1.0f
  );
  fvec2 newMouseNDC(
      ((_dragPrevMouse.x + mouseDelta.x) / _viewportDim.x) * 2.0f - 1.0f,
      ((_dragPrevMouse.y + mouseDelta.y) / _viewportDim.y) * 2.0f - 1.0f
  );

  // Project mouse movement onto the constraint axis/plane
  // Use ray-plane intersection for accurate world-space movement
  fvec3 camEye = _getCameraEye();
  fvec3 camDir = _getCameraDir();

  auto unprojectToRay = [&](const fvec2& ndc) -> fray3 {
    fvec3 rayNear, rayFar;
    fvec3 vWinN(ndc.x, ndc.y, 0.0f);
    fvec3 vWinF(ndc.x, ndc.y, 1.0f);
    fmtx4::unProject(_camMatrices.GetIVPMatrix(), vWinN, rayNear);
    fmtx4::unProject(_camMatrices.GetIVPMatrix(), vWinF, rayFar);
    return fray3(rayNear, (rayFar - rayNear).normalized());
  };

  fray3 prevRay = unprojectToRay(curMouseNDC);
  fray3 curRay = unprojectToRay(newMouseNDC);

  // For single-axis constraints, find the plane that contains the axis
  // and is most perpendicular to the camera view
  auto computeAxisDelta = [&](const fvec3& constraintAxis) -> fvec3 {
    // Choose a plane containing the axis that's well-oriented to the camera
    fvec3 toCamera = (camEye - gizmoPos).normalized();
    fvec3 planeNormal = constraintAxis.crossWith(toCamera);
    if (planeNormal.magnitudeSquared() < 0.001f) {
      // Axis points at camera, use camera up as fallback
      planeNormal = constraintAxis.crossWith(_getCameraUp());
    }
    planeNormal = planeNormal.crossWith(constraintAxis).normalized();

    fplane3 plane;
    plane.CalcFromNormalAndOrigin(planeNormal, gizmoPos);

    float prevDist, curDist;
    fvec3 prevIsect, curIsect;
    if (plane.Intersect(prevRay, prevDist, prevIsect) && plane.Intersect(curRay, curDist, curIsect)) {
      fvec3 delta = curIsect - prevIsect;
      // Project onto constraint axis
      return constraintAxis * delta.dotWith(constraintAxis);
    }
    return fvec3();
  };

  // For plane constraints, intersect with the plane
  auto computePlaneDelta = [&](const fvec3& planeNormal) -> fvec3 {
    fplane3 plane;
    plane.CalcFromNormalAndOrigin(planeNormal, gizmoPos);

    float prevDist, curDist;
    fvec3 prevIsect, curIsect;
    if (plane.Intersect(prevRay, prevDist, prevIsect) && plane.Intersect(curRay, curDist, curIsect)) {
      return curIsect - prevIsect;
    }
    return fvec3();
  };

  switch (axis) {
    case ManipAxis::X:
      return computeAxisDelta(axisX);
    case ManipAxis::Y:
      return computeAxisDelta(axisY);
    case ManipAxis::Z:
      return computeAxisDelta(axisZ);
    case ManipAxis::XY:
      return computePlaneDelta(axisZ);  // XY plane has Z normal
    case ManipAxis::XZ:
      return computePlaneDelta(axisY);  // XZ plane has Y normal
    case ManipAxis::YZ:
      return computePlaneDelta(axisX);  // YZ plane has X normal
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

fquat ManipController::_computeRotationAbsolute(const fvec2& mousePos, ManipAxis axis) {
  if (!_target) return _dragStartRot;

  float currentAngle;
  if (!_computeRotationAngle(mousePos, currentAngle)) {
    return _dragStartRot;  // No intersection, keep start rotation
  }

  // Compute total angle from drag start (not delta from last frame)
  float totalAngle = currentAngle - _rotationBaseAngle;

  // Wrap angle
  while (totalAngle > PI) totalAngle -= 2.0f * PI;
  while (totalAngle < -PI) totalAngle += 2.0f * PI;

  // Create rotation around pure LOCAL axis (identity space)
  fvec3 localAxis;
  switch (axis) {
    case ManipAxis::X: localAxis = fvec3(1, 0, 0); break;
    case ManipAxis::Y: localAxis = fvec3(0, 1, 0); break;
    case ManipAxis::Z: localAxis = fvec3(0, 0, 1); break;
    default: localAxis = fvec3(0, 1, 0); break;
  }

  fquat localRot;
  localRot.fromAxisAngle(fvec4(localAxis, totalAngle));

  // Return absolute rotation: localRotation * startRotation
  // (local rotation applied in world frame, then base orientation)
  return localRot * _dragStartRot;
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
              _rotationPlaneNormal = localX;
              _rotationPlanePerp1 = localZ;
              _rotationPlanePerp2 = localY;
              break;
            case ManipAxis::Y:
              _rotationPlaneNormal = localY;
              _rotationPlanePerp1 = localX;
              _rotationPlanePerp2 = localZ;
              break;
            case ManipAxis::Z:
              _rotationPlaneNormal = localZ;
              _rotationPlanePerp1 = localY;
              _rotationPlanePerp2 = localX;
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
            fquat newRot = _computeRotationAbsolute(mousePos, _activeAxis);
            _target->setWorldRotation(newRot);
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
