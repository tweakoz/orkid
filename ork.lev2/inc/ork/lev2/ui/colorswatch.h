////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/coloredit.h>  // for coloredit_lambda_t

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ColorSwatch Widget
//  Inline color preview widget - shows a colored rectangle
//  Clickable to trigger detail editor (e.g., ColorPicker popup)
////////////////////////////////////////////////////////////////////

struct ColorSwatch final : public Widget {
public:
  ColorSwatch(const std::string& name,
      fvec4 color = fvec4(0.5f, 0.5f, 0.5f, 1.0f),
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  // Color accessors
  void setColor(const fvec4& color) { _color = color; }
  const fvec4& color() const { return _color; }

  // Callbacks
  void_lambda_t _onClick;           // Called when swatch is clicked
  coloredit_lambda_t _onColorChanged;  // Called when color changes (from external source)

  // Appearance
  fvec4 _color;
  fvec4 _border_color = fvec4(0.4f, 0.4f, 0.4f, 1.0f);
  fvec4 _hover_border_color = fvec4(0.7f, 0.7f, 0.7f, 1.0f);
  int _border_width = 1;
  int _corner_radius = 2;
  bool _show_hex = false;  // Optionally show hex color value
  bool _hovering = false;

  // For user data (Python integration)
  svar64_t _pyuserdata;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using colorswatch_ptr_t = std::shared_ptr<ColorSwatch>;

} // namespace ork::ui
