////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

Rvalue::Rvalue(expr_t r){
    _payload = r;
}
Rvalue::Rvalue(Ref r){
    _payload = r;
}
Rvalue::Rvalue(KUIntC u){
    _payload = u;
}
Rvalue::Rvalue(KSIntC s){
    _payload = s;
}

size_t Rvalue::bitwidth() const {
    if(auto as = _payload.TryAs<expr_t>()){
        return as.value()->bitwidth();
    }
    else if(auto as = _payload.TryAs<Ref>()){
        return as.value().bitwidth();
    }
    else if(auto as = _payload.TryAs<KUIntC>()){
        return as.value().bitwidth();
    }
    else if(auto as = _payload.TryAs<KSIntC>()){
        return as.value().bitwidth();
    }
    assert(false);
    return 0;
}

}} // namespace ork { namespace hdl {
