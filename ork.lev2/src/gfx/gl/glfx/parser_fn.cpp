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

int ParsedFunctionNode::parse(const ork::ScannerView& view) {
  int i         = 0;
  auto open_tok = view.token(i++);
  assert(open_tok->text == "{");
  bool done = false;
  FnParseContext pctx(_container,view);
  while(not done){
    auto try_tok = view.token(i)->text;
    pctx._startIndex = i;
    if( IfNode::match(pctx) ){
      auto subnode = new IfNode(_container);
      i += subnode->parse(view,i);
      _statements.push_back(subnode);
    }
    else if( ForLoopNode::match(pctx) ){
      auto subnode = new ForLoopNode(_container);
      i += subnode->parse(view,i);
      _statements.push_back(subnode);
    }
    else if( WhileLoopNode::match(pctx) ){
      auto subnode = new WhileLoopNode(_container);
      i += subnode->parse(view,i);
      _statements.push_back(subnode);
    }
    else if( ReturnNode::match(pctx) ){
      auto subnode = new ReturnNode(_container);
      i += subnode->parse(view,i);
      _statements.push_back(subnode);
    }
    else {
      bool valid_dt = _container->validateTypeName(try_tok);
      if( valid_dt and VariableDefinitionNode::match(pctx) ){ // assignment?
        auto subnode = new VariableDefinitionNode(_container);
        i += subnode->parse(view,i);
      _statements.push_back(subnode);
      }
      else {
        assert(false);
      }
    }
  }
  assert(false);
  auto close_tok = view.token(i++);
  assert(close_tok->text == "}");
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ParsedFunctionNode::emit(ork::lev2::glslfx::shaderbuilder::BackEnd& backend) const {
  assert(false); // not implemented yet...
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool ReturnNode::match(FnParseContext& ctx) {
  return ctx.tokenValue(0) == "return";
}

int ReturnNode::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void ReturnNode::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool ForLoopNode::match(FnParseContext& ctx) {
  return ctx.tokenValue(0) == "for";
}

int ForLoopNode::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void ForLoopNode::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool WhileLoopNode::match(FnParseContext& ctx) {
  return ctx.tokenValue(0) == "while";
}

int WhileLoopNode::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void WhileLoopNode::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool IfNode::match(FnParseContext& ctx) {
  return ctx.tokenValue(0) == "if";
}

int IfNode::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void IfNode::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool VariableDefinitionNode::match(FnParseContext& ctx) {
  int i = 0;
  auto tokDT = ctx.tokenValue(i);
  if( tokDT == "const" ) {
    tokDT = ctx.tokenValue(++i);
  }
  bool valid_dt = ctx._container->validateTypeName(tokDT);
  auto tokN = ctx.tokenValue(i+1);
  bool valid_id = ctx._container->validateIdentifierName(tokN);
  auto tokE = ctx.tokenValue(i+2);
  return valid_dt and valid_id and (tokE=="=");
}

int VariableDefinitionNode::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void VariableDefinitionNode::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

std::string FnParseContext::tokenValue(size_t offset) const {
  return _view.token(_startIndex+offset)->text;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
