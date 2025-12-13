////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/midi/tweakables.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
////////////////////////////////////////////////////////////////////////////////

constexpr int MIDIDELAY_US = 12000; // microseconds

////////////////////////////////////////////////////////////////////////////////
// LED control helpers
////////////////////////////////////////////////////////////////////////////////

uint8_t KNOB_RGB_STROBE(int level) {
  OrkAssert(level >= 0 && level <= 7);
  return 1 + level;
}

uint8_t KNOB_RGB_PULSE(int level) {
  OrkAssert(level >= 0 && level <= 7);
  return 9 + level;
}

uint8_t KNOB_RGB_BRIGHTNESS(int level) {
  OrkAssert(level >= 0 && level <= 30);
  return 17 + level;
}

uint8_t KNOB_IND_STROBE(int level) {
  OrkAssert(level >= 0 && level <= 7);
  return 49 + level;
}

uint8_t KNOB_IND_PULSE(int level) {
  OrkAssert(level >= 0 && level <= 7);
  return 57 + level;
}

uint8_t KNOB_IND_BRIGHTNESS(int level) {
  OrkAssert(level >= 0 && level <= 30);
  return 65 + level;
}

uint8_t KNOB_RGB_RAINBOW() {
  return 127;
}

////////////////////////////////////////////////////////////////////////////////
// DirectTransport
////////////////////////////////////////////////////////////////////////////////

// Global C callback for RtMidi - routes to DirectTransport instance via userData
static void directTransportMidiCallback(double deltatime, message_t* message, void* userData) {
  auto self = static_cast<DirectTransport*>(userData);
  if (!self->_on_input || message->size() < 3) {
    return;
  }

  int cmdbyte = message->at(0);
  int cmd = cmdbyte >> 4;
  int controller = message->at(1);
  int value = message->at(2);

  auto ev = std::make_shared<ui::Event>();
  ev->_midiController = controller;
  ev->_midiValue = value;

  switch (cmd) {
    case 0x9: // key down
      ev->_eventcode = ui::EventCode::MIDI_KEY_DOWN;
      self->_on_input(ev);
      break;
    case 0x8: // key up
      ev->_eventcode = ui::EventCode::MIDI_KEY_UP;
      self->_on_input(ev);
      break;
    case 0xb: // controller
      ev->_eventcode = ui::EventCode::MIDI_CONTROLLER;
      self->_on_input(ev);
      break;
    default:
      break;
  }
}

bool DirectTransport::open(const std::string& device_name) {
  _device_name = device_name;

  // Create input context
  _input = std::make_shared<InputContext>();
  auto inputs = _input->enumerateMidiInputs();

  // Create output context
  _output = std::make_shared<OutputContext>();
  auto outputs = _output->enumerateMidiOutputs();

  // Find device by name
  int input_index = -1;
  for (auto& i : inputs) {
    if (i.first.find(device_name) != std::string::npos) {
      input_index = i.second;
      break;
    }
  }

  int output_index = -1;
  for (auto& o : outputs) {
    if (o.first.find(device_name) != std::string::npos) {
      output_index = o.second;
      break;
    }
  }

  if (input_index < 0 || output_index < 0) {
    return false;
  }

  // Open output port
  _output->openPort(output_index);

  // Start input with global callback, passing this as userData
  _input->startMidiInputByIndex(input_index, directTransportMidiCallback, this);

  return true;
}

void DirectTransport::close() {
  _input.reset();
  _output.reset();
}

void DirectTransport::sendMessage(message_t& msg) {
  if (_output) {
    _output->sendMessage(msg);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Tweakable base
////////////////////////////////////////////////////////////////////////////////

Tweakable::Tweakable(TweakableSet* twkset)
    : _twkset(twkset) {
}

void Tweakable::updateColor(int color) {
  OrkAssert(color >= 0 && color < 127);
  _twkset->midiKnobSetColor(_knobID, color);
  _switch_color = color;
}

////////////////////////////////////////////////////////////////////////////////
// TweakableSet
////////////////////////////////////////////////////////////////////////////////

TweakableSet::TweakableSet(tweakable_transport_ptr_t transport)
    : _transport(transport) {
  _timer.Start();

  // Initialize all 64 knobs with NoOpTweakable
  for (int i = 0; i < 64; i++) {
    auto noop = std::make_shared<NoOpTweakable>(this);
    noop->_name = "NOOP";
    noop->_knobID = i;
    _tweakable_by_knobID[i] = noop;
  }

  // Wire up input handler
  if (_transport) {
    _transport->setInputHandler([this](ui::event_ptr_t ev) {
      this->onMidiEvent(ev);
    });
  }
}

void TweakableSet::_addTweakable(tweakable_ptr_t twk) {
  auto it_name = _tweakable_by_name.find(twk->_name);
  OrkAssert(it_name == _tweakable_by_name.end());
  _tweakable_by_knobID[twk->_knobID] = twk;
  _tweakable_by_name[twk->_name] = twk;
  twk->_twkset = this;
}

void TweakableSet::finalize() {
  for (int i = 0; i < 2; i++) {
    ::usleep(50000);
    resetKnobStates();
  }
  for (auto& it : _tweakable_by_knobID) {
    auto t = it.second;
    ::usleep(MIDIDELAY_US);
    t->finalize();
  }
}

void TweakableSet::midiKnobSetColor(uint8_t knobID, uint8_t color) {
  ::usleep(MIDIDELAY_US);
  message_t msg = {0x91, knobID, color};
  _transport->sendMessage(msg);
}

void TweakableSet::midiKnobSetBrightness(uint8_t knobID, uint8_t level) {
  OrkAssert(level <= 30);
  midiKnobSetAnimState(knobID, KNOB_RGB_BRIGHTNESS(level));
}

void TweakableSet::midiKnobSetValue(uint8_t knobID, uint8_t v127) {
  message_t msg = {0xB0, knobID, v127};
  _transport->sendMessage(msg);
}

void TweakableSet::midiKnobSetAnimState(uint8_t knobID, uint8_t state) {
  ::usleep(MIDIDELAY_US);
  message_t msg = {0x95, knobID, state};
  _transport->sendMessage(msg);
}

void TweakableSet::midiKnobClearAnimState(uint8_t knobID) {
  midiKnobSetAnimState(knobID, KNOB_RGB_BRIGHTNESS(0));
  midiKnobSetAnimState(knobID, KNOB_IND_BRIGHTNESS(0));
  midiKnobSetColor(knobID, 0);
}

void TweakableSet::resetKnobStates() {
  setPage(0);
}

void TweakableSet::setPage(int page) {
  OrkAssert(page >= 0 && page <= 3);
  int istart = page * 16;
  int iend = istart + 15;
  for (int i = istart; i <= iend; i++) {
    auto it = _tweakable_by_knobID.find(i);
    if (it != _tweakable_by_knobID.end()) {
      auto t = it->second;
      t->refresh();
    }
  }
}

void TweakableSet::onMidiEvent(ui::event_ptr_t ev) {
  int controller = ev->_midiController;
  int value = ev->_midiValue;

  auto it = _tweakable_by_knobID.find(controller);
  if (it == _tweakable_by_knobID.end()) {
    return;
  }

  auto knob = it->second;

  switch (ev->_eventcode) {
    case ui::EventCode::MIDI_CONTROLLER:
      // Handle page switching
      if (value == 127) {
        setPage(controller);
      }
      // Fall through for knob handling
      if (knob->_onMidiEvent) {
        knob->_onMidiEvent(knob, ev);
      }
      break;

    case ui::EventCode::MIDI_KEY_DOWN:
    case ui::EventCode::MIDI_KEY_UP:
      if (knob->_onMidiEvent) {
        knob->_onMidiEvent(knob, ev);
      }
      break;

    default:
      break;
  }
}

void TweakableSet::saveToFile(const std::string& filename) const {
  // TODO: implement JSON serialization
}

void TweakableSet::loadFromFile(const std::string& filename) {
  // TODO: implement JSON deserialization
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
