#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/texman.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Button Widget
//  Simple push button with optional textures for up/down states
//  Falls back to color rendering if textures not set
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

  void setUpTexture(lev2::texture_ptr_t tex) { _up_texture = tex; }
  void setDownTexture(lev2::texture_ptr_t tex) { _down_texture = tex; }

  // Callback for button press
  void_lambda_t _onPressed;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _down_color;
  lev2::texture_ptr_t _up_texture;
  lev2::texture_ptr_t _down_texture;
  bool _pressed = false;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using button_ptr_t = std::shared_ptr<Button>;

} // namespace ork::ui
