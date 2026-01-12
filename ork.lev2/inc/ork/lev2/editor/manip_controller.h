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

struct ManipController {
  ManipController();

  // Mode control
  void setMode(ManipMode mode);
  ManipMode mode() const { return _mode; }

  // Space control (local vs world)
  void setSpace(ManipSpace space) { _space = space; }
  ManipSpace space() const { return _space; }

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
  float _gizmo_scale = 100.0f;

  // Hit threshold in pixels
  float _hit_threshold = 12.0f;

  // Ring visibility threshold (degrees) - dim ring when viewed more edge-on than this
  float _min_ring_elevation_degrees = 25.0f;

  // Ring radius scale factor (relative to gizmo scale)
  float _ring_radius_scale = 1.2f;

  // Ring tube radius scale factor (for torus tube thickness)
  float _ring_tube_radius_scale = 0.04f;

  // Ring color band size in degrees (alternating intensity pattern)
  float _ring_band_degrees = 30.0f;

  // Axis sizing
  float _axis_length_scale = 1.0f;
  float _axis_thickness_scale = 0.04f;
  float _plane_handle_scale = 0.3f;

  // Hovered axis (for highlight, updated on MOVE)
  ManipAxis hoveredAxis() const { return _hovered_axis; }

  // Active axis (during drag)
  ManipAxis activeAxis() const { return _active_axis; }

  // Is currently dragging?
  bool isDragging() const { return _is_dragging; }

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

  // Compute absolute rotation from mouse position (not delta-based)
  fquat _computeRotationAbsolute(const fvec2& mousePos, ManipAxis axis);

  // Compute rotation angle via ray-plane intersection
  // Returns angle in radians around the rotation axis
  bool _computeRotationAngle(const fvec2& mousePos, float& outAngle) const;

  // Compute scale delta from mouse movement
  float _computeScaleDelta(const fvec2& mouseDelta, ManipAxis axis);

  // Compute ring dimming factor based on view angle
  float _computeRingDimFactor(const fvec3& ringNormal) const;

  // Compute axis dimming factor (axis usable when perpendicular to camera)
  float _computeAxisDimFactor(const fvec3& axisDir) const;

  // Compute plane dimming factor (plane usable when not edge-on)
  float _computePlaneDimFactor(const fvec3& planeNormal) const;

  // State
  ManipMode _mode = ManipMode::TRANSLATE;
  ManipSpace _space = ManipSpace::LOCAL;
  manipinterface_ptr_t _target;
  ManipAxis _hovered_axis = ManipAxis::NONE;
  ManipAxis _active_axis = ManipAxis::NONE;
  bool _is_dragging = false;

  // Camera data
  CameraMatrices _cam_matrices;
  fvec2 _viewport_dim;

  // Drag state
  fvec2 _drag_start_mouse;
  fvec2 _drag_prev_mouse;
  fvec3 _drag_start_pos;      // Object position at drag start
  fquat _drag_start_rot;      // Object rotation at drag start
  float _drag_start_scale;    // Object scale at drag start

  // Rotation drag state - ray-plane intersection
  float _rotation_base_angle = 0.0f;    // Angle at drag start
  fvec3 _rotation_plane_normal;         // Normal of rotation plane (local axis)
  fvec3 _rotation_plane_perp1;          // First perpendicular axis in plane
  fvec3 _rotation_plane_perp2;          // Second perpendicular axis in plane

  // ManipHandler for ray-plane intersection
  ManipHandler _handler;

  // Cached dimming factors at drag start (for frozen visual state during drag)
  float _drag_start_dim_x = 1.0f;
  float _drag_start_dim_y = 1.0f;
  float _drag_start_dim_z = 1.0f;
  float _drag_start_dim_xy = 1.0f;
  float _drag_start_dim_xz = 1.0f;
  float _drag_start_dim_yz = 1.0f;
};

using manipcontroller_ptr_t = std::shared_ptr<ManipController>;

} // namespace ork::lev2::editor
