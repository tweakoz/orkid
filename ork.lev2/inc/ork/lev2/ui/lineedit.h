#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct LineEdit final : public Widget {
public:
  LineEdit(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

    HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setValue(const std::string& val);

  fvec4 _bg_color;
  fvec4 _fg_color;
  std::string _value;
  std::string _original_value;
  
private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

} //namespace ork::ui {
