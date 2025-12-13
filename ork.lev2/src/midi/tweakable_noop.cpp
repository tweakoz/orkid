////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/midi/tweakables.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
////////////////////////////////////////////////////////////////////////////////

NoOpTweakable::NoOpTweakable(TweakableSet* twkset)
    : Tweakable(twkset) {

  _onMidiEvent = [this](tweakable_ptr_t t, ui::event_ptr_t ev) {
    if (ev->_midiController == _knobID) {
      switch (ev->_eventcode) {
        case ui::EventCode::MIDI_KEY_UP:
        case ui::EventCode::MIDI_KEY_DOWN: {
          _twkset->midiKnobSetBrightness(_knobID, 0);
          _twkset->midiKnobSetColor(_knobID, 0);
          break;
        }
        default:
          break;
      }
    }
  };
}

void NoOpTweakable::finalize() {
  _twkset->midiKnobSetColor(_knobID, 0);
  _twkset->midiKnobSetAnimState(_knobID, KNOB_RGB_BRIGHTNESS(0));
  _twkset->midiKnobSetAnimState(_knobID, KNOB_IND_BRIGHTNESS(0));
}

void NoOpTweakable::refresh() {
  finalize();
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
