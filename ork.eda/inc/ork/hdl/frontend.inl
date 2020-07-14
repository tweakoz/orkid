////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

FrontEnd& FrontEnd::get(){
    thread_local static FrontEnd ctx;
    return ctx;
}

FrontEnd::FrontEnd()
    : sysclock("sysclock")
    , _cursegment(nullptr) {
    _dslOK = 0;
}

///////////////////////////////////////////////////

void FrontEnd::generate(backend_t engine, Module&m){

    _engine = engine;
    m.finalize();
    m.do_generate();

    engine->visitRootModule(m);
    engine->flush();
    _engine = nullptr;

}

}} // namespace ork { namespace hdl {
