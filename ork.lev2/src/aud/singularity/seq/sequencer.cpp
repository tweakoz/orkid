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
    });
  });
}

////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
////////////////////////////////////////////////////////////////
