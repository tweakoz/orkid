////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/ui/event.h>
#include <unordered_map>
#include <vector>
#include <memory>

namespace ork::ui {

// Forward declarations
struct Widget;

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////

struct Style;
struct StyleDatabase;
struct ThemeEngine;

using style_ptr_t = std::shared_ptr<Style>;
using styledatabase_ptr_t = std::shared_ptr<StyleDatabase>;
using styledatabase_weakptr_t = std::weak_ptr<StyleDatabase>;
using styledblist_t = std::vector<styledatabase_ptr_t>;
using themeengine_ptr_t = std::shared_ptr<ThemeEngine>;

///////////////////////////////////////////////////////////////////////////////
// Style - Complete styling definition for a widget
///////////////////////////////////////////////////////////////////////////////

struct Style {

  style_ptr_t clone() const;

  // Colors
  fvec4 _bg_color          = fvec4(0.2, 0.2, 0.2, 1.0);
  fvec4 _fg_color          = fvec4(0.9, 0.9, 0.9, 1.0);
  fvec4 _aux_color1        = fvec4(0.3, 0.6, 0.8, 1.0);
  fvec4 _aux_color2        = fvec4(0.5, 0.5, 0.5, 1.0);
  fvec4 _border_color      = fvec4(0.4, 0.4, 0.4, 1.0);
  fvec4 _text_color        = fvec4(0.9, 0.9, 0.9, 1.0);

  // Geometry
  int _corner_radius       = 0;           // pixels
  int _border_width        = 0;           // pixels
  int _padding             = 4;           // pixels

  // Rendering
  lev2::BlendingMacro _blend_mode = lev2::BlendingMacro::ALPHA;

  // Typography
  lev2::font_ptr_t _font;
  // NOTE: font size is a property of font, not stored here

  // Future: textures, shadows, gradients, etc.
  // lev2::texture_ptr_t _bg_texture;
  // bool _has_shadow = false;
  // fvec4 _shadow_color;
  // int _shadow_offset_x = 0;
  // int _shadow_offset_y = 0;
};

///////////////////////////////////////////////////////////////////////////////
// StyleDatabase - Registry of named styles with hierarchy
///////////////////////////////////////////////////////////////////////////////

struct StyleDatabase {

  StyleDatabase();

  static styledatabase_ptr_t createChild(styledatabase_ptr_t parent);

  void registerStyle(uint64_t tag, style_ptr_t style);
  style_ptr_t getStyle(uint64_t tag) const;

  std::unordered_map<uint64_t, style_ptr_t> _styles;
  styledatabase_weakptr_t _parent;
  styledblist_t _children;
};

///////////////////////////////////////////////////////////////////////////////
// ThemeEngine - Renders widgets using StyleDatabase
///////////////////////////////////////////////////////////////////////////////

struct ThemeEngine {

  ThemeEngine(styledatabase_ptr_t db);

  void gpuInit(lev2::Context* ctx);

  // Widget rendering methods
  void drawBox(const Widget* w, drawevent_constptr_t drwev, const Style* style);
  void drawText(const Widget* w, drawevent_constptr_t drwev, const Style* style, const std::string& text);

  styledatabase_ptr_t _styledb;
  svar16_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
// Helper functions for creating common style databases
///////////////////////////////////////////////////////////////////////////////

styledatabase_ptr_t createDefaultStyleDatabase();
styledatabase_ptr_t createDarkStyleDatabase();
styledatabase_ptr_t createLightStyleDatabase();

} // namespace ork::ui
