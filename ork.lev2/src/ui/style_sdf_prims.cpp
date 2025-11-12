////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

#include "style_impl.inl"

///////////////////////////////////////////////////////////////////////////////
// Tab Shape - Applies style's corner_radius to top corners, sharp bottom
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawTab(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style) {

  // Tab shape: round top corners per style, sharp bottom corners
  float r = (float)style->_corner_radius;
  fvec4 corner_radii(r, r, 0.0f, 0.0f);  // TL, TR, BR, BL
  drawBoxPerCorner(x, y, w, h, drwev, style, corner_radii);
}

///////////////////////////////////////////////////////////////////////////////
// Per-Corner Rounded Box (for tabs with different corner radii)
// corner_radii: x=TL, y=TR, z=BR, w=BL
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawBoxPerCorner(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    const fvec4& corner_radii) {

  auto impl = _impl.getShared<ThemeEngineImpl>();
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();

  if (nullptr == impl->_sdf_material) {
    gpuInit(tgt);
  }

  auto mtl = impl->_sdf_material;
  auto rst = mtl->_rasterstate;
  mtxi->PushUIMatrix();

  // Use provided geometry
  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  // Bind parameters
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_box_per_corner_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_corner_radii, corner_radii);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  // Set raster state from style
  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2,   // x0, x1
      fy1, fy2,   // y0, y1
      0.0f,       // z
      0.0f, 1.0f, // u0, u1
      0.0f, 1.0f  // v0, v1
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
// Circle (for buttons, indicators, dots)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawCircle(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    float radius) {

  auto impl = _impl.getShared<ThemeEngineImpl>();
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();

  if (nullptr == impl->_sdf_material) {
    gpuInit(tgt);
  }

  auto mtl = impl->_sdf_material;
  auto rst = mtl->_rasterstate;
  mtxi->PushUIMatrix();

  // Use provided geometry
  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  // Bind parameters
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_circle_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_shape_param, radius);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  // Set raster state from style
  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2,   // x0, x1
      fy1, fy2,   // y0, y1
      0.0f,       // z
      0.0f, 1.0f, // u0, u1
      0.0f, 1.0f  // v0, v1
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
// Capsule/Pill (rectangle with rounded ends)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawCapsule(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    bool horizontal) {

  auto impl = _impl.getShared<ThemeEngineImpl>();
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();

  if (nullptr == impl->_sdf_material) {
    gpuInit(tgt);
  }

  auto mtl = impl->_sdf_material;
  auto rst = mtl->_rasterstate;
  mtxi->PushUIMatrix();

  // Use provided geometry
  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  // Bind parameters
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  float axis = horizontal ? 0.0f : 1.0f;

  fxi->BeginBlock(impl->_sdf_capsule_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_shape_param, axis);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  // Set raster state from style
  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2,   // x0, x1
      fy1, fy2,   // y0, y1
      0.0f,       // z
      0.0f, 1.0f, // u0, u1
      0.0f, 1.0f  // v0, v1
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
// Ring/Donut (for progress indicators, badges)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawRing(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    float inner_radius) {

  auto impl = _impl.getShared<ThemeEngineImpl>();
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();

  if (nullptr == impl->_sdf_material) {
    gpuInit(tgt);
  }

  auto mtl = impl->_sdf_material;
  auto rst = mtl->_rasterstate;
  mtxi->PushUIMatrix();

  // Use provided geometry
  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  // Bind parameters
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_ring_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_shape_param, inner_radius);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  // Set raster state from style
  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2,   // x0, x1
      fy1, fy2,   // y0, y1
      0.0f,       // z
      0.0f, 1.0f, // u0, u1
      0.0f, 1.0f  // v0, v1
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
