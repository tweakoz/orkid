////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/border_frame.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////

BorderFrame::BorderFrame(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h)
    , _child(nullptr) {
}

/////////////////////////////////////////////////////////////////////////

BorderFrame::~BorderFrame() {
}

/////////////////////////////////////////////////////////////////////////

void BorderFrame::setChild(widget_ptr_t w) {
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

void BorderFrame::DoLayout() {
  if (_child) {
    int bw = _border_width;
    int child_w = _geometry._w - 2 * bw;
    int child_h = _geometry._h - 2 * bw;
    if (child_w < 0) child_w = 0;
    if (child_h < 0) child_h = 0;
    _child->SetRect(bw, bw, child_w, child_h);
  }
}

/////////////////////////////////////////////////////////////////////////

void BorderFrame::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ixr, iyr;
  LocalToRoot(0, 0, ixr, iyr);

  int bw = _border_width;
  int ew = _border_edge_width;
  int W = _geometry._w;
  int H = _geometry._h;

  // Helper lambda: draw 4 quads forming a rectangular frame
  auto drawFrame = [&](int x0, int y0, int x1, int y1, int thickness) {
    // Top
    primi->RenderQuadAtZ(defmtl.get(),
        x0, x1, y0, y0 + thickness,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    // Bottom
    primi->RenderQuadAtZ(defmtl.get(),
        x0, x1, y1 - thickness, y1,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    // Left
    primi->RenderQuadAtZ(defmtl.get(),
        x0, x0 + thickness, y0 + thickness, y1 - thickness,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    // Right
    primi->RenderQuadAtZ(defmtl.get(),
        x1 - thickness, x1, y0 + thickness, y1 - thickness,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  };

  mtxi->PushUIMatrix();
  {
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);

    // 1. Outer edge
    if (ew > 0) {
      tgt->PushModColor(_border_outer_color);
      drawFrame(ixr, iyr, ixr + W, iyr + H, ew);
      tgt->PopModColor();
    }

    // 2. Body fill
    int body = bw - 2 * ew;
    if (body > 0) {
      tgt->PushModColor(_border_color);
      drawFrame(ixr + ew, iyr + ew, ixr + W - ew, iyr + H - ew, body);
      tgt->PopModColor();
    }

    // 3. Inner edge
    if (ew > 0) {
      tgt->PushModColor(_border_inner_color);
      drawFrame(ixr + bw - ew, iyr + bw - ew,
                ixr + W - bw + ew, iyr + H - bw + ew, ew);
      tgt->PopModColor();
    }
  }
  mtxi->PopUIMatrix();

  // Draw child
  if (_child) {
    _child->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* BorderFrame::doRouteUiEvent(event_constptr_t ev) {
  if (_child && _child->IsEventInside(ev)) {
    Widget* target = _child->routeUiEvent(ev);
    if (target) return target;
  }
  if (IsEventInside(ev)) {
    return this;
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

HandlerResult BorderFrame::DoOnUiEvent(event_constptr_t ev) {
  // Route: find deepest child under mouse via Group's routing
  Widget* target = Group::doRouteUiEvent(ev);
  if (target && target != this) {
    return target->OnUiEvent(ev);
  }
  return HandlerResult(this);
}

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
