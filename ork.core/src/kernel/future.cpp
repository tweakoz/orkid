////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/future.hpp>

namespace ork {

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
