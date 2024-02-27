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

struct IterationState {
    int step = -1;
    int track = -1; // 0=A, 1=B, 2=C, 3=D
};

////////////////////////////////////////////////////////////////

Drum4Clip::Drum4Clip(){
  _trackA.resize(16);
  _trackB.resize(16);
  _trackC.resize(16);
  _trackD.resize(16);
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t Drum4Clip::firstEvent() const {
  return _makeEvent(0,0);
}

////////////////////////////////////////////////////////////////

bool Drum4Clip::eventValid(eventiterator_ptr_t abstract_iter) const {
  return ( abstract_iter != nullptr );
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t Drum4Clip::_makeEvent(int step, int track_id) const {
    auto ts = std::make_shared<TimeStamp>(0,0,0); // You might need to adjust this based on actual timing
    auto iter = std::make_shared<EventIterator>(ts);
    auto event = std::make_shared<Event>();
    event->_timestamp = ts;
    event->_duration = std::make_shared<TimeStamp>(0,1,-1); // Assuming fixed duration for simplicity

    // Adjust note and velocity based on the track
    switch (track_id) {
        case 0: // Track A
            event->_note = _noteA;
            event->_vel = _velA;
            break;
        case 1: // Track B
            event->_note = _noteB;
            event->_vel = _velB;
            break;
        case 2: // Track C
            event->_note = _noteC;
            event->_vel = _velC;
            break;
        case 3: // Track D
            event->_note = _noteD;
            event->_vel = _velD;
            break;
    }

    iter->_event = event;
    // Use IterationState to keep track of the current step and track
    auto state = IterationState{step, track_id};
    iter->_impl.set<IterationState>(state); // Assuming this is how you set values in your variant class

    printf( "mkev tr<%d> step<%d>\n", track_id, step);
    return iter;
}

eventiterator_ptr_t Drum4Clip::nextEvent(eventiterator_ptr_t abstract_iter) const {
    auto state = abstract_iter->_impl.get<IterationState>(); // Retrieve the current state
    int next_step = state.step;
    int next_track = state.track + 1;

    // Check if we need to move to the next step
    if (next_track > 3) {
        next_track = 0;
        next_step = (next_step + 1) % 16; // Assuming 16 steps per sequence, wrap around
    }

    // Find the next event, considering all tracks
    for (int track = next_track; track < 4; ++track) {
        const std::vector<bool>* current_track = nullptr;
        switch (track) {
            case 0: current_track = &_trackA; break;
            case 1: current_track = &_trackB; break;
            case 2: current_track = &_trackC; break;
            case 3: current_track = &_trackD; break;
        }
        if ((*current_track)[next_step]) { // If there's an event in this track at this step
            return _makeEvent(next_step, track);
        }
    }

    // If no event found in the remaining tracks, move to next step and repeat search
    // This might involve recursive call or a loop to find the next available event
    // Depending on your design, you might need to handle this scenario

    return nullptr; // Or however you signify the end or no event
}

////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
