////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

int FunctionNode::parse(const ork::ScannerView& view) {
  int i         = 0;
  auto open_tok = view.token(i);
  assert(open_tok->text == "(");
  i++;
  /////////////////////////////////
  // arguments
  /////////////////////////////////
  bool args_done = false;
  const Token* dirspec = nullptr;
  while (false == args_done) {
    auto argtype_tok = view.token(i);
    if (argtype_tok->text == ")") {
      args_done = true;
      i++;
    } else if( argtype_tok->text=="in"){
      dirspec = argtype_tok;
      i++;
    } else if( argtype_tok->text=="out"){
      dirspec = argtype_tok;
      i++;
    } else if( argtype_tok->text=="inout"){
      dirspec = argtype_tok;
      i++;
    } else {
      assert(_container->validateTypeName(argtype_tok->text));
      i++;
      auto nam_tok = view.token(i);
      assert(_container->validateIdentifierName(nam_tok->text));
      i++;
      auto argnode   = new FunctionArgumentNode(_container);
      argnode->_type = argtype_tok;
      argnode->_name = nam_tok;
      argnode->_direction = dirspec;
      dirspec = nullptr;
      _arguments.push_back(argnode);
      auto try_comma = view.token(i)->text;
      if (try_comma == ",") {
        i++;
      }
    }
  }
  /////////////////////////////////
  // body
  /////////////////////////////////
  assert(view.token(i)->text == "{");
  ScannerView bodyview(view, i);
  i += _body.parse(bodyview);
  bodyview.numTokens();
  assert(i == view.numTokens());
  /////////////////////////////////
  // parsedfnnode
  /////////////////////////////////
  //_parsedfnnode = new ParsedFunctionNode(_container);
  //int j = _parsedfnnode->parse(bodyview);
  //assert(j==i);
  /////////////////////////////////
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void FunctionNode::pregen(shaderbuilder::BackEnd& backend) {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void FunctionNode::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  codegen.beginLine();
  codegen.output(_returnType->text + " ");
  codegen.output(_name->text + "(");
  ///////////////////////////////////////////////////
  // arguments
  ///////////////////////////////////////////////////
  size_t numargs = _arguments.size();
  if (0 == numargs) {
    codegen.output(")");
    codegen.endLine();
    codegen.incIndent();
  } else {
    codegen.endLine();
    codegen.incIndent();
    for (size_t i = 0; i < numargs; i++) {
      auto a = _arguments[i];
      codegen.beginLine();
      if( a->_direction )
        codegen.output(a->_direction->text + " ");
      codegen.output(a->_type->text + " ");
      codegen.output(a->_name->text);
      bool is_last = ((i + 1) == numargs);
      if (is_last) {
        codegen.output(")");
      } else {
        codegen.output(",");
      }
      codegen.endLine();
    }
  }
  ///////////////////////////////////////////////////
  // arguments
  ///////////////////////////////////////////////////
  codegen.formatLine("{");
    codegen.incIndent();
  _body.emit(backend);
    codegen.decIndent();
  codegen.formatLine("}");
    codegen.decIndent();
}
  /////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
