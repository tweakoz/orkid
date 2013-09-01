#pragma once

#include <ork/kernel/svariant.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/atomic.h>
#include <functional>

namespace ork {

struct Future
{
    typedef svar160_t var_t;
    typedef int future_id_t;

    Future();
    bool IsSignaled() const { return mState.load()>0; }
    template <typename T> void Signal( const T& result );
    void Clear();
    void WaitForSignal() const;
    void SetId(future_id_t id) { mID=id; }
    future_id_t GetId() const { return mID; }
    const var_t& GetResult() const;
    ////////////////////

    typedef std::function<void(const Future& fut)> fut_blk_cb_t;

    ////////////////////

    future_id_t             mID;
    ork::atomic<int>        mState;
    var_t                   mResult;
    var_t                   mCallback;
    //mutable std::condition_variable mWaitCV;

};

template <typename T>
void Future::Signal( const T& result )
{
    mResult.Set<T>(result);

    if( mCallback.IsA<fut_blk_cb_t>() )
    {
        const fut_blk_cb_t& blk = mCallback.Get<fut_blk_cb_t>();
        blk(*this);
    }

    mState.fetch_add(1);
}

}
