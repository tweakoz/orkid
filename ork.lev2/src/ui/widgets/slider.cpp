#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/slider.h>
#include <ork/lev2/ui/popups.inl>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// IntSlider
///////////////////////////////////////////////////////////////////////////////

IntSlider::IntSlider(
    const std::string& name, //
    fvec4 color,
    int min,
    int max,
    int value,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _bg_color(color)
    , _value(value)
    , _min(min)
    , _max(max) {
  _fg_color = fvec4(1, 1, 1, 1);
  _fill_color = fvec4(0.2, 0.6, 0.8, 1);
  _draw_label = true;

  if (_max == _min) {
    _max = _min + 1;
  }

  _text_pos = 64;

  setValue(value);

}

///////////////////////////////////////////////////////////////////////////////

void IntSlider::setValue(int val) {
  if (val < _min)
    val = _min;
  if (val > _max)
    val = _max;
  _value = val;
  _value_str = std::to_string(_value);
  _refresh();

}

///////////////////////////////////////////////////////////////////////////////

void IntSlider::setRange(int min, int max) {
  _min = min;
  _max = max;
  if (_max == _min) {
    _max = _min + 1;
  }
  setValue(_value);
}

///////////////////////////////////////////////////////////////////////////////

float IntSlider::_valToUnit(int val) const {
  float range = float(_max - _min);
  return (float(val) - float(_min)) / range;
}

///////////////////////////////////////////////////////////////////////////////

int IntSlider::_unitToVal(float unit) const {
  float range = float(_max - _min);
  float fval = (unit * range) + _min;
  return int(fval + 0.5f);
}

void IntSlider::DoLayout() {
  _refresh();
}

///////////////////////////////////////////////////////////////////////////////

void IntSlider::_refresh() {
  auto content = contentRect();
  float unit = _valToUnit(_value);

  _indicator_pos = (unit * (content._w-4));

  // Smart text positioning - avoid overlap with filled bar
  float text_unit = 0.0f;
  if (unit < 0.6f) {
    text_unit = 0.66f;
  } else {
    text_unit = 0.36f;
  }

  _text_pos = (text_unit * content._w);
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult IntSlider::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      _dragging = true;
      rval.setHandled(this);
      break;
    }

    case EventCode::DRAG: {
      if (_dragging) {
        //_update_on_drag = cev->mbCTRL;

        auto content = contentRect();
        int local_x = cev->miX - _geometry._x - content._x;

        float unit = float(local_x) / float(content._w);
        if (unit < 0.0f)
          unit = 0.0f;
        else if (unit > 1.0f)
          unit = 1.0f;

        int new_val = _unitToVal(unit);

        // Right button = smoothed value change
        if (cev->IsButton2DownF()) {
          new_val = int(float(_value) * 0.9f + float(new_val) * 0.1f);
        }

        _value = new_val;
        if (_value < _min)
          _value = _min;
        if (_value > _max)
          _value = _max;

        _value_str = std::to_string(_value);
        _refresh();

        if (_update_on_drag && _onValueChanged) {
          _onValueChanged();
        }

        rval.setHandled(this);
      }
      break;
    }

    case EventCode::RELEASE: {
      if (_dragging) {
        _dragging = false;
        if (!_update_on_drag && _onValueChanged) {
          _onValueChanged();
        }
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::DOUBLECLICK: {
      _text_editing = true;
      _edit_buffer = _value_str;
      rval.setHandled(this);
      break;
    }

    case EventCode::KEY_DOWN: {
      if (_text_editing) {
        int key = cev->miKeyCode;

        // Enter key - commit value
        if (key == 257) { // Qt::Key_Return or Qt::Key_Enter
          try {
            int new_val = std::stoi(_edit_buffer);
            setValue(new_val);
            if (_onValueChanged) {
              _onValueChanged();
            }
          } catch (...) {
          }
          _text_editing = false;
          _edit_buffer.clear();
          rval.setHandled(this);
        }
        // Escape key - cancel
        else if (key == 256) { // Qt::Key_Escape
          _text_editing = false;
          _edit_buffer.clear();
          rval.setHandled(this);
        }
        // Backspace
        else if (key == 259) { // Qt::Key_Backspace
          if (!_edit_buffer.empty()) {
            _edit_buffer.pop_back();
          }
          rval.setHandled(this);
        }
        // Numeric keys, minus
        else if ((key >= 48 && key <= 57) ||  // 0-9
                 key == 45) {                  // minus
          _edit_buffer += char(key);
          rval.setHandled(this);
        }
      }
      break;
    }

    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void IntSlider::DoDraw(drawevent_constptr_t drwev) {

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
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

    int content_x1 = ix1 + label_w;
    int content_x2 = ix2 - 2;
    int content_y1 = iy1 + 2;
    int content_y2 = iy2 - 2;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

    ///////////////////////////////
    // draw background
    ///////////////////////////////

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
    // draw content background
    ///////////////////////////////

    tgt->PushModColor(fvec4(_bg_color.xyz() * 0.5, 1));
    primi->RenderQuadAtZ(
        defmtl.get(),
        content_x1 + 2, // x0
        content_x2,     // x1
        content_y1 + 1, // y0
        content_y2 - 1, // y1
        0.0f,           // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw filled indicator
    ///////////////////////////////

    int fill_x2 = content_x1 + int(_indicator_pos);
    if (fill_x2 > content_x1 + 2) {
      tgt->PushModColor(_fill_color);
      primi->RenderQuadAtZ(
          defmtl.get(),
          content_x1 + 2, // x0
          fill_x2,        // x1
          content_y1 + 3, // y0
          content_y2 - 3, // y1
          0.0f,           // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();
    }

    ///////////////////////////////
    // draw value text
    ///////////////////////////////

    ork::lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(_fg_color);

    std::string display_text = _text_editing ? _edit_buffer + "_" : _value_str;
    if(display_text.empty())
      display_text = "0";
    int text_x = content_x1 + int(_text_pos);
    lev2::FontMan::beginTextBlock(tgt, display_text.length());
    lev2::FontMan::DrawText(
        tgt, //
        text_x,
        iyc - 6,
        display_text.c_str());
    lev2::FontMan::endTextBlock(tgt);

    tgt->PopModColor();
    ork::lev2::FontMan::PopFont();

    ///////////////////////////////
    // draw label
    ///////////////////////////////

    _drawLabel(drwev);
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////
// FloatSlider
///////////////////////////////////////////////////////////////////////////////

FloatSlider::FloatSlider(
    const std::string& name, //
    fvec4 color,
    float min,
    float max,
    float value,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _bg_color(color)
    , _value(value)
    , _min(min)
    , _max(max) {
  _fg_color = fvec4(1, 1, 1, 1);
  _fill_color = fvec4(0.2, 0.6, 0.8, 1);
  _draw_label = true;

  if (_max == _min) {
    _max = _min + 1.0f;
  }
  _text_pos = 64;

  setValue(value);
}

void FloatSlider::DoLayout() {
  _refresh();
}

///////////////////////////////////////////////////////////////////////////////

void FloatSlider::setValue(float val) {
  if (val < _min)
    val = _min;
  if (val > _max)
    val = _max;
  _value = val;

  char buf[64];
  snprintf(buf, sizeof(buf), "%.4g", _value);
  _value_str = buf;

  _refresh();
}

///////////////////////////////////////////////////////////////////////////////

void FloatSlider::setRange(float min, float max) {
  _min = min;
  _max = max;
  if (_max == _min) {
    _max = _min + 1.0f;
  }
  setValue(_value);
}

///////////////////////////////////////////////////////////////////////////////

void FloatSlider::setLogMode(bool log) {
  _log_mode = log;
  _refresh();
}

///////////////////////////////////////////////////////////////////////////////

float FloatSlider::_valToUnit(float val) const {
  if (_log_mode) {
    const float offset = fabs(_max - _min) / 100.0f;
    float log_min = log10f(offset + _min);
    float log_max = log10f(offset + _max);
    float log_range = log_max - log_min;

    float log_val = log10f(offset + val);
    float log_dist = (log_val - log_min);

    return log_dist / log_range;
  } else {
    float range = (_max - _min);
    return (val - _min) / range;
  }
}

///////////////////////////////////////////////////////////////////////////////

float FloatSlider::_unitToVal(float unit) const {
  if (_log_mode) {
    const float offset = fabs(_max - _min) / 100.0f;
    float log_min = log10f(offset + _min);
    float log_max = log10f(offset + _max);
    float log_range = log_max - log_min;

    float log_dist = (unit * log_range);
    float log_dist2 = log_dist + log_min;

    float flin = powf(10.0f, log_dist2);

    return flin - offset;
  } else {
    float range = (_max - _min);
    float fval = (unit * range) + _min;
    return fval;
  }
}

///////////////////////////////////////////////////////////////////////////////

void FloatSlider::_refresh() {
  auto content = contentRect();
  float unit = _valToUnit(_value);

  _indicator_pos = (unit * (content._w-4));

  // Smart text positioning - avoid overlap with filled bar
  float text_unit = 0.0f;
  if (unit < 0.6f) {
    text_unit = 0.66f;
  } else {
    text_unit = 0.36f;
  }

  _text_pos = (text_unit * content._w);
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult FloatSlider::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      _dragging = true;
      rval.setHandled(this);
      break;
    }

    case EventCode::DRAG: {
      if (_dragging) {
        //_update_on_drag = cev->mbCTRL;

        auto content = contentRect();
        int local_x = cev->miX - _geometry._x - content._x;

        float unit = float(local_x) / float(content._w);
        if (unit < 0.0f)
          unit = 0.0f;
        else if (unit > 1.0f)
          unit = 1.0f;

        float new_val = _unitToVal(unit);

        // Right button = smoothed value change
        if (cev->IsButton2DownF()) {
          new_val = _value * 0.9f + new_val * 0.1f;
        }

        _value = new_val;
        if (_value < _min)
          _value = _min;
        if (_value > _max)
          _value = _max;

        char buf[64];
        snprintf(buf, sizeof(buf), "%.4g", _value);
        _value_str = buf;

        _refresh();

        if (_update_on_drag && _onValueChanged) {
          _onValueChanged();
        }

        rval.setHandled(this);
      }
      break;
    }

    case EventCode::RELEASE: {
      if (_dragging) {
        _dragging = false;
        if (!_update_on_drag && _onValueChanged) {
          _onValueChanged();
        }
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::DOUBLECLICK: {
      _text_editing = true;
      _edit_buffer = _value_str;
      rval.setHandled(this);
      break;
    }

    case EventCode::KEY_DOWN: {
      if (_text_editing) {
        int key = cev->miKeyCode;

        // Enter key - commit value
        if (key == 257) { // Qt::Key_Return or Qt::Key_Enter
          try {
            float new_val = std::stof(_edit_buffer);
            setValue(new_val);
            if (_onValueChanged) {
              _onValueChanged();
            }
          } catch (...) {
          }
          _text_editing = false;
          _edit_buffer.clear();
          rval.setHandled(this);
        }
        // Escape key - cancel
        else if (key == 256) { // Qt::Key_Escape
          _text_editing = false;
          _edit_buffer.clear();
          rval.setHandled(this);
        }
        // Backspace
        else if (key == 259) { // Qt::Key_Backspace
          if (!_edit_buffer.empty()) {
            _edit_buffer.pop_back();
          }
          rval.setHandled(this);
        }
        // Numeric keys, minus, decimal point
        else if ((key >= 48 && key <= 57) ||  // 0-9
                 key == 45 ||                  // minus
                 key == 46) {                  // decimal point
          _edit_buffer += char(key);
          rval.setHandled(this);
        }
      }
      break;
    }

    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void FloatSlider::DoDraw(drawevent_constptr_t drwev) {

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
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

    int content_x1 = ix1 + label_w;
    int content_x2 = ix2 - 2;
    int content_y1 = iy1 + 2;
    int content_y2 = iy2 - 2;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

    ///////////////////////////////
    // draw background
    ///////////////////////////////

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
    // draw content background
    ///////////////////////////////

    tgt->PushModColor(fvec4(_bg_color.xyz() * 0.5, 1));
    primi->RenderQuadAtZ(
        defmtl.get(),
        content_x1 + 2, // x0
        content_x2,     // x1
        content_y1 + 1, // y0
        content_y2 - 1, // y1
        0.0f,           // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw filled indicator
    ///////////////////////////////

    int fill_x2 = content_x1 + int(_indicator_pos);
    if (fill_x2 > content_x1 + 2) {
      tgt->PushModColor(_fill_color);
      primi->RenderQuadAtZ(
          defmtl.get(),
          content_x1 + 2, // x0
          fill_x2,        // x1
          content_y1 + 3, // y0
          content_y2 - 3, // y1
          0.0f,           // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();
    }

    ///////////////////////////////
    // draw value text
    ///////////////////////////////

    ork::lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(_fg_color);

    std::string display_text = _text_editing ? _edit_buffer + "_" : _value_str;
    if(display_text.empty())
      display_text = "0";
    int text_x = content_x1 + int(_text_pos);
    lev2::FontMan::beginTextBlock(tgt, display_text.length());
    lev2::FontMan::DrawText(
        tgt, //
        text_x,
        iyc - 6,
        display_text.c_str());
    lev2::FontMan::endTextBlock(tgt);

    tgt->PopModColor();
    ork::lev2::FontMan::PopFont();

    ///////////////////////////////
    // draw label
    ///////////////////////////////

    _drawLabel(drwev);
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
