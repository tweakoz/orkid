#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Checkbox Widget
//  Toggle/checkbox widget
////////////////////////////////////////////////////////////////////

struct Checkbox final : public Widget {
public:
  Checkbox(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setToggled(bool val) { _toggled = val; }
  bool isToggled() const { return _toggled; }

  // Callback for checkbox toggle
  void_lambda_t _onToggled;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _check_color;
  bool _toggled = false;
  bool _highlight = false;
  svar64_t _pyuserdata;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using checkbox_ptr_t = std::shared_ptr<Checkbox>;

} // namespace ork::ui
