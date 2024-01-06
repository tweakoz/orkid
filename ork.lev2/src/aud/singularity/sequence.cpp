////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/util/logger.h>

////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeStamp::clone() const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = _measures;
  out->_beats    = _beats;
  out->_clocks   = _clocks;
  return out;
}

timestamp_ptr_t TimeStamp::add(timestamp_ptr_t duration) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = _measures + duration->_measures;
  out->_beats    = _beats + duration->_beats;
  out->_clocks   = _clocks + duration->_clocks;
  return out;
}
timestamp_ptr_t TimeStamp::sub(timestamp_ptr_t duration) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = _measures - duration->_measures;
  out->_beats    = _beats - duration->_beats;
  out->_clocks   = _clocks - duration->_clocks;
  return out;
}

////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeBase::reduceTimeStamp(timestamp_ptr_t inp) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = inp->_measures;
  out->_beats    = inp->_beats;
  out->_clocks   = inp->_clocks;

  // Normalize clocks and beats
  if (out->_clocks < 0) {
    int borrow_beats = (-out->_clocks + _ppb - 1) / _ppb; // Ceiling division
    out->_beats -= borrow_beats;
    out->_clocks += borrow_beats * _ppb;
  }
  if (out->_beats < 0) {
    int borrow_measures = (-out->_beats + _numerator - 1) / _numerator; // Ceiling division
    out->_measures -= borrow_measures;
    out->_beats += borrow_measures * _numerator;
  }

  // Handle excess clocks
  int full_beats_from_clocks = out->_clocks / _ppb;
  out->_clocks %= _ppb;
  out->_beats += full_beats_from_clocks;

  // Handle excess beats
  int full_measures_from_beats = out->_beats / _numerator;
  out->_beats %= _numerator;
  out->_measures += full_measures_from_beats;

  return out;
}

////////////////////////////////////////////////////////////////

float TimeBase::time(timestamp_ptr_t tstamp) const {
  // Convert BPM to BPS (Beats Per Second), adjusted for the beat length
  float beatLengthMultiplier = 4.0f / static_cast<float>(_denominator);
  float beatsPerSecond       = (_tempo / 60.0f) * beatLengthMultiplier;

  // Time duration of one pulse (clock) in seconds
  float secondsPerPulse = 1.0f / (beatsPerSecond * _ppb);

  // Calculate time for the measures, beats, and clocks
  float timeForMeasures = tstamp->_measures * _numerator * _ppb * secondsPerPulse;
  float timeForBeats    = tstamp->_beats * _ppb * secondsPerPulse;
  float timeForClocks   = tstamp->_clocks * secondsPerPulse;

  // Total time in seconds
  return _basetime + (timeForMeasures + timeForBeats + timeForClocks);
}

////////////////////////////////////////////////////////////////

Event::Event() {
  _timestamp           = std::make_shared<TimeStamp>();
  _duration            = std::make_shared<TimeStamp>();
  _duration->_measures = 0;
}

Clip::Clip(){
  _duration = std::make_shared<TimeStamp>();
  _duration->_measures = 1; 
}

event_ptr_t EventClip::createNoteEvent(timestamp_ptr_t ts, timestamp_ptr_t dur, int note, int vel){
  auto event = std::make_shared<Event>();
  event->_timestamp = ts;
  event->_duration = dur;
  event->_note = note;
  event->_vel = vel;
  _events.insert(std::make_pair(ts,event));
  return event;
}

////////////////////////////////////////////////////////////////

clip_ptr_t Track::createEventClipAtTimeStamp(std::string named, timestamp_ptr_t ts){
  auto clip = std::make_shared<EventClip>();
  clip->_name = named;
  _clips_by_timestamp[ts] = clip;
  return clip;
}

////////////////////////////////////////////////////////////////

Sequence::Sequence(std::string named) {
  _name = named;
  _timebase = std::make_shared<TimeBase>(); // 4/4 120bpm
}

track_ptr_t Sequence::createTrack(const std::string& name){
  auto track = std::make_shared<Track>();
  _tracks[name] = track;
  return track;
}

////////////////////////////////////////////////////////////////

void Sequence::addNote(
    int meas, //
    int beat,
    int clocks,
    int note,
    int vel,
    int dur) {
  auto event                   = std::make_shared<Event>();
  event->_timestamp            = std::make_shared<TimeStamp>();
  event->_duration             = std::make_shared<TimeStamp>();
  event->_timestamp->_measures = meas;
  event->_timestamp->_beats    = beat;
  event->_timestamp->_clocks   = clocks;
  event->_duration->_beats     = dur;
  event->_note                 = note;
  event->_vel                  = vel;
  _events.push_back(event);
}

////////////////////////////////////////////////////////////////

void Sequence::enqueue(prgdata_constptr_t program) {
  for (auto e : _events) {
    float time_start = _timebase->time(e->_timestamp);
    auto t2          = e->_timestamp->clone();
    t2->_beats += e->_duration->_beats;
    t2->_measures += e->_duration->_measures;
    t2->_clocks += e->_duration->_clocks;
    auto reduced   = _timebase->reduceTimeStamp(t2);
    float time_end = _timebase->time(reduced);
    auto dur       = time_end - time_start;
    enqueue_audio_event(program, time_start, dur, e->_note, e->_vel);
  }
}

////////////////////////////////////////////////////////////////

sequenceplayback_ptr_t Sequencer::playSequence(sequence_ptr_t sequence){
  auto playback = std::make_shared<SequencePlayback>();
  playback->_sequence = sequence;
  return playback;
}

////////////////////////////////////////////////////////////////

void enqueue_audio_event(
    prgdata_constptr_t prog, //
    float time,
    float duration,
    int midinote,
    int velocity) {

  auto s = synth::instance();

  if (time < s->_timeaccum) {
    time = s->_timeaccum;
  }
  // printf("time<%g> note<%d> program<%s>\n", time, midinote, prog->_name.c_str());

  s->addEvent(time, [=]() {
    // NOTE ON
    auto noteinstance = s->keyOn(midinote, velocity, prog);
    assert(noteinstance);
    // NOTE OFF
    s->addEvent(time + duration, [=]() { //
      s->keyOff(noteinstance);
    });
  });
}
} // namespace ork::audio::singularity
