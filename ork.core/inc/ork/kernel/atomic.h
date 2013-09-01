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
};
