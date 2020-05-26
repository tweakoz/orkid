#pragma once

#include <ork/lev2/ui/coord.h>
#include <ork/lev2/ui/touch.h>
#include <ork/lev2/ui/ui.h>
#include <ork/kernel/fixedstring.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////////////////
struct UpdateData {
  double _dt      = 0.0;
  double _abstime = 0.0;
};
using updatedata_ptr_t = std::shared_ptr<UpdateData>;
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
};

struct EventCooked {
  int miEventCode;
  int miKeyCode;
  int miX;
  int miY;
  int mLastX;
  int mLastY;
  float mUnitX;
  float mUnitY;
  float mLastUnitX;
  float mLastUnitY;
  bool mBut0;
  bool mBut1;
  bool mBut2;
  bool mCTRL;
  bool mALT;
  bool mSHIFT;
  bool mMETA;
  ork::FixedString<64> mAction;

  void Reset();
};

struct Event final // RawEvent
{
  int mEventCode;

  lev2::OffscreenBuffer* mpGfxWin;
  Coordinate mUICoord;

  int miEventCode;
  int miX;
  int miY;
  int miRawX;
  int miRawY;
  int miLastX;
  int miLastY;
  int miMWY;
  int miState;
  int miKeyCode;
  int miNumHits;

  f32 mfX;
  f32 mfY;
  f32 mfUnitX;
  f32 mfUnitY;
  f32 mfLastUnitX;
  f32 mfLastUnitY;
  f32 mfPressure;

  bool mbCTRL;
  bool mbALT;
  bool mbSHIFT;
  bool mbMETA;

  bool mbLeftButton;
  bool mbMiddleButton;
  bool mbRightButton;

  fvec4 mvRayN;
  fvec4 mvRayF;

  void* mpBlindEventData;

  mutable EventCooked mFilteredEvent;
  fvec2 _vpdim;

  static const int kmaxmtpoints = 4;

  MultiTouchPoint mMultiTouchPoints[kmaxmtpoints];
  int miNumMultiTouchPoints;

  bool isKeyDepressed(uint32_t key) const;
  lev2::Context* _context = nullptr;

  fvec2 GetUnitCoordBP() const {
    fvec2 rval;
    rval.SetX(2.0f * mfUnitX - 1.0f);
    rval.SetY(-(2.0f * mfUnitY - 1.0f));
    return rval;
  }

  Event()
      : mUICoord()
      , miEventCode(0)
      , miX(0)
      , miY(0)
      , mfUnitX(0.0f)
      , mfUnitY(0.0f)
      , mfLastUnitX(0.0f)
      , mfLastUnitY(0.0f)
      , miLastX(0)
      , miLastY(0)
      , miState(0)
      , miKeyCode(0)
      , miNumHits(0)
      , mbCTRL(false)
      , mbALT(false)
      , mbSHIFT(false)
      , mbMETA(false)
      , mbLeftButton(false)
      , mbMiddleButton(false)
      , mbRightButton(false)
      , mvRayN(float(0.0f), float(0.0f), float(0.0f), float(0.0f))
      , mvRayF(float(0.0f), float(0.0f), float(0.0f), float(0.0f))
      , mpBlindEventData(0)
      , mpGfxWin(0)
      , miNumMultiTouchPoints(0) {
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
};

using event_ptr_t      = std::shared_ptr<Event>;
using event_constptr_t = std::shared_ptr<const Event>;

///////////////////////////////////////////////////////////////////////////////

struct DrawEvent {
  DrawEvent(lev2::Context* ptarg)
      : mpTarget(ptarg) {
  }
  lev2::Context* GetTarget() const {
    return mpTarget;
  }

  lev2::Context* mpTarget;
};

using drawevent_constptr_t = std::shared_ptr<const DrawEvent>;

}} // namespace ork::ui
