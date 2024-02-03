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

////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeStamp::add(timestamp_ptr_t duration) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = _measures + duration->_measures;
  out->_beats    = _beats + duration->_beats;
  out->_clocks   = _clocks + duration->_clocks;
  return out;
}

////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeStamp::sub(timestamp_ptr_t duration) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = _measures - duration->_measures;
  out->_beats    = _beats - duration->_beats;
  out->_clocks   = _clocks - duration->_clocks;
  return out;
}

////////////////////////////////////////////////////////////////

timebase_ptr_t TimeBase::clone() const{
  auto out = std::make_shared<TimeBase>();
  out->_numerator = _numerator;
  out->_denominator = _denominator;
  out->_tempo = _tempo;
  out->_ppq = _ppq;
  out->_basetime = _basetime;
  return out;
}

////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeBase::reduceTimeStamp(timestamp_ptr_t inp) const {
  auto out       = std::make_shared<TimeStamp>();
  out->_measures = inp->_measures;
  out->_beats    = inp->_beats;
  out->_clocks   = inp->_clocks;
  reduceTimeStampInPlace(out);
  return out;
}

////////////////////////////////////////////////////////////////

void TimeBase::reduceTimeStampInPlace(timestamp_ptr_t inp) const{
  int IM = inp->_measures;
  int IB = inp->_beats;
  int IC = inp->_clocks;

  // Normalize clocks and beats
  if (inp->_clocks < 0) {
    int borrow_beats = (-inp->_clocks + _ppq - 1) / _ppq; // Ceiling division
    inp->_beats -= borrow_beats;
    inp->_clocks += borrow_beats * _ppq;
  }
  if (inp->_beats < 0) {
    int borrow_measures = (-inp->_beats + _numerator - 1) / _numerator; // Ceiling division
    inp->_measures -= borrow_measures;
    inp->_beats += borrow_measures * _numerator;
  }

  // Handle excess clocks
  int full_beats_from_clocks = inp->_clocks / _ppq;
  inp->_clocks %= _ppq;
  inp->_beats += full_beats_from_clocks;

  // Handle excess beats
  int full_measures_from_beats = inp->_beats / _numerator;
  inp->_beats %= _numerator;
  inp->_measures += full_measures_from_beats;

  int OM = inp->_measures;
  int OB = inp->_beats;
  int OC = inp->_clocks;

  int measure_length = (_ppq * _numerator);
  int TM = int(IC / measure_length);
  int TB = (IC % measure_length); 
  int TC = IC % _ppq;

  //printf( "RTIP inp<%d:%d:%d> out<%d %d %d> tst<%d:%d:%d>\n", IM, IB, IC, OM, OB, OC, TM, TB, TC );
}

////////////////////////////////////////////////////////////////

float TimeBase::time(timestamp_ptr_t tstamp) const {
  // Convert BPM to BPS (Beats Per Second), adjusted for the beat length
  float beatLengthMultiplier = 4.0f / static_cast<float>(_denominator);
  float beatsPerSecond       = (_tempo / 60.0f) * beatLengthMultiplier;

  // Time duration of one pulse (clock) in seconds
  float secondsPerPulse = 1.0f / (beatsPerSecond * _ppq);

  // Calculate time for the measures, beats, and clocks
  float timeForMeasures = tstamp->_measures * _numerator * _ppq * secondsPerPulse;
  float timeForBeats    = tstamp->_beats * _ppq * secondsPerPulse;
  float timeForClocks   = tstamp->_clocks * secondsPerPulse;

  // Total time in seconds
  return _basetime + (timeForMeasures + timeForBeats + timeForClocks);
}

////////////////////////////////////////////////////////////////

timestamp_ptr_t TimeBase::timeToTimeStamp(float time) const{
  // Convert BPM to BPS (Beats Per Second), adjusted for the beat length
  float beatLengthMultiplier = 4.0f / static_cast<float>(_denominator);
  float beatsPerSecond       = (_tempo / 60.0f) * beatLengthMultiplier;

  // Time duration of one pulse (clock) in seconds
  float secondsPerPulse = 1.0f / (beatsPerSecond * _ppq);

  // Calculate measures, beats, and clocks
  int measures = static_cast<int>(floor(time / (secondsPerPulse * _numerator * _ppq)));
  time -= measures * secondsPerPulse * _numerator * _ppq;
  int beats = static_cast<int>(floor(time / (secondsPerPulse * _ppq)));
  time -= beats * secondsPerPulse * _ppq;
  int clocks = static_cast<int>(floor(time / secondsPerPulse));

  // Create and return the timestamp
  auto rval = std::make_shared<TimeStamp>(measures, beats, clocks);
  reduceTimeStampInPlace(rval);
  return rval;
}

////////////////////////////////////////////////////////////////

float TimeBase::accumBaseTimes() const{
  float rval = 0.0f;
  if(_parent){
    rval += _parent->_duration;
  }
  return rval;
}

////////////////////////////////////////////////////////////////

EventIterator::EventIterator(timestamp_ptr_t ts){
  _timestamp = ts;
}

////////////////////////////////////////////////////////////////

Event::Event() {
  _timestamp           = std::make_shared<TimeStamp>();
  _duration            = std::make_shared<TimeStamp>();
  _duration->_measures = 0;
}

////////////////////////////////////////////////////////////////

Clip::Clip(){
  _duration = std::make_shared<TimeStamp>();
  _duration->_measures = 1; 
}

////////////////////////////////////////////////////////////////

clip_ptr_t Track::createEventClipAtTimeStamp(std::string named, timestamp_ptr_t ts, timestamp_ptr_t dur){
  auto clip = std::make_shared<EventClip>();
  clip->_name = named;
  clip->_duration = dur;
  _clips_by_timestamp[ts] = clip;
  return clip;
}

////////////////////////////////////////////////////////////////

clip_ptr_t Track::createFourOnFloorClipAtTimeStamp(std::string named, timestamp_ptr_t ts, timestamp_ptr_t dur){
  auto clip = std::make_shared<FourOnFloorClip>();
  clip->_name = named;
  clip->_duration = dur;
  _clips_by_timestamp[ts] = clip;
  return clip;
}

////////////////////////////////////////////////////////////////

Sequence::Sequence(std::string named) {
  _name = named;
  _timebase = std::make_shared<TimeBase>(); // 4/4 120bpm
}

////////////////////////////////////////////////////////////////

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
   // enqueue_audio_event(program, time_start, dur, e->_note, e->_vel);
  }
}

} // namespace ork::audio::singularity
