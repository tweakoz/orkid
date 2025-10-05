#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/combobox.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
ComboBox::ComboBox(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _bg_color(color) {
  _fg_color = fvec4(1, 1, 1, 1);
  _button_color = fvec4(0.4, 0.4, 0.5, 1);
  _draw_label = true;
  _selected_index = 0;
  _scroll_pos = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::addItem(const std::string& item) {
  _items.push_back(item);
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::setItems(const std::vector<std::string>& items) {
  _items = items;
  if (_selected_index >= _items.size()) {
    _selected_index = _items.empty() ? 0 : _items.size() - 1;
  }
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::setSelectedIndex(int idx) {
  if (idx >= 0 && idx < _items.size()) {
    _selected_index = idx;
    _scroll_pos = (_selected_index<<4);
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string ComboBox::selectedItem() const {
  if (_selected_index >= 0 && _selected_index < _items.size()) {
    return _items[_selected_index];
  }
  return "";
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::_incrementSelection() {
  if (_items.empty())
    return;

  _selected_index++;
  if (_selected_index >= _items.size())
    _selected_index = 0; // Wrap to start

  _scroll_pos = (_selected_index<<4);    

  if (_onSelectionChanged) {
    _onSelectionChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::_decrementSelection() {
  if (_items.empty())
    return;

  _selected_index--;
  if (_selected_index < 0)
    _selected_index = _items.size() - 1; // Wrap to end
 
  _scroll_pos = (_selected_index<<4);    

  if (_onSelectionChanged) {
    _onSelectionChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult ComboBox::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      // Activate widget on click
      _active = true;

      auto content = contentRect();
      int local_x = cev->miX - _geometry._x - content._x;

      // Left button (−) - decrement
      if (local_x < BUTTON_WIDTH) {
        _decrementSelection();
      }
      // Right button (+) - increment
      else if (local_x > content._w - BUTTON_WIDTH) {
        _incrementSelection();
      }

      rval.setHandled(this);
      break;
    }

    case EventCode::MOUSEWHEEL: {
      // Only respond to wheel if active
      if (_active) {
        _scroll_pos += cev->miMWY;
        // choose selection based on absolute scroll position
        if (_items.size()) {
          int new_index = ((_scroll_pos>>4) % _items.size() + _items.size()) % _items.size();
          if (new_index != _selected_index) {
            _selected_index = new_index;
            if (_onSelectionChanged) {
              _onSelectionChanged();
            }
          }
        }
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::KEY_DOWN: {
      // Arrow keys (event already inside widget via routing)
      // Qt key codes: Up=16777235, Down=16777237
      if (cev->miKeyCode == 16777235) { // Up Arrow
        _decrementSelection();
        rval.setHandled(this);
      } else if (cev->miKeyCode == 16777237) { // Down Arrow
        _incrementSelection();
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::MOUSE_ENTER: {
      _active = true;
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _active = false;
      break;
    }

    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ComboBox::DoDraw(drawevent_constptr_t drwev) {

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

    tgt->PushModColor(fvec4(_bg_color.xyz() * 0.7, 1));
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
    // draw dec button (−)
    ///////////////////////////////

    int btn_dec_x1 = content_x1 + 2;
    int btn_dec_x2 = btn_dec_x1 + BUTTON_WIDTH;

    tgt->PushModColor(_button_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        btn_dec_x1,     // x0
        btn_dec_x2,     // x1
        content_y1 + 2, // y0
        content_y2 - 2, // y1
        0.0f,           // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw inc button (+)
    ///////////////////////////////

    int btn_inc_x2 = content_x2 - 2;
    int btn_inc_x1 = btn_inc_x2 - BUTTON_WIDTH;

    tgt->PushModColor(_button_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        btn_inc_x1,     // x0
        btn_inc_x2,     // x1
        content_y1 + 2, // y0
        content_y2 - 2, // y1
        0.0f,           // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////
    // draw button symbols and text
    ///////////////////////////////

    ork::lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(_fg_color);

    // Dec button symbol (−)
    lev2::FontMan::beginTextBlock(tgt, 1);
    lev2::FontMan::DrawText(tgt, btn_dec_x1 + 6, iyc - 6, "-");
    lev2::FontMan::endTextBlock(tgt);

    // Inc button symbol (+)
    lev2::FontMan::beginTextBlock(tgt, 1);
    lev2::FontMan::DrawText(tgt, btn_inc_x1 + 5, iyc - 6, "+");
    lev2::FontMan::endTextBlock(tgt);

    // Selected item text (center)
    std::string text = selectedItem();
    if (!text.empty()) {
      int text_x = btn_dec_x2 + 8;
      lev2::FontMan::beginTextBlock(tgt, text.length());
      lev2::FontMan::DrawText(tgt, text_x, iyc - 6, text.c_str());
      lev2::FontMan::endTextBlock(tgt);
    }

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
