////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
