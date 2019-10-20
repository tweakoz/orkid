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
    if (auto m = VariableDeclaration::match(pctx)) {
      auto parsed = m.parse();
      i += parsed._numtokens;
      _elements.push_back(parsed._node);
    } else if (auto m = CompoundStatement::match(pctx)) {
      auto parsed = m.parse();
      i += parsed._numtokens;
      _elements.push_back(parsed._node);
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
  for( auto elem : _elements )
    elem->emit(backend);
  assert(false); // not implemented yet...
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/*FnMatchResults ReturnStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "return";
  return rval;
}

int ReturnStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
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

int ForLoopStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
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

int WhileLoopStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
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

int IfStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
  assert(false);
  return 0;
}
void IfStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}*/

/////////////////////////////////////////////////////////////////////////////////////////////////

VariableDeclaration::match_t VariableDeclaration::match(FnParseContext ctx) {
  match_t rval(ctx);
  ////////////////////////////////////
  // check variable instantiation
  ////////////////////////////////////
  int i      = 0;
  auto tokDT = ctx.tokenValue(i);
  if (tokDT == "const") {
    tokDT = ctx.tokenValue(++i);
  }
  bool valid_dt         = ctx._container->validateTypeName(tokDT);
  auto tokN             = ctx.tokenValue(i + 1);
  bool valid_id         = ctx._container->validateIdentifierName(tokN);
  auto tokE             = ctx.tokenValue(i + 2);
  bool instantiation_ok = valid_dt and valid_id and (tokE == ";");
  ////////////////////////////////////
  if( instantiation_ok ) {
    rval._matched = true;
    rval._start   = ctx._startIndex;
    rval._count     = i + 2;
    assert(false);
  }
  return rval;
}

VariableDeclaration::parsed_t VariableDeclaration::parse(const match_t& m) {
  parsed_t rval;
  assert(false);
  return rval;
}
void VariableDeclaration::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
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

int VariableDefinitionStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
  assert(false);
  return 0;
}
void VariableDefinitionStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}*/

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
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

int VariableAssignmentStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
  assert(false);
  return 0;
}
void VariableAssignmentStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}
*/

/////////////////////////////////////////////////////////////////////////////////////////////////

ExpressionNode::match_t ExpressionNode::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}

ExpressionNode::parsed_t ExpressionNode::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
//void ExpressionNode::emit(shaderbuilder::BackEnd& backend) const {
  //assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

DeclarationList::match_t DeclarationList::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}
DeclarationList::parsed_t DeclarationList::parse(const match_t& match) {
  parsed_t rval;
  return rval;
}
void DeclarationList::emit(shaderbuilder::BackEnd& backend) const {
    for( auto c : _children )
        c->emit(backend);
    assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

Statement::match_t Statement::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}

Statement::parsed_t Statement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void Statement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

StatementList::match_t StatementList::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}

StatementList::parsed_t StatementList::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void StatementList::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CompoundStatement::match_t CompoundStatement::match(FnParseContext ctx) {
  match_t rval(ctx);
  int i = 0;
  if( ctx.tokenValue(i++) != "{" )
    return rval;
  ////////////////////////////////////
  ctx._startIndex++; // consume {
  ////////////////////////////////////
  if( auto mdl = DeclarationList::match(ctx) ){
    if( ctx.tokenValue(mdl._count) == "}" ){
      rval._matched = true;
      rval._count = mdl._count;
      rval._count++; // consume }
      rval._start = mdl._start;
      assert(false);
    }
    else {
      ctx = mdl.consume(); // consume mdl
    }
  }
  ////////////////////////////////////
  if( auto msl = StatementList::match(ctx)){
    if( ctx.tokenValue(msl._count) == "}" ){
      rval._matched = true;
      rval._count = msl._count;
      rval._start = msl._start;
      assert(false);
    }
    assert(false);
  }
  ////////////////////////////////////
  assert(false);
  return rval;
}

CompoundStatement::parsed_t CompoundStatement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void CompoundStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

std::string FnParseContext::tokenValue(size_t offset) const {
  return _view.token(_startIndex + offset)->text;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
