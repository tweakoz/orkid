#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/util/triple_buffer.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
struct ProfilerView final : public ui::Surface {
  ProfilerView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  int _updatecount             = 0;
  SynthProfilerFrame _curprofframe;
};
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProfilerView(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto programview = std::make_shared<ProfilerView>();
  auto uipanelitem = vp->makeChild<ui::Panel>("profiler", 0, 0, 32, 32);
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
ProfilerView::ProfilerView() //
    : ui::Surface("ProfilerView", 0, 0, 32, 32, fvec3(), 1.0) {

  auto on_prof = [this](const SynthProfilerFrame& profframe) { //
    _curprofframe = profframe;
    this->SetDirty();
  };
  synth::instance()->_onprofilerframe = on_prof;
}
///////////////////////////////////////////////////////////////////////////////
void ProfilerView::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto syn     = synth::instance();
  auto vp      = syn->_hudvp;
  double time  = syn->_timeaccum;

  mRtGroup->_clearColor = _clearColor;
  fbi->rtGroupClear(mRtGroup);

  // auto name   = _curprogram->_name;
  int ycursor = 0;

  drawtext(
      this,
      context, //
      FormatString("SampleRate: %g hz", _curprofframe._samplerate),
      0,
      ycursor,
      fontscale,
      1,
      1,
      0);
  ycursor += hud_lineheight();
  drawtext(
      this,
      context, //
      FormatString("ControlRate: %g hz", _curprofframe._controlrate),
      0,
      ycursor,
      fontscale,
      1,
      1,
      0);
  ycursor += hud_lineheight();
  drawtext(
      this,
      context, //
      FormatString("CpuLoad: %0.2f %%", _curprofframe._cpuload * 100.0f),
      0,
      ycursor,
      fontscale,
      1,
      1,
      0);
  ycursor += hud_lineheight();
  drawtext(
      this,
      context, //
      FormatString("ActiveVoices: %d", _curprofframe._numlayers),
      0,
      ycursor,
      fontscale,
      1,
      1,
      0);
  ycursor += hud_lineheight();
  drawtext(
      this,
      context, //
      FormatString("ActiveDspBlocks: %d", _curprofframe._numdspblocks),
      0,
      ycursor,
      fontscale,
      1,
      1,
      0);
  ycursor += hud_lineheight();

  for (auto b : syn->_outputBusses) {
    auto busname = b.first;
    auto fxname  = b.second->_fxname;
    drawtext(
        this,
        context, //
        FormatString("OutputBus<%s>: fx<%s>", busname.c_str(), fxname.c_str()),
        0,
        ycursor,
        fontscale,
        1,
        1,
        0);
    ycursor += hud_lineheight();
  }

  /*for (auto l : _curprogram->_hudinfos) {
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
  }*/

  // drawHudLines(this, context, lines);
}
///////////////////////////////////////////////////////////////////////////////
void ProfilerView::DoInit(lev2::Context* pt) {
  _pickbuffer = new lev2::PickBuffer(this, pt, width(), height());
  _ctxbase    = pt->GetCtxBase();
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult ProfilerView::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);
  return ret;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
