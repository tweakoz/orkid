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

match_shptr_t Statement::match(FnParseContext ctx) {
  match_shptr_t rval;;
  if (auto mvi = InstantiationStatement::match(ctx)) {
    rval = mvi;
  } else if (auto mve = ExpressionStatement::match(ctx)) {
    rval = mve;
  } else if (auto mvi = IterationStatement::match(ctx)) {
    rval = mvi;
  } else if (auto mrs = ReturnStatement::match(ctx)) {
    rval = mrs; // empty statement
  }
  return rval;
}

//parsed_t Statement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void Statement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t InstantiationStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  if (auto m = TypeName::match(ctx)) {
    auto ctx2 = m->consume();
    if (auto m2 = Identifier::match(ctx2)) {
      auto ctx3 = (m + m2)->consume();
      if (auto m3 = InitialAssignmentOperator::match(ctx3)) {
        auto ctx4 = (m + m2 + m3)->consume();
        if (auto m4 = Expression::match(ctx4)) {
          auto mfinal = (m + m2 + m3 + m4);
          auto& mf = *(mfinal.get());
          rval = std::make_shared<match_t>(mf);
        }
      }
    }
  }
  return rval;
}

//parsed_t InstantiationStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void InstantiationStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t ExpressionStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == ";") {
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_start   = ctx._startIndex;
    rval->_count   = 1;
    return rval;
  } else if (auto mve = Expression::match(ctx)) {
    rval = mve;
  }
  return rval;
}

//parsed_t ExpressionStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void ExpressionStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t IterationStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  if (auto mfs = ForLoopStatement::match(ctx)) {
    rval = mfs;
    return rval;
  }
  return rval;
}

//parsed_t IterationStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t ForLoopStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == "for") {
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_start   = ctx._startIndex;
    rval->_count   = 1;
    return rval;
  }
  return rval;
}

//parsed_t ForLoopStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void ForLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t StatementList::match(FnParseContext ctx) {
  match_shptr_t rval;
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  while (not done) {
    auto mvd = Statement::match(ctx);
    if (mvd) {
      if( not rval ){
        rval = std::make_shared<match_t>(ctx);
      }
      rval = rval + mvd;
      ctx = rval->consume();
      if (auto msemi = SemicolonOp::match(ctx)) {
        rval = rval + msemi;
        ctx = rval->consume();
      }
    } else {
      done = true;
    }
  }
  return rval;
}

//parsed_t StatementList::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void StatementList::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t CompoundStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  ////////////////////////////////////
  auto mfinal = OpenCurly::match(ctx);
  if (not mfinal)
    return rval;
  ////////////////////////////////////
  // empty statement ?
  ////////////////////////////////////
  if (auto mcb = CloseCurly::match(mfinal->consume())) {
    mfinal = mfinal + mcb;
    rval = std::make_shared<match_t>(*mfinal.get());
  }
  ////////////////////////////////////
  // declaration list optional
  ////////////////////////////////////
  if (auto mdl = DeclarationList::match(mfinal->consume()))
    mfinal = mfinal + mdl;
  ////////////////////////////////////
  // statement list mandatory
  ////////////////////////////////////
  auto msl = StatementList::match(mfinal->consume());
  assert(msl); // statement list non optional in this case
  mfinal = mfinal + msl;
  ////////////////////////////////////
  // closing bracket mandatory
  ////////////////////////////////////
  auto ctxx = mfinal->consume();
  auto mcb = CloseCurly::match(ctxx);
  mfinal   = mfinal + mcb;
  return std::make_shared<match_t>(*mfinal.get());
}

//parsed_t CompoundStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void CompoundStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_shptr_t ReturnStatement::match(FnParseContext ctx) {
  match_shptr_t rval;
  bool matched = ctx.tokenValue(0) == "return";
  if( matched ){
    auto ctx2 = ctx;
    ctx2._startIndex++;
    if( auto me = ExpressionNode::match(ctx2)){
      rval = std::make_shared<match_t>(*me.get());
      rval->_count++; //  consume return keyword
    }
  }
  return rval;
}

//int ReturnStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
//  assert(false);
  //return 0;
//}
//void ReturnStatement::emit(shaderbuilder::BackEnd& backend) const {
  //assert(false);
//}

/*

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
