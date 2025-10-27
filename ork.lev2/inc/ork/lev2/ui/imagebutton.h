#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ImageButton Widget
//  Button that displays images for different states
//  Supports inactive, active_released, and active_pressed states
//  Images can be set directly or via providers
//  Themable (except images) with configurable blend modes
////////////////////////////////////////////////////////////////////

using image_provider_t = std::function<lev2::image_ptr_t()>;

struct ImageButton final : public Widget {
public:
  ImageButton(
      const std::string& name,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  ///////////////////////////////////////////////
  // Image setters (direct)
  ///////////////////////////////////////////////

  void setInactiveImage(lev2::image_ptr_t img) { _inactive_image = img; }
  void setActiveReleasedImage(lev2::image_ptr_t img) { _active_released_image = img; }
  void setActivePressedImage(lev2::image_ptr_t img) { _active_pressed_image = img; }

  ///////////////////////////////////////////////
  // Image provider setters (dynamic)
  ///////////////////////////////////////////////

  void setInactiveImageProvider(lev2::image_provider_ptr_t provider) { _inactive_image_provider = provider; }
  void setActiveReleasedImageProvider(lev2::image_provider_ptr_t provider) { _active_released_image_provider = provider; }
  void setActivePressedImageProvider(lev2::image_provider_ptr_t provider) { _active_pressed_image_provider = provider; }

  ///////////////////////////////////////////////
  // Blend mode setters
  ///////////////////////////////////////////////

  void setInactiveBlendMode(lev2::BlendingMacro mode) { _inactive_blend_mode = mode; }
  void setActiveReleasedBlendMode(lev2::BlendingMacro mode) { _active_released_blend_mode = mode; }
  void setActivePressedBlendMode(lev2::BlendingMacro mode) { _active_pressed_blend_mode = mode; }

  ///////////////////////////////////////////////
  // Callback for button press
  ///////////////////////////////////////////////

  void_lambda_t _onPressed;

  ///////////////////////////////////////////////
  // State
  ///////////////////////////////////////////////

  bool _preserve_aspect_ratio = true;
  bool _pressed = false;
  bool _hovered = false;
  fvec4 _bgcolor = fvec4(0,0,0, 1.0f);

  ///////////////////////////////////////////////
  // Image/Texture state for each button state
  ///////////////////////////////////////////////

  // Inactive state
  lev2::image_ptr_t _inactive_image;
  lev2::texture_ptr_t _inactive_texture;
  lev2::image_provider_ptr_t _inactive_image_provider;
  lev2::BlendingMacro _inactive_blend_mode = lev2::BlendingMacro::OFF;

  // Active Released state (hovered but not pressed)
  lev2::image_ptr_t _active_released_image;
  lev2::texture_ptr_t _active_released_texture;
  lev2::image_provider_ptr_t _active_released_image_provider;
  lev2::BlendingMacro _active_released_blend_mode = lev2::BlendingMacro::OFF;

  // Active Pressed state (clicked down)
  lev2::image_ptr_t _active_pressed_image;
  lev2::texture_ptr_t _active_pressed_texture;
  lev2::image_provider_ptr_t _active_pressed_image_provider;
  lev2::BlendingMacro _active_pressed_blend_mode = lev2::BlendingMacro::OFF;

  private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;

  void _updateTextures(lev2::Context* ctx);
};

using imagebutton_ptr_t = std::shared_ptr<ImageButton>;

} // namespace ork::ui
