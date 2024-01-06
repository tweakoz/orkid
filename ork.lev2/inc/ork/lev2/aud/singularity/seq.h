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
struct TimeStampComparator {
  bool operator()(const timestamp_ptr_t& lhs, const timestamp_ptr_t& rhs) const {
    // First compare measures
    if (lhs->_measures != rhs->_measures) {
      return lhs->_measures < rhs->_measures;
    }
    // Then compare beats
    if (lhs->_beats != rhs->_beats) {
      return lhs->_beats < rhs->_beats;
    }
    // Finally compare clocks
    return lhs->_clocks < rhs->_clocks;
  }
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
  Clip();
  std::string _name;
  timestamp_ptr_t _duration;
  virtual ~Clip(){}
};

using evmap_t = std::multimap<timestamp_ptr_t, event_ptr_t,TimeStampComparator>;

struct EventClip : public Clip{
  event_ptr_t createNoteEvent(timestamp_ptr_t ts, timestamp_ptr_t dur, int note, int vel);
  evmap_t _events;
};

using clipmap_t = std::map<timestamp_ptr_t, clip_ptr_t,TimeStampComparator>;

struct Track{
    clip_ptr_t createEventClipAtTimeStamp(std::string named, timestamp_ptr_t ts);
    clipmap_t _clips_by_timestamp;
    prgdata_constptr_t _program;
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

  Sequence(std::string name);
  std::vector<event_ptr_t> _events;
  void addNote(
      int meas, //
      int sixteenth,
      int clocks,
      int note,
      int vel,
      int dur = 1);
  void enqueue(prgdata_constptr_t program);
  track_ptr_t createTrack(const std::string& name);
  void play();
  void stop();
  void record();

  std::string _name;
  trackmap_t _tracks; 
  timebase_ptr_t _timebase;
  TransportState _transportState;

};

struct SequencePlayback{
  sequence_ptr_t _sequence;
};

using seqmap_t = std::unordered_map<std::string, sequence_ptr_t>;

struct Sequencer{
    sequenceplayback_ptr_t playSequence(sequence_ptr_t sequence);
    seqmap_t _sequences;
};

} //namespace ork::audio::singularity {
