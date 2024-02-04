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
struct ProfilerView final : public ui::Surface {
  ProfilerView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void _doGpuInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  int _updatecount             = 0;
  SynthProfilerFrame _curprofframe;
  std::string _timestampLine = "---";
};
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProfilerView2(
    uilayoutgroup_ptr_t vp, //
    std::string named) {

  auto syn = synth::instance();
  auto hudpanel    = std::make_shared<HudPanel>();
  auto prof_view = std::make_shared<ProfilerView>();
  auto pnl = vp->makeChild<ui::Panel>("profiler", 0, 0, 32, 32);
  hudpanel->_uipanel                = pnl.typedWidget();
  hudpanel->_panelLayout            = pnl._layout;
  hudpanel->_uipanel->_closeEnabled = false;
  hudpanel->_uipanel->_moveEnabled  = false;
  hudpanel->_uipanel->setTitle(named);
  hudpanel->_uisurface = prof_view;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->_stdcolor   = fvec4(0.2, 0.2, 0.3f, 0.5f);
  hudpanel->_uipanel->_focuscolor = fvec4(0.3, 0.2, 0.4f, 0.5f);
  hudpanel->_layoutitem = pnl.as_shared();
  ///////////////////////////////////////////////////////////////////////
  vp->addChild(hudpanel->_uipanel);
  ///////////////////////////////////////////////////////////////////////
  auto tshandler = std::make_shared<HudEventSink>();
  tshandler->_onEvent = [prof_view](hudevent_ptr_t event) {
    auto ts = event->_eventData.getShared<TimeStamp>();
    int M = ts->_measures+1;
    int B = ts->_beats+1;
    int C = ts->_clocks;
    auto tsstr = FormatString("[ M%02d : B%02d ]", M, B);
    prof_view->_timestampLine = tsstr;
  };
  syn->registerSinkForHudEvent(
    "seq.playback.timestamp.click"_crcu,
    tshandler
  );
  ///////////////////////////////////////////////////////////////////////
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createProfilerView(
    uilayoutgroup_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named) {
    auto rval = createProfilerView2(vp,named);
    rval->_layoutitem->applyBounds(bounds);
    return rval;
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

  _rtgroup->_clearColor = _clearColor;
  fbi->rtGroupClear(_rtgroup.get());

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
      FormatString("SystemTempo: %g bpm", syn->_system_tempo),
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
      FormatString("SeqTime: %s", _timestampLine.c_str()),
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

  if (syn->_globalprog)
    drawtext(
        this,
        context, //
        FormatString("CurrentProgram: %s", syn->_globalprog->_name.c_str()),
        0,
        ycursor,
        fontscale,
        1,
        1,
        0);
  ycursor += hud_lineheight();

  auto draw4cols = [&]( const std::string stra, //
                        const std::string strb, // 
                        const std::string strc, // 
                        const std::string strd, // 
                        float r, //
                        float g, //
                        float b) { //
    int w = width()/8;
    int w1 = w/4;
    int w3 = w/5;
    int w4 = w/4;
    int w2 = w-w1-w3-w4;
    auto str1 = FormatString("%-*s", w1, stra.c_str());
    auto str2 = FormatString("%-*s", w2, strb.c_str());
    auto str3 = FormatString("%-*s", w3, strc.c_str());
    auto str4 = FormatString("%*s", w4, strd.c_str());
    drawtext(
        this,
        context, //
        FormatString("%s%s%s%s", str1.c_str(), str2.c_str(), str3.c_str(),str4.c_str()),
        0,
        ycursor,
        fontscale,
        r,
        g,
        b);
    ycursor += hud_lineheight();
  };


  draw4cols("OutputBus","Program","Gain","FX",1,1,1);

  hudlines_t lines;
  float line_y = ycursor+2;
  lines.push_back(
    HudLine{ fvec2(0, line_y), 
             fvec2(width(), line_y), 
             fvec3(1, 1, 1)
            });


    drawHudLines(this,context,lines);

  ycursor += hud_lineheight()-4;

  for (auto item : syn->_outputBusses) {
    auto busname = item.first;
    auto bus = item.second;
    auto fxname  = item.second->_fxname;
    float gain = bus->_prog_gain;
    float r = 1;
    float g = 1;
    float b = 1;
    if(bus==syn->_curprogrambus){
      r = 1;
      g = 0;
      b = 0;
    } 

    auto prgname = bus->_uiprogram ? bus->_uiprogram->_name : "----";

    auto dbstr = FormatString("%gdB", gain);

    draw4cols(busname,prgname,dbstr.c_str(),fxname,r,g,b);
  }

  // drawHudLines(this, context, lines);
}
///////////////////////////////////////////////////////////////////////////////
void ProfilerView::_doGpuInit(lev2::Context* pt) {
  Surface::_doGpuInit(pt);
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
