#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct Box final : public Widget {
public:
  Box(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);
  fvec4 _color;
  lev2::fxpipeline_ptr_t _pipeline_override;
  private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
};

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  the color changes depending on the last input event
//  mostly used for ui event testing,
// but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct EvTestBox final : public Widget {
public:
  EvTestBox(
      const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);
  EvTestBox(
      const std::string& name, //
      fvec4 colornormal,
      fvec4 colorclick,
      fvec4 colordoubleclick,
      fvec4 colordrag,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  fvec4 _colorNormal;
  fvec4 _colorClick;
  fvec4 _colorDoubleClick;
  fvec4 _colorDrag;
  fvec4 _colorKeyDown;
  fvec4 _fontColor;
  EventCode _colorsel = EventCode::UNKNOWN;
  lev2::BlendingMacro _blendingBG = lev2::BlendingMacro::OFF;
  lev2::BlendingMacro _blendingFG = lev2::BlendingMacro::ADDITIVE;
  lev2::uimaterial_ptr_t _material;
private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  the color changes depending on the last input event
//  mostly used for ui event testing,
// but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct LambdaBox final : public Widget {
public:
  LambdaBox(
      const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);
  LambdaBox(
      const std::string& name, //
      fvec4 colornormal,
      fvec4 colorclick,
      fvec4 colordoubleclick,
      fvec4 colordrag,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  fvec4 _colorNormal;
  fvec4 _colorClick;
  fvec4 _colorDoubleClick;
  fvec4 _colorDrag;
  fvec4 _colorKeyDown;
  EventCode _colorsel = EventCode::UNKNOWN;
  void_lambda_t _onPressed;  
private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

} // namespace ork::ui
