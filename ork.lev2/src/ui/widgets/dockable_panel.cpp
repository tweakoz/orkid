////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/dockable_panel.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////

DockablePanel::DockablePanel(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h)
    , _child(nullptr) {
  _font = lev2::FontMan::fontForId("i13");
}

/////////////////////////////////////////////////////////////////////////

DockablePanel::~DockablePanel() {
}

/////////////////////////////////////////////////////////////////////////

void DockablePanel::setChild(widget_ptr_t w) {
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

void DockablePanel::DoLayout() {
  if (_child) {
    // Child gets all space below titlebar
    int child_y = _titlebar_height;
    int child_h = _geometry._h - _titlebar_height;
    if (child_h < 0) child_h = 0;
    _child->SetRect(0, child_y, _geometry._w, child_h);
  }
}

/////////////////////////////////////////////////////////////////////////

void DockablePanel::DoDraw(ui::drawevent_constptr_t drwev) {
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

    // Draw title text (child's name)
    if (_child) {
      const auto& title = _child->GetName();
      if (!title.empty()) {
        tgt->PushModColor(_title_color);
        lev2::FontMan::PushFont(_font);
        int font_height = lev2::FontMan::currentFont()->charHeight();
        int text_y = iyr + (_titlebar_height - font_height) / 2;
        lev2::FontMan::beginTextBlock(tgt);
        lev2::FontMan::DrawText(tgt, ixr + 8, text_y, title.c_str());
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

Widget* DockablePanel::doRouteUiEvent(event_constptr_t ev) {
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

HandlerResult DockablePanel::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult ret(this);

  // For now, just basic handling - future: drag titlebar to undock
  int lx, ly;
  RootToLocal(ev->miX, ev->miY, lx, ly);

  // Check if event is in titlebar area
  bool in_titlebar = (ly >= 0 && ly < _titlebar_height);

  // Future: handle titlebar drag for undocking
  (void)in_titlebar;

  return ret;
}

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
