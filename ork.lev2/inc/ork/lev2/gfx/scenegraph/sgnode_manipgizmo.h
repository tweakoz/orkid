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

  // Axis colors
  fvec4 _colorX = fvec4(1.0f, 0.2f, 0.2f, 1.0f);
  fvec4 _colorY = fvec4(0.2f, 1.0f, 0.2f, 1.0f);
  fvec4 _colorZ = fvec4(0.2f, 0.2f, 1.0f, 1.0f);

  // Highlight and active colors
  fvec4 _colorHighlight = fvec4(1.0f, 1.0f, 0.5f, 1.0f);
  fvec4 _colorActive = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct ManipGizmoDrawableImpl {

  ManipGizmoDrawableImpl(std::shared_ptr<const ManipGizmoDrawableData> data);
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
  void _drawRing(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& center,
                 const fvec3& normal, const fvec3& perp1, const fvec3& perp2,
                 const fvec4& color, float majorRadius, float minorRadius);
  void _drawCube(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                 const fvec4& color, float size);
  void _drawPlaneHandle(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                        const fvec3& axis1, const fvec3& axis2,
                        const fvec4& color, float sign1, float sign2, float size);

  std::shared_ptr<const ManipGizmoDrawableData> _data;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _param_mvp = nullptr;
  const FxShaderParam* _param_modcolor = nullptr;
  const FxShaderParam* _param_lightdir = nullptr;
  const FxShaderParam* _param_planesize = nullptr;
  const FxShaderParam* _param_unlit = nullptr;

  // Static geometry buffers (unit scale, at origin)
  vtxbufferbase_ptr_t _unit_cylinder_vb;
  vtxbufferbase_ptr_t _unit_cone_vb;
  vtxbufferbase_ptr_t _unit_torus_vb;
  int _cylinder_vert_count = 0;
  int _cone_vert_count = 0;
  int _torus_vert_count = 0;

  void _generateStaticGeometry(Context* ctx);

  bool _initted = false;
};

using manipgizmodrawabledata_ptr_t = std::shared_ptr<ManipGizmoDrawableData>;
using manipgizmodrawableimpl_ptr_t = std::shared_ptr<ManipGizmoDrawableImpl>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
