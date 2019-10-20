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
  FnParseContext pctx(_container, view);
  while (not done) {
    auto try_tok     = view.token(i)->text;
    pctx._startIndex = i;
    if (IfStatement::match(pctx)) {
      auto subnode = new IfStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else if (ForLoopStatement::match(pctx)) {
      auto subnode = new ForLoopStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else if (WhileLoopStatement::match(pctx)) {
      auto subnode = new WhileLoopStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else if (ReturnStatement::match(pctx)) {
      auto subnode = new ReturnStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else if (VariableDefinitionStatement::match(pctx)) {
      auto subnode = new VariableDefinitionStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else if (VariableAssignmentStatement::match(pctx)) {
      auto subnode = new VariableAssignmentStatement(_container);
      i += subnode->parse(view, i);
      _statements.push_back(subnode);
    } else {
      assert(false);
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

FnMatchResults ReturnStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "return";
  return rval;
}

int ReturnStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void ReturnStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults ForLoopStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "for";
  return rval;
}

int ForLoopStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void ForLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults WhileLoopStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "while";
  return rval;
}

int WhileLoopStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void WhileLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults IfStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "if";
  return rval;
}

int IfStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void IfStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults VariableDefinitionStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  ////////////////////////////////////
  // check variable instantiation
  ////////////////////////////////////
  int i      = 0;
  auto tokDT = ctx.tokenValue(i);
  if (tokDT == "const") {
    tokDT = ctx.tokenValue(++i);
  }
  bool valid_dt = ctx._container->validateTypeName(tokDT);
  auto tokN     = ctx.tokenValue(i + 1);
  bool valid_id = ctx._container->validateIdentifierName(tokN);
  auto tokE     = ctx.tokenValue(i + 2);
  bool instantiation_ok = valid_dt and valid_id and (tokE == "=");
  if( false == instantiation_ok )
    return rval;
  ////////////////////////////////////
  // check assignment
  ////////////////////////////////////
  FnParseContext lctx = ctx;
  lctx._startIndex = i+1;
  auto matchlv = LValue::match(ctx);
  if( matchlv ){
    auto try_eq = ctx.tokenValue(matchlv._end+1);
    if( try_eq=="="){
      FnParseContext rctx = ctx;
      rctx._startIndex = matchlv._end+2;
      auto matchrv = RValue::match(ctx);
      if( matchrv ) {
        rval._matched = true;
        rval._start = ctx._startIndex;
        rval._end = matchrv._end;
      }
    }
  }
  ////////////////////////////////////
  return rval;
}

int VariableDefinitionStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void VariableDefinitionStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults VariableAssignmentStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  auto matchlv = LValue::match(ctx);
  if( matchlv ){
    auto try_eq = ctx.tokenValue(matchlv._end+1);
    if( try_eq=="="){
      FnParseContext rctx = ctx;
      rctx._startIndex = matchlv._end+2;
      auto matchrv = RValue::match(ctx);
      if( matchrv ) {
        rval._matched = true;
        rval._start = ctx._startIndex;
        rval._end = matchrv._end;
      }
    }
  }
  return rval;
}

int VariableAssignmentStatement::parse(const ScannerView& view, int start) {
  assert(false);
  return 0;
}
void VariableAssignmentStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults LValue::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  assert(false);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults RValue::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  assert(false);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

std::string FnParseContext::tokenValue(size_t offset) const {
  return _view.token(_startIndex + offset)->text;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
