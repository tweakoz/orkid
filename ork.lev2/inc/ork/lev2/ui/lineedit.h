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
  fvec4 _input_color;  // Color of the text input area (defaults to _bg_color * 0.5)
  bool _input_color_set = false;  // Track if explicitly set
  std::string _value;
  std::string _original_value;
  bool _highlight = false;

  // Callbacks
  std::function<void(const std::string&)> _onTextChanged;
  std::function<void(const std::string&)> _onTextCommitted;  // Called on Enter

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

} //namespace ork::ui {
