////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////
// Always block AST Node
///////////////////////////////////////////////////

struct AlwaysBlockNode : public AstNode {
    AlwaysBlockNode(segment_t s,Edge e,vlambda_t l)
        : _seg(s)
        , _clockedge(e){
        l();
    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        auto clkname = _clockedge._clock->_name.c_str();
        auto edgename = _clockedge._name.c_str();
        engine->startline();
        engine->output("always @ (%s %s) begin", edgename, clkname );
        engine->endline();
        engine->indent();
        engine->emitSegment(_seg);
        engine->unindent();
        engine->startline();
        engine->output( "end");
        engine->endline();
    }
    size_t bitwidth() const final { return 0; }
    segment_t _seg;
    Edge _clockedge;
};

///////////////////////////////////////////////////
// If/ElseIf/Else flow AST Node
///////////////////////////////////////////////////

struct IfNode : public AstNode {

    struct elifitem {
        expr_t _conditional;
        segment_t _seg;
    };

    IfNode(segment_t s, expr_t c,vlambda_t l)
        : _conditional( c )
        , _seg(s){

        l();
    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->startline();
        engine->output("if ");
        engine->emitExpression(_conditional);
        engine->output(" begin");
        engine->endline();
        engine->indent();
        engine->emitSegment(_seg);
        engine->unindent();
        engine->startline();
        engine->output("end");
        engine->endline();
        /////////////////////////////
        for( auto item : _elifitems ){
            engine->startline();
            engine->output("else if ");
            engine->emitExpression(item._conditional);
            engine->output(" begin");
            engine->endline();
            engine->indent();
            engine->emitSegment(item._seg);
            engine->unindent();
            engine->startline();
            engine->output("end");
            engine->endline();
        }
        /////////////////////////////
        if(_elseseg){
            engine->startline();
            engine->output("else begin");
            engine->endline();
            engine->indent();
            engine->emitSegment(_elseseg);
            engine->unindent();
            engine->startline();
            engine->output("end");
            engine->endline();
        }
    }
    void onElse(segment_t seg, vlambda_t l){
        _elseseg = seg;
        l();
    }
    void onElif(segment_t seg, expr_t c, vlambda_t l){
        elifitem item;
        item._conditional = c;
        item._seg = seg;
        _elifitems.push_back(item);
        l();
    }

    size_t bitwidth() const final { return 1; }
    expr_t _conditional;
    segment_t _seg;
    segment_t _elseseg;
    std::vector<elifitem> _elifitems;
};

///////////////////////////////////////////////////
// Switch/Case flow AST Node
///////////////////////////////////////////////////

struct CaseNode : public AstNode {
    CaseNode(int bitw, segment_t s, int v,vlambda_t l)
        : _bitw(bitw)
        , _caseval( KUIntC(_bitw,v) )
        , _seg(s){

        l();
    }
    void emitVerilog(VerilogBackEnd* engine) const final {}
    size_t bitwidth() const final { return _bitw; }
    size_t _bitw;
    KUIntC _caseval;
    segment_t _seg;
};

///////////////////////////////////////////////////
// Switch/Case flow AST Node
///////////////////////////////////////////////////

struct SwitchNode : public AstNode {
    SwitchNode(expr_t selector)
        : _selector(selector){

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->startline();
        engine->output( "case ");
        engine->emitExpression(_selector);
        engine->endline();
        engine->indent();

        for( auto c : _casenodes ){

            auto v = c->_caseval;
            engine->startline();
            engine->emitRvalue(v);
            engine->output(": begin");
            engine->endline();
            engine->indent();
            engine->emitSegment(c->_seg);
            engine->unindent();
            engine->startline();
            engine->output("end");
            engine->endline();
        }
        engine->unindent();

        engine->outputline("endcase");
    }
    void onCase( segment_t s, int v,vlambda_t l ){
        auto cn = new CaseNode(bitwidth(),s,v,l);
        _casenodes.push_back(cn);
    }
    size_t bitwidth() const final { return _selector->bitwidth(); }
    expr_t _selector;
    std::vector<CaseNode*> _casenodes;
};

}} // namespace ork { namespace hdl {
