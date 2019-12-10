////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

Expression::Expression() {
    _child = Unknown();
}
Expression::Expression(Ref&v) {
    _child = Unknown();
}

///////////////////////////////////////////////////

size_t Expression::bitwidth() const {

    if(auto as=_child.TryAs<Ref>()){
        return as.value().bitwidth();
    }
    else if(auto as=_child.TryAs<OperationNot>()){
        return as.value().bitwidth();
    }
    else if(auto as=_child.TryAs<OperationPlus>()){
        return as.value().bitwidth();
    }
    else if(auto as=_child.TryAs<OperationSlice>()){
        return as.value().bitwidth();
    }
    else if(auto as=_child.TryAs<astnode_t>()){
        return as.value()->bitwidth();
    }
    else {
        //printf( "_childtype<%s>\n", _child.typestr());
        assert(false);
    }
    return 0;
}

///////////////////////////////////////////////////

inline expr_t operator / (expr_t lhs, Rvalue rhs){
    auto s = FrontEnd::segment();
    expr_t expr = s->createExpression(false);
    expr->MakeAstNode<OperationDividedBy>(lhs,rhs);
    return expr;
}

inline expr_t operator << (expr_t lhs, Rvalue rhs){
    auto s = FrontEnd::segment();
    expr_t expr = s->createExpression(false);
    expr->MakeAstNode<OperationShiftLeft>(lhs,rhs);
    return expr;
}

inline expr_t operator >> (expr_t lhs, Rvalue rhs){
    auto s = FrontEnd::segment();
    expr_t expr = s->createExpression(false);
    expr->MakeAstNode<OperationShiftRight>(lhs,rhs);
    return expr;
}

inline expr_t operator << (expr_t lhs, uint32_t rhs){
    return lhs << ku32(rhs);
}

inline expr_t operator >> (expr_t lhs, uint32_t rhs){
    return lhs >> ku32(rhs);
}

///////////////////////////////////////////////////

inline expr_t operator ! (Rvalue inp) {
    auto s = FrontEnd::segment();
    expr_t expr = s->createExpression(false);
    expr->MakeAstNode<OperationNot>(inp);
    return expr;
}

}} // namespace ork { namespace hdl {
