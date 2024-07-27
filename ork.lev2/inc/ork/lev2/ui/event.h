////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/coord.h>
#include <ork/lev2/ui/touch.h>
#include <ork/lev2/ui/ui.h>
#include <ork/kernel/fixedstring.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/event/Event.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////////////////

struct HandlerResult {
  HandlerResult(Widget* ph = nullptr);

  bool wasHandled() const {
    return mHandler != nullptr;
  }
  void setHandled(Widget* by) {
    mHandler = by;
  }

  Widget* mHandler;
  bool mHoldFocus;
  bool _widget_finished = false;
};

struct EventCooked {
  EventCode _eventcode = EventCode::UNKNOWN;
  int miKeyCode        = -1;
  int miX              = 0;
  int miY              = 0;
  int mLastX           = 0;
  int mLastY           = 0;
  float mUnitX         = 0.0f;
  float mUnitY         = 0.0f;
  float mLastUnitX     = 0.0f;
  float mLastUnitY     = 0.0f;
  bool mBut0           = false;
  bool mBut1           = false;
  bool mBut2           = false;
  bool mCTRL           = false;
  bool mALT            = false;
  bool mSHIFT          = false;
  bool mMETA           = false;
  bool mSUPER          = false;
  ork::FixedString<64> mAction;

  void Reset();
};

struct Event final // RawEvent
{
  lev2::DisplayBuffer* mpGfxWin = nullptr;
  Coordinate mUICoord;

  Context* _uicontext  = nullptr;
  EventCode _eventcode = EventCode::UNKNOWN;
  int miX              = 0;
  int miY              = 0;
  int miRawX           = 0;
  int miRawY           = 0;
  int miLastX          = 0;
  int miLastY          = 0;
  int miMWY            = 0;
  int miMWX            = 0;
  int miState          = 0;
  int miKeyCode        = 0;
  int miNumHits        = 0;
  int miScreenPosX     = 0;
  int miScreenPosY     = 0;
  int miScreenWidth    = 0;
  int miScreenHeight   = 0;
  int _midiController  = 0;
  int _midiValue       = 0;
  f32 mfX         = 0.0f;
  f32 mfY         = 0.0f;
  f32 mfUnitX     = 0.0f;
  f32 mfUnitY     = 0.0f;
  f32 mfLastUnitX = 0.0f;
  f32 mfLastUnitY = 0.0f;
  f32 mfPressure  = 0.0f;

  bool mbCTRL  = false;
  bool mbALT   = false;
  bool mbSHIFT = false;
  bool mbSUPER  = false;

  bool mbLeftButton   = false;
  bool mbMiddleButton = false;
  bool mbRightButton  = false;

  fvec4 mvRayN;
  fvec4 mvRayF;

  void* mpBlindEventData = nullptr;

  mutable EventCooked mFilteredEvent;
  fvec2 _vpdim;

  static const int kmaxmtpoints = 4;
  std::string _paste_text;

  MultiTouchPoint mMultiTouchPoints[kmaxmtpoints];
  int miNumMultiTouchPoints = 0;

  bool isKeyDepressed(uint32_t key) const;
  lev2::Context* _context = nullptr;

  fvec2 GetUnitCoordBP() const {
    fvec2 rval;
    rval.x = (2.0f * mfUnitX - 1.0f);
    rval.y = (-(2.0f * mfUnitY - 1.0f));
    return rval;
  }

  fvec2 xfToVpUnitCoord(fvec2 inp) const {
    fvec2 rval;
    rval.x = float(inp.x) / _vpdim.x - 0.5f;
    rval.y = float(inp.y) / _vpdim.y - 0.5f;
    return rval;
  }

  void setvpDim(Widget* w);
  Event()
      : mvRayN(float(0.0f), float(0.0f), float(0.0f), float(0.0f))
      , mvRayF(float(0.0f), float(0.0f), float(0.0f), float(0.0f)) {
  }

  void GetFromOS(void);

  bool IsButton0DownF() const {
    return mbLeftButton || mFilteredEvent.mBut0;
  }
  bool IsButton1DownF() const {
    return mbMiddleButton || mFilteredEvent.mBut1;
  }
  bool IsButton2DownF() const {
    return mbRightButton || mFilteredEvent.mBut2;
  }

  static HandlerResult sendToContext(event_constptr_t ev);

  std::string description() const;
};

///////////////////////////////////////////////////////////////////////////////

using event_lambda_t = std::function<HandlerResult(event_constptr_t)>;

///////////////////////////////////////////////////////////////////////////////

struct DrawEvent {
  DrawEvent(lev2::Context* ptarg)
      : _target(ptarg) {
  }
  lev2::Context* GetTarget() const {
    return _target;
  }

  lev2::Context* _target;
  svar64_t _userData;
  lev2::acqdrawbuffer_constptr_t _acqdbuf;
};

using drawevent_constptr_t = std::shared_ptr<const DrawEvent>;

}} // namespace ork::ui
