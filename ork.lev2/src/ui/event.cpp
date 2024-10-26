#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/context.h>
#include <ork/profiling.inl>

namespace ork::ui {
HandlerResult Event::sendToContext(event_constptr_t ev) {
  if (ev->_uicontext)
    return ev->_uicontext->handleEvent(ev);
  else
    return HandlerResult();
}
void Event::setvpDim(Widget* w) {
  _vpdim = fvec2(w->width(), w->height());
  if (lev2::_HIDPI()) {
    _vpdim *= 0.5;
  }
}

///////////////////////////////////////////////////////////
void EventCooked::Reset() {
  _eventcode = EventCode::UNKNOWN;
  miKeyCode  = 0;

  miX        = 0;
  miY        = 0;
  mLastX     = 0;
  mLastY     = 0;
  mUnitX     = 0.0f;
  mUnitY     = 0.0f;
  mLastUnitX = 0.0f;
  mLastUnitY = 0.0f;

  mBut0   = false;
  mBut1   = false;
  mBut2   = false;
  mCTRL   = false;
  mALT    = false;
  mSHIFT  = false;
  mMETA   = false;
  mAction = "";
}
///////////////////////////////////////////////////////////
IWidgetEventFilter::IWidgetEventFilter(Widget& w)
    : mWidget(w) {
  mKeyTimer.Start();
  mDoubleTimer.Start();
  mMoveTimer.Start();
}
///////////////////////////////////////////////////////////
void IWidgetEventFilter::Filter(event_constptr_t Ev) {
  auto& fev      = Ev->mFilteredEvent;
  fev._eventcode = Ev->_eventcode;
  fev.mBut0      = Ev->mbLeftButton;
  fev.mBut1      = Ev->mbMiddleButton;
  fev.mBut2      = Ev->mbRightButton;

  fev.miX        = Ev->miX;
  fev.miY        = Ev->miY;
  fev.mLastX     = Ev->miLastX;
  fev.mLastY     = Ev->miLastY;
  fev.mUnitX     = Ev->mfUnitX;
  fev.mUnitY     = Ev->mfUnitX;
  fev.mLastUnitX = Ev->mfLastUnitX;
  fev.mLastUnitY = Ev->mfLastUnitY;

  fev.miKeyCode = Ev->miKeyCode;
  fev.mCTRL     = Ev->mbCTRL;
  fev.mALT      = Ev->mbALT;
  fev.mSHIFT    = Ev->mbSHIFT;
  fev.mSUPER    = Ev->mbSUPER;

  DoFilter(Ev);
}

///////////////////////////////////////////////////////////

std::string Event::description() const{

  auto as_str = FormatString(
      "Event<0x%zx> x<%d> y<%d> key<%d> ctrl<%d> alt<%d> shift<%d> meta<%d>",
      uint64_t(_eventcode),
      miX,
      miY,
      miKeyCode,
      mbCTRL,
      mbALT,
      mbSHIFT,
      mbSUPER);
    return as_str;
}

///////////////////////////////////////////////////////////
} // namespace ork::ui
