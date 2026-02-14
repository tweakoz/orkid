#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/gfx/texman.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Button Widget
//  Pretty push button with SDF-rendered rounded box background
//  and centered text label. Uses ThemeEngine for rendering.
////////////////////////////////////////////////////////////////////

struct Button final : public Widget {
public:
  Button(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  // Callback for button press
  void_lambda_t _onPressed;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _down_color;
  fvec4 _hover_color;
  bool _pressed = false;
  bool _hovering = false;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using button_ptr_t = std::shared_ptr<Button>;

} // namespace ork::ui
