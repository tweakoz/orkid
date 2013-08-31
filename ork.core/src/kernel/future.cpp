#include <ork/kernel/future.hpp>

namespace ork {

Future::Future()
{
    mState.store(0,MemFullFence);
}

void Future::Clear()
{
    mResult.Set<bool>(false);
    mState.store(0,MemFullFence);
}

void Future::WaitForSignal() const
{
	while(int(mState.load(MemFullFence))==0) usleep(100);
}
const Future::var_t& Future::GetResult() const
{
    WaitForSignal();
    return mResult;
}

}
