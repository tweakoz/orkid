////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/midi/tweakables.h>
#include <cmath>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
////////////////////////////////////////////////////////////////////////////////

FloatTweakable::FloatTweakable(TweakableSet* twkset)
    : Tweakable(twkset) {

  _onMidiEvent = [this](tweakable_ptr_t t, ui::event_ptr_t ev) {
    if (ev->_midiController == _knobID) {
      switch (ev->_eventcode) {
        case ui::EventCode::MIDI_CONTROLLER: {
          int step_val = 0;
          int step_mag = _is_down ? _coarse_step : 1;

          switch (ev->_midiValue) {
            case 63: // step down
              step_val = -step_mag;
              break;
            case 65: // step up
              step_val = step_mag;
              break;
          }

          _ivalue = std::clamp(_ivalue + step_val, _ivalue_min, _steps);
          float unit_val = float(_ivalue) / float(_steps);
          float shaped = powf(unit_val, _shape);
          float fval = _minval + shaped * (_maxval - _minval);
          _value = fval;
          _twkset->midiKnobSetValue(_knobID, int(unit_val * 127.0f));

          if (_onTwist) {
            _onTwist(t);
          }
          if (_userOnChanged) {
            _userOnChanged(t);
          }
          break;
        }

        case ui::EventCode::MIDI_KEY_DOWN: {
          if (_onPush) {
            _onPush(t);
          } else {
            setFromValue(_defval);
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
          _is_down = true;

          if (_onPush) {
            _onPush(t);
          }
          if (_userOnChanged) {
            _userOnChanged(t);
          }
          break;
        }

        case ui::EventCode::MIDI_KEY_UP: {
          _is_down = false;
          if (_onRelease) {
            _onRelease(t);
          }
          if (_userOnChanged) {
            _userOnChanged(t);
          }
          break;
        }

        default:
          break;
      }
    }
  };
}

void FloatTweakable::finalize() {
  setFromValue(_defval);
  updateColor(_switch_color);
  _twkset->midiKnobSetAnimState(_knobID, KNOB_RGB_BRIGHTNESS(30));
  _twkset->midiKnobSetAnimState(_knobID, KNOB_IND_BRIGHTNESS(30));
}

void FloatTweakable::refresh() {
  setFromValue(_value);
  updateColor(_switch_color);
  _twkset->midiKnobSetAnimState(_knobID, KNOB_RGB_BRIGHTNESS(30));
  _twkset->midiKnobSetAnimState(_knobID, KNOB_IND_BRIGHTNESS(30));
}

float FloatTweakable::valToUnit(float val) const {
  float unit_val = (val - _minval) / (_maxval - _minval);
  unit_val = std::clamp(unit_val, 0.0f, 1.0f);
  return unit_val;
}

void FloatTweakable::setFromValue(float val) {
  float unit_val = valToUnit(val);
  float unshaped = powf(unit_val, 1.0f / _shape);
  _ivalue = std::clamp(int(unshaped * _steps), _ivalue_min, _ivalue_max);
  _value = val;
  _twkset->midiKnobSetValue(_knobID, int(unit_val * 127.0f));
}

void FloatTweakable::recompute() {
  setFromValue(_value);
  _ivalue_min = 0;
  _ivalue_max = _steps;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
