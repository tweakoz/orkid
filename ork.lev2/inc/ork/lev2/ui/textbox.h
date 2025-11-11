////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Label Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct TextBox final : public Widget {
public:
  TextBox(
      const std::string& name, //
      fvec4 color,
      std::string text);
  void setText(std::string txt);
  fvec4 _color;
  fvec4 _textcolor;
  lev2::font_ptr_t _font;

  ETextAlignH _halign = ETextAlignH::CENTER;
  ETextAlignV _valign = ETextAlignV::CENTER;
  lev2::BlendingMacro _blending = lev2::BlendingMacro::OFF;

  std::vector<std::string> _lines;
  size_t _numchars = 0;
  size_t _maxlinelen = 0;
  event_lambda_t _onMousePush;
  event_lambda_t _onMouseRelease;
  event_lambda_t _onMouseMove;
  event_lambda_t _onMouseDrag;
  event_lambda_t _onKeyDown;
  event_lambda_t _onKeyUp;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  Widget* doRouteUiEvent(event_constptr_t Ev) override;

};

} // namespace ork::ui
