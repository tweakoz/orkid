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

event_ptr_t EventClip::createNoteEvent(timestamp_ptr_t ts, timestamp_ptr_t dur, int note, int vel){
  auto event = std::make_shared<Event>();
  event->_timestamp = ts;
  event->_duration = dur;
  event->_note = note;
  event->_vel = vel;
  _events.insert(std::make_pair(ts,event));
  return event;
}

void EventClip::clear() {
  _events.clear();
}

void EventClip::quantize(int quant_clks){

  auto timebase = _sequence->_timebase;

  auto events = _events;

  _events.clear();
  printf("///////////////////////////////\n");
  for( auto ev : events ){
    auto event = ev.second;
    auto ts_start = ev.first;

    int OM = ts_start->_measures;
    int OB = ts_start->_beats;
    int OC = ts_start->_clocks;

    int clocks = ts_start->_clocks;
    clocks += (ts_start->_beats)*timebase->_ppq;
    clocks += (ts_start->_measures*4)*timebase->_ppq;
    int rem = clocks%quant_clks;
    int qclocks = (rem<quant_clks/2) //
                ? (clocks-rem) //
                : (clocks+quant_clks-rem);

   qclocks = (qclocks+clocks)>>1;

    auto new_ts = std::make_shared<TimeStamp>();
    new_ts->_clocks = qclocks;
    new_ts = timebase->reduceTimeStamp(new_ts);

    int NM = new_ts->_measures;
    int NB = new_ts->_beats;
    int NC = new_ts->_clocks;

     printf("quant clocks [ %05d : %08x : %02d %02d %03d ] -> [ %05d : %08x : %02d %02d %03d ] \n",clocks,clocks, OM, OB, OC, qclocks, qclocks, NM, NB, NC);

    _events.insert( std::make_pair(new_ts,event) );
  }
  printf("///////////////////////////////\n");
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t EventClip::firstEvent() const {
  if( _events.size() > 0 ){
    auto first_it = _events.begin();
    auto first_ts = first_it->first;
    auto first_event = first_it->second;
    auto iter = std::make_shared<EventIterator>(first_ts);
    iter->_event = first_event;
    iter->_impl.set<evmap_it_t>(first_it);
    return iter;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t EventClip::nextEvent(eventiterator_ptr_t abstract_iter) const {
  auto evmap_iter = abstract_iter->_impl.get<evmap_it_t>();
  auto next_iter = ++evmap_iter;
  if( next_iter != _events.end() ){
    auto next_ts = next_iter->first;
    auto next_event = next_iter->second;
    auto iter = std::make_shared<EventIterator>(next_ts);
    iter->_event = next_event;
    iter->_impl.set<evmap_it_t>(next_iter);
    printf( "nextev<%p> ts<%d>\n", next_event.get(), next_ts->_measures );
    return iter;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

bool EventClip::eventValid(eventiterator_ptr_t abstract_iter) const {
  if( auto as_evmap_iter = abstract_iter->_impl.tryAs<evmap_it_t>() ){
    auto evmap_iter = as_evmap_iter.value();
    return ( evmap_iter != _events.end() );
  }
  return false;
}

////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
