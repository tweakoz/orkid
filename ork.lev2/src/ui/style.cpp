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
#include <ork/util/crc.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// ThemeEngine Implementation
///////////////////////////////////////////////////////////////////////////////

struct ThemeEngineImpl {
  lev2::freestyle_mtl_ptr_t _sdf_material;
  lev2::fxshader_ptr_t _sdf_shader;
  lev2::fxtechnique_constptr_t _sdf_box_tek;

  // Cached parameter handles
  lev2::fxparam_constptr_t _param_mvp = nullptr;
  lev2::fxparam_constptr_t _param_box_size = nullptr;
  lev2::fxparam_constptr_t _param_box_pos = nullptr;
  lev2::fxparam_constptr_t _param_corner_radius = nullptr;
  lev2::fxparam_constptr_t _param_border_width = nullptr;
  lev2::fxparam_constptr_t _param_fill_color = nullptr;
  lev2::fxparam_constptr_t _param_border_color = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Style implementation
///////////////////////////////////////////////////////////////////////////////

style_ptr_t Style::clone() const {
  auto new_style = std::make_shared<Style>();

  // Copy colors
  new_style->_bg_color = _bg_color;
  new_style->_fg_color = _fg_color;
  new_style->_aux_color1 = _aux_color1;
  new_style->_aux_color2 = _aux_color2;
  new_style->_border_color = _border_color;
  new_style->_text_color = _text_color;

  // Copy geometry
  new_style->_corner_radius = _corner_radius;
  new_style->_border_width = _border_width;
  new_style->_padding = _padding;

  // Copy rendering
  new_style->_blend_mode = _blend_mode;

  // Copy typography
  new_style->_font = _font;

  return new_style;
}

///////////////////////////////////////////////////////////////////////////////
// StyleDatabase implementation
///////////////////////////////////////////////////////////////////////////////

StyleDatabase::StyleDatabase() {
}

///////////////////////////////////////////////////////////////////////////////

styledatabase_ptr_t StyleDatabase::createChild(styledatabase_ptr_t parent) {
  auto child = std::make_shared<StyleDatabase>();
  child->_parent = parent;
  parent->_children.push_back(child);
  return child;
}

///////////////////////////////////////////////////////////////////////////////

void StyleDatabase::registerStyle(uint64_t tag, style_ptr_t style) {
  _styles[tag] = style;
}

///////////////////////////////////////////////////////////////////////////////

style_ptr_t StyleDatabase::getStyle(uint64_t tag) const {
  // First check local styles
  auto it = _styles.find(tag);
  if (it != _styles.end()) {
    return it->second;
  }

  // If not found, check parent
  if (auto parent = _parent.lock()) {
    return parent->getStyle(tag);
  }

  // Not found anywhere in hierarchy
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// ThemeEngine implementation
///////////////////////////////////////////////////////////////////////////////

ThemeEngine::ThemeEngine(styledatabase_ptr_t db)
  : _styledb(db) {
  _impl.makeShared<ThemeEngineImpl>();
}

///////////////////////////////////////////////////////////////////////////////

void ThemeEngine::gpuInit(lev2::Context* ctx) {
  auto impl = _impl.getShared<ThemeEngineImpl>();

  // Create freestyle material with SDF shader
  auto mtl = std::make_shared<lev2::FreestyleMaterial>();
  mtl->gpuInit(ctx, "orkshader://sdf_ui");

  impl->_sdf_material = mtl;
  impl->_sdf_shader = mtl->_shader;

  auto rst = mtl->_rasterstate;
  rst->setCullTest(lev2::ECullTest::OFF);
  rst->setDepthTest(lev2::EDepthTest::OFF);
  rst->setWriteMaskRGB(true);
  rst->setWriteMaskA(true);
  rst->setWriteMaskZ(false);

  // Get the technique
  auto fxi = ctx->FXI();
  impl->_sdf_box_tek = fxi->technique(impl->_sdf_shader, "sdf_box");

  // Cache parameter handles
  impl->_param_mvp = mtl->param("mvp");
  impl->_param_box_size = mtl->param("box_size");
  impl->_param_box_pos = mtl->param("box_pos");
  impl->_param_corner_radius = mtl->param("corner_radius");
  impl->_param_border_width = mtl->param("border_width");
  impl->_param_fill_color = mtl->param("fill_color");
  impl->_param_border_color = mtl->param("border_color");
}

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
  mtl->bindParam( impl->_param_box_size, fvec2(w->width(), w->height()));
  mtl->bindParam( impl->_param_box_pos, fvec2(fx1, fy1));
  mtl->bindParam( impl->_param_corner_radius, (float)style->_corner_radius);
  mtl->bindParam( impl->_param_border_width, (float)style->_border_width);
  mtl->bindParam( impl->_param_fill_color, style->_bg_color);
  mtl->bindParam( impl->_param_border_color, style->_border_color);

  // Set up raster state from style
  rst->setBlendingMacro(style->_blend_mode);

  primi->RenderEMLQuadAtZV16T16C16(
      fx1, fx2,   // x0, x1
      fy1, fy2,   // y0, y1
      0.0f,       // z
      0.0f, 1.0f, // u0, u1
      0.0f, 1.0f  // v0, v1
  );
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
// Helper functions for creating common style databases
///////////////////////////////////////////////////////////////////////////////

styledatabase_ptr_t createDefaultStyleDatabase() {
  auto db = std::make_shared<StyleDatabase>();

  // Create default box style
  auto box_style = std::make_shared<Style>();
  box_style->_bg_color = fvec4(0.2, 0.2, 0.2, 1.0);
  box_style->_fg_color = fvec4(0.9, 0.9, 0.9, 1.0);
  box_style->_border_color = fvec4(0.4, 0.4, 0.4, 1.0);
  box_style->_text_color = fvec4(0.9, 0.9, 0.9, 1.0);
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create default slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.15, 0.15, 0.15, 1.0);
  slider_style->_fg_color = fvec4(0.3, 0.6, 0.8, 1.0);
  slider_style->_aux_color1 = fvec4(0.5, 0.7, 0.9, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.4, 0.4, 0.4, 1.0);
  slider_style->_border_width = 1;
  slider_style->_padding = 2;
  db->registerStyle("slider"_crcu, slider_style);

  // Create default text style
  auto text_style = std::make_shared<Style>();
  text_style->_text_color = fvec4(0.9, 0.9, 0.9, 1.0);
  text_style->_bg_color = fvec4(0.1, 0.1, 0.1, 1.0);
  db->registerStyle("text"_crcu, text_style);

  return db;
}

styledatabase_ptr_t createDarkStyleDatabase() {
  auto db = std::make_shared<StyleDatabase>();

  // Create dark box style
  auto box_style = std::make_shared<Style>();
  box_style->_bg_color = fvec4(0.1, 0.1, 0.1, 1.0);
  box_style->_fg_color = fvec4(0.95, 0.95, 0.95, 1.0);
  box_style->_border_color = fvec4(0.3, 0.3, 0.3, 1.0);
  box_style->_text_color = fvec4(0.95, 0.95, 0.95, 1.0);
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create dark slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.08, 0.08, 0.08, 1.0);
  slider_style->_fg_color = fvec4(0.2, 0.5, 0.7, 1.0);
  slider_style->_aux_color1 = fvec4(0.4, 0.6, 0.8, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.3, 0.3, 0.3, 1.0);
  slider_style->_border_width = 1;
  slider_style->_padding = 2;
  db->registerStyle("slider"_crcu, slider_style);

  // Create dark text style
  auto text_style = std::make_shared<Style>();
  text_style->_text_color = fvec4(0.95, 0.95, 0.95, 1.0);
  text_style->_bg_color = fvec4(0.05, 0.05, 0.05, 1.0);
  db->registerStyle("text"_crcu, text_style);

  return db;
}

///////////////////////////////////////////////////////////////////////////////

styledatabase_ptr_t createLightStyleDatabase() {
  auto db = std::make_shared<StyleDatabase>();

  // Create light box style
  auto box_style = std::make_shared<Style>();
  box_style->_bg_color = fvec4(0.95, 0.95, 0.95, 1.0);
  box_style->_fg_color = fvec4(0.1, 0.1, 0.1, 1.0);
  box_style->_border_color = fvec4(0.7, 0.7, 0.7, 1.0);
  box_style->_text_color = fvec4(0.1, 0.1, 0.1, 1.0);
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create light slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.9, 0.9, 0.9, 1.0);
  slider_style->_fg_color = fvec4(0.3, 0.5, 0.7, 1.0);
  slider_style->_aux_color1 = fvec4(0.5, 0.7, 0.9, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.7, 0.7, 0.7, 1.0);
  slider_style->_border_width = 1;
  slider_style->_padding = 2;
  db->registerStyle("slider"_crcu, slider_style);

  // Create light text style
  auto text_style = std::make_shared<Style>();
  text_style->_text_color = fvec4(0.1, 0.1, 0.1, 1.0);
  text_style->_bg_color = fvec4(0.98, 0.98, 0.98, 1.0);
  db->registerStyle("text"_crcu, text_style);

  return db;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
