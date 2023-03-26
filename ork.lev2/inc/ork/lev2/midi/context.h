////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
/////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
/////////////////////////////////////////////////////////////////////////////

struct InputContext;
struct OutputContext;

/////////////////////////////////////////////////////////////////////////////

using byte_t = unsigned char;
using message_t = std::vector<byte_t>;
using midi_callback_t = void(*)(double deltatime, message_t* message, void* userData);

using inputcontext_ptr_t = std::shared_ptr<InputContext>;
using outputcontext_ptr_t = std::shared_ptr<OutputContext>;

using midiinputmap_t = std::map<std::string, int>;
using midioutputmap_t = std::map<std::string, int>;

/////////////////////////////////////////////////////////////////////////////

struct InputContext {


  static inputcontext_ptr_t instance();

  InputContext();
  ~InputContext();
  midiinputmap_t enumerateMidiInputs();
  void startMidiInputByName(std::string named,midi_callback_t input_callback);
  void startMidiInputByIndex(int inputid,midi_callback_t input_callback);
  svar128_t _impl;
  midiinputmap_t _portmap;
};

/////////////////////////////////////////////////////////////////////////////

struct OutputContext {


  static outputcontext_ptr_t instance();

  OutputContext();
  ~OutputContext();
  midioutputmap_t enumerateMidiOutputs();
  void openPort(int index);
  void sendMessage(message_t& message);
  svar128_t _impl;
  midioutputmap_t _portmap;
};

/////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
