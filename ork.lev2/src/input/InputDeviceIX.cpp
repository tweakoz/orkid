////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/thread.h>
#include <ork/pch.h>

#include "InputDeviceIX.h"

#include <tuio/TuioClient.h>
#include <tuio/TuioListener.h>
#include <functional>
#include <ork/kernel/csystem.h>
#include <ork/lev2/qtui/qtui.h>

using namespace TUIO;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef std::function<void(TuioCursor*)> on_tuio_cur_t;

///////////////////////////////////////////////////////////////////////////////

struct TuioInputReader : public TuioListener {

  on_tuio_cur_t mOnDown;
  on_tuio_cur_t mOnUpdate;
  on_tuio_cur_t mOnUp;

  TuioInputReader() : mOnDown(nullptr), mOnUpdate(nullptr), mOnUp(nullptr) {}

  void addTuioObject(TuioObject* tobj) {}
  void updateTuioObject(TuioObject* tobj) {}
  void removeTuioObject(TuioObject* tobj) {}
  void refresh(TuioTime frameTime) {
    // std::cout << "refresh " << frameTime.getTotalMilliseconds() << std::endl;
  }

  void addTuioCursor(TuioCursor* tcur) {
    if (mOnDown)
      mOnDown(tcur);
  }

  void updateTuioCursor(TuioCursor* tcur) {
    if (mOnUpdate)
      mOnUpdate(tcur);
  }

  void removeTuioCursor(TuioCursor* tcur) {
    if (mOnUp)
      mOnUp(tcur);
  }
};

///////////////////////////////////////////////////////////////////////////////

static float lanay = 0.0f;
static float ranay = 0.0f;
int lpid = -1;
int rpid = -1;

InputDeviceIX::InputDeviceIX() : InputDevice() {

  printf("CREATED IX INPUTDEVICE\n");
  /*OldStlSchoolMapInsert( mInputMap, 'W', (int) ETRIG_RAW_JOY0_LDIG_UP );
  OldStlSchoolMapInsert( mInputMap, 'A', (int) ETRIG_RAW_JOY0_LDIG_LEFT );
  OldStlSchoolMapInsert( mInputMap, 'D', (int) ETRIG_RAW_KEY_RIGHT );
  OldStlSchoolMapInsert( mInputMap, 'S', (int) ETRIG_RAW_KEY_DOWN );
  OldStlSchoolMapInsert( mInputMap, ETRIG_RAW_KEY_LEFT, (int) ETRIG_RAW_KEY_LEFT );
  OldStlSchoolMapInsert( mInputMap, ETRIG_RAW_KEY_UP, (int) ETRIG_RAW_KEY_UP );
  OldStlSchoolMapInsert( mInputMap, ETRIG_RAW_KEY_RIGHT, (int) ETRIG_RAW_KEY_RIGHT );
  OldStlSchoolMapInsert( mInputMap, ETRIG_RAW_KEY_DOWN, (int) ETRIG_RAW_KEY_DOWN );*/

  _ixinputmap[ETRIG_RAW_JOY0_LANA_YAXIS]=ETRIG_RAW_JOY0_LANA_YAXIS;
  _ixinputmap[ETRIG_RAW_JOY0_RANA_YAXIS]=ETRIG_RAW_JOY0_RANA_YAXIS;

  auto thr = new ork::Thread("InputDeviceIX");

  thr->start([=]() {
    TuioInputReader reader;

    reader.mOnDown = [=](TuioCursor* tcur) {
      auto id = tcur->getCursorID();
      auto x = tcur->getX();
      auto y = tcur->getY();

      if (x < 0.2f) // left stick
      {
        lpid = id;
      } else if (x > 0.8f) // right stick
      {
        rpid = id;
      }

      // std::cout << "add cur " << tcur->getCursorID() << " (" <<  tcur->getSessionID() << ") " << tcur->getX() << " " <<
      // tcur->getY() << std::endl;
    };
    reader.mOnUpdate = [=](TuioCursor* tcur) {
      auto id = tcur->getCursorID();
      auto x = tcur->getX();
      auto y = tcur->getY();

      if (id == lpid) // left stick
      {
        lanay = 1.0f - y;
      } else if (id == rpid) // right stick
      {
        ranay = 1.0f - y;
      }

      // std::cout << "set cur " << tcur->getCursorID() << " (" <<  tcur->getSessionID() << ") " << tcur->getX() << " " <<
      // tcur->getY()
      //                        << " " << tcur->getMotionSpeed() << " " << tcur->getMotionAccel() << " " << std::endl;
    };
    reader.mOnUp = [=](TuioCursor* tcur) {
      auto id = tcur->getCursorID();

      if (id == lpid) // left stick
      {
        lanay = 0.0f;
        lpid = -1;

      } else if (id == rpid) // right stick
      {
        ranay = 0.0f;
        rpid = -1;
      }

      // std::cout << "del cur " << tcur->getCursorID() << " (" <<  tcur->getSessionID() << ")" << std::endl;
    };

    TuioClient client(3333);
    client.addTuioListener(&reader);
    client.connect(true);
  });

}

InputDeviceIX::~InputDeviceIX() {}

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::Input_Init(void) { return; }

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::poll() {
  // printf( "POLL IXID\n");
  mConnectionStatus = CONN_STATUS_ACTIVE;

  InputState& inpstate = RefInputState();

  inpstate.BeginCycle();

  for (const auto& item : _ixinputmap) {
    uint32_t k = item.first;
    uint32_t v = item.second;
    int ist = int(OldSchool::IsKeyDepressed(k)) * 127;
    // printf( "KEY<%d> ST<%d>\n", int(k), ist );
    inpstate.SetPressure(v, ist);
  }

  inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, lanay * 127.0f);
  inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, ranay * 127.0f);

  inpstate.EndCycle();

  return;
}

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::Input_Configure() { return; }

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
