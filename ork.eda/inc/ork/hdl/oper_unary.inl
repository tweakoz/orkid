////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////
// ! unary operation AST Node
///////////////////////////////////////////////////

struct OperationNot : public AstNode {
    OperationNot( Rvalue inp )
        : _inp(inp){

    }
    void emitVerilog(VerilogBackEnd* engine) const final {
        engine->output( "(!" );
        engine->emitRvalue(_inp);
        engine->output( ")" );
    }
    size_t bitwidth() const final { return _inp.bitwidth(); }
    Rvalue _inp;
};

}} // namespace ork { namespace hdl {
