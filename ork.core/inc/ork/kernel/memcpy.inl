////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 
#include <stdint.h>

namespace ork {

	void memcpy_fast(void* dest, const void* src, size_t length);
	void memcpy_async(void* dest, const void* src, size_t length, std::atomic<int>& async_counter);
	void memcpy_parallel(void* dest, const void* src, size_t length);

}
