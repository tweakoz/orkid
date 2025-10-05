#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Button Widget
//  Supports push button and toggle (checkbox) modes
////////////////////////////////////////////////////////////////////

enum class ButtonMode : crc_enum_t {
  CrcEnum(PUSH),    // Push button - fires on click
  CrcEnum(TOGGLE),  // Toggle/checkbox - toggles state on click
};

struct Button final : public Widget {
public:
  Button(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setMode(ButtonMode mode) { _mode = mode; }
  ButtonMode mode() const { return _mode; }

  void setToggled(bool val) { _toggled = val; }
  bool isToggled() const { return _toggled; }

  // Callback for button events
  void_lambda_t _onPressed;
  void_lambda_t _onToggled;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _check_color;
  ButtonMode _mode = ButtonMode::TOGGLE;
  bool _toggled = false;
  bool _highlight = false;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using button_ptr_t = std::shared_ptr<Button>;

} // namespace ork::ui
