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
////////////////////////////////////////////////////////////////

struct TimeStamp {
  TimeStamp()
      : _measures(0)
      , _beats(0)
      , _clocks(0) {
  }
  TimeStamp(int m, int b, int c)
      : _measures(m)
      , _beats(b)
      , _clocks(c) {
  }
  timestamp_ptr_t clone() const;
  timestamp_ptr_t add(timestamp_ptr_t duration) const;
  timestamp_ptr_t sub(timestamp_ptr_t duration) const;
  int _measures;
  int _beats;
  int _clocks;
};

////////////////////////////////////////////////////////////////

struct TimeStampComparatorLess {
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

////////////////////////////////////////////////////////////////

struct TimeStampComparatorLessEqual {
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
    return lhs->_clocks <= rhs->_clocks;
  }
};

////////////////////////////////////////////////////////////////

struct TimeBase {
  timebase_ptr_t clone() const;
  float accumBaseTimes() const;
  float time(timestamp_ptr_t tstamp) const;
  timestamp_ptr_t timeToTimeStamp(float time) const;
  timestamp_ptr_t reduceTimeStamp(timestamp_ptr_t inp) const;
  void reduceTimeStampInPlace(timestamp_ptr_t inp) const;
  float _basetime  = 0.0f;
  float _duration = 0.0f;
  int _numerator   = 4;
  int _denominator = 4;
  float _tempo     = 120.0f;
  int _ppq         = 96;
  int _measureMax  = 0;
  timebase_ptr_t _parent;
};

////////////////////////////////////////////////////////////////

struct Event {
  Event();
  timestamp_ptr_t _timestamp;
  timestamp_ptr_t _duration;
  int _note = 0;
  int _vel  = 0;
};

struct ActiveEvent{
  event_ptr_t _event;
  timestamp_ptr_t _time_start;
  timestamp_ptr_t _time_end;
};

using activeevent_ptr_t = std::shared_ptr<ActiveEvent>;

////////////////////////////////////////////////////////////////

struct EventIterator {
  EventIterator(timestamp_ptr_t ts);
  timestamp_ptr_t _timestamp;
  event_ptr_t _event;
  svar64_t _impl;
};
using eventiterator_ptr_t = std::shared_ptr<EventIterator>;

////////////////////////////////////////////////////////////////

struct Clip {
  Clip();
  std::string _name;
  timestamp_ptr_t _duration;
  virtual eventiterator_ptr_t firstEvent() const                   = 0;
  virtual eventiterator_ptr_t nextEvent(eventiterator_ptr_t) const = 0;
  virtual bool eventValid(eventiterator_ptr_t) const               = 0;
  virtual ~Clip() {
  }
};

////////////////////////////////////////////////////////////////

struct ClipPlayback {
  clip_ptr_t _clipX;
  timestamp_ptr_t _clip_timestamp;
  eventiterator_ptr_t _next_event_it;
};

////////////////////////////////////////////////////////////////

struct EventClip : public Clip {
  using evmap_t    = std::multimap<timestamp_ptr_t, event_ptr_t, TimeStampComparatorLess>;
  using evmap_it_t = evmap_t::const_iterator;
  eventiterator_ptr_t firstEvent() const final;
  eventiterator_ptr_t nextEvent(eventiterator_ptr_t) const final;
  bool eventValid(eventiterator_ptr_t) const final;
  event_ptr_t createNoteEvent(timestamp_ptr_t ts, timestamp_ptr_t dur, int note, int vel);
  evmap_t _events;

  std::unordered_map<int,timestamp_ptr_t> _rec_notetimes;
};

////////////////////////////////////////////////////////////////

struct FourOnFloorClip : public Clip {
  eventiterator_ptr_t firstEvent() const final;
  eventiterator_ptr_t nextEvent(eventiterator_ptr_t) const final;
  bool eventValid(eventiterator_ptr_t) const final;
  int _note = 60;
  int _vel  = 127;
};

////////////////////////////////////////////////////////////////

struct ClickClip : public Clip {
  eventiterator_ptr_t firstEvent() const final;
  eventiterator_ptr_t nextEvent(eventiterator_ptr_t) const final;
  bool eventValid(eventiterator_ptr_t) const final;
  int _noteH = 60;
  int _velH  = 32;
  int _noteL = 60;
  int _velL  = 127;
};

////////////////////////////////////////////////////////////////
using clipmap_t = std::map<timestamp_ptr_t, clip_ptr_t, TimeStampComparatorLess>;
////////////////////////////////////////////////////////////////

struct Track {
  clip_ptr_t createEventClipAtTimeStamp(std::string named, timestamp_ptr_t ts, timestamp_ptr_t dur);
  clip_ptr_t createFourOnFloorClipAtTimeStamp(std::string named, timestamp_ptr_t ts, timestamp_ptr_t dur);
  clip_ptr_t createClickClip(std::string named);
  std::string _name;
  clipmap_t _clips_by_timestamp;
  prgdata_constptr_t _program;
  outbus_ptr_t _outbus;
};

////////////////////////////////////////////////////////////////

struct TrackPlayback {
  TrackPlayback(track_ptr_t track);
  void process(const SequencePlayback* seqpb);

  void _tryAdvanceClip(const SequencePlayback* seqpb);
  void _cleanupPastEvents(const SequencePlayback* seqpb);
  void _postDueEvents(const SequencePlayback* seqpb);

  track_ptr_t _track;
  clipmap_t::const_iterator _next_clip;
  clipplayback_ptr_t _clip_playback;
  std::unordered_set<activeevent_ptr_t> _active_events;
  std::vector<activeevent_ptr_t> _active_events_to_erase;

  std::string _clip_str;
  std::string _ev_str;
};

////////////////////////////////////////////////////////////////

void enqueue_audio_event(
    prgdata_constptr_t track, //
    float time,
    float duration,
    int midinote,
    int velocity = 128);

void enqueue_audio_event(
    track_ptr_t track, //
    float time,
    float duration,
    int midinote,
    int velocity
    );

////////////////////////////////////////////////////////////////

enum class TransportState : uint64_t {
  CrcEnum(STOPPED),
  CrcEnum(PLAYING),
  CrcEnum(RECORDING),
};

////////////////////////////////////////////////////////////////

struct Sequence {
  using trackmap_t = std::unordered_map<std::string, track_ptr_t>;
  using timebase_map_t = std::map<float,timebase_ptr_t>;
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
  timebase_map_t _timebases;
};

////////////////////////////////////////////////////////////////

struct SequencePlayback {
  using trackpbmap_t = std::unordered_map<std::string, trackplayback_ptr_t>;
  SequencePlayback(sequence_ptr_t seq);
  void process(Sequencer* seq);
  sequence_ptr_t _sequence;
  trackpbmap_t _track_playbacks;
  float _timeoffet = 0.0f;
  timestamp_ptr_t _currentTS;
};

////////////////////////////////////////////////////////////////

struct Sequencer {
  using seqmap_t = std::unordered_map<std::string, sequence_ptr_t>;
  Sequencer(synth* the_synth);
  sequenceplayback_ptr_t playSequence(sequence_ptr_t sequence,float timeoffset);
  void process();
  void clearPlaybacks();
  seqmap_t _sequences;
  std::vector<sequenceplayback_ptr_t> _sequence_playbacks;
  synth* _the_synth = nullptr;
  track_ptr_t _recording_track;
  clip_ptr_t _recording_clip;
};

} // namespace ork::audio::singularity
