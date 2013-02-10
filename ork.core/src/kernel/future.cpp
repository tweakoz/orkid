#include <ork/kernel/future.hpp>

namespace ork {

Future::Future()
{
    mState = 0;
}

void Future::Clear()
{
    mResult.Set<bool>(false);
    mState=0;
}

void Future::WaitForSignal() const
{
	while(int(mState)==0) usleep(100);
}
const Future::var_t& Future::GetResult() const
{
    WaitForSignal();
    return mResult;
}

}
