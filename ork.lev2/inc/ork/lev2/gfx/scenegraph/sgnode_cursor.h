#pragma once
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct CursorDrawableData final : public DrawableData {

  DeclareConcreteX(CursorDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  CursorDrawableData();
  ~CursorDrawableData();

  // Cursor appearance
  fvec4 _color = fvec4(1, 1, 1, 0.9f);
  float _size = 0.02f;        // crosshair arm length in view-space meters
  float _thickness = 0.005f;  // crosshair bar thickness in view-space meters
  float _depth = 1.0f;        // depth in view space (meters in front of camera)

  // If true, cursor position is automatically set from GLFW context mouse position
  bool _autopos = true;
};

struct CursorDrawableImpl {

  CursorDrawableImpl(const CursorDrawableData* data);
  ~CursorDrawableImpl();
  void gpuInit(lev2::Context* ctx);
  void _render(const RenderContextInstData& RCID);
  static void renderCursor(RenderContextInstData& RCID);

  const CursorDrawableData* _data = nullptr;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _paramMVP = nullptr;
  const FxShaderParam* _paramModColor = nullptr;
  bool _initted = false;

  // Updated each frame before rendering
  float _cursorX = 0.0f;  // NDC x [-1, 1]
  float _cursorY = 0.0f;  // NDC y [-1, 1]
};

using cursordrawabledata_ptr_t = std::shared_ptr<CursorDrawableData>;
using cursordrawableimpl_ptr_t = std::shared_ptr<CursorDrawableImpl>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
