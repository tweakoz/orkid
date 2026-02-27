////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/kernel.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/svariant.h>
#include <ork/orkstl.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Timer {

  Timer();
  ~Timer();

  void Start();
  void End();
  float InternalSecsSinceStart() const;
  float SecsSinceStart() const;
  float SpanInSecs() const;
  void OnInterval(float interval, const void_lambda_t& oper);
  void setCurrentTime(float value);
  static float get_sync_time();

  static void staticInit();

private:
  static svar64_t _gimpl;

  float mStartTime;
  float mEndTime;
  float mLambdaInterval;
  void_lambda_t mOnInterval;
  ork::Thread* mThread;
  bool mKill;
};

using timer_ptr_t = std::shared_ptr<Timer>;

///////////////////////////////////////////////////////////////////////////////

struct PerfItem2 {
  const char* mpMarkerName;
  float mfMarkerTime;
};

void PerfMarkerPush(const char* str);
bool PerfMarkerPop(PerfItem2& outmkr);
void PerfMarkerEnable();
void PerfMarkerDisable();
void PerfMarkerPushState();
void PerfMarkerPopState();

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

