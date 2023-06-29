////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::parse(GlSlFxParser* parser, 
                             const ScannerView& view) {

  DecoBlockNode::parse(parser,view);

  auto topnode = parser->_topNode;

  int ist = view._start + 1;
  int ien = view._end - 1;
  for (size_t i = ist; i <= ien;) {
    auto ptok   = view.token(i);
    auto namtok = view.token(i + 1);
    if (ptok->text == "pragma_typelib") {
      _is_typelib = true;
      i++;
      view.checktoken(i, ";");
      i++;
    }
    else if (ptok->text == "struct") {
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
      OrkAssert(_is_typelib == false);
      view.checktoken(i + 2, "(");
      auto returntype = ptok->text;
      i += 2; // advance to (
      auto fnnode         = std::make_shared<FunctionNode>();
      fnnode->_name       = namtok;
      fnnode->_returnType = ptok;
      ScannerView fnview(view, i);
      int j = fnnode->parse(parser,fnview);
      _children.push_back(fnnode);

      #if defined(XX_USE_ORKSL_LANG)

      /////////////////////////////////
      // parsedfnnode (testing, wip...)
      /////////////////////////////////
      auto parsedfnnode = std::make_shared<OrkSlFunctionNode>(parser);
      ScannerView pfnview(view, i);
      int k = parsedfnnode->parse(pfnview);
      OrkAssert(k==j);
      /////////////////////////////////
      #endif
      
      i += j;
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
    if( auto as_struct = c.tryAs<structnode_ptr_t>() ){
      topnode->validateIdentifierName(as_struct.value()->_name->text);
    }
    else if( auto as_fn = c.tryAs<functionnode_ptr_t>() ){
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
    if (auto as_struct = item.tryAs<structnode_ptr_t>()) {
      as_struct.value()->emit(backend);
    } else if (auto as_fn = item.tryAs<functionnode_ptr_t>()) {
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
