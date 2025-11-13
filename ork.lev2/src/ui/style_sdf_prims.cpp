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
#include <ork/lev2/gfx/dbgfontman.h>

namespace ork::ui {

#include "style_impl.inl"

///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawBox(const Widget* w, drawevent_constptr_t drwev, const Style* style) {
  auto impl = _impl.getShared<ThemeEngineImpl>();
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();
  if(nullptr==impl->_sdf_material){
    gpuInit(tgt);
  }

  // Use SDF shader

  auto mtl = impl->_sdf_material;
  auto rst = mtl->_rasterstate;
  mtxi->PushUIMatrix();

  // Get widget position and size
  int ix1, iy1;
  w->LocalToRoot(0, 0, ix1, iy1);
  float fx1 = (float)ix1;
  float fy1 = (float)iy1;
  float fx2 = fx1 + (float)w->width();
  float fy2 = fy1 + (float)w->height();

  // Bind parameters using cached handles
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);

  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_box_tek, RCID);
  mtl->bindParam( impl->_param_mvp, mvp_mtx );
  mtl->bindParam( impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam( impl->_param_box_size, fvec2(w->width(), w->height()));
  mtl->bindParam( impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam( impl->_param_corner_radius, (float)style->_corner_radius);
  mtl->bindParam( impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam( impl->_param_fill_color, style->_bg_color);
  mtl->bindParam( impl->_param_border_color, style->_border_color);

  // Set up raster state from style
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
  fxi->EndBlock( );
}

///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawText(const Widget* w, drawevent_constptr_t drwev, const Style* style, const std::string& text) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1;
    w->LocalToRoot(0, 0, ix1, iy1);
    int ixc = ix1 + (w->width() >> 1);
    int iyc = iy1 + (w->height() >> 1);

    tgt->PushModColor(style->_text_color);

    // Use style font if available, otherwise use default
    if (style->_font) {
      lev2::FontMan::PushFont(style->_font);
    } else {
      lev2::FontMan::PushFont("i14");
    }

    lev2::FontMan::beginTextBlock(tgt, 16);
    int sw = lev2::FontMan::stringWidth(text.length());
    lev2::FontMan::DrawText(
        tgt,
        ixc - (sw >> 1),
        iyc - 6,
        text.c_str());
    lev2::FontMan::endTextBlock(tgt);
    lev2::FontMan::PopFont();

    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}

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

  //printf("corner_radii: %f %f %f %f\n", corner_radii.x, corner_radii.y, corner_radii.z, corner_radii.w);
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
// Equilateral Triangle (for arrows, indicators, patterns)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawTriangle(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    float rotation) {

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

  fxi->BeginBlock(impl->_sdf_triangle_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_corner_radius, (float)style->_corner_radius);
  mtl->bindParam(impl->_param_shape_param, rotation);
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
// Pause Icon (two vertical bars)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawPause(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    float spacing) {

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

  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_pause_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_corner_radius, (float)style->_corner_radius);
  mtl->bindParam(impl->_param_shape_param, spacing);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2, fy1, fy2, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
// Star/Pentagram (for ratings, decorations)
///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::drawStar(
    int x, int y, int w, int h,
    drawevent_constptr_t drwev,
    const Style* style,
    float rotation) {

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

  float fx1 = (float)x;
  float fy1 = (float)y;
  float fx2 = fx1 + (float)w;
  float fy2 = fy1 + (float)h;

  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  lev2::RenderContextInstData RCID(rcfd);
  const fmtx4& mvp_mtx = mtxi->RefMVPMatrix();

  fxi->BeginBlock(impl->_sdf_star_tek, RCID);
  mtl->bindParam(impl->_param_mvp, mvp_mtx);
  mtl->bindParam(impl->_param_modcolor, fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  mtl->bindParam(impl->_param_box_size, fvec2((float)w, (float)h));
  mtl->bindParam(impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam(impl->_param_corner_radius, (float)style->_corner_radius);
  mtl->bindParam(impl->_param_shape_param, rotation);
  mtl->bindParam(impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam(impl->_param_fill_color, style->_bg_color);
  mtl->bindParam(impl->_param_border_color, style->_border_color);

  rst->setBlendingMacro(style->_blend_mode);
  fxi->pushRasterState(rst);
  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2, fy1, fy2, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f
  );
  fxi->popRasterState();
  mtxi->PopUIMatrix();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
