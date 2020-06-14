#include "harness.h"
#include <iostream>
#include <cstdlib>
#include "RtMidi.h"

void mycallback(double deltatime, std::vector<unsigned char>* message, void* userData) {
  auto numbytes = message->size();

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
          auto pi   = synth::instance()->liveKeyOn(note, vel, prog);
          auto it   = _keymap.find(note);
          OrkAssert(it == _keymap.end());
          _keymap[note] = pi;
          break;
        }
        case 8: { // note off
          int note = message->at(1);
          auto it  = _keymap.find(note);
          OrkAssert(it != _keymap.end());
          auto pi = it->second;
          synth::instance()->liveKeyOff(pi);
          _keymap.erase(it);
          break;
        }
      }
      break;
    }
  }
  for (unsigned int i = 0; i < numbytes; i++)
    printf("%02x ", (int)message->at(i));
  printf("\n");
}
void startMidi() {
  RtMidiIn* midiin = new RtMidiIn();
  // Check available ports.
  unsigned int nPorts = midiin->getPortCount();
  if (nPorts == 0) {
    std::cout << "No ports available!\n";
    return;
  }
  midiin->openPort(0);
  // Set our callback function.  This should be done immediately after
  // opening the port to avoid having incoming messages written to the
  // queue.
  midiin->setCallback(&mycallback);
  // Don't ignore sysex, timing, or active sensing messages.
  midiin->ignoreTypes(false, false, false);
  // Clean up
}
