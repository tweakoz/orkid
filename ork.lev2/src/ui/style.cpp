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
// Constants
///////////////////////////////////////////////////////////////////////////////

static constexpr int DEFAULT_CORNER_RADIUS = 16;

///////////////////////////////////////////////////////////////////////////////
// ThemeEngine Implementation
///////////////////////////////////////////////////////////////////////////////

#include "style_impl.inl"

///////////////////////////////////////////////////////////////////////////////
// Style implementation
///////////////////////////////////////////////////////////////////////////////

Style::Style()
  : _bg_color(0.2, 0.2, 0.2, 1.0)
  , _fg_color(0.9, 0.9, 0.9, 1.0)
  , _aux_color1(0.3, 0.6, 0.8, 1.0)
  , _aux_color2(0.5, 0.5, 0.5, 1.0)
  , _border_color(0.4, 0.4, 0.4, 1.0)
  , _text_color(0.9, 0.9, 0.9, 1.0)
  , _corner_radius(4)
  , _border_width(1)
  , _padding(4)
  , _blend_mode(lev2::BlendingMacro::ALPHA) {
}

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

  // Note: parent and sub-styles NOT copied (shallow clone)

  return new_style;
}

///////////////////////////////////////////////////////////////////////////////

style_ptr_t Style::derive(style_ptr_t parent) {
  auto derived = std::make_shared<Style>();

  if (parent) {
    // Copy all values from parent style
    derived->_bg_color = parent->_bg_color;
    derived->_fg_color = parent->_fg_color;
    derived->_aux_color1 = parent->_aux_color1;
    derived->_aux_color2 = parent->_aux_color2;
    derived->_border_color = parent->_border_color;
    derived->_text_color = parent->_text_color;
    derived->_corner_radius = parent->_corner_radius;
    derived->_border_width = parent->_border_width;
    derived->_padding = parent->_padding;
    derived->_blend_mode = parent->_blend_mode;
    derived->_font = parent->_font;

    // Set parent for CSS-like cascade
    derived->_parent = parent;
  }

  return derived;
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
  rst->_priority = 1<<24;
  rst->setCullTest(lev2::ECullTest::OFF);
  rst->setDepthTest(lev2::EDepthTest::OFF);
  rst->setWriteMaskRGB(true);
  rst->setWriteMaskA(true);
  rst->setWriteMaskZ(false);

  // Get all techniques
  auto fxi = ctx->FXI();
  impl->_sdf_box_tek = fxi->technique(impl->_sdf_shader, "sdf_box");
  impl->_sdf_box_per_corner_tek = fxi->technique(impl->_sdf_shader, "sdf_box_per_corner");
  impl->_sdf_circle_tek = fxi->technique(impl->_sdf_shader, "sdf_circle");
  impl->_sdf_capsule_tek = fxi->technique(impl->_sdf_shader, "sdf_capsule");
  impl->_sdf_ring_tek = fxi->technique(impl->_sdf_shader, "sdf_ring");

  // Cache parameter handles (shared across all techniques)
  impl->_param_mvp = mtl->param("mvp");
  impl->_param_modcolor = mtl->param("ModColor");
  impl->_param_box_size = mtl->param("box_size");
  impl->_param_box_pos = mtl->param("box_pos");
  impl->_param_corner_radius = mtl->param("corner_radius");
  impl->_param_border_width = mtl->param("border_width");
  impl->_param_fill_color = mtl->param("fill_color");
  impl->_param_border_color = mtl->param("border_color");
  impl->_param_corner_radii = mtl->param("corner_radii");
  impl->_param_shape_param = mtl->param("shape_param");
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
  box_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create default slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.15, 0.15, 0.15, 1.0);
  slider_style->_fg_color = fvec4(0.3, 0.6, 0.8, 1.0);
  slider_style->_aux_color1 = fvec4(0.5, 0.7, 0.9, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.4, 0.4, 0.4, 1.0);
  slider_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  slider_style->_border_width = 1;
  slider_style->_padding = 2;
  db->registerStyle("slider"_crcu, slider_style);

  // Create default text style
  auto text_style = std::make_shared<Style>();
  text_style->_text_color = fvec4(0.9, 0.9, 0.9, 1.0);
  text_style->_bg_color = fvec4(0.1, 0.1, 0.1, 1.0);
  db->registerStyle("text"_crcu, text_style);

  // Create default box style
  auto bbox_style = std::make_shared<Style>();
  bbox_style->_bg_color = fvec4(0.7, 0.7, 0.7, 1.0);
  bbox_style->_fg_color = fvec4(0.9, 0.9, 0.9, 1.0);
  bbox_style->_border_color = fvec4(1, 1, 0, 1.0);
  bbox_style->_text_color = fvec4(0.0, 0.0, 0.0, 1.0);
  bbox_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  bbox_style->_border_width = 4;
  bbox_style->_padding = 4;
  db->registerStyle("bright_box"_crcu, bbox_style);  

  // Create default box style
  auto hcbox_style = std::make_shared<Style>();
  hcbox_style->_bg_color = fvec4(0.5, 0.0, 0.5, 0.5);
  hcbox_style->_fg_color = fvec4(0.9, 0.9, 0.9, 1.0);
  hcbox_style->_border_color = fvec4(1, 1, 1, 1.0);
  hcbox_style->_text_color = fvec4(0.0, 0.0, 0.0, 1.0);
  hcbox_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  hcbox_style->_border_width = 6;
  hcbox_style->_padding = 4;
  hcbox_style->_blend_mode = lev2::BlendingMacro::ALPHA;
  db->registerStyle("highc_box"_crcu, hcbox_style);

  // Create default tab styles (rounder corners for modern look)
  auto tab_style = std::make_shared<Style>();
  tab_style->_bg_color = fvec4(0.25, 0.25, 0.3, 1.0);
  tab_style->_text_color = fvec4(0.9, 0.9, 0.9, 1.0);
  tab_style->_border_color = fvec4(0.4, 0.4, 0.4, 1.0);
  tab_style->_corner_radius = 12;  // More rounded for modern UI
  tab_style->_border_width = 1;
  tab_style->_blend_mode = lev2::BlendingMacro::ALPHA;
  db->registerStyle("tab"_crcu, tab_style);

  auto tab_active_style = std::make_shared<Style>();
  tab_active_style->_bg_color = fvec4(0.35, 0.35, 0.4, 1.0);
  tab_active_style->_text_color = fvec4(1.0, 1.0, 1.0, 1.0);
  tab_active_style->_border_color = fvec4(0.5, 0.5, 0.5, 1.0);
  tab_active_style->_corner_radius = 12;
  tab_active_style->_border_width = 2;
  tab_active_style->_blend_mode = lev2::BlendingMacro::ALPHA;
  db->registerStyle("tab_active"_crcu, tab_active_style);

  auto tab_hover_style = std::make_shared<Style>();
  tab_hover_style->_bg_color = fvec4(0.3, 0.3, 0.35, 1.0);
  tab_hover_style->_text_color = fvec4(0.95, 0.95, 0.95, 1.0);
  tab_hover_style->_border_color = fvec4(0.45, 0.45, 0.45, 1.0);
  tab_hover_style->_corner_radius = 12;
  tab_hover_style->_border_width = 1;
  tab_hover_style->_blend_mode = lev2::BlendingMacro::ALPHA;
  db->registerStyle("tab_hover"_crcu, tab_hover_style);

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
  box_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create dark slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.08, 0.08, 0.08, 1.0);
  slider_style->_fg_color = fvec4(0.2, 0.5, 0.7, 1.0);
  slider_style->_aux_color1 = fvec4(0.4, 0.6, 0.8, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.3, 0.3, 0.3, 1.0);
  slider_style->_corner_radius = DEFAULT_CORNER_RADIUS;
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
  box_style->_corner_radius = DEFAULT_CORNER_RADIUS;
  box_style->_border_width = 1;
  box_style->_padding = 4;
  db->registerStyle("box"_crcu, box_style);

  // Create light slider style
  auto slider_style = std::make_shared<Style>();
  slider_style->_bg_color = fvec4(0.9, 0.9, 0.9, 1.0);
  slider_style->_fg_color = fvec4(0.3, 0.5, 0.7, 1.0);
  slider_style->_aux_color1 = fvec4(0.5, 0.7, 0.9, 1.0);  // highlight
  slider_style->_border_color = fvec4(0.7, 0.7, 0.7, 1.0);
  slider_style->_corner_radius = DEFAULT_CORNER_RADIUS;
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
