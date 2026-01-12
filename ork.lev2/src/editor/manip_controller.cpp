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
  _ring_tube_radius_scale = 0.024f;  // 40% reduction from 0.04
  _axis_thickness_scale = 0.032f;    // 20% reduction from 0.04
}

////////////////////////////////////////////////////////////////////////////////

void ManipController::setMode(ManipMode mode) {
  _mode = mode;
  _hovered_axis = ManipAxis::NONE;
}

void ManipController::setTarget(manipinterface_ptr_t target) {
  _target = target;
  _hovered_axis = ManipAxis::NONE;
  _active_axis = ManipAxis::NONE;
  _is_dragging = false;
}

void ManipController::updateCamera(const CameraMatrices& matrices, const fvec2& viewport_dim) {
  _cam_matrices = matrices;
  _viewport_dim = viewport_dim;
}

////////////////////////////////////////////////////////////////////////////////
// Projection helpers
////////////////////////////////////////////////////////////////////////////////

fvec3 ManipController::_getCameraEye() const {
  // Extract eye position from inverse view matrix
  const fmtx4& ivmat = _cam_matrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(3, 0), ivmat.elemXY(3, 1), ivmat.elemXY(3, 2));
}

fvec3 ManipController::_getCameraDir() const {
  const fmtx4& ivmat = _cam_matrices.GetIVMatrix();
  return -fvec3(ivmat.elemXY(2, 0), ivmat.elemXY(2, 1), ivmat.elemXY(2, 2)).normalized();
}

fvec3 ManipController::_getCameraRight() const {
  const fmtx4& ivmat = _cam_matrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(0, 0), ivmat.elemXY(0, 1), ivmat.elemXY(0, 2)).normalized();
}

fvec3 ManipController::_getCameraUp() const {
  const fmtx4& ivmat = _cam_matrices.GetIVMatrix();
  return fvec3(ivmat.elemXY(1, 0), ivmat.elemXY(1, 1), ivmat.elemXY(1, 2)).normalized();
}

fvec2 ManipController::_project(const fvec3& worldPos) const {
  fvec4 clip = _cam_matrices.GetVPMatrix() * fvec4(worldPos, 1.0f);
  if (clip.w <= 0.0001f) {
    return fvec2(-10000, -10000); // Behind camera
  }
  fvec2 ndc = fvec2(clip.x, clip.y) / clip.w;
  // Vulkan convention: NDC Y=-1 at top, Y=+1 at bottom
  // Screen Y=0 at top, Y=height at bottom
  // So: screenY = (ndc.y * 0.5 + 0.5) * height (no flip needed)
  return fvec2(
      (ndc.x * 0.5f + 0.5f) * _viewport_dim.x,
      (ndc.y * 0.5f + 0.5f) * _viewport_dim.y
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

  // Check which axes and planes are selectable (not too edge-on)
  bool xSelectable = _computeAxisDimFactor(axisX) > 0.9f;
  bool ySelectable = _computeAxisDimFactor(axisY) > 0.9f;
  bool zSelectable = _computeAxisDimFactor(axisZ) > 0.9f;
  bool xySelectable = _computePlaneDimFactor(axisZ) > 0.9f;  // XY plane has Z normal
  bool xzSelectable = _computePlaneDimFactor(axisY) > 0.9f;  // XZ plane has Y normal
  bool yzSelectable = _computePlaneDimFactor(axisX) > 0.9f;  // YZ plane has X normal

  // Match the visual axis dimensions (cylinder starts offset from origin)
  float axisLen = scale * _axis_length_scale;
  float coneH = axisLen * 0.15f;
  float thickness = scale * _axis_thickness_scale;
  float cylinderOffset = thickness * 2.0f;
  float cylinderLen = (axisLen - coneH) - cylinderOffset;

  // Project cylinder thickness to screen space for accurate hit testing
  fvec2 origin2D = _project(origin);
  fvec2 thicknessTest = _project(origin + axisX * thickness);
  float screenThickness = (thicknessTest - origin2D).length();
  float axisHitThreshold = _hit_threshold + screenThickness;

  fvec2 xStart = _project(origin + axisX * cylinderOffset);
  fvec2 xEnd = _project(origin + axisX * (cylinderOffset + cylinderLen));
  fvec2 yStart = _project(origin + axisY * cylinderOffset);
  fvec2 yEnd = _project(origin + axisY * (cylinderOffset + cylinderLen));
  fvec2 zStart = _project(origin + axisZ * cylinderOffset);
  fvec2 zEnd = _project(origin + axisZ * (cylinderOffset + cylinderLen));

  // Test single axes first (priority) - only if selectable
  if (xSelectable) {
    float distX = _distToSegment(mousePos, xStart, xEnd);
    if (distX < axisHitThreshold) return ManipAxis::X;
  }
  if (ySelectable) {
    float distY = _distToSegment(mousePos, yStart, yEnd);
    if (distY < axisHitThreshold) return ManipAxis::Y;
  }
  if (zSelectable) {
    float distZ = _distToSegment(mousePos, zStart, zEnd);
    if (distZ < axisHitThreshold) return ManipAxis::Z;
  }

  // Test plane handles (cornered at origin, forming hemi-cube) - only if selectable
  float planeSize = scale * _plane_handle_scale;

  // Get camera position for view-dependent plane placement
  fvec3 camPos = _getCameraEye();

  // For each plane, test if mouse is within the cornered box bounds
  auto testPlaneBox = [&](const fvec3& axis1, const fvec3& axis2, float sign1, float sign2) -> bool {
    // Plane extends from origin to sign1*axis1*size + sign2*axis2*size
    fvec2 corner0 = _project(origin);
    fvec2 corner1 = _project(origin + axis1 * sign1 * planeSize);
    fvec2 corner2 = _project(origin + axis2 * sign2 * planeSize);
    fvec2 corner3 = _project(origin + axis1 * sign1 * planeSize + axis2 * sign2 * planeSize);

    // Use 2D triangle containment test (two triangles forming the quad)
    auto pointInTriangle = [](const fvec2& p, const fvec2& a, const fvec2& b, const fvec2& c) -> bool {
      auto sign = [](const fvec2& p1, const fvec2& p2, const fvec2& p3) -> float {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
      };
      float d1 = sign(p, a, b);
      float d2 = sign(p, b, c);
      float d3 = sign(p, c, a);
      bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
      bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
      return !(hasNeg && hasPos);
    };

    return pointInTriangle(mousePos, corner0, corner1, corner3) ||
           pointInTriangle(mousePos, corner0, corner3, corner2);
  };

  // Compute view-dependent signs (same logic as rendering)
  fvec3 camToGizmo = camPos - origin;

  // For LOCAL mode, transform camera direction to object's local space
  if (_space == ManipSpace::LOCAL) {
    fquat targetRot = _target->getWorldRotation();
    fquat invRot = targetRot.inverse();
    camToGizmo = invRot.transform(camToGizmo);
  }

  // Compute signs based on local-space (or world-space for WORLD mode) camera direction
  float signXY_X = (camToGizmo.x > 0) ? +1.0f : -1.0f;
  float signXY_Y = (camToGizmo.y > 0) ? +1.0f : -1.0f;
  float signXZ_X = (camToGizmo.x > 0) ? +1.0f : -1.0f;
  float signXZ_Z = (camToGizmo.z > 0) ? +1.0f : -1.0f;
  float signYZ_Y = (camToGizmo.y > 0) ? +1.0f : -1.0f;
  float signYZ_Z = (camToGizmo.z > 0) ? +1.0f : -1.0f;

  if (xySelectable && testPlaneBox(axisX, axisY, signXY_X, signXY_Y)) return ManipAxis::XY;
  if (xzSelectable && testPlaneBox(axisX, axisZ, signXZ_X, signXZ_Z)) return ManipAxis::XZ;
  if (yzSelectable && testPlaneBox(axisY, axisZ, signYZ_Y, signYZ_Z)) return ManipAxis::YZ;

  // Test center (free movement)
  if ((mousePos - origin2D).length() < _hit_threshold) return ManipAxis::FREE;

  return ManipAxis::NONE;
}

ManipAxis ManipController::_hitTestRotation(const fvec2& mousePos) {
  if (!_target) return ManipAxis::NONE;

  fvec3 origin = _target->getWorldPosition();
  float scale = _computeWorldGizmoScale();
  float ringRadius = scale * _ring_radius_scale;

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
    return angleDegrees >= _min_ring_elevation_degrees;
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
  if (minDist < _hit_threshold) {
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
  if (!_target) return ManipAxis::NONE;

  fvec3 origin = _target->getWorldPosition();
  float scale = _computeWorldGizmoScale();

  // Test center cube for uniform scale
  fvec2 origin2D = _project(origin);
  float cubeSize = scale * _axis_length_scale * 0.1f;
  float cubeScreenSize = (_project(origin + fvec3(cubeSize, 0, 0)) - origin2D).length();
  if ((mousePos - origin2D).length() < cubeScreenSize * 1.5f) {
    return ManipAxis::FREE;
  }

  // Only test individual axes if target supports non-uniform scaling
  if (_target->supportsNonUniformScaling()) {
    // Get axes based on space mode
    fvec3 axisX(1, 0, 0), axisY(0, 1, 0), axisZ(0, 0, 1);
    if (_space == ManipSpace::LOCAL) {
      fquat targetRot = _target->getWorldRotation();
      axisX = targetRot.transform(fvec3(1, 0, 0));
      axisY = targetRot.transform(fvec3(0, 1, 0));
      axisZ = targetRot.transform(fvec3(0, 0, 1));
    }

    // Match the visual axis dimensions (cylinder starts offset from origin)
    float axisLen = scale * _axis_length_scale;
    float coneH = axisLen * 0.15f;
    float thickness = scale * _axis_thickness_scale;
    float cylinderOffset = thickness * 2.0f;
    float cylinderLen = (axisLen - cubeSize) - cylinderOffset;

    // Project cylinder thickness to screen space for accurate hit testing
    fvec2 thicknessTest = _project(origin + axisX * thickness);
    float screenThickness = (thicknessTest - origin2D).length();
    float axisHitThreshold = _hit_threshold + screenThickness;

    fvec2 xStart = _project(origin + axisX * cylinderOffset);
    fvec2 xEnd = _project(origin + axisX * (cylinderOffset + cylinderLen));
    fvec2 yStart = _project(origin + axisY * cylinderOffset);
    fvec2 yEnd = _project(origin + axisY * (cylinderOffset + cylinderLen));
    fvec2 zStart = _project(origin + axisZ * cylinderOffset);
    fvec2 zEnd = _project(origin + axisZ * (cylinderOffset + cylinderLen));

    // Test single axes - no selectability check for scale mode
    float distX = _distToSegment(mousePos, xStart, xEnd);
    if (distX < axisHitThreshold) return ManipAxis::X;

    float distY = _distToSegment(mousePos, yStart, yEnd);
    if (distY < axisHitThreshold) return ManipAxis::Y;

    float distZ = _distToSegment(mousePos, zStart, zEnd);
    if (distZ < axisHitThreshold) return ManipAxis::Z;
  }

  return ManipAxis::NONE;
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
      (_drag_prev_mouse.x / _viewport_dim.x) * 2.0f - 1.0f,
      (_drag_prev_mouse.y / _viewport_dim.y) * 2.0f - 1.0f
  );
  fvec2 newMouseNDC(
      ((_drag_prev_mouse.x + mouseDelta.x) / _viewport_dim.x) * 2.0f - 1.0f,
      ((_drag_prev_mouse.y + mouseDelta.y) / _viewport_dim.y) * 2.0f - 1.0f
  );

  // Project mouse movement onto the constraint axis/plane
  // Use ray-plane intersection for accurate world-space movement
  fvec3 camEye = _getCameraEye();
  fvec3 camDir = _getCameraDir();

  auto unprojectToRay = [&](const fvec2& ndc) -> fray3 {
    fvec3 rayNear, rayFar;
    fvec3 vWinN(ndc.x, ndc.y, 0.0f);
    fvec3 vWinF(ndc.x, ndc.y, 1.0f);
    fmtx4::unProject(_cam_matrices.GetIVPMatrix(), vWinN, rayNear);
    fmtx4::unProject(_cam_matrices.GetIVPMatrix(), vWinF, rayFar);
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
      (mousePos.x / _viewport_dim.x) * 2.0f - 1.0f,
      (mousePos.y / _viewport_dim.y) * 2.0f - 1.0f
  );

  // Generate ray from camera through mouse position
  fvec3 rayNear, rayDir;
  fvec3 vWinN(mouseNDC.x, mouseNDC.y, 0.0f);
  fvec3 vWinF(mouseNDC.x, mouseNDC.y, 1.0f);
  fvec3 rayFar;
  fmtx4::unProject(_cam_matrices.GetIVPMatrix(), vWinN, rayNear);
  fmtx4::unProject(_cam_matrices.GetIVPMatrix(), vWinF, rayFar);
  rayDir = (rayFar - rayNear).normalized();

  // Create plane from stored normal and gizmo origin
  fplane3 rotationPlane;
  rotationPlane.CalcFromNormalAndOrigin(_rotation_plane_normal, gizmoPos);

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
  float coord1 = toIsect.dotWith(_rotation_plane_perp1);
  float coord2 = toIsect.dotWith(_rotation_plane_perp2);
  outAngle = atan2(coord2, coord1);

  return true;
}

fquat ManipController::_computeRotationAbsolute(const fvec2& mousePos, ManipAxis axis) {
  if (!_target) return _drag_start_rot;

  float currentAngle;
  if (!_computeRotationAngle(mousePos, currentAngle)) {
    return _drag_start_rot;  // No intersection, keep start rotation
  }

  // Compute total angle from drag start (not delta from last frame)
  float totalAngle = currentAngle - _rotation_base_angle;

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
  return localRot * _drag_start_rot;
}

float ManipController::_computeScaleDelta(const fvec2& mouseDelta, ManipAxis axis) {
  float sensitivity = 0.01f;
  return 1.0f + mouseDelta.x * sensitivity;
}

float ManipController::_computeRingDimFactor(const fvec3& ringNormal) const {
  fvec3 camDir = _getCameraDir();
  float dotProduct = fabs(ringNormal.dotWith(camDir));
  float angleDegrees = 90.0f - (acos(dotProduct) * 180.0f / PI);

  float threshold = _min_ring_elevation_degrees;
  float transitionBand = 1.0f;  // 1 degree transition band

  if (angleDegrees >= threshold) {
    return 1.0f;  // Full brightness - active
  } else if (angleDegrees >= threshold - transitionBand) {
    // Sharp linear transition over 1 degree
    float t = (angleDegrees - (threshold - transitionBand)) / transitionBand;
    return 0.5f + 0.5f * t;
  } else {
    return 0.5f;  // Dimmed - inactive
  }
}

float ManipController::_computeAxisDimFactor(const fvec3& axisDir) const {
  // Axis is usable when perpendicular to camera (dot product near 0)
  // Axis is NOT usable when parallel to camera (dot product near 1)
  fvec3 camDir = _getCameraDir();
  float dotProduct = fabs(axisDir.dotWith(camDir));
  float angleDegrees = acos(dotProduct) * 180.0f / PI;  // Angle from camera direction

  float threshold = _min_ring_elevation_degrees;  // Reuse same threshold value
  float transitionBand = 1.0f;

  // Axis is bright when it's far from parallel (angle from camera > threshold)
  // This is the inverse of rings: rings measure elevation from plane,
  // axes measure angle from being parallel to camera
  if (angleDegrees >= threshold) {
    return 1.0f;  // Full brightness - not parallel to camera
  } else if (angleDegrees >= threshold - transitionBand) {
    float t = (angleDegrees - (threshold - transitionBand)) / transitionBand;
    return 0.5f + 0.5f * t;
  } else {
    return 0.5f;  // Dimmed - too close to parallel to camera
  }
}

float ManipController::_computePlaneDimFactor(const fvec3& planeNormal) const {
  // Plane is usable when facing camera (normal aligned with camera direction)
  // Plane is NOT usable when edge-on (normal perpendicular to camera)
  // This is the same logic as rings
  return _computeRingDimFactor(planeNormal);
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
      _hovered_axis = _hitTestGizmo(mousePos);
      break;
    }

    case ui::EventCode::PUSH: {
      ManipAxis hit = _hitTestGizmo(mousePos);
      if (hit != ManipAxis::NONE) {
        _active_axis = hit;
        _is_dragging = true;
        _drag_start_mouse = mousePos;
        _drag_prev_mouse = mousePos;
        _drag_start_pos = _target->getWorldPosition();
        _drag_start_rot = _target->getWorldRotation();

        // Initialize ManipHandler with NDC coordinates
        fvec2 mouseNDC(
            (mousePos.x / _viewport_dim.x) * 2.0f - 1.0f,
            (mousePos.y / _viewport_dim.y) * 2.0f - 1.0f
        );
        _handler.Init(mouseNDC, _cam_matrices.GetIVPMatrix(), fquat());

        // Cache dimming state based on mode (frozen during drag)
        if (_mode == ManipMode::TRANSLATE) {
          // Get axes based on space mode
          fvec3 axisX(1, 0, 0), axisY(0, 1, 0), axisZ(0, 0, 1);
          if (_space == ManipSpace::LOCAL) {
            fquat targetRot = _target->getWorldRotation();
            axisX = targetRot.transform(fvec3(1, 0, 0));
            axisY = targetRot.transform(fvec3(0, 1, 0));
            axisZ = targetRot.transform(fvec3(0, 0, 1));
          }

          // Cache dimming for axes
          _drag_start_dim_x = _computeAxisDimFactor(axisX);
          _drag_start_dim_y = _computeAxisDimFactor(axisY);
          _drag_start_dim_z = _computeAxisDimFactor(axisZ);

          // Cache dimming for planes (use plane normals)
          fvec3 normalXY = axisZ;  // XY plane has Z normal
          fvec3 normalXZ = axisY;  // XZ plane has Y normal
          fvec3 normalYZ = axisX;  // YZ plane has X normal
          _drag_start_dim_xy = _computePlaneDimFactor(normalXY);
          _drag_start_dim_xz = _computePlaneDimFactor(normalXZ);
          _drag_start_dim_yz = _computePlaneDimFactor(normalYZ);
        } else if (_mode == ManipMode::ROTATE) {
          // For rotation mode, set up the rotation plane in LOCAL space
          fquat targetRot = _target->getWorldRotation();
          fvec3 localX = targetRot.transform(fvec3(1, 0, 0));
          fvec3 localY = targetRot.transform(fvec3(0, 1, 0));
          fvec3 localZ = targetRot.transform(fvec3(0, 0, 1));

          switch (hit) {
            case ManipAxis::X:
              _rotation_plane_normal = localX;
              _rotation_plane_perp1 = localZ;
              _rotation_plane_perp2 = localY;
              break;
            case ManipAxis::Y:
              _rotation_plane_normal = localY;
              _rotation_plane_perp1 = localX;
              _rotation_plane_perp2 = localZ;
              break;
            case ManipAxis::Z:
              _rotation_plane_normal = localZ;
              _rotation_plane_perp1 = localY;
              _rotation_plane_perp2 = localX;
              break;
            case ManipAxis::FREE:
            case ManipAxis::VIEW:
            default:
              // View-aligned rotation
              _rotation_plane_normal = _getCameraDir();
              _rotation_plane_perp1 = _getCameraRight();
              _rotation_plane_perp2 = _getCameraUp();
              break;
          }

          // Compute and store base angle
          _computeRotationAngle(mousePos, _rotation_base_angle);

          // Cache the current dimming state for all rings (frozen during drag)
          _drag_start_dim_x = _computeRingDimFactor(localX);
          _drag_start_dim_y = _computeRingDimFactor(localY);
          _drag_start_dim_z = _computeRingDimFactor(localZ);
        }

        _target->onBeginManipulation(_mode);
        result.setHandled(reinterpret_cast<ui::Widget*>(this));  // non-null to mark handled
      }
      break;
    }

    case ui::EventCode::DRAG: {
      if (_is_dragging && _active_axis != ManipAxis::NONE) {
        fvec2 mouseDelta = mousePos - _drag_prev_mouse;

        switch (_mode) {
          case ManipMode::TRANSLATE: {
            fvec3 delta = _computeTranslationDelta(mouseDelta, _active_axis);
            _target->applyTranslationDelta(delta);
            break;
          }
          case ManipMode::ROTATE: {
            fquat newRot = _computeRotationAbsolute(mousePos, _active_axis);
            _target->setWorldRotation(newRot);
            break;
          }
          case ManipMode::SCALE: {
            float delta = _computeScaleDelta(mouseDelta, _active_axis);
            _target->applyScaleDelta(delta);
            break;
          }
        }

        _drag_prev_mouse = mousePos;
        result.setHandled(reinterpret_cast<ui::Widget*>(this));  // non-null to mark handled
      }
      break;
    }

    case ui::EventCode::RELEASE: {
      if (_is_dragging) {
        _target->onEndManipulation(_mode);
        _is_dragging = false;
        _active_axis = ManipAxis::NONE;
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
