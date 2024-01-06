////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"


void seq1(float tempo, int basebar, prgdata_constptr_t program) {
  Sequence sq;
  sq._timebase->_tempo = tempo;
  //sq._tempo = tempo;
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
