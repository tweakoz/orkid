////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include "krztypes.h"
#include "synthdata.h"
#include "layer.h"
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/svariant.h>
//#include <ork/lev2/gfx/gfxenv.h>

namespace ork::audio::singularity {

struct Event {
  float _time = 0.0f;
  float _dur  = 0.0f;
  int _note   = 0;
  int _vel    = 0;
};

struct Clip{
  std::vector<Event> _events;
};

using clipmap_t = std::map<float, clip_ptr_t>;

struct Track{
    clipmap_t _clips_by_timestamp;
};

using trackmap_t = std::unordered_map<std::string, track_ptr_t>;

void enqueue_audio_event(
    prgdata_constptr_t prog, //
    float time,
    float duration,
    int midinote,
    int velocity = 128);


struct Sequence{

  Sequence();
  std::vector<Event> _events;
  float mbs2time(int meas, int sixteenth, int clocks) const;
  void addNote(
      int meas, //
      int sixteenth,
      int clocks,
      int note,
      int vel,
      int dur = 1);
  void enqueue(prgdata_constptr_t program);

  trackmap_t _tracks; 
  float _tempo = 0.0f;

};

using seqmap_t = std::unordered_map<std::string, sequence_ptr_t>;

struct Sequencer{
    seqmap_t _sequences;
};

} //namespace ork::audio::singularity {
