#pragma once 
#include <stdint.h>

namespace ork {

void memcpy_avxnocache(uint8_t* dst, uint8_t* src, size_t size);
void memcpy_fast(void* dest, const void* src, size_t length);
}
