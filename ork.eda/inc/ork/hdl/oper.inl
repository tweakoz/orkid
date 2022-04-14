////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////

struct WrapNode final : public AstNode {
    WrapNode(expr_t e)
        : _expr(e)
    {}
    size_t bitwidth() const final { return _expr->bitwidth()-1; }
    void emitVerilog(VerilogBackEnd* engine) const final {
        size_t w = _expr->bitwidth()-1;
        size_t mask = (1L<<w)-1L;
        engine->output( "((");
        engine->emitExpression(_expr);
        engine->output(")&%zu'h%zx)", w, mask );
    }
    expr_t _expr;
};

///////////////////////////////////////////////////
// Syncronous Assignment AST Node
///////////////////////////////////////////////////

struct SelectAstNode : public AstNode {
    SelectAstNode(Rvalue s,Rvalue a,Rvalue b)
        : _a(a)
        , _b(b) 
        , _s(s) {
        check_bitwidths(a,b);
    }
    void emitVerilog(VerilogBackEnd* engine) const final {
      engine->emitRvalue(_s);
      engine->output(" ? ");
      engine->emitRvalue(_a);
      engine->output( " : ");
      engine->emitRvalue(_b);
    }
    size_t bitwidth() const final { return _a.bitwidth(); }
    Rvalue _a,_b,_s;
};

///////////////////////////////////////////////////
// Syncronous Assignment AST Node
///////////////////////////////////////////////////

struct SyncAssignNode : public AstNode {
    SyncAssignNode(Ref lhs,Rvalue rhs)
        : _lhs(lhs)
        , _rhs(rhs) {

        check_bitwidths(lhs,rhs);

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
      engine->startline();
      engine->emitNext(_lhs);
      engine->output(" <= ");
      engine->emitRvalue(_rhs);
      engine->output( ";");
      engine->endline();
    }
    size_t bitwidth() const final { return _lhs.bitwidth(); }
    Ref _lhs;
    Rvalue _rhs;
};

///////////////////////////////////////////////////
// Combinatorial Assignment AST Node
///////////////////////////////////////////////////

struct CombAssignNode : public AstNode {
    CombAssignNode( Ref lhs,
                    Rvalue rhs )
        : _lhs(lhs)
        , _rhs(rhs){

        if( auto as = _lhs._ref->tryAs<Input>() ){

        }
        else if( auto as = _lhs._ref->tryAs<Output>() ){
            if(as.value()._register){
                auto trace = bt_to_string(boost::stacktrace::stacktrace());
                printf( "%s\n", trace.c_str() );
                printf( "Invalid CombAssign to Output Register\n");
                assert(false);
            }
        }
        else if( auto as = _lhs._ref->tryAs<Reg>() ){
            auto trace = bt_to_string(boost::stacktrace::stacktrace());
            printf( "%s\n", trace.c_str() );
            printf( "Invalid CombAssign to Register\n");
            assert(false);
        }
        else if( auto as = _lhs._ref->tryAs<Wire>() ){

        }
        else{
            //printf( "type<%s>\n",_lhs._ref->typestr());
            assert(false);
        }
        check_bitwidths(lhs,rhs);
    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->startline();
        engine->output( "assign ");
        engine->emitRef(_lhs);
        engine->output( " = ");
        engine->emitRvalue(_rhs);
        engine->output( " ;");
        engine->endline();

    }
    size_t bitwidth() const final { return _lhs.bitwidth(); }
    Ref _lhs;
    Rvalue _rhs;
};

///////////////////////////////////////////////////
// Slicing operation AST Node
///////////////////////////////////////////////////

struct OperationSlice : public AstNode {
    OperationSlice( size_t start, size_t width, Rvalue inp )
        : _start(start)
        , _width(width)
        , _input(inp){

        printf( "Slice inp<%zu> out<%zu>\n", inp.bitwidth(), bitwidth() );
        if( _start+_width>inp.bitwidth() ) {
            auto trace = bt_to_string(boost::stacktrace::stacktrace());
            printf( "%s\n", trace.c_str() );
            printf( "Slice Bounds Violation\n");
            assert(false);
        }

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        size_t mask = (1L<<_width)-1L;
        if(_start==0){
            engine->output( "(");
            engine->emitRvalue(_input);
            engine->output(")&%zu'h%zx",
                           _width,
                           mask );
        }
        else {
            engine->output( "(((");
            engine->emitRvalue(_input);
            engine->output(")>>%zu)&%zu'h%zx)",
                           _start,
                           _width,
                           mask );
        }
    }
    size_t bitwidth() const final {
        return _width;
    }
    size_t _start;
    size_t _width;
    Rvalue _input;
};

///////////////////////////////////////////////////
// LeftZeroExtend operation AST Node
///////////////////////////////////////////////////

struct OperationLeftExtend : public AstNode {
    OperationLeftExtend( Rvalue inp, size_t width )
        : _width(width)
        , _input(inp){

        if( _width<=inp.bitwidth() ) {
            auto trace = bt_to_string(boost::stacktrace::stacktrace());
            printf( "%s\n", trace.c_str() );
            printf( "Invalid LeftExtend Width\n");
            assert(false);
        }

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        size_t extension = _width-_input.bitwidth();
        engine->output( "{%d'd0,",extension);
        engine->emitRvalue(_input);
        engine->output( "}");
    }
    size_t bitwidth() const final {
        return _width;
    }
    size_t _width;
    Rvalue _input;
};

///////////////////////////////////////////////////
// LeftZeroExtend operation AST Node
///////////////////////////////////////////////////

struct ConcatAstNode : public AstNode {
    ConcatAstNode( std::vector<Rvalue> items )
        : _input(items){
    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->output( "{");
        size_t count = _input.size();
        for( size_t i=1; i<=count; i++ ){
            auto item = _input[i-1];
            engine->emitRvalue(item);
            if(i!=count)
                engine->output( ",");
        }
        engine->output( "}");
    }
    size_t bitwidth() const final {
        size_t rval =0;
        for( auto item : _input )
            rval += item.bitwidth();
        return rval;
    }
    std::vector<Rvalue> _input;
};

///////////////////////////////////////////////////
// RightZeroExtend operation AST Node
///////////////////////////////////////////////////

struct OperationRightExtend : public AstNode {
    OperationRightExtend( Rvalue inp, size_t width )
        : _width(width)
        , _input(inp){

        if( _width<=inp.bitwidth() ) {
            auto trace = bt_to_string(boost::stacktrace::stacktrace());
            printf( "%s\n", trace.c_str() );
            printf( "Invalid RightExtend Width\n");
            assert(false);
        }

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        size_t extension = _width-_input.bitwidth();
        engine->output( "{(");
        engine->emitRvalue(_input);
        engine->output( "),%d'0}",extension);
    }
    size_t bitwidth() const final {
        return _width;
    }
    size_t _width;
    Rvalue _input;
};

}} // namespace ork { namespace hdl {

///////////////////////////////////////////////////

#include "oper_binary.inl"
#include "oper_unary.inl"
#include "oper_flow.inl"
