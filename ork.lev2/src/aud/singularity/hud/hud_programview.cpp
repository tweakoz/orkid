////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/util/triple_buffer.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/ezapp.h> // todo move updatedata_ptr_t out..

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProgramView2(
    uilayoutgroup_ptr_t vp, //
    std::string named) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto programview = std::make_shared<ProgramView>();
  auto uipanelitem = vp->makeChild<ui::Panel>("progview", 0, 0, 32, 32);
  hudpanel->_uipanel                = uipanelitem.typedWidget();
  hudpanel->_panelLayout            = uipanelitem._layout;
  hudpanel->_uipanel->_closeEnabled = false;
  hudpanel->_uipanel->_moveEnabled  = false;
  hudpanel->_uipanel->setTitle(named);
  hudpanel->_uisurface = programview;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->_stdcolor   = fvec4(0.2, 0.2, 0.3f, 0.5f);
  hudpanel->_uipanel->_focuscolor = fvec4(0.3, 0.2, 0.4f, 0.5f);
  hudpanel->_layoutitem = uipanelitem.as_shared();
  ///////////////////////////////////////////////////////////////////////
  vp->addChild(hudpanel->_uipanel);
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProgramView(
    uilayoutgroup_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named) {
    auto rval = createProgramView2(vp,named);
    rval->_layoutitem->applyBounds(bounds);
    return rval;
}
///////////////////////////////////////////////////////////////////////////////
ProgramView::ProgramView() //
    : ui::Surface("ProgramView", 0, 0, 32, 32, fvec3(), 1.0) {

  auto on_key = [this](
                    int note, //
                    int velocity,
                    programInst* pinst) { //
    _curprogram = pinst->_progdata;
    this->SetDirty();
  };
  synth::instance()->_onkey_subscribers.push_back(on_key);
}
///////////////////////////////////////////////////////////////////////////////
void ProgramView::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto syn     = synth::instance();
  auto vp      = syn->_hudvp;
  double time  = syn->_timeaccum;

  _rtgroup->_clearColor = _clearColor;
  fbi->rtGroupClear(_rtgroup.get());

  if (_curprogram) {
    auto name   = _curprogram->_name;
    int ycursor = 0;

    drawtext(
        this,
        context, //
        FormatString("Program: %s", name.c_str()),
        0,
        ycursor,
        fontscale,
        1,
        1,
        0);
    ycursor += hud_lineheight();

    for (auto l : _curprogram->_hudinfos) {
      drawtext(
          this,
          context, //
          FormatString("%s", l.c_str()),
          0,
          ycursor,
          fontscale,
          1,
          1,
          0);
      ycursor += hud_lineheight();
    }
  }

  // drawHudLines(this, context, lines);
}
///////////////////////////////////////////////////////////////////////////////
void ProgramView::_doGpuInit(lev2::Context* pt) {
  Surface::_doGpuInit(pt);
  _pickbuffer = new lev2::PickBuffer(this, pt, width(), height());
  _ctxbase    = pt->GetCtxBase();
}
///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult ProgramView::DoOnUiEvent(ui::event_constptr_t ev) {
  ui::HandlerResult ret;
  bool isalt  = ev->mbALT;
  bool isctrl = ev->mbCTRL;
  switch (ev->_eventcode) {
    case ui::EventCode::KEY_DOWN:
      break;
    case ui::EventCode::KEY_UP:
      break;
    default:
      break;
  }
  return ret;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
