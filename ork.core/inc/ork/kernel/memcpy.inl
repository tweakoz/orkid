////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 
#include <stdint.h>

namespace ork {

	void memcpy_fast(void* dest, const void* src, size_t length);
	void memcpy_async(void* dest, const void* src, size_t length, std::atomic<int>& async_counter);
	void memcpy_parallel(void* dest, const void* src, size_t length);

}
