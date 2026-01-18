#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/f32edit.h>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
F32Edit::F32Edit(
    const std::string& name,
    const std::string& label,
    float value,
    float minval,
    float maxval,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _label(label)
    , _value(value)
    , _original_value(value)
    , _min(minval)
    , _max(maxval) {
  _bg_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  _fg_color = fvec4(1, 1, 1, 1);
}

///////////////////////////////////////////////////////////////////////////////
void F32Edit::setValue(float val) {
  _value = clampValue(val);
  _original_value = _value;
  _edit_buffer.clear();
  _editing = false;
}

///////////////////////////////////////////////////////////////////////////////
std::string F32Edit::formatValue() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(_precision) << _value;
  return oss.str();
}

///////////////////////////////////////////////////////////////////////////////
bool F32Edit::parseValue(const std::string& str, float& out) const {
  if (str.empty() || str == "-" || str == ".") {
    return false;
  }
  try {
    out = std::stof(str);
    return true;
  } catch (...) {
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////
float F32Edit::clampValue(float v) const {
  if (v < _min) return _min;
  if (v > _max) return _max;
  return v;
}

///////////////////////////////////////////////////////////////////////////////
bool F32Edit::isValidChar(char c) const {
  return (c >= '0' && c <= '9') || c == '.' || c == '-';
}

///////////////////////////////////////////////////////////////////////////////
HandlerResult F32Edit::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;
  switch (cev->_eventcode) {
    case EventCode::KEY_DOWN:
    case EventCode::KEY_REPEAT: {
      int key = cev->miKeyCode;
      if (_editing) {
        switch (key) {
          case 256: // esc - cancel edit
            _edit_buffer.clear();
            _editing = false;
            _highlight = false;
            rval._widget_finished = true;
            break;
          case 257: { // enter - commit
            float parsed;
            if (parseValue(_edit_buffer, parsed)) {
              float old_value = _value;
              _value = clampValue(parsed);
              _original_value = _value;
              if (_onValueChanged && _value != old_value) {
                _onValueChanged(_value);
              }
              if (_onValueCommitted) {
                _onValueCommitted(_value);
              }
            }
            _edit_buffer.clear();
            _editing = false;
            _highlight = false;
            rval._widget_finished = true;
            break;
          }
          case 259: // backspace
            if (_edit_buffer.length()) {
              _edit_buffer.pop_back();
            }
            break;
          default: {
            char ch = 0;
            if (key >= '0' && key <= '9') {
              ch = char(key);
            } else if (key == '.' || key == 46) {
              // only allow one decimal point
              if (_edit_buffer.find('.') == std::string::npos) {
                ch = '.';
              }
            } else if (key == '-' || key == 45) {
              // minus only at start
              if (_edit_buffer.empty()) {
                ch = '-';
              }
            }
            if (ch) {
              _edit_buffer += ch;
            }
            break;
          }
        }
      }
      rval.setHandled(this);
      break;
    }
    case EventCode::PUSH: {
      _highlight = true;
      _editing = true;
      _edit_buffer = formatValue();
      break;
    }
    case EventCode::DOUBLECLICK: {
      rval.setHandled(this);
      rval._widget_finished = true;
      break;
    }
    case EventCode::MOUSE_ENTER: {
      break;
    }
    case EventCode::MOUSE_LEAVE: {
      // commit on mouse leave if editing
      if (_editing) {
        float parsed;
        if (parseValue(_edit_buffer, parsed)) {
          float old_value = _value;
          _value = clampValue(parsed);
          _original_value = _value;
          if (_onValueChanged && _value != old_value) {
            _onValueChanged(_value);
          }
        }
        _edit_buffer.clear();
        _editing = false;
      }
      _highlight = false;
      break;
    }
    case EventCode::PASTE_TEXT: {
      // try to parse pasted text as float
      float parsed;
      if (parseValue(cev->_paste_text, parsed)) {
        float old_value = _value;
        _value = clampValue(parsed);
        if (_onValueChanged && _value != old_value) {
          _onValueChanged(_value);
        }
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
void F32Edit::DoDraw(drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    // draw background
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1, ix2, iy1, iy2,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();

    // draw highlight border if active
    if (_highlight) {
      tgt->PushModColor(fvec4(_bg_color.xyz() * 2.0, 1));
      primi->RenderQuadAtZ(
          defmtl.get(),
          ix1 + 1, ix2 - 1, iy1 + 1, iy2 - 1,
          0.0f,
          0.0f, 1.0f,
          0.0f, 1.0f);
      tgt->PopModColor();
    }

    // draw inner input area
    fvec4 input_col = _input_color_set ? _input_color : fvec4(_bg_color.xyz() * 0.5f, 1);
    tgt->PushModColor(input_col);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1 + 2, ix2 - 2, iy1 + 2, iy2 - 2,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();

    // draw "label: value" text
    std::string display_text;
    if (_editing) {
      display_text = _label + ": " + _edit_buffer;
    } else {
      display_text = _label + ": " + formatValue();
    }

    int text_x = ix1 + 4;
    int text_y = _label_font->centerY(iyc);
    ork::lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(_fg_color);

    lev2::FontMan::beginTextBlock(tgt, display_text.length());
    lev2::FontMan::DrawText(tgt, text_x, text_y, display_text.c_str());
    lev2::FontMan::endTextBlock(tgt);

    // draw cursor when editing
    if (_editing) {
      auto now = std::chrono::steady_clock::now();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      bool cursor_visible = (ms / 500) % 2 == 0;

      if (cursor_visible) {
        int text_width = _label_font->stringWidth(display_text.length());
        int cursor_x = text_x + text_width;
        int cursor_y1 = iy1 + 4;
        int cursor_y2 = iy2 - 4;

        tgt->PushModColor(fvec4(1, 1, 1, 1));
        primi->RenderQuadAtZ(
            defmtl.get(),
            cursor_x, cursor_x + 2,
            cursor_y1, cursor_y2,
            0.0f,
            0.0f, 1.0f,
            0.0f, 1.0f);
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
