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

void SequencePlayback::process(Sequencer* sequencer) {
  auto syn = sequencer->_the_synth;
  float time             = syn->_timeaccum-_timeoffet;
  auto tbase             = _sequence->_timebase;
  auto current_timestamp = tbase->timeToTimeStamp(time);
  int M                  = current_timestamp->_measures;
  int B                  = current_timestamp->_beats;
  int C                  = current_timestamp->_clocks;
  static int _lastM      = -1;
  static int _lastB      = -1;

  ////////////////////////////////////////
  // reset clock for looping playback/record
  ////////////////////////////////////////

  if(tbase->_measureMax!=0){
    if(M>=tbase->_measureMax){
      syn->_timeaccum = 0.0f;
      _timeoffet = 0.0f;
      current_timestamp = tbase->timeToTimeStamp(0.0f);
      M = current_timestamp->_measures;
      B = current_timestamp->_beats;
      C = current_timestamp->_clocks;
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
  
  ////////////////////////////////////////

  if ((M != _lastM) or (B != _lastB)) {
    auto hud_event = std::make_shared<HudEvent>();
    hud_event->_eventData.setShared<TimeStamp>(current_timestamp);
    hud_event->_eventID = "seq.playback.timestamp.click"_crcu;
    syn->enqueueHudEvent(hud_event);
    //printf("PB TS[%d:%d:%d]    time<%g>\n", M, B, C, time);
  }

  //
  // 
  //

  for (auto item : _track_playbacks) {
    trackplayback_ptr_t track_pb                 = item.second;
    track_pb->process(this);
    /////////////////////////////////////////////////////////////////
  } // for each track playback
  _lastM = M;
  _lastB = B;
}

////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
