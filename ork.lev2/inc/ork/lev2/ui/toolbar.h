////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license-mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>
#include <functional>
#include <vector>
#include <memory>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Toolbar Orientation
////////////////////////////////////////////////////////////////////

enum class ToolbarOrientation {
  Horizontal,
  Vertical,
  Auto  // Determined from geometry
};

////////////////////////////////////////////////////////////////////
// ToolbarItem - base class for items in a toolbar
////////////////////////////////////////////////////////////////////

struct ToolbarItem;
using toolbar_item_ptr_t = std::shared_ptr<ToolbarItem>;

struct ToolbarItem {
  ToolbarItem() = default;
  virtual ~ToolbarItem() = default;

  std::string _id;
  bool _enabled = true;
  bool _visible = true;

  // Computed layout position/size (set by Toolbar during layout)
  int _x = 0;
  int _y = 0;
  int _width = 0;
  int _height = 0;
};

////////////////////////////////////////////////////////////////////
// ToolbarButton - image button in toolbar
////////////////////////////////////////////////////////////////////

struct ToolbarButton;
using toolbar_button_ptr_t = std::shared_ptr<ToolbarButton>;

struct ToolbarButton : public ToolbarItem {
  ToolbarButton() = default;
  ToolbarButton(const std::string& id);

  // Icon images (similar to ImageButton)
  lev2::image_ptr_t _icon_image;
  lev2::image_ptr_t _prev_icon_image;
  lev2::image_provider_ptr_t _icon_provider;
  lev2::texture_ptr_t _icon_texture;

  // Hover state image (optional - tints normal if not set)
  lev2::image_ptr_t _hover_image;
  lev2::image_ptr_t _prev_hover_image;
  lev2::image_provider_ptr_t _hover_provider;
  lev2::texture_ptr_t _hover_texture;

  // Pressed state image (optional - tints normal if not set)
  lev2::image_ptr_t _pressed_image;
  lev2::image_ptr_t _prev_pressed_image;
  lev2::image_provider_ptr_t _pressed_provider;
  lev2::texture_ptr_t _pressed_texture;

  // Tooltip text
  std::string _tooltip;

  // Text label (drawn instead of or alongside icon)
  std::string _label;

  // Custom width override (0 = use icon_size, >0 = explicit pixel width for icon area)
  int _custom_width = 0;

  // Toggle mode (stays pressed until clicked again)
  bool _toggle_mode = false;
  bool _toggled = false;

  // Per-button color override (alpha=0 means use toolbar default)
  fvec4 _color_override = fvec4(0, 0, 0, 0);

  // Visual state
  bool _hovered = false;
  bool _pressed = false;

  // Callbacks
  std::function<void()> _onPressed;
  std::function<void(bool)> _onToggled;  // For toggle mode
  std::function<void(int)> _onKeyEvent;  // key code on KEY_DOWN/KEY_REPEAT when focused

  // Update textures from images/providers
  void updateTextures(lev2::Context* ctx);
};

////////////////////////////////////////////////////////////////////
// ToolbarSeparator - visual divider between button groups
////////////////////////////////////////////////////////////////////

struct ToolbarSeparator;
using toolbar_separator_ptr_t = std::shared_ptr<ToolbarSeparator>;

struct ToolbarSeparator : public ToolbarItem {
  ToolbarSeparator() = default;
  ToolbarSeparator(const std::string& id);

  int _thickness = 1;  // Line thickness in pixels
  int _padding = 4;    // Extra space around separator
};

////////////////////////////////////////////////////////////////////
// Toolbar Widget
////////////////////////////////////////////////////////////////////

struct Toolbar;
using toolbar_ptr_t = std::shared_ptr<Toolbar>;

struct Toolbar : public Widget {
  Toolbar(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~Toolbar();

  //////////////////////////////////////////////////////////////
  // Item management
  //////////////////////////////////////////////////////////////

  // Add a button with icon image
  toolbar_button_ptr_t addButton(
      const std::string& id,
      lev2::image_ptr_t icon,
      const std::string& tooltip = "");

  // Add a button with icon provider
  toolbar_button_ptr_t addButtonWithProvider(
      const std::string& id,
      lev2::image_provider_ptr_t icon_provider,
      const std::string& tooltip = "");

  // Add a text-only button (no icon)
  toolbar_button_ptr_t addTextButton(
      const std::string& id,
      const std::string& label,
      const std::string& tooltip = "");

  // Add a separator
  toolbar_separator_ptr_t addSeparator(const std::string& id = "");

  // Remove an item by ID
  void removeItem(const std::string& id);

  // Get item by ID
  toolbar_item_ptr_t getItem(const std::string& id);
  toolbar_button_ptr_t getButton(const std::string& id);

  // Clear all items
  void clear();

  // Get all items
  const std::vector<toolbar_item_ptr_t>& getItems() const { return _items; }

  //////////////////////////////////////////////////////////////
  // Orientation
  //////////////////////////////////////////////////////////////

  void setOrientation(ToolbarOrientation orient) { _orientation = orient; _needs_layout = true; }
  ToolbarOrientation getOrientation() const { return _orientation; }

  // Get effective orientation (resolves Auto)
  ToolbarOrientation getEffectiveOrientation() const;

  // Check if horizontal
  bool isHorizontal() const { return getEffectiveOrientation() == ToolbarOrientation::Horizontal; }

  //////////////////////////////////////////////////////////////
  // Appearance
  //////////////////////////////////////////////////////////////

  int _icon_size = 24;           // Icon width/height
  int _button_padding = 4;       // Padding inside button around icon
  int _item_spacing = 2;         // Space between items
  int _edge_padding = 4;         // Padding at toolbar edges

  fvec4 _bgcolor = fvec4(0.15f, 0.15f, 0.15f, 1.0f);
  fvec4 _button_color = fvec4(0.0f, 0.0f, 0.0f, 0.0f);          // normal-state bg (transparent = no bg)
  fvec4 _button_hover_color = fvec4(0.25f, 0.25f, 0.3f, 1.0f);
  fvec4 _button_pressed_color = fvec4(0.2f, 0.4f, 0.6f, 1.0f);
  fvec4 _button_toggled_color = fvec4(0.3f, 0.5f, 0.7f, 1.0f);
  fvec4 _separator_color = fvec4(0.3f, 0.3f, 0.3f, 1.0f);
  fvec4 _disabled_tint = fvec4(0.5f, 0.5f, 0.5f, 0.5f);
  fvec4 _button_border_color = fvec4(0.0f, 0.0f, 0.0f, 0.0f);   // border color (transparent = no border)
  int _button_border_width = 1;                                   // border thickness in pixels

  bool _draw_background = true;

  lev2::font_ptr_t _label_font;   // Font for button text labels
  fvec4 _label_color = fvec4(0.9f, 0.9f, 0.9f, 1.0f);  // Text label color
  fvec4 _label_toggled_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);  // Text color when toggled
  int _label_padding = 6;         // Horizontal padding around label text

  lev2::font_ptr_t _tooltip_font;

  //////////////////////////////////////////////////////////////
  // Tooltip
  //////////////////////////////////////////////////////////////

  bool _show_tooltips = true;
  int _tooltip_delay_ms = 500;  // Delay before showing tooltip

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  void _doGpuInit(lev2::Context* ctx) override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _rebuildLayout();
  int _getItemAt(int local_x, int local_y) const;
  void _drawButton(drawevent_constptr_t drwev, toolbar_button_ptr_t btn, int abs_x, int abs_y);
  void _drawSeparator(drawevent_constptr_t drwev, toolbar_separator_ptr_t sep, int abs_x, int abs_y);
  void _drawTooltip(drawevent_constptr_t drwev);
  void _updateButtonTextures(lev2::Context* ctx);

  std::vector<toolbar_item_ptr_t> _items;
  ToolbarOrientation _orientation = ToolbarOrientation::Auto;
  bool _needs_layout = true;

  // Focus state (for key events)
  int _focused_index = -1;

  // Tooltip state
  int _hovered_index = -1;
  double _hover_start_time = 0;
  bool _tooltip_visible = false;

  // Separator ID counter
  int _separator_counter = 0;
};

} // namespace ork::ui
