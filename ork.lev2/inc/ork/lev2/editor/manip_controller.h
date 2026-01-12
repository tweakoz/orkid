////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/editor/manip.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/ui/event.h>

namespace ork::lev2 {
struct Context;
}

namespace ork::lev2::editor {

////////////////////////////////////////////////////////////////////////////////
// ManipController - handles gizmo interaction and rendering
////////////////////////////////////////////////////////////////////////////////

class ManipController {
public:
  ManipController();

  // Mode control
  void setMode(ManipMode mode);
  ManipMode mode() const { return _mode; }

  // Target object
  void setTarget(manipinterface_ptr_t target);
  manipinterface_ptr_t target() const { return _target; }

  // Camera setup (call before handleEvent/draw)
  void updateCamera(const CameraMatrices& matrices, const fvec2& viewport_dim);

  // Event handling - returns true if event was consumed
  ui::HandlerResult handleEvent(ui::event_constptr_t ev);

  // Gizmo rendering
  void draw(Context* ctx);

  // Gizmo scale (screen-space size factor)
  float _gizmoScale = 100.0f;

  // Hit threshold in pixels
  float _hitThreshold = 12.0f;

  // Hovered axis (for highlight, updated on MOVE)
  ManipAxis hoveredAxis() const { return _hoveredAxis; }

  // Active axis (during drag)
  ManipAxis activeAxis() const { return _activeAxis; }

  // Is currently dragging?
  bool isDragging() const { return _isDragging; }

private:
  // Camera helpers
  fvec3 _getCameraEye() const;
  fvec3 _getCameraDir() const;
  fvec3 _getCameraRight() const;
  fvec3 _getCameraUp() const;

  // Screen-space hit testing
  ManipAxis _hitTestTranslation(const fvec2& mousePos);
  ManipAxis _hitTestRotation(const fvec2& mousePos);
  ManipAxis _hitTestScale(const fvec2& mousePos);
  ManipAxis _hitTestGizmo(const fvec2& mousePos);

  // Project world point to screen
  fvec2 _project(const fvec3& worldPos) const;

  // Distance from point to line segment (2D)
  float _distToSegment(const fvec2& p, const fvec2& a, const fvec2& b) const;

  // Compute world-space gizmo scale based on distance from camera
  float _computeWorldGizmoScale() const;

  // Compute translation delta from mouse movement
  fvec3 _computeTranslationDelta(const fvec2& mouseDelta, ManipAxis axis);

  // Compute rotation delta from mouse movement
  fquat _computeRotationDelta(const fvec2& mousePos, ManipAxis axis);

  // Compute rotation angle via ray-plane intersection
  // Returns angle in radians around the rotation axis
  bool _computeRotationAngle(const fvec2& mousePos, float& outAngle) const;

  // Compute scale delta from mouse movement
  float _computeScaleDelta(const fvec2& mouseDelta, ManipAxis axis);

  // State
  ManipMode _mode = ManipMode::TRANSLATE;
  manipinterface_ptr_t _target;
  ManipAxis _hoveredAxis = ManipAxis::NONE;
  ManipAxis _activeAxis = ManipAxis::NONE;
  bool _isDragging = false;

  // Camera data
  CameraMatrices _camMatrices;
  fvec2 _viewportDim;

  // Drag state
  fvec2 _dragStartMouse;
  fvec2 _dragPrevMouse;
  fvec3 _dragStartPos;      // Object position at drag start
  fquat _dragStartRot;      // Object rotation at drag start
  float _dragStartScale;    // Object scale at drag start

  // Rotation drag state - ray-plane intersection
  float _rotationBaseAngle = 0.0f;    // Angle at drag start
  fvec3 _rotationPlaneNormal;         // Normal of rotation plane (local axis)
  fvec3 _rotationPlanePerp1;          // First perpendicular axis in plane
  fvec3 _rotationPlanePerp2;          // Second perpendicular axis in plane

  // ManipHandler for ray-plane intersection
  ManipHandler _handler;
};

using manipcontroller_ptr_t = std::shared_ptr<ManipController>;

} // namespace ork::lev2::editor
