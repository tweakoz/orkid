////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 
#include <stdint.h>

namespace ork {

void memcpy_avxnocache(uint8_t* dst, uint8_t* src, size_t size);
void memcpy_fast(void* dest, const void* src, size_t length);
}
