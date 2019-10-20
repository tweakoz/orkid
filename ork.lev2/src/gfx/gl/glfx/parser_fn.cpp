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
  auto open_tok = view.token(i);
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
  for (auto elem : _elements)
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
  if (instantiation_ok) {
    rval._matched = true;
    rval._start   = ctx._startIndex;
    rval._count   = i + 2;
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

Expression::match_t Expression::match(FnParseContext ctx) {
  match_t rval(ctx);
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  bool match   = true;
  while (not done) {
    auto mva = AssignmentExpression::match(ctx);
    if (mva) {
      count += mva._count;
      ctx = mva.consume();
    }
    auto try_tok = ctx.tokenValue(0);
    if (try_tok == ",") {
      ctx._startIndex++;
      count++;
    } else if (try_tok == ";") {
      done = true;
    } else {
      match = false;
    }
  }
  if (match) {
    rval._count   = count;
    rval._start   = start;
    rval._matched = true;
  }
  return rval;
}

Expression::parsed_t Expression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

AssignmentExpression::match_t AssignmentExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto mvc = ConditionalExpression::match(ctx)) {
    rval = mvc;
  } else if (auto mvu = UnaryExpression::match(ctx)) {
    size_t count = mvu._count;
    auto ctx2    = mvu.consume();
    if (auto mvo = AssignmentOperator::match(ctx2)) {
      count += mvo._count;
      auto ctx3 = mvo.consume();
      if (auto mva = AssignmentExpression::match(ctx3)) {
        count += mva._count;
        rval._count   = count;
        rval._start   = ctx._startIndex;
        rval._matched = true;
      }
    }
  }
  assert(false);
  return rval;
}

AssignmentExpression::parsed_t AssignmentExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

UnaryExpression::match_t UnaryExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}

UnaryExpression::parsed_t UnaryExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

ConditionalExpression::match_t ConditionalExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto mte = TernaryExpression::match(ctx)){
    rval = mte;
  }
  else if( auto mtl = LogicalOrExpression::match(ctx)){
    rval = mtl;
  }
  return rval;
}

ConditionalExpression::parsed_t ConditionalExpression::parse(const match_t& match) {
  parsed_t rval;

  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

TernaryExpression::match_t TernaryExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto mvl = LogicalOrExpression::match(ctx)){
    if(ctx.tokenValue(mvl.end())=="?"){
      auto ctx2 = mvl.consume();
      ctx2._startIndex++; // consume ?
      if(auto mve = Expression::match(ctx2)){
        if(ctx.tokenValue(mve.end())==":"){
          auto ctx3 = mve.consume();
          ctx3._startIndex++; // consume :
          if( auto mvx = ConditionalExpression::match(ctx3)){
            rval._matched = true;
            assert(false);
          }
        }
      }

    }
  }
  return rval;
}

TernaryExpression::parsed_t TernaryExpression::parse(const match_t& match) {
  parsed_t rval;

  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

LogicalOrExpression::match_t LogicalOrExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  assert(false);
  return rval;
}

LogicalOrExpression::parsed_t LogicalOrExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

AssignmentOperator::match_t AssignmentOperator::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "=") {
    auto op = ctx.tokenValue(1);
    if (op == "*=" or op == "+=" or op == "-=" or op == "/=" or op == "&=" or op == "|=" or op == "<<=" or op == ">>=" or
        op == "^=" ) {
      rval._start   = ctx._startIndex;
      rval._count   = 2;
      rval._matched = true;
    }
  }
  return rval;
}

AssignmentOperator::parsed_t AssignmentOperator::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

DeclarationList::match_t DeclarationList::match(FnParseContext ctx) {
  match_t rval(ctx);
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  while (not done) {
    auto mvd = VariableDeclaration::match(ctx);
    if (mvd) {
      count += mvd._count;
      count++; // consume }
      ctx = mvd.consume();
    } else {
      done = true;
    }
  }
  if (count) {
    rval._count   = count;
    rval._start   = start;
    rval._matched = true;
  }
  return rval;
}
DeclarationList::parsed_t DeclarationList::parse(const match_t& match) {
  parsed_t rval;
  return rval;
}
void DeclarationList::emit(shaderbuilder::BackEnd& backend) const {
  for (auto c : _children)
    c->emit(backend);
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

Statement::match_t Statement::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto mve = ExpressionStatement::match(ctx)) {
    rval = mve;
  } else if (auto mvi = IterationStatement::match(ctx)) {
    rval = mvi;
  }
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

ExpressionStatement::match_t ExpressionStatement::match(FnParseContext ctx) {
  match_t rval(ctx);
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == ";") {
    rval._matched = true;
    rval._start   = ctx._startIndex;
    rval._count   = 1;
    return rval;
  }
  else if (auto mve = Expression::match(ctx)) {
    rval = mve;
  }
  return rval;
}

ExpressionStatement::parsed_t ExpressionStatement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void ExpressionStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

IterationStatement::match_t IterationStatement::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto mfs = ForLoopStatement::match(ctx)) {
    rval = mfs;
    return rval;
  }
  return rval;
}

IterationStatement::parsed_t IterationStatement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

ForLoopStatement::match_t ForLoopStatement::match(FnParseContext ctx) {
  match_t rval(ctx);
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == "for") {
    rval._matched = true;
    rval._start   = ctx._startIndex;
    rval._count   = 1;
    return rval;
  }
  assert(false);
  return rval;
}

ForLoopStatement::parsed_t ForLoopStatement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void ForLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

StatementList::match_t StatementList::match(FnParseContext ctx) {
  match_t rval(ctx);
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  while (not done) {
    auto mvd = Statement::match(ctx);
    if (mvd) {
      count += mvd._count;
      count++; // consume }
      ctx = mvd.consume();
    } else {
      done = true;
    }
  }
  if (count) {
    rval._count   = count;
    rval._start   = start;
    rval._matched = true;
  }
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
  ////////////////////////////////////
  auto mfinal = OpenBracket::match(ctx);
  if( not mfinal )
    return match_t(ctx);
  ////////////////////////////////////
  // empty statement ?
  ////////////////////////////////////
  if(auto mcb = CloseBracket::match(mfinal.consume())) {
    mfinal = mfinal + mcb;
    return match_t(mfinal);
  }
  ////////////////////////////////////
  // declaration list optional
  ////////////////////////////////////
  if (auto mdl = DeclarationList::match(mfinal.consume()))
      mfinal = mfinal + mdl;
  ////////////////////////////////////
  // statement list mandatory
  ////////////////////////////////////
  auto msl = StatementList::match(mfinal.consume());
  assert(msl); // statement list non optional in this case
  mfinal = mfinal + msl;
  ////////////////////////////////////
  // closing bracket mandatory
  ////////////////////////////////////
  auto mcb = CloseBracket::match(mfinal.consume());
  mfinal = mfinal + mcb;
  return mfinal;
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

OpenBracket::match_t OpenBracket::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="{"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

OpenBracket::parsed_t OpenBracket::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void OpenBracket::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CloseBracket::match_t CloseBracket::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="}"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

CloseBracket::parsed_t CloseBracket::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void CloseBracket::emit(shaderbuilder::BackEnd& backend) const {
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
