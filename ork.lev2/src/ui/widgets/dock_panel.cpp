////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/dock_panel.h>
#include <ork/lev2/ui/dock_space.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////

DockPanel::DockPanel(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h)
    , _child(nullptr) {
  _font = lev2::FontMan::fontForId("i13");
}

/////////////////////////////////////////////////////////////////////////

DockPanel::~DockPanel() {
}

/////////////////////////////////////////////////////////////////////////

void DockPanel::setChild(widget_ptr_t w) {
  // Remove old child if exists
  if (_child) {
    removeChild(_child);
  }
  _child = w;
  if (_child) {
    addChild(_child);
    DoLayout();
  }
}

/////////////////////////////////////////////////////////////////////////

void DockPanel::DoLayout() {
  if (_child) {
    // Child gets all space below titlebar
    int child_y = _titlebar_height;
    int child_h = _geometry._h - _titlebar_height;
    if (child_h < 0) child_h = 0;
    _child->SetRect(0, child_y, _geometry._w, child_h);
  }
}

/////////////////////////////////////////////////////////////////////////

void DockPanel::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ixr, iyr;
  LocalToRoot(0, 0, ixr, iyr);

  mtxi->PushUIMatrix();
  {
    // Draw titlebar background
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    tgt->PushModColor(_titlebar_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ixr, ixr + _geometry._w,
        iyr, iyr + _titlebar_height,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();

    // Draw border line at bottom of titlebar
    tgt->PushModColor(_border_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ixr, ixr + _geometry._w,
        iyr + _titlebar_height - 1, iyr + _titlebar_height,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();

    // Draw title text
    {
      std::string title = _title_override.empty()
        ? (_child ? _child->GetName() : _name)
        : _title_override;
      if (!title.empty()) {
        tgt->PushModColor(_title_color);
        lev2::FontMan::PushFont(_font);
        auto font = lev2::FontMan::currentFont();
        int font_height = font->charHeight();
        int text_y = iyr + (_titlebar_height - font_height) / 2;
        int text_x;
        if (_title_center) {
          int text_w = font->stringWidth(title.length());
          text_x = ixr + (_geometry._w - text_w) / 2;
        } else {
          text_x = ixr + 8;
        }
        lev2::FontMan::beginTextBlock(tgt);
        lev2::FontMan::DrawText(tgt, text_x, text_y, title.c_str());
        lev2::FontMan::endTextBlock(tgt);
        lev2::FontMan::PopFont();
        tgt->PopModColor();
      }
    }
  }
  mtxi->PopUIMatrix();

  // Draw child
  if (_child) {
    _child->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* DockPanel::doRouteUiEvent(event_constptr_t ev) {
  Widget* target = nullptr;

  // Check if event is in child area (below titlebar)
  if (_child) {
    int lx, ly;
    RootToLocal(ev->miX, ev->miY, lx, ly);

    if (ly >= _titlebar_height && _child->IsEventInside(ev)) {
      target = _child->routeUiEvent(ev);
    }
  }

  // If not handled by child, check if in our bounds
  if (!target && IsEventInside(ev)) {
    target = this;
  }

  return target;
}

/////////////////////////////////////////////////////////////////////////

DockSpace* DockPanel::_findDockSpace() {
  Group* p = parent();
  while (p) {
    if (auto ds = dynamic_cast<DockSpace*>(p))
      return ds;
    p = p->parent();
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

dockpanel_ptr_t DockPanel::_selfPtr() {
  if (auto par = parent())
    return std::dynamic_pointer_cast<DockPanel>(par->findChildPtr(this));
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

HandlerResult DockPanel::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult ret(this);

  int lx, ly;
  RootToLocal(ev->miX, ev->miY, lx, ly);
  bool in_titlebar = (ly >= 0 && ly < _titlebar_height);

  // Titlebar = drag source. The drag rides Context::_evdragtarget: a titlebar
  // PUSH makes this panel the push/drag target, then BEGIN/DRAG/END_DRAG are
  // synthesized to it. We forward the session to the owning DockSpace.
  switch (ev->_eventcode) {
    case EventCode::PUSH:
      _push_on_titlebar = in_titlebar;
      break;
    case EventCode::BEGIN_DRAG: {
      if (_push_on_titlebar) {
        _drag_owner = _findDockSpace();
        auto self   = _selfPtr();
        if (_drag_owner && self)
          _drag_owner->beginPanelDrag(self);
        else
          _drag_owner = nullptr;
      }
      break;
    }
    case EventCode::DRAG:
      if (_drag_owner)
        _drag_owner->updatePanelDrag(ev->miX, ev->miY);
      break;
    case EventCode::END_DRAG:
      if (_drag_owner) {
        _drag_owner->endPanelDrag(ev->miX, ev->miY, ev->_dragCanceled);
        _drag_owner = nullptr;
      }
      _push_on_titlebar = false;
      break;
    default:
      break;
  }

  return ret;
}

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
