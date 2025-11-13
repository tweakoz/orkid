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
#include <optional>

namespace ork::ui {

// Forward declarations
struct Widget;

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////

struct Style;
struct StyleDatabase;
struct ThemeEngine;

// Widget-specific sub-styles
struct StyleTab;
struct StyleButton;
struct StyleTextBox;
struct StyleSlider;
struct StyleScrollbar;
struct StylePanel;
struct StyleGraph;

using style_ptr_t = std::shared_ptr<Style>;
using styledatabase_ptr_t = std::shared_ptr<StyleDatabase>;
using styledatabase_weakptr_t = std::weak_ptr<StyleDatabase>;
using styledblist_t = std::vector<styledatabase_ptr_t>;
using themeengine_ptr_t = std::shared_ptr<ThemeEngine>;

// Sub-style pointer types
using styletab_ptr_t = std::shared_ptr<StyleTab>;
using stylebutton_ptr_t = std::shared_ptr<StyleButton>;
using styletextbox_ptr_t = std::shared_ptr<StyleTextBox>;
using styleslider_ptr_t = std::shared_ptr<StyleSlider>;
using stylescrollbar_ptr_t = std::shared_ptr<StyleScrollbar>;
using stylepanel_ptr_t = std::shared_ptr<StylePanel>;
using stylegraph_ptr_t = std::shared_ptr<StyleGraph>;

///////////////////////////////////////////////////////////////////////////////
// Widget-specific sub-styles (CSS-like specificity)
///////////////////////////////////////////////////////////////////////////////

struct StyleTab {
  std::optional<fvec4> _active_bg_color;
  std::optional<fvec4> _inactive_bg_color;
  std::optional<fvec4> _hover_bg_color;
  std::optional<int> _top_corner_radius;     // Tabs often round top only
  std::optional<int> _bottom_corner_radius;  // Usually 0 for tabs
  std::optional<float> _tab_height;
  std::optional<float> _tab_spacing;
};

struct StyleButton {
  std::optional<fvec4> _hover_bg_color;
  std::optional<fvec4> _pressed_bg_color;
  std::optional<fvec4> _disabled_bg_color;
  std::optional<fvec2> _press_offset;  // Visual feedback on press
};

struct StyleTextBox {
  std::optional<fvec4> _selection_color;
  std::optional<fvec4> _cursor_color;
  std::optional<fvec4> _placeholder_color;
  std::optional<int> _line_spacing;
  std::optional<int> _cursor_width;
};

struct StyleSlider {
  std::optional<fvec4> _track_color;
  std::optional<fvec4> _thumb_color;
  std::optional<fvec4> _fill_color;  // Filled portion of track
  std::optional<float> _thumb_size;
  std::optional<float> _track_height;
};

struct StyleScrollbar {
  std::optional<fvec4> _track_color;
  std::optional<fvec4> _thumb_color;
  std::optional<fvec4> _thumb_hover_color;
  std::optional<float> _thumb_min_size;
  std::optional<int> _width;
};

struct StylePanel {
  std::optional<fvec4> _title_bg_color;
  std::optional<fvec4> _shadow_color;
  std::optional<fvec2> _shadow_offset;
  std::optional<float> _shadow_blur;
};

struct StyleGraph {
  std::optional<fvec4> _grid_color;
  std::optional<fvec4> _axis_color;
  std::optional<std::vector<fvec4>> _plot_colors;  // Multiple series colors
  std::optional<float> _grid_line_width;
  std::optional<float> _plot_line_width;
};

///////////////////////////////////////////////////////////////////////////////
// Style - Complete styling definition for a widget (CSS-like cascade)
///////////////////////////////////////////////////////////////////////////////

struct Style {

  Style();

  style_ptr_t clone() const;

  // CSS-style derivation: create a new style inheriting from parent
  // Usage: auto derived = Style::derive(base_style);
  static style_ptr_t derive(style_ptr_t parent);

  // Colors
  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _aux_color1;
  fvec4 _aux_color2;
  fvec4 _border_color;
  fvec4 _text_color;

  // Geometry
  int _corner_radius;       // pixels
  int _border_width;        // pixels
  int _padding;             // pixels

  // Rendering
  lev2::BlendingMacro _blend_mode;

  // Typography
  lev2::font_ptr_t _font;
  // NOTE: font size is a property of font, not stored here

  // CSS-like inheritance
  style_ptr_t _parent;  // Inherit from this style if properties not overridden

  // Widget-specific overrides (optional, only allocated if needed)
  styletab_ptr_t _tab;
  stylebutton_ptr_t _button;
  styletextbox_ptr_t _textbox;
  styleslider_ptr_t _slider;
  stylescrollbar_ptr_t _scrollbar;
  stylepanel_ptr_t _panel;
  stylegraph_ptr_t _graph;
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

  // SDF primitive rendering methods (geometry-based)
  void drawTab(int x, int y, int w, int h, drawevent_constptr_t drwev, const Style* style);  // Applies style corner_radius to top corners, sharp bottom
  void drawBoxPerCorner(int x, int y, int w, int h, drawevent_constptr_t drwev, const Style* style, const fvec4& corner_radii);
  void drawCircle(int x, int y, int w, int h, drawevent_constptr_t drwev, const Style* style, float radius);
  void drawTriangle(int x, int y, int w, int h, drawevent_constptr_t drwev, const Style* style, float rotation = 0.0f);
  void drawRing(int x, int y, int w, int h, drawevent_constptr_t drwev, const Style* style, float inner_radius);
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
