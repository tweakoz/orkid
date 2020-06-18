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

const std::map<char, int> __notemap = {
    {'z', 36},
    {'s', 37},
    {'x', 38},
    {'d', 39},
    {'c', 40},
    {'v', 41},
    {'g', 42},
    {'b', 43},
    {'h', 44},
    {'n', 45},
    {'j', 46},
    {'m', 47},
    {',', 48},
    {'l', 49},
    {'.', 50},
    {';', 51},
    {'/', 52},
};
std::map<int, programInst*> _activenotes;
///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult ProgramView::DoOnUiEvent(ui::event_constptr_t ev) {
  ui::HandlerResult ret(this);
  bool isalt  = ev->mbALT;
  bool isctrl = ev->mbCTRL;

  switch (ev->_eventcode) {
    case ui::EventCode::KEY:
      switch (ev->miKeyCode) {
        case '5': {
          synth::instance()->nextEffect();
          break;
        }
        case '9': {
          _velocity += 16;
          _velocity = std::clamp(_velocity, 0, 127);
          break;
        }
        case '7': {
          _velocity -= 16;
          _velocity = std::clamp(_velocity, 0, 127);
          break;
        }
        case '6': {
          synth::instance()->nextProgram();
          break;
        }
        case '4': {
          synth::instance()->prevProgram();
          break;
        }
        case '3': {
          _octaveshift++;
          _octaveshift = std::clamp(_octaveshift, -1, 4);
          break;
        }
        case '1': {
          _octaveshift--;
          _octaveshift = std::clamp(_octaveshift, -1, 4);
          break;
        }
        case ' ': {
          for (auto item : _activenotes) {
            auto pi = item.second;
            synth::instance()->liveKeyOff(pi);
          }
          _activenotes.clear();
          break;
        }
        default: {
          auto prog = synth::instance()->_globalprog;
          auto it   = __notemap.find(ev->miKeyCode);
          if (it != __notemap.end()) {
            int note = it->second;
            note += (_octaveshift * 12);
            auto pi            = synth::instance()->liveKeyOn(note, _velocity, prog);
            _activenotes[note] = pi;
          }
          break;
        }
      }
      break;
    case ui::EventCode::KEYUP: {
      switch (ev->miKeyCode) {
        default: {
          auto it = __notemap.find(ev->miKeyCode);
          if (it != __notemap.end()) {
            int note = it->second;
            note += (_octaveshift * 12);
            auto it2 = _activenotes.find(note);
            if (it2 != _activenotes.end()) {
              auto pi = it2->second;
              synth::instance()->liveKeyOff(pi);
              _activenotes.erase(it2);
            }
          }
          break;
        }
      }
    } break;
  }
  return ret;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
