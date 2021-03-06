////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/orksl/parser.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orksl::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::parse(OrkSlParser* parser, 
                             const ScannerView& view) {

  DecoBlockNode::parse(parser,view);

  auto topnode = parser->_topNode;

  int ist = view._start + 1;
  int ien = view._end - 1;
  for (size_t i = ist; i <= ien;) {
    auto ptok   = view.token(i);
    auto namtok = view.token(i + 1);
    if (ptok->text == "struct") {
      // parse struct
      view.checktoken(i + 2, "{");
      i += 2; // advance to {
      ScannerView structview(view, i);
      auto snode   = std::make_shared<StructNode>();
      snode->_name = namtok;
      topnode->addStructType(snode);
      i += snode->parse(parser,structview);
      _children.push_back(snode);
      view.checktoken(i, ";");
      i++;
    } else { // must be a function
      view.checktoken(i + 2, "(");
      auto returntype = ptok->text;
      i += 2; // advance to (
      auto fnnode         = std::make_shared<FunctionNode>();
      fnnode->_name       = namtok;
      fnnode->_returnType = ptok;
      ScannerView fnview(view, i);
      i += fnnode->parse(parser,fnview);
      _children.push_back(fnnode);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::pregen(shaderbuilder::BackEnd& backend) const {
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::emitLibrary(shaderbuilder::BackEnd& backend) const {

  /////////////////////////////////////////////
  // validate libblock
  /////////////////////////////////////////////

  DecoBlockNode::_pregen(backend);
  auto topnode = backend._parser->_topNode;
  for(auto c : _children){
    if( auto as_struct = c.TryAs<structnode_ptr_t>() ){
      topnode->validateIdentifierName(as_struct.value()->_name->text);
    }
    else if( auto as_fn = c.TryAs<functionnode_ptr_t>() ){
      topnode->validateIdentifierName(as_fn.value()->_name->text);
      backend.validateTypeName(as_fn.value()->_returnType->text);
    }
    else{
      assert(false);
    }
  }

  /////////////////////////////////////////////

  auto& codegen = backend._codegen;
  codegen.formatLine("// begin libblock<%s> ///////////////////////////////////", _name.c_str());
  for (auto item : _children) {
    if (auto as_struct = item.TryAs<structnode_ptr_t>()) {
      as_struct.value()->emit(backend);
    } else if (auto as_fn = item.TryAs<functionnode_ptr_t>()) {
      as_fn.value()->emit(backend);
    } else {
      assert(false);
    }
  }
  //_body.emit(backend);
  codegen.formatLine("// end libblock<%s> ///////////////////////////////////", _name.c_str());
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::generate(shaderbuilder::BackEnd& backend) const {
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
