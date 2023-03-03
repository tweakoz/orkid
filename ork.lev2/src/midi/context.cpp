/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/midi/context.h>

#include <iostream>
#include <cstdlib>

#include <rtmidi/RtMidi.h>
#include <ork/util/logger.h>

/////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::midi {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_midi = logger()->createChannel("midi.context", fvec3(1, 0.3, 1));
using input_impl_t  = std::shared_ptr<RtMidiIn>;
using output_impl_t = std::shared_ptr<RtMidiOut>;
/////////////////////////////////////////////////////////////////////////////
inputcontext_ptr_t InputContext::instance() {
  static auto i = std::make_shared<InputContext>();
  return i;
}
/////////////////////////////////////////////////////////////////////////////
InputContext::InputContext() {
  auto rtinpimpl = std::make_shared<RtMidiIn>();
  _impl.set<input_impl_t>(rtinpimpl);
  enumerateMidiInputs();
}
/////////////////////////////////////////////////////////////////////////////
InputContext::~InputContext() {
  auto rtinpimpl = _impl.get<input_impl_t>();
  rtinpimpl->closePort();
}
/////////////////////////////////////////////////////////////////////////////
midiinputmap_t InputContext::enumerateMidiInputs() {
  auto rtinpimpl      = _impl.get<input_impl_t>();
  unsigned int nPorts = rtinpimpl->getPortCount();
  for (int i = 0; i < nPorts; i++) {
    std::string portname      = toLower(rtinpimpl->getPortName(i));
    _portmap[portname] = i;
  }
  return _portmap;
}
/////////////////////////////////////////////////////////////////////////////
void InputContext::startMidiInputByName(std::string named, midi_callback_t input_callback) {
  logchan_midi->log("startMidiInputByName<%s>", named.c_str() );

  auto it   = _portmap.find(named);
  int index = 0;
  if (it != _portmap.end()) {
    index = it->second;
  }
  startMidiInputByIndex(index, input_callback);
}
/////////////////////////////////////////////////////////////////////////////
void InputContext::startMidiInputByIndex(int inputid, midi_callback_t input_callback) {
  logchan_midi->log("startMidiInputByIndex<%d>", inputid );
  auto rtinpimpl = _impl.get<input_impl_t>();
  rtinpimpl->openPort(inputid);
  rtinpimpl->ignoreTypes( false, true, true );
  // Set our callback function.  This should be done immediately after
  // opening the port to avoid having incoming messages written to the
  // queue.
  rtinpimpl->setCallback(input_callback);
  // Don't ignore sysex, timing, or active sensing messages.
  rtinpimpl->ignoreTypes(true, true, true);
  // Clean up
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

outputcontext_ptr_t OutputContext::instance() {
  return std::make_shared<OutputContext>();
}

OutputContext::OutputContext() {
  auto rtoutimpl = std::make_shared<RtMidiOut>();
  _impl.set<output_impl_t>(rtoutimpl);
  enumerateMidiOutputs();
}
OutputContext::~OutputContext() {
  auto rtoutimpl = _impl.get<output_impl_t>();
}
midioutputmap_t OutputContext::enumerateMidiOutputs() {
  auto rtoutimpl      = _impl.get<output_impl_t>();
  unsigned int nPorts = rtoutimpl->getPortCount();
  for (int i = 0; i < nPorts; i++) {
    std::string portname      = toLower(rtoutimpl->getPortName(i));
    _portmap[portname] = i;
  }
  return _portmap;
}
void OutputContext::openPort(int index) {
  auto rtoutimpl      = _impl.get<output_impl_t>();
  rtoutimpl->openPort( index );
}
void OutputContext::sendMessage(message_t& message) {
  auto rtoutimpl      = _impl.get<output_impl_t>();
  rtoutimpl->sendMessage( &message );
}

/////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::midi
