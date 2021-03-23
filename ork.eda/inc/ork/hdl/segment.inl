////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////
Segment::Segment(){
    _l = [](){};
}
///////////////////////////////////////
expr_t Segment::createExpression(bool add){
    auto expr = std::make_shared<Expression>();
    expr->_segment = this;
    if(add)
        _exprs.push_back(expr);
    return expr;
}
///////////////////////////////////////

}} // namespace ork { namespace hdl {
