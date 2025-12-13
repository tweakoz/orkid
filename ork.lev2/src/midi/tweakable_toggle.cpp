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

ToggleTweakable::ToggleTweakable(TweakableSet* twkset)
    : Tweakable(twkset) {

  _onMidiEvent = [this](tweakable_ptr_t t, ui::event_ptr_t ev) {
    if (ev->_midiController == _knobID) {
      switch (ev->_eventcode) {
        case ui::EventCode::MIDI_KEY_DOWN: {
          _value = !_value;
          _twkset->midiKnobSetBrightness(_knobID, _value ? 30 : 0);

          if (_onPush) {
            _onPush(t);
          }

          float push_time = _twkset->_timer.SecsSinceStart();
          if (_prev_push_time > 0.0f) {
            float delta = push_time - _prev_push_time;
            if (delta < KDOUBLE_PUSH_TIME) {
              if (_onDoublePush) {
                _onDoublePush(t);
              }
            }
          }
          _prev_push_time = push_time;

          if (_onPush) {
            _onPush(t);
          }
          if (_userOnChanged) {
            _userOnChanged(t);
          }
          break;
        }

        case ui::EventCode::MIDI_KEY_UP: {
          if (_onRelease) {
            _onRelease(t);
          }
          break;
        }

        default:
          break;
      }
    }
  };
}

void ToggleTweakable::finalize() {
  _twkset->midiKnobSetBrightness(_knobID, _value ? 30 : 0);
  updateColor(_switch_color);
}

void ToggleTweakable::refresh() {
  _twkset->midiKnobSetAnimState(_knobID, KNOB_IND_BRIGHTNESS(0));
  _twkset->midiKnobSetBrightness(_knobID, _value ? 30 : 0);
  updateColor(_switch_color);
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
