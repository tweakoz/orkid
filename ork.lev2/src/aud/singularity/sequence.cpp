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

namespace ork::audio::singularity {
    
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
  //printf("time<%g> note<%d> program<%s>\n", time, midinote, prog->_name.c_str());

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

Sequence::Sequence()
    : _tempo(120.0f) {
}
float Sequence::mbs2time(int meas, int sixteenth, int clocks) const {
  float timepermeasure = 60.0 * 4.0 / _tempo;
  float out_time       = float(meas) * timepermeasure;
  out_time += float(sixteenth) * timepermeasure / 16.0f;
  out_time += float(clocks) * timepermeasure / 256.0f;
  return out_time;
}
void Sequence::addNote(
    int meas, //
    int sixteenth,
    int clocks,
    int note,
    int vel,
    int dur) {
  Event out;
  out._time = mbs2time(meas, sixteenth, clocks);
  out._note = note;
  out._vel  = vel;
  out._dur  = mbs2time(0, 0, dur);
  _events.push_back(out);
}
void Sequence::enqueue(prgdata_constptr_t program) {
  for (auto e : _events) {
    enqueue_audio_event(program, e._time, e._dur, e._note, e._vel);
  }
}

} //namespace ork::audio::singularity {
