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
#include <ork/kernel/string/string.h>
namespace ork {

inline void hexdumpbytes(const uint8_t* bytes, size_t length) {

  std::string dump_text;
  size_t line_start = 0;
  for( size_t i=0; i<length; i++ ){
    bool start_line = (i%16)==0;
    bool end_line = (i%16)==15;
    bool end_file = (i==(length-1));
    if( start_line ){
      dump_text += FormatString("0x%04x: ", i);
      line_start = dump_text.length();
    }
    dump_text += FormatString("%02x ", bytes[i]);
    // print as characters
    if( end_line || end_file ){
      size_t line_len = dump_text.length()-line_start;
      for( size_t j=line_len; j<48; j++ ){
        dump_text += " ";
        line_len ++;
      }
      dump_text += " | ";
      for( size_t j=i-(i%16); j<=i; j++ ){
        char byte = (char)bytes[j];
        if (false == isprint(byte))
          byte = '.';
        dump_text += FormatString("%c", byte);
      }
      dump_text += FormatString(" |\n");
    }
  }
  printf("%s", dump_text.c_str());
}

inline void hexdumpbytes(std::vector<uint8_t> bytes) {
  hexdumpbytes(bytes.data(), bytes.size());
}

inline void hexdumpu32s(std::vector<uint32_t> words) {
  size_t index = 0;
  for( auto w : words ){
    switch( index%4 ){
      case 0: 
        printf("%zx: ", index*4);
        printf("%08x ", w);
        break;
      case 3: 
        printf("\n");
        break;
      default: 
        printf("%08x ", w);
        break;
    }
    index++;
  }
  printf("\n");
}
} // namespace ork
