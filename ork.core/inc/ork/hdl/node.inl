////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

RefAstNode::RefAstNode(const Ref& ref)
    : _ref(ref){
}
size_t RefAstNode::bitwidth() const {
    return _ref.bitwidth();
}
void RefAstNode::emitVerilog(VerilogBackEnd* engine) const {
    auto s = engine->_currentexpr->_segment;
    engine->output("("+ _ref.resolveInstanceName(s)+")");
}

}} // namespace ork { namespace hdl {
