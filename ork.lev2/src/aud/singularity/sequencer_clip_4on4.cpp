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
eventiterator_ptr_t FourOnFloorClip::firstEvent() const {
  auto first_ts = std::make_shared<TimeStamp>(0,0,0);
  auto iter = std::make_shared<EventIterator>(first_ts);
  auto event = std::make_shared<Event>();
  event->_timestamp = first_ts;
  event->_duration = std::make_shared<TimeStamp>(0,1,-1);
  event->_note = _note;
  event->_vel = _vel;
  iter->_event = event;
  return iter;
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t FourOnFloorClip::nextEvent(eventiterator_ptr_t abstract_iter) const {
  auto inp_ts = abstract_iter->_timestamp;
  auto out_ts = inp_ts->clone();
  out_ts->_beats += 1;
  if( out_ts->_beats >= 4 ){
    out_ts->_beats = 0;
    out_ts->_measures += 1;
  }
  auto out_iter = std::make_shared<EventIterator>(out_ts);
  auto event = std::make_shared<Event>();
  event->_timestamp = out_ts;
  event->_duration = std::make_shared<TimeStamp>(0,1,-1);
  event->_note = _note;
  event->_vel = _vel;
  out_iter->_event = event;
  return out_iter;
}

////////////////////////////////////////////////////////////////

bool FourOnFloorClip::eventValid(eventiterator_ptr_t abstract_iter) const {
  return ( abstract_iter != nullptr );
}

////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
