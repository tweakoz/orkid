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

TrackPlayback::TrackPlayback(track_ptr_t track) {
  _track     = track;
  _next_clip = track->_clips_by_timestamp.begin();
  if (_next_clip != track->_clips_by_timestamp.end()) {
    _clip_playback                 = std::make_shared<ClipPlayback>();
    auto clip                      = _next_clip->second;
    _clip_playback->_clipX         = clip;
    _clip_playback->_next_event_it = clip->firstEvent();
  }
}

/////////////////////////////////////////////////////////////////
// TrackPlayback::_tryAdvanceClip
// find the clip that starts at (or before) this timestamp
//  but start search at next_track_pb_clip
/////////////////////////////////////////////////////////////////

void TrackPlayback::_tryAdvanceClip(const SequencePlayback* seqpb){
  auto next_track_pb_clip = _next_clip;
  const clipmap_t& clips                       = _track->_clips_by_timestamp;
  bool done_clip_iteration = false;
  _clip_str = "";
  _ev_str = "";
  while (done_clip_iteration == false) {
    if (next_track_pb_clip == clips.end()) {
      done_clip_iteration = true;
    } else {
      auto clip_start = next_track_pb_clip->first;
      auto clip       = next_track_pb_clip->second;
      TimeStampComparatorLessEqual compare;
      bool check = compare(clip_start, seqpb->_currentTS);
      int CM     = clip_start->_measures;
      int CB     = clip_start->_beats;
      int CC     = clip_start->_clocks;
      printf("    tr<%s> clip_start<%d:%d:%d> check<%d>", _track->_name.c_str(), CM, CB, CC, check);
      if (check) {
        ////////////////////////////////////////
        // clip starts before current timestamp
        //  so we need to process it
        ////////////////////////////////////////
        _clip_str += FormatString("[CLIP<%s> START %d:%d:%d]", clip->_name.c_str(), CM, CB, CC);
        _clip_playback->_clipX          = clip;
        _clip_playback->_clip_timestamp = clip_start;
        _clip_playback->_next_event_it  = clip->firstEvent();
        next_track_pb_clip++;
        _next_clip = next_track_pb_clip;
        ////////////////////////////////////////
        // (in advance)
        //  check to see if there are clips after this one
        ////////////////////////////////////////
        if (next_track_pb_clip == clips.end()) {
          done_clip_iteration = true;
          _clip_str += "[NO MORE CLIPS]";
        } else {
          clip_start          = next_track_pb_clip->first;
          check               = compare(clip_start, seqpb->_currentTS);
          done_clip_iteration = not check;
          _clip_str += FormatString("[ADVANCE CLIP:%d]", int(done_clip_iteration));
        }
      } else {
        done_clip_iteration = true;
      }
    }
  } // while (done_clip_iteration == false) {

}

////////////////////////////////////////////////////////////////
// TrackPlayback::_cleanupPastEvents
//  erase any active events that are past their end time
////////////////////////////////////////////////////////////////

void TrackPlayback::_cleanupPastEvents(const SequencePlayback* seqpb){
  _active_events_to_erase.clear();
  TimeStampComparatorLessEqual compare;
  for (auto item : _active_events) {
    auto active_event = item;
    auto event        = active_event->_event;
    auto event_end    = active_event->_time_end;
    if (compare(event_end, seqpb->_currentTS)) {
      //int EEM = event_end->_measures;
      //int EEB = event_end->_beats;
      //int EEC = event_end->_clocks;
      // _ev_str += FormatString("[EVENT.END %d:%d:%d] ", EEM, EEB, EEC);
      _active_events_to_erase.push_back(active_event);
    }
  }
  for (auto item : _active_events_to_erase) {
    auto active_event = item;
    _active_events.erase(active_event);
  }

}

////////////////////////////////////////////////////////////////
// TrackPlayback::_postDueEvents
//  process any events that are due to start
////////////////////////////////////////////////////////////////

void TrackPlayback::_postDueEvents(const SequencePlayback* seqpb){
  auto current_timestamp = seqpb->_currentTS;
  auto seq = seqpb->_sequence;
  auto tbase = seq->_timebase;
  /////////////////////////////////////////////////////////////////
  // clip iteration is done, now process the clip playbacks
  /////////////////////////////////////////////////////////////////
  clipplayback_ptr_t clippb = _clip_playback;
  if (clippb and clippb->_clip_timestamp) {
    auto clip = clippb->_clipX;
    ///////////////////////////
    // check for clip end
    ///////////////////////////
    if (clip) {
      auto clip_end         = clip->_duration->add(clippb->_clip_timestamp);
      auto clip_end_reduced = tbase->reduceTimeStamp(clip_end);
      TimeStampComparatorLessEqual compare_end;
      bool check_end = compare_end(clip_end_reduced, current_timestamp);
      if (check_end) {
        _clip_str += "[CLIP END]";
        clippb->_clipX         = nullptr;
        clippb->_next_event_it = nullptr;
        clip                   = nullptr;
        clippb                 = nullptr;
      }
    }
    ///////////////////////////
    if (clip and clippb) {
      auto next_event_it = clippb->_next_event_it;
      if (next_event_it and clip->eventValid(next_event_it)) {
        auto next_event      = next_event_it->_event;
        auto event_timestamp = next_event_it->_timestamp;

        auto evtsplusclip = event_timestamp->add(clippb->_clip_timestamp);
        evtsplusclip      = tbase->reduceTimeStamp(evtsplusclip);

        int EM = evtsplusclip->_measures;
        int EB = evtsplusclip->_beats;
        int EC = evtsplusclip->_clocks;
        TimeStampComparatorLessEqual compare;
        bool check = compare(evtsplusclip, current_timestamp);
        if (check) {
          // event starts before current timestamp
          //  so we need to process it
          _ev_str += FormatString("[EVENT.BEG %d:%d:%d]", EM, EB, EC);
          auto program = _track->_program;
          int note     = next_event->_note;
          int vel      = next_event->_vel;
          /////////////////
          auto active_event         = std::make_shared<ActiveEvent>();
          active_event->_event      = next_event;
          active_event->_time_start = evtsplusclip;
          auto event_duration       = next_event->_duration;
          auto event_end            = event_duration->add(evtsplusclip);
          event_end                 = tbase->reduceTimeStamp(event_end);
          active_event->_time_end   = event_end;
          _active_events.insert(active_event);
          float duration = tbase->time(next_event->_duration);
          /////////////////
          enqueue_audio_event(_track, 0.0f, duration, note, vel);
          next_event_it          = clip->nextEvent(next_event_it);
          clippb->_next_event_it = next_event_it;
          if (next_event_it and clip->eventValid(next_event_it)) {
            auto nmpclip = next_event_it->_timestamp->add(clippb->_clip_timestamp);
            nmpclip      = tbase->reduceTimeStamp(nmpclip);
            int NM       = nmpclip->_measures;
            int NB       = nmpclip->_beats;
            int NC       = nmpclip->_clocks;
            // _ev_str += FormatString("[NEXT %d:%d:%d]", NM, NB, NC);
          } else {
            _ev_str += FormatString("[NO MORE EVENTS]");
            clippb->_clipX         = nullptr;
            clippb->_next_event_it = nullptr;
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////
// TrackPlayback::process
//  process the track playback
////////////////////////////////////////////////////////////////

void TrackPlayback::process(const SequencePlayback* seqpb) {

  _tryAdvanceClip(seqpb);
  _cleanupPastEvents(seqpb);
  _postDueEvents(seqpb);

  ///////////////////////////
  // debug output
  ///////////////////////////
  if (_clip_str.length() or _ev_str.length()) {
    printf("  TR<%s>\n", _track->_name.c_str());
  }
  if (_clip_str.length()) {
    printf("    %s\n", _clip_str.c_str());
  }
  if (_ev_str.length()) {
    printf("    %s\n", _ev_str.c_str());
  }
}

////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
////////////////////////////////////////////////////////////////
