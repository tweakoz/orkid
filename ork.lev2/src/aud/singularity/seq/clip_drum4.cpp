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
    // PPQ = 100
    ts->_measures = 0;
    ts->_beats = (0 + (step / 4) % 4); // Assuming 4/4 time signature for simplicity
    ts->_clocks = (step % 4) * 25; // Assuming 100 PPQ for simplicity
    event->_timestamp = ts;

    int duration = 10;

    // Adjust note and velocity based on the track
    switch (track_id) {
        case 0: // Track A
            event->_note = _noteA;
            event->_vel = _velA;
            duration = _durA;
            break;
        case 1: // Track B
            event->_note = _noteB;
            event->_vel = _velB;
            duration = _durB;
            break;
        case 2: // Track C
            event->_note = _noteC;
            event->_vel = _velC;
            duration = _durC;
            break;
        case 3: // Track D
            event->_note = _noteD;
            event->_vel = _velD;
            duration = _durD;
            break;
    }

    event->_duration = std::make_shared<TimeStamp>(0,0,duration); 

    iter->_event = event;
    // Use IterationState to keep track of the current step and track
    auto state = IterationState{step, track_id};
    iter->_impl.set<IterationState>(state); // Assuming this is how you set values in your variant class

    return iter;
}

////////////////////////////////////////////////////////////////

eventiterator_ptr_t Drum4Clip::nextEvent(eventiterator_ptr_t abstract_iter) const {
    auto state = abstract_iter->_impl.get<IterationState>(); // Retrieve the current state
    int next_step = state.step;
    int next_track = state.track + 1;

    while (next_step < 16) { // Assuming 16 steps are the total steps in the sequence
        // Check if we need to move to the next step
        if (next_track > 3) {
            next_track = 0;
            next_step++;
        }

        for (int track = next_track; track < 4; ++track) {
            const std::vector<bool>* current_track = nullptr;
            switch (track) {
                case 0: current_track = &_trackA; break;
                case 1: current_track = &_trackB; break;
                case 2: current_track = &_trackC; break;
                case 3: current_track = &_trackD; break;
            }
            if (next_step < 16 && (*current_track)[next_step]) { // If there's an event in this track at this step
                return _makeEvent(next_step, track);
            }
        }

        // If no event was found in the current step for any track, prepare to check the next step
        next_track = 0; // Reset track to start from the first track in the next iteration
        if (next_step < 15) {
            next_step++; // Move to the next step if not already at the end
        } else {
            break; // End search if we've reached the last step
        }
    }

    return nullptr; // Return nullptr if no next event is found
}


////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////
