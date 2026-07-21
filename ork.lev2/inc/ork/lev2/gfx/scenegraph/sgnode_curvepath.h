////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/math/transform_curve.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct CurvePathDrawableData final : public DrawableData {

  DeclareConcreteX(CurvePathDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;              // line strip
  drawable_ptr_t createControlPointDrawable() const;        // instanced spheres

  void updateControlPoints() const;

  int hitTestScreenCoord(const fmtx4& vpMatrix, const fvec2& screenPos,
                         int vpW, int vpH, float hitRadius = 12.0f) const;

  CurvePathDrawableData();
  ~CurvePathDrawableData();

  math::transformcurve_ptr_t _curve;
  float _controlPointScale = 0.08f;
  fvec4 _lineColor = fvec4(1.0f, 0.8f, 0.2f, 1.0f);
  fvec4 _cpColor = fvec4(0.5f, 1.0f, 1.0f, 1.0f);
  fvec4 _cpSelectedColor = fvec4(1.0f, 1.0f, 0.4f, 1.0f);
  int _selectedPointIndex = -1;
  int _lineSubdivisions = 32;

  mutable instanceddrawinstancedata_ptr_t _cpInstanceData;
  mutable instanced_modeldrawable_ptr_t _cpDrawable;
};

struct CurvePathDrawableImpl {

  CurvePathDrawableImpl(std::shared_ptr<const CurvePathDrawableData> data);
  ~CurvePathDrawableImpl();
  void gpuInit(lev2::Context* ctx);
  void _render(const RenderContextInstData& RCID);
  static void renderCurvePath(RenderContextInstData& RCID);

  std::shared_ptr<const CurvePathDrawableData> _data;
  freestyle_mtl_ptr_t _lineMaterial;
  const FxShaderTechnique* _lineTechnique = nullptr;
  fxparam_constptr_t _paramMVP = nullptr;
  fxparam_constptr_t _paramModColor = nullptr;
  bool _initted = false;
};

using curvepath_drawabledata_ptr_t = std::shared_ptr<CurvePathDrawableData>;
using curvepathdrawableimpl_ptr_t = std::shared_ptr<CurvePathDrawableImpl>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
