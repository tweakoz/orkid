#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>
#include <ctype.h>

namespace ork {

inline void hexdumpbytes(const uint8_t* bytes, size_t length) {
  int numlines = length / 16;
  for (int r = 0; r < numlines; r++) {
    int bidx = (r * 16);
    printf("0x%02x: ", bidx);

    for (int c = 0; c < 16; c++) {
      u8 byte = bytes[bidx + c];
      printf("%02x ", byte);
    }

    printf(" |");
    for (int c = 0; c < 16; c++) {
      char byte = (char)bytes[bidx + c];
      if (false == isprint(byte))
        byte = '.';
      printf("%c", byte);
    }
    printf("|\n");
  }
}

inline void hexdumpbytes(std::vector<uint8_t> bytes) {
  int numlines = bytes.size() / 16;
  for (int r = 0; r < numlines; r++) {
    int bidx = (r * 16);
    printf("0x%02x: ", bidx);

    for (int c = 0; c < 16; c++) {
      u8 byte = bytes[bidx + c];
      printf("%02x ", byte);
    }

    printf(" |");
    for (int c = 0; c < 16; c++) {
      char byte = (char)bytes[bidx + c];
      if (false == isprint(byte))
        byte = '.';
      printf("%c", byte);
    }
    printf("|\n");
  }
}

} // namespace ork
