////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////
// base for all Binary operator AST Node's
///////////////////////////////////////////////////

struct BinaryOp : public AstNode {

    BinaryOp(Rvalue lhs,Rvalue rhs,std::string opname)
        : _lhs(lhs)
        , _rhs(rhs)
        , _opname(opname) {

        check_bitwidths(lhs,rhs);
    }

    ///////////////////////////////////

    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->output( "(" );
        engine->emitRvalue(_lhs);
        engine->output( " %s ", _opname.c_str() );
        engine->emitRvalue(_rhs);
        engine->output( ")" );
    }

    ///////////////////////////////////

    size_t bitwidth() const override {
        return _lhs.bitwidth();
    }

    ///////////////////////////////////

    Rvalue _lhs;
    Rvalue _rhs;
    std::string _opname;
};

///////////////////////////////////////////////////

struct OperationLessThan : public BinaryOp {
    OperationLessThan( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"<"){}
};
struct OperationGreaterThan : public BinaryOp {
    OperationGreaterThan( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,">"){}
};
struct OperationPlus : public BinaryOp {
    OperationPlus( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"+"){}
    size_t bitwidth() const {
        size_t l = _lhs.bitwidth();
        size_t r = _lhs.bitwidth();
        return l;
    }
};
struct OperationMinus : public BinaryOp {
    OperationMinus( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"-"){}
};
struct OperationTimes : public BinaryOp {
    OperationTimes( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"*"){}
    size_t bitwidth() const {
        size_t l = _lhs.bitwidth();
        size_t r = _lhs.bitwidth();
        return l+r;
    }
};
struct OperationDividedBy: public BinaryOp {
    OperationDividedBy( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"/"){}
};
struct OperationBitwiseAnd : public BinaryOp {
    OperationBitwiseAnd( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"and"){}
};
struct OperationBitwiseOr : public BinaryOp {
    OperationBitwiseOr( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"or"){}
};
struct OperationIsEqualTo : public BinaryOp {
    OperationIsEqualTo( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"=="){}
};
struct OperationNotEqualsTo : public BinaryOp {
    OperationNotEqualsTo( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"!="){}
};

struct OperationShiftLeft : public BinaryOp {
    OperationShiftLeft( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,"<<"){}
};
struct OperationShiftRight : public BinaryOp {
    OperationShiftRight( Rvalue lhs,Rvalue rhs )
    : BinaryOp(lhs,rhs,">>"){}
};

}} // namespace ork { namespace hdl {
