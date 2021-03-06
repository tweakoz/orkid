////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "harness.h"

Sequence::Sequence(float tempo)
    : _tempo(tempo) {
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
void seq1(float tempo, int basebar, prgdata_constptr_t program) {
  Sequence sq(tempo);
  for (int baro = 0; baro < 4; baro += 2) {
    int dur = 8 + (baro >> 1) * 8;
    int bar = basebar + baro;
    sq.addNote(bar + 0, 0, 0, 48, 72, dur);
    sq.addNote(bar + 0, 4, 0, 48, 72, dur);
    sq.addNote(bar + 0, 8, 0, 48, 72, dur);
    sq.addNote(bar + 0, 12, 0, 48, 96, dur * 2);
    sq.addNote(bar + 0, 12, 2, 48 + 7, 112, dur * 32);
    sq.addNote(bar + 1, 0, 0, 48, 32, dur);
    sq.addNote(bar + 1, 2, 0, 48, 76, dur);
    sq.addNote(bar + 1, 4, 0, 48, 80, dur);
    sq.addNote(bar + 1, 6, 1, 48, 48, dur);
    sq.addNote(bar + 1, 8, 0, 48, 92, dur);
    sq.addNote(bar + 1, 10, 0, 48, 100, dur);
    sq.addNote(bar + 1, 12, 0, 48, 110, dur);
    sq.addNote(bar + 1, 12, 1, 48 + 8, 127, dur * 3);
    sq.addNote(bar + 1, 14, 3, 48 + 12, 127, dur * 2);
    sq.addNote(bar + 1, 14, -1, 48, 127);
    sq.addNote(bar + 1, 16, -2, 48 + 15, 127);
  }
  sq.enqueue(program);
}
