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

SequencePlayback::SequencePlayback(sequence_ptr_t seq) {
  _sequence = seq;
  for (auto item : seq->_tracks) {
    auto track                   = item.second;
    auto pbtrack                 = std::make_shared<TrackPlayback>(track);
    pbtrack->_next_clip          = track->_clips_by_timestamp.begin();
    _track_playbacks[item.first] = pbtrack;
  }
}

////////////////////////////////////////////////////////////////

void SequencePlayback::_advanceClock(Sequencer* sequencer){
  auto syn = sequencer->_the_synth;
  float time             = syn->_timeaccum-_timeoffet;
  auto tbase             = _sequence->_timebase;
  auto current_timestamp = tbase->timeToTimeStamp(time);

  ////////////////////////////////////////
  // reset clock for looping playback/record
  ////////////////////////////////////////

  if(tbase->_measureMax!=0){
    if(current_timestamp->_measures>=tbase->_measureMax){
      syn->_timeaccum = 0.0f;
      _timeoffet = 0.0f;
      current_timestamp = tbase->timeToTimeStamp(0.0f);
      _track_playbacks.clear();
      for (auto item : _sequence->_tracks) {
        auto track                   = item.second;
        auto pbtrack                 = std::make_shared<TrackPlayback>(track);
        pbtrack->_next_clip          = track->_clips_by_timestamp.begin();
        _track_playbacks[item.first] = pbtrack;
      }
    }
  }

  _currentTS = current_timestamp;
}

////////////////////////////////////////////////////////////////

void SequencePlayback::process(Sequencer* sequencer) {

  auto syn = sequencer->_the_synth;

  _advanceClock(sequencer);
  
  ////////////////////////////////////////
  // process hud events
  //  (eg.. update measures/beats on HUD)
  ////////////////////////////////////////

  int M = _currentTS->_measures;
  int B = _currentTS->_beats;
  int C = _currentTS->_clocks;

  if ((M != _lastM) or (B != _lastB)) {
    auto hud_event = std::make_shared<HudEvent>();
    hud_event->_eventData.setShared<TimeStamp>(_currentTS);
    hud_event->_eventID = "seq.playback.timestamp.click"_crcu;
    syn->enqueueHudEvent(hud_event);
    //printf("PB TS[%d:%d:%d]    time<%g>\n", M, B, C, time);
  }

  _lastM = M;
  _lastB = B;

  ////////////////////////////////////////
  // process track playbacks
  ////////////////////////////////////////

  for (auto item : _track_playbacks) {
    trackplayback_ptr_t track_pb                 = item.second;
    track_pb->process(this);
  } 

  ////////////////////////////////////////

}

////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
