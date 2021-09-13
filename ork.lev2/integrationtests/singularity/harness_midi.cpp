#include "harness.h"
#include <iostream>
#include <cstdlib>

#if defined(__APPLE__)
#include <RtMidi.h>
#else
#include <rtmidi/RtMidi.h>
#endif

void mycallback(double deltatime, std::vector<unsigned char>* message, void* userData) {
  auto numbytes = message->size();
  for (unsigned int i = 0; i < numbytes; i++)
    printf("%02x ", (int)message->at(i));
  printf("\n");

  static std::map<int, programInst*> _keymap;

  switch (numbytes) {
    case 3: {
      int cmdbyte = message->at(0);
      int cmd     = cmdbyte >> 4;
      int chan    = cmdbyte & 0xf;
      switch (cmd) {
        case 9: { // note on
          int note  = message->at(1);
          int vel   = message->at(2);
          auto prog = synth::instance()->_globalprog;
          auto it   = _keymap.find(note);
          if (it == _keymap.end()) {
            auto pi       = synth::instance()->liveKeyOn(note, vel, prog);
            _keymap[note] = pi;
          }
          break;
        }
        case 8: { // note off
          int note = message->at(1);
          auto it  = _keymap.find(note);
          if (it != _keymap.end()) {
            auto pi = it->second;
            synth::instance()->liveKeyOff(pi);
            _keymap.erase(it);
          }
          break;
        }
      }
      break;
    }
  }
}

using impl_t = std::shared_ptr<RtMidiIn>;

midicontext_ptr_t MidiContext::instance() {
  static midicontext_ptr_t ctx = std::make_shared<MidiContext>();
  return ctx;
}

MidiContext::MidiContext() {
  auto rtinpimpl = std::make_shared<RtMidiIn>();
  _impl.set<impl_t>(rtinpimpl);
  enumerateMidiInputs();
}
MidiContext::~MidiContext() {
  auto rtinpimpl = _impl.get<impl_t>();
}
MidiContext::midiinputmap_t MidiContext::enumerateMidiInputs() {
  auto rtinpimpl = _impl.get<impl_t>();
  midiinputmap_t rval;
  unsigned int nPorts = rtinpimpl->getPortCount();
  for (int i = 0; i < nPorts; i++) {
    auto portname      = rtinpimpl->getPortName(i);
    _portmap[portname] = i;
  }
  return rval;
}
void MidiContext::startMidiInputByName(std::string named) {
  auto it   = _portmap.find(named);
  int index = 0;
  if (it != _portmap.end()) {
    index = it->second;
  }
  startMidiInputByIndex(index);
}
void MidiContext::startMidiInputByIndex(int inputid) {
  auto rtinpimpl = _impl.get<impl_t>();
  rtinpimpl->openPort(inputid);
  // Set our callback function.  This should be done immediately after
  // opening the port to avoid having incoming messages written to the
  // queue.
  rtinpimpl->setCallback(&mycallback);
  // Don't ignore sysex, timing, or active sensing messages.
  rtinpimpl->ignoreTypes(true, true, true);
  // Clean up
}
