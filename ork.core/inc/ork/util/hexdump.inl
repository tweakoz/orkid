////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>
#include <ctype.h>

namespace ork {

inline void hexdumpbytes(const uint8_t* bytes, size_t length) {

  bool keep_going = length > 0;

  size_t byte_index = 0;

  while(keep_going){
    size_t line_length = 16;

    if(line_length + byte_index > length)
      line_length = length - byte_index;

    ///////////////////////////////////////////
    
    size_t bidx0 = byte_index;
    printf("0x%02x: ", (unsigned int) byte_index);
    for (int c = 0; c < line_length; c++) {
      uint8_t byte = bytes[bidx0++];
      printf("%02x ", byte);
    }
    for (int c = line_length; c < 16; c++) {
      printf("-- ");
    }
    ///////////////////////////////////////////
    printf(" |");
    ///////////////////////////////////////////
    bidx0 = byte_index;
    for (int c = 0; c < line_length; c++) {
      char byte = (char) bytes[bidx0++];
      if (false == isprint(byte))
        byte = '.';
      printf("%c", byte);
    }
    for (int c = line_length; c < 16; c++) {
      printf(" ");
    }
    ///////////////////////////////////////////
    printf( "|\n");
    ///////////////////////////////////////////
    byte_index += 16;
    keep_going = byte_index < length;
  }
}

inline void hexdumpbytes(std::vector<uint8_t> bytes) {
  hexdumpbytes(bytes.data(), bytes.size());
}

} // namespace ork
