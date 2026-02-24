////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/overlay_lineedit.h>
#include <ork/lev2/ui/context.h>
#include <chrono>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

OverlayLineEdit::OverlayLineEdit(const std::string& name, const std::string& initial_value)
    : Widget(name, 0, 0, 0, 0)
    , _value(initial_value)
    , _original_value(initial_value) {
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult OverlayLineEdit::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult rval;

  switch (ev->_eventcode) {
    case EventCode::KEY_DOWN:
    case EventCode::KEY_REPEAT: {
      int key = ev->miKeyCode;
      switch (key) {
        case 256: // ESC
          if (_onCancel) {
            _onCancel();
          }
          if (_uicontext) {
            _uicontext->popOverlay();
          }
          rval.setHandled(this);
          break;
        case 257: // ENTER
          if (_onCommit) {
            _onCommit(_value);
          }
          if (_uicontext) {
            _uicontext->popOverlay();
          }
          rval.setHandled(this);
          break;
        case 259: // BACKSPACE
          if (!_value.empty()) {
            _value.pop_back();
          }
          rval.setHandled(this);
          break;
        default: {
          // Handle printable characters (same as LineEdit)
          char ch = 0;
          if (key >= 'A' && key <= 'Z') {
            ch = ev->mbSHIFT ? char(key) : char(key + 32);
          } else if (key >= '0' && key <= '9') {
            if (ev->mbSHIFT) {
              static const char shifted[] = ")!@#$%^&*(";
              ch = shifted[key - '0'];
            } else {
              ch = char(key);
            }
          } else if (key >= 32 && key <= 126) {
            if (ev->mbSHIFT) {
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
          }
          rval.setHandled(this);
          break;
        }
      }
      break;
    }
    case EventCode::KEY_UP:
      // Absorb key-up so it doesn't re-trigger character insertion
      rval.setHandled(this);
      break;
    case EventCode::PUSH:
      // Absorb clicks so overlay doesn't dismiss
      rval.setHandled(this);
      break;
    case EventCode::PASTE_TEXT:
      _value = ev->_paste_text;
      rval.setHandled(this);
      break;
    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void OverlayLineEdit::DoDraw(drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  mtxi->PushUIMatrix();
  {
    // Draw background
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();

    // Draw input area
    tgt->PushModColor(_input_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1 + 2, ix2 - 2, iy1 + 2, iy2 - 2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();

    // Draw text
    auto font = lev2::FontMan::fontForId("i14");
    if (font) {
      lev2::FontMan::PushFont(font);
      tgt->PushModColor(_fg_color);

      int text_x = ix1 + 6;
      int text_y = font->centerY((iy1 + iy2) / 2);

      lev2::FontMan::beginTextBlock(tgt, _value.length());
      lev2::FontMan::DrawText(tgt, text_x, text_y, _value.c_str());
      lev2::FontMan::endTextBlock(tgt);

      // Blinking cursor
      auto now = std::chrono::steady_clock::now();
      auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      bool cursor_visible = (ms / 500) % 2 == 0;

      if (cursor_visible) {
        int text_width = font->stringWidth(_value.length());
        int cursor_x   = text_x + text_width;
        int cursor_y1  = iy1 + 4;
        int cursor_y2  = iy2 - 4;

        tgt->PushModColor(fvec4(1, 1, 1, 1));
        defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
        primi->RenderQuadAtZ(defmtl.get(), cursor_x, cursor_x + 2, cursor_y1, cursor_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
        tgt->PopModColor();
      }

      tgt->PopModColor();
      lev2::FontMan::PopFont();
    }
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
