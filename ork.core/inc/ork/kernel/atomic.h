////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
//
#pragma once

#include <atomic>

#define MemFullFence std::memory_order_acq_rel
#define MemRelaxed std::memory_order_relaxed
#define MemAcquire std::memory_order_acquire
#define MemRelease std::memory_order_release

namespace ork
{
	template <typename T> using atomic = std::atomic<T>;
}
