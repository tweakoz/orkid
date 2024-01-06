////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include "krztypes.h"
#include "synthdata.h"
#include "layer.h"
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/svariant.h>
#include <ork/util/crc.h>
//#include <ork/lev2/gfx/gfxenv.h>

namespace ork::audio::singularity {

struct TimeStamp{
  TimeStamp() : _measures(0), _beats(0), _clocks(0) {}
  TimeStamp(int m, int b, int c) : _measures(m), _beats(b), _clocks(c) {}
  timestamp_ptr_t clone() const;
  timestamp_ptr_t add(timestamp_ptr_t duration) const;
  timestamp_ptr_t sub(timestamp_ptr_t duration) const;
  int _measures;
  int _beats;
  int _clocks;
};

struct TimeBase{
  float time(timestamp_ptr_t tstamp) const;
  timestamp_ptr_t reduceTimeStamp(timestamp_ptr_t inp) const;
  float _basetime = 0.0f;
  int _numerator = 4;
  int _denominator = 4;
  float _tempo = 120.0f;
  int _ppb = 96;
};

struct Event {
  Event();
  timestamp_ptr_t _timestamp;
  timestamp_ptr_t _duration;
  int _note   = 0;
  int _vel    = 0;
};

struct Clip{
  std::vector<Event> _events;
};

using clipmap_t = std::map<float, clip_ptr_t>;

struct Track{
    clipmap_t _clips_by_timestamp;
};

using trackmap_t = std::unordered_map<std::string, track_ptr_t>;

void enqueue_audio_event(
    prgdata_constptr_t prog, //
    float time,
    float duration,
    int midinote,
    int velocity = 128);

enum class TransportState : uint64_t{
  CrcEnum(STOPPED),
  CrcEnum(PLAYING),
  CrcEnum(RECORDING),
};

struct Sequence{

  Sequence();
  std::vector<event_ptr_t> _events;
  void addNote(
      int meas, //
      int sixteenth,
      int clocks,
      int note,
      int vel,
      int dur = 1);
  void enqueue(prgdata_constptr_t program);

  void play();
  void stop();
  void record();

  trackmap_t _tracks; 
  timebase_ptr_t _timebase;
  TransportState _transportState;

};

using seqmap_t = std::unordered_map<std::string, sequence_ptr_t>;

struct Sequencer{
    seqmap_t _sequences;
};

} //namespace ork::audio::singularity {
