////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/kernel/core/singleton.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/editor/types.h>
#include <ork/math/TransformNode.h>
#include <ork/math/transform_curve.h>

namespace ork::lev2::editor {

////////////////////////////////////////////////////////////////////////////////
// Manipulation modes and axis constraints
////////////////////////////////////////////////////////////////////////////////

enum class ManipMode {
  TRANSLATE,
  ROTATE,
  SCALE
};

enum class ManipSpace {
  LOCAL,
  WORLD
};

enum class ManipAxis {
  NONE,
  X,
  Y,
  Z,
  XY,
  XZ,
  YZ,
  VIEW,
  FREE
};

////////////////////////////////////////////////////////////////////////////////
// ManipulatorInterface - abstract interface for manipulatable objects
////////////////////////////////////////////////////////////////////////////////

struct ManipulatorInterface : public Object {

  DeclareAbstractX(ManipulatorInterface, Object);

  // Capability queries
  virtual bool supportsTranslation() const { return true; }
  virtual bool supportsRotation() const { return true; }
  virtual bool supportsUniformScaling() const { return true; }
  virtual bool supportsNonUniformScaling() const { return false; }

  // Get transform for gizmo rendering
  virtual fmtx4 getWorldMatrix() const = 0;
  virtual fvec3 getWorldPosition() const = 0;
  virtual fquat getWorldRotation() const = 0;

  // Apply deltas from manipulation
  virtual void applyTranslationDelta(const fvec3& delta) = 0;
  virtual void applyRotationDelta(const fquat& delta) = 0;
  virtual void applyScaleDelta(float uniformDelta) = 0;

  // Set absolute values (for non-delta based manipulation)
  virtual void setWorldRotation(const fquat& rot) = 0;

  // Begin/end callbacks (for undo/redo snapshots)
  virtual void onBeginManipulation(ManipMode mode) {}
  virtual void onEndManipulation(ManipMode mode) {}
};

using manipinterface_ptr_t = std::shared_ptr<ManipulatorInterface>;

////////////////////////////////////////////////////////////////////////////////
// DecompTransformManipulator - manipulates a DecompTransform
////////////////////////////////////////////////////////////////////////////////

struct DecompTransformManipulator : public ManipulatorInterface {

  DeclareConcreteX(DecompTransformManipulator, ManipulatorInterface);

  DecompTransformManipulator();
  DecompTransformManipulator(decompxf_ptr_t target);

  void setTarget(decompxf_ptr_t target);
  decompxf_ptr_t target() const { return _target; }

  // ManipulatorInterface overrides
  fmtx4 getWorldMatrix() const override;
  fvec3 getWorldPosition() const override;
  fquat getWorldRotation() const override;

  void applyTranslationDelta(const fvec3& delta) override;
  void applyRotationDelta(const fquat& delta) override;
  void applyScaleDelta(float uniformDelta) override;
  void setWorldRotation(const fquat& rot) override;

  void onBeginManipulation(ManipMode mode) override;
  void onEndManipulation(ManipMode mode) override;

  bool supportsNonUniformScaling() const override;

private:
  decompxf_ptr_t _target;

  // Snapshots for undo
  fvec3 _initialTranslation;
  fquat _initialRotation;
  float _initialScale;
};

using decompxfmanip_ptr_t = std::shared_ptr<DecompTransformManipulator>;

////////////////////////////////////////////////////////////////////////////////
// CurvePointManipulator - manipulates a single control point on a TransformCurve
////////////////////////////////////////////////////////////////////////////////

struct CurvePointManipulator : public ManipulatorInterface {

  DeclareConcreteX(CurvePointManipulator, ManipulatorInterface);

  CurvePointManipulator();
  CurvePointManipulator(math::transformcurve_ptr_t curve, int pointIndex);

  void setTarget(math::transformcurve_ptr_t curve, int pointIndex);
  math::transformcurve_ptr_t curve() const { return _curve; }
  int pointIndex() const { return _pointIndex; }

  bool supportsTranslation() const override { return true; }
  bool supportsRotation() const override { return false; }
  bool supportsUniformScaling() const override { return false; }

  fmtx4 getWorldMatrix() const override;
  fvec3 getWorldPosition() const override;
  fquat getWorldRotation() const override;
  void applyTranslationDelta(const fvec3& delta) override;
  void applyRotationDelta(const fquat& delta) override {}
  void applyScaleDelta(float uniformDelta) override {}
  void setWorldRotation(const fquat& rot) override {}

  std::function<void()> _onPointMoved;

private:
  math::transformcurve_ptr_t _curve;
  int _pointIndex = -1;
};

using curveptmanip_ptr_t = std::shared_ptr<CurvePointManipulator>;

////////////////////////////////////////////////////////////////////////////////
// Legacy interface (kept for compatibility)
////////////////////////////////////////////////////////////////////////////////

struct JointManipulatorInterface : ManipulatorInterface {

  DeclareConcreteX(ManipulatorInterface, Object);

  public:

  bool supportsTranslation() const final;
  bool supportsRotation() const final;
  bool supportsUniformScaling() const final;

  fmtx4 getWorldMatrix() const override { return fmtx4::Identity(); }
  fvec3 getWorldPosition() const override { return fvec3(); }
  fquat getWorldRotation() const override { return fquat(); }

  void applyTranslationDelta(const fvec3& delta) override {}
  void applyRotationDelta(const fquat& delta) override {}
  void applyScaleDelta(float uniformDelta) override {}
  void setWorldRotation(const fquat& rot) override {}

  void _onBeginTranslation(ui::event_constptr_t EV);
  void _onUpdateTranslation(ui::event_constptr_t EV);
  void _onEndTranslation(ui::event_constptr_t EV);

  void _onBeginRotation(ui::event_constptr_t EV);
  void _onUpdateRotation(ui::event_constptr_t EV);
  void _onEndRotation(ui::event_constptr_t EV);

  void _onBeginScaling(ui::event_constptr_t EV);
  void _onUpdateScaling(ui::event_constptr_t EV);
  void _onEndScaling(ui::event_constptr_t EV);

};

using jointmanipulatorinterface_ptr_t = std::shared_ptr<JointManipulatorInterface>;

} //namespace ork::lev2::editor {
