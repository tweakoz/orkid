////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////

Ref::Ref( Module* m,
          const std::string& key,
          modulevar_t* r )
    : _module(m)
    , _ref(r)
    , _key(key) {
}

///////////////////////////////////////////////////

size_t Ref::slicewidth() const {
    if(_slice_start>=0 and _slice_end>=0){
        size_t slice_rem = 1+(_slice_end-_slice_start);
        printf( "ref<%s> sw<%zu>\n", _key.c_str(), slice_rem );
        return slice_rem;
    }
    return 0;
}

///////////////////////////////////////////////////

bool Ref::is_signed() const {
    auto v = _ref;
    if(_connected)
        v = _connected;
    ///////////////////////////////

    if(auto as = v->tryAs<Input>()){
        return as.value()._signed;
    }
    else if(auto as = v->tryAs<Output>()){
        return as.value()._signed;
    }
    else if(auto as = v->tryAs<Reg>()){
        return as.value()._signed;
    }
    else if(auto as = v->tryAs<Wire>()){
        return as.value()._signed;
    }
    ///////////////////////////////

    assert(false);
    return 0;
}

///////////////////////////////////////////////////

size_t Ref::bitwidth() const {

    auto v = _ref;
    if(_connected)
      v = _connected;

    ///////////////////////////////
    // take into account slicing
    ///////////////////////////////

    size_t slicew = slicewidth();

    if( slicew != 0 )
        return slicew;

    ///////////////////////////////

    if(auto as = v->tryAs<Input>()){
        return as.value().bitwidth();
    }
    else if(auto as = v->tryAs<Output>()){
        return as.value().bitwidth();
    }
    else if(auto as = v->tryAs<Reg>()){
        return as.value().bitwidth();
    }
    else if(auto as = v->tryAs<Wire>()){
        return as.value().bitwidth();
    }
    ///////////////////////////////

    assert(false);
    return 0;
}

///////////////////////////////////////////////////

std::string Ref::resolveInstanceName(Segment* s) const{

    if( nullptr == s )
        return _key;

    auto sm = s->_module;

    auto it = std::find(sm->_children.begin(),
                        sm->_children.end(),
                        this->_module );

    bool is_instance = (it!=sm->_children.end());

    return is_instance
          ? "__"+_module->_name+"."+_key
          : _key;
}

///////////////////////////////////////////////////

Ref:: operator std::shared_ptr<Expression>(){

    auto s = FrontEnd::segment();
    auto expr = s->createExpression(false);
    expr->MakeAstNode<RefAstNode>(*this);
    return expr;
}

///////////////////////////////////////////////////

void Ref::next(Rvalue rhs){
    auto s = FrontEnd::segment();
    auto expr = s->createExpression(true);
    auto op = expr->MakeAstNode<SyncAssignNode>(*this,rhs);
}
void Ref::next(int rhs){
    auto kuc = KUIntC(bitwidth(),rhs);
    next(kuc);
}

///////////////////////////////////////////////////

Ref& Ref::operator = (Rvalue rhs) {
    if( auto as = rhs._payload.tryAs<Ref>() ){
        *this = as.value();
    }
    if( auto as = rhs._payload.tryAs<expr_t>() ){
        auto s = FrontEnd::segment();
        auto expr = s->createExpression(true);
        auto op = expr->MakeAstNode<CombAssignNode>(*this,as.value());
    }
    else {
        assert(false);
    }

    return *this;
}

///////////////////////////////////////////////////

Ref& Ref::operator = (Ref rhs) {

    check_bitwidths(*this,rhs);

    auto s = FrontEnd::segment();
    expr_t expr = s->createExpression(true);
    auto op = expr->MakeAstNode<CombAssignNode>(*this,rhs);

    //assert(_connected==nullptr);
    //_connected = rhs._ref;

    return *this;
}

///////////////////////////////////////////////////
// [size_t] slicing operator, slices 1 bit
///////////////////////////////////////////////////

Ref Ref::operator[](size_t slicebit) const {
    Ref rval = *this;
    rval._slice_start = slicebit;
    rval._slice_end = slicebit;
    return rval;
}

///////////////////////////////////////////////////
// [Ref] memory reference operator
///////////////////////////////////////////////////

Ref Ref::operator[](Ref address) const {
    auto r = *this;
    r._address = std::make_shared<Ref>(address);
    return r;
}

///////////////////////////////////////////////////

expr_t Ref::operator < (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationLessThan>(*this,rhs);
    return rval;
}
expr_t Ref::operator > (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationGreaterThan>(*this,rhs);
    return rval;
}
expr_t Ref::operator - (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationMinus>(*this,rhs);
    return rval;
}
expr_t Ref::operator + (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationPlus>(*this,rhs);
    return rval;
}
expr_t Ref::operator + (int rhs) const {
    auto kuc = KUIntC(bitwidth(),rhs);
    auto ksc = KSIntC(bitwidth(),rhs);
    if( is_signed() )
        return (*this+ksc);
    else
        return (*this+kuc);
}
expr_t Ref::operator - (int rhs) const {
    auto kuc = KUIntC(bitwidth(),rhs);
    auto ksc = KSIntC(bitwidth(),rhs);
    if( is_signed() )
        return (*this-ksc);
    else
        return (*this-kuc);
}
expr_t Ref::operator * (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationTimes>(*this,rhs);
    return rval;
}
expr_t Ref::operator / (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationDividedBy>(*this,rhs);
    return rval;
}
expr_t Ref::operator and (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationBitwiseAnd>(*this,rhs);
    return rval;
}
expr_t Ref::operator or (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationBitwiseOr>(*this,rhs);
    return rval;
}
expr_t Ref::operator == (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationIsEqualTo>(*this,rhs);
    return rval;
}
expr_t Ref::operator == (int rhs) const {
    auto kuc = KUIntC(bitwidth(),rhs);
    auto ksc = KSIntC(bitwidth(),rhs);
    if( is_signed() )
        return (*this==ksc);
    else
        return (*this==kuc);
}
expr_t Ref::operator != (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationNotEqualsTo>(*this,rhs);
    return rval;
}

expr_t Ref::operator << (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationShiftLeft>(*this,rhs);
    return rval;
}
expr_t Ref::operator >> (Rvalue rhs) const {
    auto s = FrontEnd::segment();
    expr_t rval = s->createExpression(false);
    rval->MakeAstNode<OperationShiftRight>(*this,rhs);
    return rval;
}

}} // namespace ork { namespace hdl {
