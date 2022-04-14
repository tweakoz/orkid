////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
    mResult.set<bool>(false);
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
