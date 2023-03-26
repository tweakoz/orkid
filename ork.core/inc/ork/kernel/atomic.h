////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
