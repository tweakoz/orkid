////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/ringbuffer.hpp>
#include <array>
#include <atomic>
#include <deque>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

// Lock-free single-producer single-consumer queue.
// push() always succeeds, overwriting oldest data on overflow.
// drain() transfers all valid items to out and returns false if overflow occurred.
template<typename T, uint64_t N>
struct SPSCQueue {
    std::array<T, N> _buf;
    std::atomic<uint64_t> _head{0};
    std::atomic<uint64_t> _tail{0};

    void push(const T& val) {
        uint64_t head = _head.load(std::memory_order_relaxed);
        _buf[head % N] = val;
        _head.store(head + 1, std::memory_order_release);
    }

    // return false if there was overflow since last drain.
    // on overflow, tail is snapped to head-N to drain the most recent N items.
    bool drain(std::deque<T>& out) {
        uint64_t tail = _tail.load(std::memory_order_relaxed);
        uint64_t head = _head.load(std::memory_order_acquire);
        bool overflow = (head - tail) > N;
        if (overflow) tail = head - N;
        uint64_t h = head % N;
        uint64_t t = tail % N;
        if (h >= t) {
            out.insert(out.end(), &_buf[t], &_buf[h]);
        } else {
            out.insert(out.end(), &_buf[t], &_buf[N]);
            out.insert(out.end(), &_buf[0], &_buf[h]);
        }
        _tail.store(head, std::memory_order_release);
        return !overflow;
    }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T,size_t max_items=256>
struct MpMcBoundedQueue
{
    typedef MpMcRingBuf<T,max_items> impl_t;
    typedef T value_type;

    MpMcBoundedQueue()
		: mImpl()
    {

    }
    ~MpMcBoundedQueue()
    {

    }
    void push(const T& item, int quanta_usec=250) // blocking
    {
        mImpl.push(item,quanta_usec);
    }
    void pop(T& item, int quanta_usec=250) // blocking
    {
        mImpl.pop(item,quanta_usec);
    }
    bool try_push(const T& item) // non-blocking
    {
        return mImpl.try_push(item);
    }
    bool try_pop(T& item) // non-blocking
    {
        return mImpl.try_pop(item);
    }

    impl_t mImpl;
    static const size_t kSIZE = sizeof(T);
};

///////////////////////////////////////////////////////////////////////////////

}
