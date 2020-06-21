#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/util/triple_buffer.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
struct ProgramView final : public ui::Surface {
  ProgramView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  int _updatecount             = 0;
  prgdata_constptr_t _curprogram;
  int _octaveshift = 0;
  int _velocity    = 127;
};
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProgramView(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto programview = std::make_shared<ProgramView>();
  auto uipanelitem = vp->makeChild<ui::Panel>("progview", 0, 0, 32, 32);
  uipanelitem.applyBounds(bounds);
  hudpanel->_uipanel                = uipanelitem._widget;
  hudpanel->_panelLayout            = uipanelitem._layout;
  hudpanel->_uipanel->_closeEnabled = false;
  hudpanel->_uipanel->_moveEnabled  = false;
  hudpanel->_uipanel->setTitle(named);
  hudpanel->_uisurface = programview;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->_stdcolor   = fvec4(0.2, 0.2, 0.3f, 0.5f);
  hudpanel->_uipanel->_focuscolor = fvec4(0.3, 0.2, 0.4f, 0.5f);
  ///////////////////////////////////////////////////////////////////////
  vp->addChild(hudpanel->_uipanel);
  vp->_hudpanels.insert(hudpanel);
  return hudpanel;
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

  mRtGroup->_clearColor = _clearColor;
  fbi->rtGroupClear(mRtGroup);

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
void ProgramView::DoInit(lev2::Context* pt) {
  _pickbuffer = new lev2::PickBuffer(this, pt, width(), height());
  _ctxbase    = pt->GetCtxBase();
}
///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult ProgramView::DoOnUiEvent(ui::event_constptr_t ev) {
  ui::HandlerResult ret;
  bool isalt  = ev->mbALT;
  bool isctrl = ev->mbCTRL;
  switch (ev->_eventcode) {
    case ui::EventCode::KEY:
      break;
    case ui::EventCode::KEYUP: {
    } break;
  }
  return ret;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
