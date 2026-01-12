////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/editor/manip_controller.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ManipGizmoDrawableData final : public DrawableData {

  DeclareConcreteX(ManipGizmoDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ManipGizmoDrawableData();
  ~ManipGizmoDrawableData();

  // Controller reference
  editor::manipcontroller_ptr_t _controller;

  // Gizmo sizing
  float _axisLength = 1.0f;
  float _axisThickness = 0.04f;
  float _ringRadius = 0.8f;
  float _planeSize = 0.3f;

  // Axis colors
  fvec4 _colorX = fvec4(1.0f, 0.2f, 0.2f, 1.0f);
  fvec4 _colorY = fvec4(0.2f, 1.0f, 0.2f, 1.0f);
  fvec4 _colorZ = fvec4(0.2f, 0.2f, 1.0f, 1.0f);

  // Highlight and active colors
  fvec4 _colorHighlight = fvec4(1.0f, 1.0f, 0.0f, 1.0f);
  fvec4 _colorActive = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct ManipGizmoDrawableImpl {

  ManipGizmoDrawableImpl(const ManipGizmoDrawableData* data);
  ~ManipGizmoDrawableImpl();
  void gpuInit(lev2::Context* ctx);
  void _render(const RenderContextInstData& RCID);
  static void renderGizmo(RenderContextInstData& RCID);

  // Drawing helpers
  void _drawTranslateGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos, float scale);
  void _drawRotateGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos, float scale);
  void _drawScaleGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos, float scale);

  void _drawAxis(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos, const fvec3& dir,
                 const fvec4& color, float length, float thickness);
  void _drawCone(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos, const fvec3& dir,
                 const fvec4& color, float radius, float height);
  void _drawRing(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& center, const fvec3& normal,
                 const fvec4& color, float radius, float thickness);
  void _drawCube(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                 const fvec4& color, float size);
  void _drawPlaneHandle(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                        const fvec3& axis1, const fvec3& axis2,
                        const fvec4& color, float offset, float size);

  const ManipGizmoDrawableData* _data = nullptr;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _paramMVP = nullptr;
  const FxShaderParam* _paramModColor = nullptr;
  bool _initted = false;
};

using manipgizmodrawabledata_ptr_t = std::shared_ptr<ManipGizmoDrawableData>;
using manipgizmodrawableimpl_ptr_t = std::shared_ptr<ManipGizmoDrawableImpl>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
