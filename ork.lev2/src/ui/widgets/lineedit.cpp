#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/lineedit.h>
#include <chrono>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
LineEdit::LineEdit(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _bg_color(color) {
      _fg_color = fvec4(1,1,1,1);
      _draw_label = true;
}
///////////////////////////////////////////////////////////////////////////////
void LineEdit::setValue(const std::string& val) {
  _value          = val;
  _original_value = val;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult LineEdit::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;
  switch (cev->_eventcode) {
    case EventCode::KEY_DOWN:
    case EventCode::KEY_REPEAT: {
      int key = cev->miKeyCode;
      if(_highlight) {
        std::string old_value = _value;
        switch (key) {
          case 256: // esc
            _value = _original_value;
            if (_onTextChanged && _value != old_value) {
              _onTextChanged(_value);
            }
            break;
          case 257: // enter
            rval._widget_finished = true;
            _highlight = false;
            if (_onTextCommitted) {
              _onTextCommitted(_value);
            }
            break;
          case 259: // backspace
            if (_value.length()) {
              _value.pop_back();
              if (_onTextChanged) {
                _onTextChanged(_value);
              }
            }
            break;
          default:
            // Handle printable characters
            // Key codes are GLFW codes which match ASCII for A-Z (65-90) and 0-9 (48-57)
            char ch = 0;
            if (key >= 'A' && key <= 'Z') {
              // Letters: lowercase unless shift
              ch = cev->mbSHIFT ? char(key) : char(key + 32);
            } else if (key >= '0' && key <= '9') {
              if (cev->mbSHIFT) {
                // Shifted number row -> special characters
                static const char shifted[] = ")!@#$%^&*(";
                ch = shifted[key - '0'];
              } else {
                ch = char(key);
              }
            } else if (key >= 32 && key <= 126) {
              // Other printable characters - handle common shifted ones
              if (cev->mbSHIFT) {
                switch (key) {
                  case '-': ch = '_'; break;
                  case '=': ch = '+'; break;
                  case '[': ch = '{'; break;
                  case ']': ch = '}'; break;
                  case '\\': ch = '|'; break;
                  case ';': ch = ':'; break;
                  case '\'': ch = '"'; break;
                  case ',': ch = '<'; break;
                  case '.': ch = '>'; break;
                  case '/': ch = '?'; break;
                  case '`': ch = '~'; break;
                  default: ch = char(key); break;
                }
              } else {
                ch = char(key);
              }
            }
            if (ch) {
              _value += ch;
              if (_onTextChanged) {
                _onTextChanged(_value);
              }
            }
            break;
        }
      }
      rval.setHandled(this);
      break;
    }
    case EventCode::PUSH: {
      _highlight = true;
      break;
    }
    case EventCode::DOUBLECLICK: {
        rval.setHandled(this);
        rval._widget_finished = true;
        break;
    }
    case EventCode::MOUSE_ENTER:{
      break;
    }
    case EventCode::MOUSE_LEAVE:{
      _highlight = false;
      break;
    }
    case EventCode::PASTE_TEXT: {
      _value = cev->_paste_text;
      if (_onTextChanged) {
        _onTextChanged(_value);
      }
      rval.setHandled(this);
      break;
    }
    default:
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void LineEdit::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int label_w = labelWidth();
  auto content = contentRect();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw text content box
    ///////////////////////////////

    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

    if(_highlight){
      tgt->PushModColor(fvec4(_bg_color.xyz()*2.0, 1));
      primi->RenderQuadAtZ(
          defmtl.get(),
          ix1 + label_w,  // x0
          ix2 - 2,          // x1
          iy1 + 2,          // y0
          iy2 - 2,          // y1
          0.0f,             // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();
    }

    fvec4 input_col = _input_color_set ? _input_color : fvec4(_bg_color.xyz()*0.5, 1);
    tgt->PushModColor(input_col);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1 + label_w+1,  // x0
        ix2 - 3,         // x1
        iy1 + 3,         // y0
        iy2 - 3,         // y1
        0.0f,            // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw label (using Widget base class)
    ///////////////////////////////

    _drawLabel(drwev);

    ///////////////////////////////
    // draw text content
    ///////////////////////////////

    int text_x = ix1 + label_w + 4;
    int text_y = _label_font->centerY(iyc);
    ork::lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(_fg_color);

    lev2::FontMan::beginTextBlock(tgt, _value.length());
    lev2::FontMan::DrawText(
        tgt, //
        text_x,
        text_y,
        _value.c_str());
    lev2::FontMan::endTextBlock(tgt);

    ///////////////////////////////
    // draw cursor when highlighted
    ///////////////////////////////

    if (_highlight) {
      // Blinking cursor (toggle every 500ms)
      auto now = std::chrono::steady_clock::now();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      bool cursor_visible = (ms / 500) % 2 == 0;

      if (cursor_visible) {
        int text_width = _label_font->stringWidth(_value.length());
        int cursor_x = text_x + text_width;
        int cursor_y1 = iy1 + 4;
        int cursor_y2 = iy2 - 4;

        // Draw cursor line
        tgt->PushModColor(fvec4(1, 1, 1, 1));
        primi->RenderQuadAtZ(
            defmtl.get(),
            cursor_x,      // x0
            cursor_x + 2,  // x1 (2 pixel wide cursor)
            cursor_y1,     // y0
            cursor_y2,     // y1
            0.0f,          // z
            0.0f, 1.0f,    // u0, u1
            0.0f, 1.0f     // v0, v1
        );
        tgt->PopModColor();
      }
    }

    tgt->PopModColor();
    ork::lev2::FontMan::PopFont();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
