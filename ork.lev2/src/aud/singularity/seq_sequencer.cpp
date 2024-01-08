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
  _track                         = track;
  _next_clip                     = track->_clips_by_timestamp.begin();
  _clip_playback                 = std::make_shared<ClipPlayback>();
  auto clip                      = _next_clip->second;
  _clip_playback->_clipX         = clip;
  _clip_playback->_next_event_it = clip->firstEvent();
}

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
  float time             = sequencer->_the_synth->_timeaccum;
  auto tbase             = _sequence->_timebase;
  auto current_timestamp = tbase->timeToTimeStamp(time);
  int M                  = current_timestamp->_measures;
  int B                  = current_timestamp->_beats;
  int C                  = current_timestamp->_clocks;
  static int _lastM      = -1;
  static int _lastB      = -1;

  if ((M != _lastM) or (B != _lastB)) {
    printf("PB TS[%d:%d:%d]    time<%g>\n", M, B, C, time);
    for (auto item : _track_playbacks) {
      auto track_name = item.first;
      trackplayback_ptr_t track_pb                 = item.second;
      track_ptr_t track                            = track_pb->_track;
      clipmap_t::const_iterator next_track_pb_clip = track_pb->_next_clip;
      const clipmap_t& clips                       = track->_clips_by_timestamp;
      /////////////////////////////////////////////////////////////////
      // find the clip that starts at (or before) this timestamp
      //  but start search at next_track_pb_clip
      /////////////////////////////////////////////////////////////////
      bool done_clip_iteration = false;
      std::string clip_str;
      std::string ev_str;
      while (done_clip_iteration == false) {
        if (next_track_pb_clip == clips.end()) {
          done_clip_iteration = true;
        } else {
          auto clip_start = next_track_pb_clip->first;
          auto clip       = next_track_pb_clip->second;
          TimeStampComparatorLessEqual compare;
          bool check = compare(clip_start, current_timestamp);
          int CM     = clip_start->_measures;
          int CB     = clip_start->_beats;
          int CC     = clip_start->_clocks;
          // printf( "    clip_start<%d:%d:%d> check<%d>", CM,CB,CC, check );
          if (check) {
            // clip starts before current timestamp
            //  so we need to process it
            clip_str += FormatString("[CLIP START %d:%d:%d]", CM, CB, CC);
            track_pb->_clip_playback->_clipX          = clip;
            track_pb->_clip_playback->_clip_timestamp = clip_start;
            track_pb->_clip_playback->_next_event_it = clip->firstEvent();
            next_track_pb_clip++;
            track_pb->_next_clip = next_track_pb_clip;
            if (next_track_pb_clip == clips.end()) {
              done_clip_iteration = true;
              clip_str += "[NO MORE CLIPS]";
            } else {
              clip_start          = next_track_pb_clip->first;
              check               = compare(clip_start, current_timestamp);
              done_clip_iteration = not check;
              clip_str += FormatString("[ADVANCE CLIP:%d]", int(done_clip_iteration));
            }
          } else {
            done_clip_iteration = true;
          }
        }
      }
      ///////////////////////////
      std::unordered_set<activeevent_ptr_t> active_events_to_erase;
      for( auto item : track_pb->_active_events ){
        auto active_event = item;
        auto event = active_event->_event;
        auto event_end = active_event->_time_end;
        TimeStampComparatorLessEqual compare;
        bool check = compare(event_end, current_timestamp);
        if (check) {
          int EEM        = event_end->_measures;
          int EEB        = event_end->_beats;
          int EEC        = event_end->_clocks;
          ev_str += FormatString("[EVENT.END %d:%d:%d] ", EEM, EEB, EEC);
          active_events_to_erase.insert(active_event);
        }
      }
      for( auto item : active_events_to_erase ){
        auto active_event = item;        
        track_pb->_active_events.erase(active_event);
      }
      /////////////////////////////////////////////////////////////////
      // clip iteration is done, now process the clip playbacks
      /////////////////////////////////////////////////////////////////
      clipplayback_ptr_t clippb = track_pb->_clip_playback;
      if (clippb and clippb->_clip_timestamp) {
        auto clip          = clippb->_clipX;
        ///////////////////////////
        // check for clip end
        ///////////////////////////
        if(clip){
          auto clip_end = clip->_duration->add(clippb->_clip_timestamp);
          auto clip_end_reduced = tbase->reduceTimeStamp(clip_end);
          TimeStampComparatorLessEqual compare_end;
          bool check_end = compare_end(clip_end_reduced, current_timestamp);
          if (check_end) {
            clip_str += "[CLIP END]";
            clippb->_clipX          = nullptr;
            clippb->_next_event_it = nullptr;
            clip = nullptr;
            clippb = nullptr;
          }
        }
        ///////////////////////////
        if(clip and clippb){
          auto next_event_it = clippb->_next_event_it;
          if (clip->eventValid(next_event_it)) {
            auto next_event      = next_event_it->_event;
            auto event_timestamp = next_event_it->_timestamp;

            auto evtsplusclip = event_timestamp->add(clippb->_clip_timestamp);
            evtsplusclip = tbase->reduceTimeStamp(evtsplusclip);

            int EM               = evtsplusclip->_measures;
            int EB               = evtsplusclip->_beats;
            int EC               = evtsplusclip->_clocks;
            TimeStampComparatorLessEqual compare;
            bool check = compare(evtsplusclip, current_timestamp);
            if (check) {
              // event starts before current timestamp
              //  so we need to process it
              ev_str += FormatString("[EVENT.BEG %d:%d:%d]", EM, EB, EC);
              auto program   = track->_program;
              int note       = next_event->_note;
              int vel        = next_event->_vel;
              /////////////////
              auto active_event = std::make_shared<ActiveEvent>();
              active_event->_event = next_event;
              active_event->_time_start = evtsplusclip;
              auto event_duration = next_event->_duration;
              auto event_end = event_duration->add(evtsplusclip);
              event_end = tbase->reduceTimeStamp(event_end);
              active_event->_time_end = event_end;
              track_pb->_active_events.insert(active_event);
              //float duration = tbase->time(next_event->_duration);
              /////////////////
              // enqueue_audio_event(program, time, duration, note, vel);
              next_event_it          = clip->nextEvent(next_event_it);
              clippb->_next_event_it = next_event_it;
              if (next_event_it and clip->eventValid(next_event_it)) {
                auto nmpclip = next_event_it->_timestamp->add(clippb->_clip_timestamp);
                nmpclip = tbase->reduceTimeStamp(nmpclip);
                int NM = nmpclip->_measures;
                int NB = nmpclip->_beats;
                int NC = nmpclip->_clocks;
                ev_str += FormatString("[NEXT %d:%d:%d]", NM, NB, NC);
              } else {
                ev_str += FormatString("[NO MORE EVENTS]");
                clippb->_clipX          = nullptr;
                clippb->_next_event_it = nullptr;
              }
            }
          }
        }
      }
      ///////////////////////////
      // debug output
      ///////////////////////////
      if (clip_str.length() or ev_str.length()) {
        printf("  TR<%s>\n", track_name.c_str());
      }
      if (clip_str.length()) {
        printf("    %s\n", clip_str.c_str());
      }
      if (ev_str.length()) {
        printf("    %s\n", ev_str.c_str());
      }
      /////////////////////////////////////////////////////////////////
    } // for each track playback
  }
  _lastM = M;
  _lastB = B;
}

////////////////////////////////////////////////////////////////

sequenceplayback_ptr_t Sequencer::playSequence(sequence_ptr_t sequence) {
  auto pb = std::make_shared<SequencePlayback>(sequence);
  _sequence_playbacks.push_back(pb);
  return pb;
}

////////////////////////////////////////////////////////////////

Sequencer::Sequencer(synth* the_synth) {
  _the_synth = the_synth;
}

////////////////////////////////////////////////////////////////

void Sequencer::process() {
  for (auto pb : _sequence_playbacks) {
    pb->process(this);
  }
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

////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
////////////////////////////////////////////////////////////////
