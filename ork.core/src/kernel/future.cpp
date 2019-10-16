#include <ork/kernel/future.hpp>

namespace ork {

Future Future::gnilfut;

Future::Future()
    : mID(0)
    , mResult(nullptr)
    , mCallback(nullptr){

    mState.store(0);
}

void Future::Clear()
{
    mResult.Set<bool>(false);
    mState.store(0);
}

void Future::WaitForSignal() const
{
	while(mState.load()==0) sched_yield();
}
const Future::var_t& Future::GetResult() const
{
    WaitForSignal();
    return mResult;
}

}
