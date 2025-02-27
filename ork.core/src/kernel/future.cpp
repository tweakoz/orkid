////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/future.hpp>

namespace ork {

Future::Future()
    : _ID(0)
    , _result(nullptr)
    , _callback(nullptr){

    _state.store(0);
}

void Future::clear()
{
    _result.set<bool>(false);
    _state.store(0);
}

void Future::waitForSignal() const
{
	while(_state.load()==0) sched_yield();
}
const Future::var_t& Future::getResult() const
{
    waitForSignal();
    return _result;
}

}
