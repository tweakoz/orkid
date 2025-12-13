////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/midi/context.h>
#include <ork/lev2/ui/event.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/svariant.h>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
////////////////////////////////////////////////////////////////////////////////

struct Tweakable;
struct FloatTweakable;
struct ToggleTweakable;
struct NoOpTweakable;
struct EnumTweakable;
struct EventTweakable;
struct TweakableSet;
struct TweakableTransport;
struct DirectTransport;

using tweakable_ptr_t = std::shared_ptr<Tweakable>;
using f32tweakable_ptr_t = std::shared_ptr<FloatTweakable>;
using toggletweakable_ptr_t = std::shared_ptr<ToggleTweakable>;
using nooptweakable_ptr_t = std::shared_ptr<NoOpTweakable>;
using enumtweakable_ptr_t = std::shared_ptr<EnumTweakable>;
using eventtweakable_ptr_t = std::shared_ptr<EventTweakable>;
using tweakableset_ptr_t = std::shared_ptr<TweakableSet>;
using tweakable_transport_ptr_t = std::shared_ptr<TweakableTransport>;
using direct_transport_ptr_t = std::shared_ptr<DirectTransport>;

////////////////////////////////////////////////////////////////////////////////
// Transport abstraction - handles bidirectional MIDI communication
////////////////////////////////////////////////////////////////////////////////

struct TweakableTransport {
  using on_input_fn_t = std::function<void(ui::event_ptr_t)>;

  virtual ~TweakableTransport() = default;

  // Output - send MIDI bytes to device
  virtual void sendMessage(message_t& msg) = 0;

  // Set callback for input - transport calls this when MIDI arrives
  void setInputHandler(on_input_fn_t fn) { _on_input = fn; }

  on_input_fn_t _on_input;
};

////////////////////////////////////////////////////////////////////////////////
// Direct transport - uses InputContext/OutputContext directly (same process)
////////////////////////////////////////////////////////////////////////////////

struct DirectTransport : TweakableTransport {
  DirectTransport() = default;
  ~DirectTransport() override = default;

  // Open MIDI device by name, start input callback
  bool open(const std::string& device_name);
  void close();

  void sendMessage(message_t& msg) override;

  inputcontext_ptr_t _input;
  outputcontext_ptr_t _output;
  std::string _device_name;
};

////////////////////////////////////////////////////////////////////////////////
// Tweakable callback types
////////////////////////////////////////////////////////////////////////////////

using on_tweak_fn_t = std::function<void(tweakable_ptr_t)>;
using on_midi_fn_t = std::function<void(tweakable_ptr_t, ui::event_ptr_t)>;

////////////////////////////////////////////////////////////////////////////////
// Base Tweakable
////////////////////////////////////////////////////////////////////////////////

struct Tweakable {
  static constexpr float KDOUBLE_PUSH_TIME = 0.5f;

  Tweakable(TweakableSet* twkset);
  virtual ~Tweakable() = default;
  virtual void finalize() {}
  virtual void refresh() {}
  void updateColor(int color);

  std::string _name;
  int _knobID = 0;
  int _switch_color = 0;
  on_tweak_fn_t _onTwist = nullptr;
  on_tweak_fn_t _onPush = nullptr;
  on_tweak_fn_t _onDefault = nullptr;
  on_tweak_fn_t _onRelease = nullptr;
  on_tweak_fn_t _onDoublePush = nullptr;
  on_tweak_fn_t _onFinalize = nullptr;
  on_tweak_fn_t _userOnChanged = nullptr;
  on_midi_fn_t _onMidiEvent = nullptr;
  float _prev_push_time = 0.0f;
  svar64_t _impl;
  TweakableSet* _twkset = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// FloatTweakable - continuous float value with shaping
////////////////////////////////////////////////////////////////////////////////

struct FloatTweakable : public Tweakable {
  FloatTweakable(TweakableSet* twkset);

  void recompute();
  float valToUnit(float val) const;
  void setFromValue(float val);
  void finalize() final;
  void refresh() final;

  float _value = 0.0f;
  bool _is_down = false;
  int _ivalue = 0;
  int _ivalue_min = 0;
  int _ivalue_max = 0;
  int _steps = 100;
  int _coarse_step = 10;
  float _shape = 1.0f;
  float _minval = 0.0f;
  float _maxval = 1.0f;
  float _defval = 0.0f;
};

////////////////////////////////////////////////////////////////////////////////
// ToggleTweakable - boolean on/off toggle
////////////////////////////////////////////////////////////////////////////////

struct ToggleTweakable : public Tweakable {
  ToggleTweakable(TweakableSet* twkset);

  void finalize() final;
  void refresh() final;

  bool _value = false;
};

////////////////////////////////////////////////////////////////////////////////
// NoOpTweakable - placeholder for unassigned knobs
////////////////////////////////////////////////////////////////////////////////

struct NoOpTweakable : public Tweakable {
  NoOpTweakable(TweakableSet* twkset);

  void finalize() final;
  void refresh() final;
};

////////////////////////////////////////////////////////////////////////////////
// EnumTweakable - discrete state cycling
////////////////////////////////////////////////////////////////////////////////

struct EnumTweakable : public Tweakable {
  EnumTweakable(TweakableSet* twkset) : Tweakable(twkset) {}

  std::unordered_map<std::string, int> _enum_to_color;
  std::string _value;
  std::string _default;
};

////////////////////////////////////////////////////////////////////////////////
// EventTweakable - push/release/twist events (not persisted)
////////////////////////////////////////////////////////////////////////////////

struct EventTweakable : public Tweakable {
  EventTweakable(TweakableSet* twkset) : Tweakable(twkset) {}
};

////////////////////////////////////////////////////////////////////////////////
// LED control helpers (for Midi Fighter Twister and similar controllers)
////////////////////////////////////////////////////////////////////////////////

uint8_t KNOB_RGB_STROBE(int level);
uint8_t KNOB_RGB_PULSE(int level);
uint8_t KNOB_RGB_BRIGHTNESS(int level);
uint8_t KNOB_IND_STROBE(int level);
uint8_t KNOB_IND_PULSE(int level);
uint8_t KNOB_IND_BRIGHTNESS(int level);
uint8_t KNOB_RGB_RAINBOW();

////////////////////////////////////////////////////////////////////////////////
// TweakableSet - manages a collection of tweakables
////////////////////////////////////////////////////////////////////////////////

struct TweakableSet {
  TweakableSet(tweakable_transport_ptr_t transport);

  void _addTweakable(tweakable_ptr_t twk);

  // MIDI output methods
  void midiKnobSetColor(uint8_t knobID, uint8_t color);
  void midiKnobSetBrightness(uint8_t knobID, uint8_t level);
  void midiKnobSetValue(uint8_t knobID, uint8_t v127);
  void midiKnobSetAnimState(uint8_t knobID, uint8_t state);
  void midiKnobClearAnimState(uint8_t knobID);

  // Lifecycle
  void finalize();
  void resetKnobStates();
  void setPage(int page);

  // Input handling - called by transport when MIDI arrives
  void onMidiEvent(ui::event_ptr_t ev);

  // Persistence (TODO: implement)
  void saveToFile(const std::string& filename) const;
  void loadFromFile(const std::string& filename);

  std::map<int, tweakable_ptr_t> _tweakable_by_knobID;
  std::map<std::string, tweakable_ptr_t> _tweakable_by_name;
  Timer _timer;
  std::string _params_path;
  tweakable_transport_ptr_t _transport;
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
