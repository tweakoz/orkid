////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::Viewport, "ui::Viewport");

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

void Viewport::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

Viewport::Viewport(const std::string& name, int x, int y, int w, int h, fcolor3 color, F32 depth)
    : Surface(name, x, y, w, h, color, depth) {
  // mWidgetFlags.Enable();
  // mWidgetFlags.SetState( EUI_WIDGET_OFF );
} // namespace ui

/////////////////////////////////////////////////////////////////////////

void Viewport::BeginFrame(lev2::Context* pTARG) {
  ork::lev2::FontMan::GetRef();
  pTARG->debugPushGroup("Viewport::BeginFrame");
  pTARG->beginFrame();
  // orkprintf( "BEG Viewport::BeginFrame::mbDrawOK\n" );
  auto MatOrtho = fmtx4::Identity();
  MatOrtho.ortho(0.0f, (F32)width(), 0.0f, (F32)height(), 0.0f, 1.0f);
  pTARG->MTXI()->SetOrthoMatrix(MatOrtho);
  ork::lev2::ViewportRect SciRect;
  SciRect._x = _geometry._x;
  SciRect._y = _geometry._y;
  SciRect._w = _geometry._w;
  SciRect._h = _geometry._h;
  pTARG->FBI()->pushScissor(SciRect);
  pTARG->MTXI()->PushPMatrix(pTARG->MTXI()->GetOrthoMatrix());

  pTARG->debugPopGroup();
}

/////////////////////////////////////////////////////////////////////////

void Viewport::EndFrame(lev2::Context* pTARG) {
  pTARG->debugPushGroup("Viewport::EndFrame");
  pTARG->MTXI()->PopPMatrix();
  pTARG->FBI()->popScissor();
  pTARG->endFrame();
  pTARG->debugPopGroup();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
