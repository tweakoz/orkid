#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/ui/imagebutton.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

ImageButton::ImageButton(
    const std::string& name,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h) {
  _draw_label = false; // No label for ImageButton
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult ImageButton::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      _pressed = true;
      rval.setHandled(this);
      break;
    }

    case EventCode::RELEASE: {
      if (_pressed) {
        _pressed = false;
        if (_onPressed) {
          _onPressed();
        }
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::MOUSE_ENTER: {
      _hovered = true;
      rval.setHandled(this);
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _pressed = false;
      _hovered = false;
      rval.setHandled(this);
      break;
    }

    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ImageButton::_updateTextures(lev2::Context* ctx) {
  auto txi = ctx->TXI();

  // Update inactive state
  if (_inactive_image_provider) {
    auto new_image = _inactive_image_provider->_func();
    if (new_image != _inactive_image) {
      _inactive_image = new_image;
      if (_inactive_image) {
        if (!_inactive_texture) {
          _inactive_texture = std::make_shared<lev2::Texture>();
        }
        txi->initTextureFromImage(_inactive_texture.get(), _inactive_image, false);
      }
    }
  } else if (_inactive_image && !_inactive_texture) {
    _inactive_texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(_inactive_texture.get(), _inactive_image, true);
  }

  // Update active_released state
  if (_active_released_image_provider) {
    auto new_image = _active_released_image_provider->_func();
    if (new_image != _active_released_image) {
      _active_released_image = new_image;
      if (_active_released_image) {
        if (!_active_released_texture) {
          _active_released_texture = std::make_shared<lev2::Texture>();
        }
        txi->initTextureFromImage(_active_released_texture.get(), _active_released_image, false);
      }
    }
  } else if (_active_released_image && !_active_released_texture) {
    _active_released_texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(_active_released_texture.get(), _active_released_image, true);
  }

  // Update active_pressed state
  if (_active_pressed_image_provider) {
    auto new_image = _active_pressed_image_provider->_func();
    if (new_image != _active_pressed_image) {
      _active_pressed_image = new_image;
      if (_active_pressed_image) {
        if (!_active_pressed_texture) {
          _active_pressed_texture = std::make_shared<lev2::Texture>();
        }
        txi->initTextureFromImage(_active_pressed_texture.get(), _active_pressed_image, false);
      }
    }
  } else if (_active_pressed_image && !_active_pressed_texture) {
    _active_pressed_texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(_active_pressed_texture.get(), _active_pressed_image, true);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ImageButton::DoDraw(drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto fxi    = tgt->FXI();
  auto defmtl = lev2::defaultUITextureMaterial();

  // Select background color based on state
  fvec4 bg_color = _bgcolor;
  if (_pressed) {
    bg_color = _pressed_color;
  } else if (_hovered) {
    bg_color = _hover_color;
  }
  _drawColoredBox(drwev, bg_color);

  // Update textures from images/providers
  _updateTextures(tgt);

  // Determine current state and select appropriate texture/blend
  lev2::texture_ptr_t current_texture;
  lev2::BlendingMacro current_blend_mode;
  lev2::image_ptr_t current_image;

  if (_pressed && _active_pressed_texture) {
    current_texture = _active_pressed_texture;
    current_blend_mode = _active_pressed_blend_mode;
    current_image = _active_pressed_image;
  } else if (_hovered && _active_released_texture) {
    current_texture = _active_released_texture;
    current_blend_mode = _active_released_blend_mode;
    current_image = _active_released_image;
  } else {
    current_texture = _inactive_texture;
    current_blend_mode = _inactive_blend_mode;
    current_image = _inactive_image;
  }

  // Only draw if we have a texture
  if (!current_texture) {
    return;
  }

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;

    // Apply margin
    int margin = 2;
    ix1 += margin;
    iy1 += margin;
    ix2 -= margin;
    iy2 -= margin;

    int draw_x1 = ix1;
    int draw_y1 = iy1;
    int draw_x2 = ix2;
    int draw_y2 = iy2;

    // Handle aspect ratio preservation
    if (_preserve_aspect_ratio && current_image) {
      int widget_w = ix2 - ix1;
      int widget_h = iy2 - iy1;
      int img_w = current_image->_width;
      int img_h = current_image->_height;

      if (img_w > 0 && img_h > 0) {
        float widget_aspect = float(widget_w) / float(widget_h);
        float image_aspect = float(img_w) / float(img_h);

        if (image_aspect > widget_aspect) {
          // Image is wider - fit to width, letterbox vertically
          int scaled_h = int(widget_w / image_aspect);
          int offset_y = (widget_h - scaled_h) / 2;
          draw_y1 = iy1 + offset_y;
          draw_y2 = draw_y1 + scaled_h;
        } else {
          // Image is taller - fit to height, pillarbox horizontally
          int scaled_w = int(widget_h * image_aspect);
          int offset_x = (widget_w - scaled_w) / 2;
          draw_x1 = ix1 + offset_x;
          draw_x2 = draw_x1 + scaled_w;
        }
      }
    }

    // Set blend mode, depth test, and texture
    tgt->PushModColor(fvec4(1, 1, 1, 1));
    defmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, current_texture.get());
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(current_blend_mode);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    int prev_pri = rs->_priority;
    rs->_priority = 1<<16; 
    fxi->pushRasterState(rs);
    // Draw textured quad
    primi->RenderQuadAtZ(
        defmtl.get(),
        draw_x1,  // x0
        draw_x2,  // x1
        draw_y1,  // y0
        draw_y2,  // y1
        0.0f,     // z
        0.0f,
        1.0f,     // u0, u1
        0.0f,
        1.0f      // v0, v1
    );
    fxi->popRasterState();
    rs->_priority = prev_pri;

    tgt->PopModColor();
    defmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, nullptr);
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
