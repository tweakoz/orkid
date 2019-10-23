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
/////////////////////////////////////////////////////////////////////////////////////////////////

Statement::match_t Statement::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto mvi = InstantiationStatement::match(ctx)) {
    rval = mvi;
  } else if (auto mve = ExpressionStatement::match(ctx)) {
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

InstantiationStatement::match_t InstantiationStatement::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto m = TypeName::match(ctx)) {
    auto ctx2 = m.consume();
    if (auto m2 = Identifier::match(ctx2)) {
      auto ctx3 = (m + m2).consume();
      if (auto m3 = InitialAssignmentOperator::match(ctx3)) {
        auto ctx4 = (m + m2 + m3).consume();
        if (auto m4 = Expression::match(ctx4)) {
          rval = (m + m2 + m3 + m4);
        }
      }
    }
  }
  return rval;
}

InstantiationStatement::parsed_t InstantiationStatement::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void InstantiationStatement::emit(shaderbuilder::BackEnd& backend) const {
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
  } else if (auto mve = Expression::match(ctx)) {
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
  auto mfinal = OpenCurly::match(ctx);
  if (not mfinal)
    return match_t(ctx);
  ////////////////////////////////////
  // empty statement ?
  ////////////////////////////////////
  if (auto mcb = CloseCurly::match(mfinal.consume())) {
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
  auto mcb = CloseCurly::match(mfinal.consume());
  mfinal   = mfinal + mcb;
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
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
