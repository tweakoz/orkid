////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/kernel.h>
#include <ork/kernel/thread.h>
#include <ork/orkstl.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Timer {

  Timer();
  ~Timer();

  void Start();
  void End();
  double InternalSecsSinceStart() const;
  double SecsSinceStart() const;
  double SpanInSecs() const;
  void OnInterval(float interval, const void_lambda_t& oper);
  void setCurrentTime(float value);

  // Must be called early before use of get_sync_tick and get_sync_time
  static void staticInit();

  static u64 get_sync_tick();
  static double get_sync_time();
  static double tick_scale_ms(); // milliseconds per tick

private:
  u64 _start_tick;
  u64 _end_tick;
  float _lambda_interval;
  void_lambda_t _on_interval;
  ork::Thread* _thread;
  bool _kill;
};

using timer_ptr_t = std::shared_ptr<Timer>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
