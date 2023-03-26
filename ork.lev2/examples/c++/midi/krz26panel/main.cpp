////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <ork/lev2/midi/context.h>

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

void krz_callback(double deltatime, midi::message_t* message, void* userData) {
  auto numbytes = message->size();

  bool handled = false;
  int cmdbyte = message->at(0);
  int cmd     = cmdbyte >> 4;
  int chan    = cmdbyte & 0xf;
  switch (numbytes) {
    case 2: {
      switch (cmd) {
        case 0xc: { // control change
          int program  = message->at(1);
          handled = true;
          break;
        }
      }
      break;
    }
    case 3: {
      switch (cmd) {
        case 8: { // note off
          int note = message->at(1);
          //printf( "note-off ch<%d> note<%d>\n", chan, note);
          handled = true;
          break;
        }
        case 9: { // note on
          int note  = message->at(1);
          int vel   = message->at(2);
          //printf( "note-on ch<%d> note<%d> vel<%d>\n", chan, note, vel);
          handled = true;
          break;
        }
        case 0xb: { // control change
          int controller  = message->at(1);
          int value   = message->at(2);
          //printf( "note-on ch<%d> note<%d> vel<%d>\n", chan, note, vel);
          handled = true;
          break;
        }
      }
      break;
    }
  }
  if(1){ //not handled){
    printf("recv: ");
    for (unsigned int i = 0; i < numbytes; i++)
      printf("%02x ", (int)message->at(i));
    printf("\n");
  }
}

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv){

  auto midiinctx = midi::InputContext::instance();
  auto inputs = midiinctx->enumerateMidiInputs();
  auto midioutctx = midi::OutputContext::instance();
  auto outputs = midioutctx->enumerateMidiOutputs();
  int input_krz = -1;
  int output_krz = -1;
  for( auto i : inputs ){
    printf( "midiinp<%d:%s>\n", i.second, i.first.c_str() );
    if(i.first.find("krz2600")!=std::string::npos){
      input_krz = i.second;
    }
  }
  for( auto o : outputs ){
    printf( "midiout<%d:%s>\n", o.second, o.first.c_str() );
    if(o.first.find("krz2600")!=std::string::npos){
      output_krz = o.second;
    }
  }
  printf( "input_krz<%d> output_krz<%d>\n", input_krz, output_krz);

  midiinctx->startMidiInputByIndex(input_krz,krz_callback);
  midioutctx->openPort(output_krz);

  auto send_message = [&](midi::message_t& msg){
    printf( "sending: " );
    for( auto ch : msg ){
       printf( "%02x ", ch );
    }
    printf("\n");
    midioutctx->sendMessage(msg);
  };

  auto note_on = [&](int chan, int note, int vel){
      midi::message_t msg;
      msg.push_back(0x90|chan);  // note on chan 0
      msg.push_back(note);  // note val
      msg.push_back(vel);  // note vel
      send_message(msg);
  };
  auto note_off = [&](int chan, int note, int vel){
      midi::message_t msg;
      msg.push_back(0x80|chan);  // note on chan 0
      msg.push_back(note);  // note val
      msg.push_back(vel);  // note vel
      send_message(msg);
  };
  auto send_sysex = [&](midi::message_t& msg){
    msg.insert(msg.begin(), 4, 0);
    msg[0] = 0xf0; // start of sysex
    msg[1] = 0x07; // manufacturer ID : Kuzweil
    msg[2] = 0x00; // device sysex id
    msg[3] = 0x78; // product id (k2600)
    msg.push_back(0xf7);  // end of sysex
    send_message(msg);
  };

  for( int n = 0; n<128; n++ ){
    note_off(0,n,0);
    usleep(1<<13);
  }


  for( int n = 60; n<70; n++ ){
    note_on(0,n,127);
    usleep(1<<15);
    note_off(0,n,0);
    usleep(1<<16);
  }

  constexpr int PLUS_BUTTON = 0x16;

  midi::message_t msg1;
  msg1.push_back(0x1b);  // PANEL_MSG
  msg1.push_back(0x2a);  // PANEL_MSG
  msg1.push_back(0);  // PANEL_MSG
  msg1.push_back(0);  // PANEL_MSG
  msg1.push_back(0x32);  // PANEL_MSG

  midi::message_t msg2;
  msg2.push_back(0x16);  // ALLTEXT
  //msg.push_back(0);  // ALLTEXT
  //msg.push_back(0x05); // INFO  
  //msg.push_back(0x05); // type  
  //msg.push_back(0x05); // type  
  //msg.push_back(0x05); // idno  
  //msg.push_back(0x05); // idno  


  send_sysex(msg1);
  //send_sysex(msg2);

  printf( "sleeping...\n");
  usleep(10<<20);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
