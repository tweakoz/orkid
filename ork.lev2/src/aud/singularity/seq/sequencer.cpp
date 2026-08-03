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
#include <ork/kernel/opq.h>

////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////

sequenceplayback_ptr_t Sequencer::playSequence(sequence_ptr_t sequence,float timeoffset) {
  auto pb = std::make_shared<SequencePlayback>(sequence);
  pb->_timeoffet = timeoffset;
  _sequence_playbacks.push_back(pb);
  return pb;
}

////////////////////////////////////////////////////////////////

void Sequencer::clearPlaybacks() {
  _sequence_playbacks.clear();
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

void Sequencer::enqueueMainThreadEventCallback(int note, int vel, float dur, const std::string& track) {
  SequencerEventData ev;
  ev._note = note;
  ev._velocity = vel;
  ev._duration = dur;
  ev._track_name = track;
  _pendingMainThreadEventCallbacks.try_push(ev);
  // wake the (possibly idle-waiting) main-thread pump so posted events are
  // drained immediately rather than on its idle-backstop timeout. RT variant:
  // this runs on the audio thread — must never take a lock.
  opq::mainSerialQueue()->mSemaphore.notify_rt();
}

////////////////////////////////////////////////////////////////

int Sequencer::drainMainThreadEventCallbacks() {
  int count = 0;
  SequencerEventData ev;
  while (_pendingMainThreadEventCallbacks.try_pop(ev)) {
    if (_on_event) {
      _on_event(ev._note, ev._velocity, ev._duration, ev._track_name);
    }
    count++;
  }
  return count;
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

void enqueue_audio_event(
    track_ptr_t track, //
    float time,
    float duration,
    int midinote,
    int velocity) {

  auto p = track->_program;
  auto b = track->_outbus;

  auto s = synth::instance();

  if (time < s->_timeaccum) {
    time = s->_timeaccum;
  }
  //printf("time<%g> note<%d> program<%s> bus<%s>\n", time, midinote, p->_name.c_str(), b->_name.c_str() );

  s->addEvent(time, [=]() {
    // NOTE ON
    auto mod = std::make_shared<KeyOnModifiers>();
    mod->_outbus_override = b;
    auto noteinstance = s->keyOn(midinote, velocity, p,mod);
    assert(noteinstance);
    // NOTE OFF
    s->addEvent(time + duration, [=]() { //
      s->keyOff(noteinstance);
      // Enqueue NOTE OFF callback for main thread delivery
      auto sequencer = s->_sequencer;
      if (sequencer) {
        sequencer->enqueueMainThreadEventCallback(midinote, 0, 0.0f, track->_name);
      }
    });
  });
}

////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
////////////////////////////////////////////////////////////////
