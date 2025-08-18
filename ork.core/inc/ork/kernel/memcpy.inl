////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 
#include <stdint.h>
#include <stdlib.h>
#include <atomic>

namespace ork {

	void memcpy_fast(void* dest, const void* src, size_t length);
	void memcpy_async(void* dest, const void* src, size_t length, std::atomic<int>& async_counter);
	void memcpy_parallel(void* dest, const void* src, size_t length);
#if defined(ORK_ARCHITECTURE_ARM_64)
	void _memcpy_neon(void* dest, const void* src, size_t n);
	void _memcpy_cache_optimized(void* dest, const void* src, size_t n);
	void _memcpy_prefetch(void* dest, const void* src, size_t n);
	void _memcpy_asm(void* dest, const void* src, size_t n);
	#if defined(__APPLE__)
	void _memcpy_accel(void* dest, const void* src, size_t n);
	#endif
#endif

}
