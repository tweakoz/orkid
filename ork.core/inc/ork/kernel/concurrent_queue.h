///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/ringbuffer.hpp>

namespace ork {

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
    void push(const T& item) // blocking
    {
        mImpl.push(item);
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
